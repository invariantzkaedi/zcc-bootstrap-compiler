#!/usr/bin/env python3
"""
Native Zero-Toolchain Win64 Verification Suite for Lua 5.4.6 & SQLite 3.45.0
=============================================================================
Verifies standalone native Windows PE32+ executables (`build_win64/lua.exe` and
`build_win64/sqlite3.exe`) built with zero MSVC/MinGW/binutils by BOTH host
`./zcc` and native Windows sovereign `build_win64/zcc_sov_stage3.exe`:
  1. PE32+ structural audit (.text RX, .data RW, .pdata, .reloc, StackReserve>=16MB,
     TimeDateStamp==0) + `msvcrt.dll`-only imports verified against host
     `C:\\Windows\\System32\\msvcrt.dll` export table.
  2. Byte-exact sovereign convergence between `./zcc` and `zcc_sov_stage3.exe`
     via SHA-256 and native Windows `fc /b`.
  3. Native Windows Lua 5.4.6 execution (`-e`, `<script.lua>`, `dofile()`, `stdin`)
     including 32-bit ints (LUA_32BITS=1), floats, closures, metamethods, GC,
     `load()`, and real Microsoft x64 `setjmp`/`longjmp` `pcall`/`xpcall`/`error`
     stack unwinding, differentially checked against `build_wasm/lua.wasm`.
  4. Native Windows SQLite 3.45.0 (`:memory:`, `-f`, inline SQL, `stdin`) across
     joins, CTEs, window functions, triggers, and transactions, differentially
     checked against host CPython `sqlite3`.
  5. Persistent on-disk `.db` roundtrip interoperability between two separate
     `sqlite3.exe` runs and independent Python `sqlite3.connect()`.
  6. ZCC-EF-0 negative controls (syntax errors, uncaught errors, missing files,
     and 1-byte PE mutation caught by SHA-256 + `fc /b`).
"""

import json
import os
import shutil
import sqlite3
import subprocess
import sys
import tempfile
import unittest

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TOOLS_DIR = os.path.join(REPO_ROOT, "tools")
if TOOLS_DIR not in sys.path:
    sys.path.insert(0, TOOLS_DIR)

from build_lua_win64 import (  # noqa: E402
    BUILD_DIR,
    MSVCRT_PATH,
    audit_pe32plus_msvcrt,
    parse_pe_exports,
    run_fc_b,
    sha256_file,
    to_rel_win,
)

LUA_EXE = os.path.join(BUILD_DIR, "lua.exe")
LUA_HOST_EXE = os.path.join(BUILD_DIR, "lua_host_zcc.exe")
SQLITE_EXE = os.path.join(BUILD_DIR, "sqlite3.exe")
SQLITE_HOST_EXE = os.path.join(BUILD_DIR, "sqlite3_host_zcc.exe")
LUA_WASM = os.path.join(REPO_ROOT, "build_wasm", "lua.wasm")


def find_node_bin() -> str:
    candidate = "/home/ZKAEDI/.local/bin/node"
    if os.path.isfile(candidate) and os.access(candidate, os.X_OK):
        return candidate
    found = shutil.which("node")
    if found:
        return found
    raise RuntimeError("node binary not found")


def run_win64_exe(exe_path: str, args: list[str], stdin_text: str | None = None, timeout_s: int = 60) -> subprocess.CompletedProcess:
    return subprocess.run(
        [exe_path] + list(args),
        input=stdin_text,
        cwd=REPO_ROOT,
        capture_output=True,
        text=True,
        timeout=timeout_s,
    )


def run_lua_wasm_node(node_bin: str, args: list[str], stdin_text: str = "") -> subprocess.CompletedProcess:
    js = r"""
const fs = require('fs');
const { WASI } = require('node:wasi');
const cfg = JSON.parse(fs.readFileSync(process.argv[1], 'utf8'));
const w = new WASI({ version: 'preview1', args: ['lua.wasm', ...cfg.args], env: {}, preopens: { '.': '.' }, returnOnExit: true });
(async () => {
  const mod = await WebAssembly.compile(fs.readFileSync(cfg.wasm));
  const raw = w.getImportObject();
  const imp = { wasi_snapshot_preview1: {} };
  for (const [k, v] of Object.entries(raw.wasi_snapshot_preview1)) {
    imp.wasi_snapshot_preview1[k] = (...a) => v.apply(w, a);
  }
  const inst = await WebAssembly.instantiate(mod, imp);
  process.exit(w.start(inst) | 0);
})().catch((e) => { console.error(e); process.exit(99); });
"""
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as tf:
        json.dump({"wasm": LUA_WASM, "args": args}, tf)
        cfg_path = tf.name
    try:
        return subprocess.run(
            [node_bin, "--no-warnings", "-e", js, cfg_path],
            input=stdin_text,
            cwd=REPO_ROOT,
            capture_output=True,
            text=True,
            timeout=60,
        )
    finally:
        if os.path.exists(cfg_path):
            os.remove(cfg_path)


LUA_WIN64_SCRIPT = r"""
-- 1. 32-bit integer (LUA_32BITS=1) & IEEE-754 arithmetic
local big = (1 << 29) + 12345678
local small = -((1 << 28) + 77)
assert(big + small == 280781057, "32-bit int arith mismatch")
local fsum = 0.0
for i = 1, 10 do
    fsum = fsum + (1.0 / (i * i))
end
assert(fsum > 1.549 and fsum < 1.551, "float sum mismatch")

-- 2. Closures, metatables, tables & GC churn
local function make_acc(start)
    local total = start
    return function(d)
        total = total + d
        return total
    end
end
local acc = make_acc(100)
local vec_mt = {
    __add = function(a, b) return setmetatable({ x = a.x + b.x, y = a.y + b.y }, getmetatable(a)) end,
    __tostring = function(v) return "(" .. v.x .. "," .. v.y .. ")" end,
}
local v1 = setmetatable({ x = 3, y = 4 }, vec_mt)
local v2 = setmetatable({ x = 7, y = -2 }, vec_mt)
local v3 = v1 + v2

local churn = {}
for i = 1, 2000 do
    churn[i] = { idx = i, label = "item_" .. (i % 17) }
end
churn = nil
collectgarbage("collect")

-- 3. Native Microsoft x64 setjmp/longjmp pcall + xpcall + recursive unwind
local ok1, err1 = pcall(function() error("win64_pcall_boom", 0) end)
assert(ok1 == false and err1 == "win64_pcall_boom", "pcall string mismatch: " .. tostring(err1))

local ok2, err2 = pcall(function() error({ code = 503, tag = "NET_ERR" }) end)
assert(ok2 == false and type(err2) == "table" and err2.code == 503 and err2.tag == "NET_ERR")

local xp_ran = false
local ok3, err3 = xpcall(function()
    local bad = nil
    return bad + 10
end, function(msg)
    xp_ran = true
    return "XPHANDLED:" .. (type(msg) == "string" and "STR" or "OTHER")
end)
assert(ok3 == false and xp_ran and err3 == "XPHANDLED:STR")

local function rec_unwind(depth)
    if depth == 0 then error("win64_deep_boom", 0) end
    return rec_unwind(depth - 1) + 1
end
local ok4, err4 = pcall(rec_unwind, 12)
assert(ok4 == false and err4 == "win64_deep_boom")

-- 4. 100-iteration pcall stack-pointer stability + load()
local caught = 0
for i = 1, 100 do
    local ok, e = pcall(function() error("iter_" .. i, 0) end)
    if (not ok) and e == ("iter_" .. i) then caught = caught + 1 end
end
assert(caught == 100)

local fn, lerr = load("return function(x) return x * x + 5 end")
assert(fn and lerr == nil)
local sq5 = fn()

print("INT32", big, big + small)
print("FP", fsum)
print("VEC", tostring(v3), acc(25), acc(15))
print("EH", err1, err2.code, err3, err4, caught, sq5(9))
"""


class TestWin64LuaSqlite(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        for req in (LUA_EXE, LUA_HOST_EXE, SQLITE_EXE, SQLITE_HOST_EXE):
            if not os.path.isfile(req):
                raise RuntimeError(f"Missing {req}; run build_lua_win64.py and build_sqlite_win64.py first.")
        cls.tmp_dir = tempfile.mkdtemp(prefix="_test_tmp_", dir=BUILD_DIR)
        cls.node_bin = find_node_bin() if os.path.isfile(LUA_WASM) else None

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp_dir, ignore_errors=True)

    def _rel_win(self, path: str) -> str:
        return to_rel_win(path)

    def test_01_pe32plus_audit_and_system32_msvcrt_export_check(self):
        lua_audit = audit_pe32plus_msvcrt(LUA_EXE)
        sq_audit = audit_pe32plus_msvcrt(SQLITE_EXE)
        self.assertGreater(len(lua_audit["msvcrt_imports"]), 20)
        self.assertGreater(len(sq_audit["msvcrt_imports"]), 20)
        if os.path.isfile(MSVCRT_PATH):
            exps = parse_pe_exports(MSVCRT_PATH)
            for sym in lua_audit["msvcrt_imports"] + sq_audit["msvcrt_imports"]:
                self.assertIn(sym, exps, f"Missing import {sym} in {MSVCRT_PATH}")

    def test_02_sovereign_cross_vs_native_byte_identity(self):
        for host_p, win_p, label in (
            (LUA_HOST_EXE, LUA_EXE, "lua.exe"),
            (SQLITE_HOST_EXE, SQLITE_EXE, "sqlite3.exe"),
        ):
            self.assertEqual(
                sha256_file(host_p),
                sha256_file(win_p),
                f"{label} SHA-256 mismatch between ./zcc and zcc_sov_stage3.exe",
            )
            fc = run_fc_b(host_p, win_p)
            self.assertEqual(fc.returncode, 0, f"fc /b failed for {label}: {fc.stdout}\n{fc.stderr}")
            self.assertIn("no differences encountered", fc.stdout.lower())

    def test_03_lua54_native_win64_pcall_and_differential_vs_wasm(self):
        # Test -e directly
        r_e = run_win64_exe(LUA_EXE, ["-e", LUA_WIN64_SCRIPT])
        self.assertEqual(r_e.returncode, 0, f"lua.exe -e failed ({r_e.returncode}):\n{r_e.stdout}\n{r_e.stderr}")
        out_e = r_e.stdout.replace("\r\n", "\n")
        self.assertIn("INT32\t549216590\t280781057\n", out_e)
        self.assertIn("VEC\t(10,2)\t125\t140", out_e)
        self.assertIn("EH\twin64_pcall_boom\t503\tXPHANDLED:STR\twin64_deep_boom\t100\t86", out_e)

        # Differential match vs build_wasm/lua.wasm if available
        if self.node_bin and os.path.isfile(LUA_WASM):
            rw = run_lua_wasm_node(self.node_bin, ["-e", LUA_WIN64_SCRIPT])
            self.assertEqual(rw.returncode, 0, f"lua.wasm failed: {rw.stderr}")
            self.assertEqual(out_e, rw.stdout.replace("\r\n", "\n"), "Differential mismatch: lua.exe vs lua.wasm")

        # Test <script.lua> + dofile() + stdin inside BUILD_DIR (native Windows paths!)
        sub_path = os.path.join(self.tmp_dir, "helper.lua")
        main_path = os.path.join(self.tmp_dir, "main.lua")
        with open(sub_path, "w", encoding="utf-8", newline="\n") as f:
            f.write("return function(x) return x * 3 + 1 end\n")
        sub_rel = os.path.relpath(sub_path, REPO_ROOT).replace("\\", "/")
        with open(main_path, "w", encoding="utf-8", newline="\n") as f:
            f.write(f'local fn = dofile("{sub_rel}")\nlocal ok, e = pcall(function() error("FILE_OK", 0) end)\nprint("FILE", fn(14), ok, e)\n')

        rf = run_win64_exe(LUA_EXE, [self._rel_win(main_path)])
        self.assertEqual(rf.returncode, 0, f"lua.exe file mode failed:\n{rf.stdout}\n{rf.stderr}")
        self.assertEqual(rf.stdout.replace("\r\n", "\n").strip(), "FILE\t43\tfalse\tFILE_OK")

        rs = run_win64_exe(LUA_EXE, ["-"], stdin_text='print("STDIN_WIN64", 6 * 7)\n')
        self.assertEqual(rs.returncode, 0, f"lua.exe stdin failed:\n{rs.stderr}")
        self.assertEqual(rs.stdout.replace("\r\n", "\n").strip(), "STDIN_WIN64\t42")

    def test_04_sqlite3_native_win64_memory_differential_vs_cpython(self):
        stmts = [
            "CREATE TABLE cats(cat TEXT PRIMARY KEY, label TEXT)",
            "INSERT INTO cats VALUES('core', 'Compiler'), ('io', 'Runtime'), ('unused', 'Empty')",
            "CREATE TABLE items(id INTEGER PRIMARY KEY, cat TEXT, price REAL, qty INT)",
            "CREATE TABLE audit(msg TEXT)",
            "CREATE TRIGGER trg_items_ins AFTER INSERT ON items BEGIN INSERT INTO audit VALUES('added:' || NEW.cat); END",
            "INSERT INTO items(cat, price, qty) VALUES ('core', 10.5, 2), ('core', 25.0, 1), ('io', 4.25, 8), ('io', NULL, 3)",
            "BEGIN",
            "DELETE FROM items WHERE cat = 'core'",
            "ROLLBACK",
            "WITH RECURSIVE cnt(n) AS (SELECT 1 UNION ALL SELECT n + 1 FROM cnt WHERE n < 5) SELECT n, n * n FROM cnt ORDER BY n",
            "SELECT c.label, i.id, i.qty FROM items i INNER JOIN cats c ON c.cat = i.cat ORDER BY i.id",
            "SELECT c.cat, COUNT(i.id) FROM cats c LEFT JOIN items i ON c.cat = i.cat GROUP BY c.cat ORDER BY c.cat",
            "SELECT cat, COUNT(*), COALESCE(SUM(price * qty), -1) FROM items GROUP BY cat HAVING COUNT(*) >= 1 ORDER BY cat",
            "SELECT id, cat, ROW_NUMBER() OVER (ORDER BY id) AS rn, SUM(qty) OVER (PARTITION BY cat ORDER BY id) AS run_qty, CASE WHEN price IS NULL THEN 'MISSING' ELSE 'OK' END FROM items ORDER BY id",
            "SELECT msg FROM audit ORDER BY rowid",
        ]
        sql_script = ";\n".join(stmts) + ";\n"
        sql_path = os.path.join(self.tmp_dir, "diff_queries.sql")
        with open(sql_path, "w", encoding="utf-8", newline="\n") as f:
            f.write(sql_script)

        con = sqlite3.connect(":memory:", isolation_level=None)
        expected = []
        for s in stmts:
            cur = con.execute(s)
            if cur.description:
                for row in cur.fetchall():
                    expected.append("|".join("NULL" if v is None else str(v) for v in row))
        con.close()

        rw = run_win64_exe(SQLITE_EXE, [":memory:", "-f", self._rel_win(sql_path)])
        self.assertEqual(rw.returncode, 0, f"sqlite3.exe -f failed:\n{rw.stdout}\n{rw.stderr}")
        got = rw.stdout.replace("\r\n", "\n").strip().splitlines()
        self.assertEqual(got, expected, f"Differential mismatch vs Python sqlite3:\ngot={got}\nexp={expected}")

        # Inline CLI arg + stdin verification
        r_inline = run_win64_exe(SQLITE_EXE, [":memory:", "SELECT sqlite_version() IS NOT NULL, 6*7, NULL;"])
        self.assertEqual(r_inline.returncode, 0, r_inline.stderr)
        self.assertEqual(r_inline.stdout.replace("\r\n", "\n").strip(), "1|42|NULL")

        r_stdin = run_win64_exe(SQLITE_EXE, [":memory:"], stdin_text="SELECT 'STDIN_SQ', typeof(3.14), hex(255), hex(X'FF');\n")
        self.assertEqual(r_stdin.returncode, 0, r_stdin.stderr)
        self.assertEqual(r_stdin.stdout.replace("\r\n", "\n").strip(), "STDIN_SQ|real|323535|FF")

    def test_05_sqlite3_native_win64_persistent_db_and_python_interop(self):
        db_path = os.path.join(self.tmp_dir, "persist_win64.db")
        db_rel = self._rel_win(db_path)

        r1 = run_win64_exe(
            SQLITE_EXE,
            [db_rel, "CREATE TABLE kv(k TEXT PRIMARY KEY, v INT); INSERT INTO kv VALUES('alpha', 10), ('beta', 20);"],
        )
        self.assertEqual(r1.returncode, 0, f"Process 1 create failed:\n{r1.stdout}\n{r1.stderr}")
        self.assertTrue(os.path.isfile(db_path) and os.path.getsize(db_path) > 0)

        r2 = run_win64_exe(
            SQLITE_EXE,
            [db_rel, "UPDATE kv SET v = v + 5 WHERE k = 'beta'; INSERT INTO kv VALUES('gamma', 30); SELECT k, v FROM kv ORDER BY k;"],
        )
        self.assertEqual(r2.returncode, 0, f"Process 2 query failed:\n{r2.stdout}\n{r2.stderr}")
        self.assertEqual(
            r2.stdout.replace("\r\n", "\n").strip().splitlines(),
            ["alpha|10", "beta|25", "gamma|30"],
        )

        # Open native Windows sqlite3.exe-created DB directly with CPython sqlite3
        con = sqlite3.connect(db_path)
        rows = con.execute("SELECT k, v FROM kv ORDER BY k").fetchall()
        con.close()
        self.assertEqual(rows, [("alpha", 10), ("beta", 25), ("gamma", 30)])

    def test_06_ef0_negative_controls(self):
        # NC-1: Lua syntax error -> rc == 2
        nc1 = run_win64_exe(LUA_EXE, ["-e", "function broken("])
        self.assertEqual(nc1.returncode, 2)
        self.assertIn("LUA_SYNTAX_ERROR", nc1.stderr)

        # NC-2: Lua uncaught error() -> rc == 3
        nc2 = run_win64_exe(LUA_EXE, ["-e", 'error("ef0_lua_boom", 0)'])
        self.assertEqual(nc2.returncode, 3)
        self.assertIn("LUA_RUNTIME_ERROR", nc2.stderr)
        self.assertIn("ef0_lua_boom", nc2.stderr)

        # NC-3: Lua missing file -> rc == 1
        nc3 = run_win64_exe(LUA_EXE, [r"build_win64\_does_not_exist_ef0.lua"])
        self.assertEqual(nc3.returncode, 1)
        self.assertIn("LUA_ERROR", nc3.stderr)

        # NC-4: SQLite invalid SQL -> rc == 1
        nc4 = run_win64_exe(SQLITE_EXE, [":memory:", "SELEC BROKEN SYNTAX;"])
        self.assertEqual(nc4.returncode, 1)
        self.assertIn("SQLITE_ERROR", nc4.stderr)

        # NC-5: SQLite missing -f SQL file -> rc == 1
        nc5 = run_win64_exe(SQLITE_EXE, [":memory:", "-f", r"build_win64\_missing_ef0.sql"])
        self.assertEqual(nc5.returncode, 1)
        self.assertIn("SQLITE_ERROR", nc5.stderr)

        # NC-6: 1-byte PE mutation detected by SHA-256 and Windows fc /b
        for src_exe, name in ((LUA_EXE, "mut_lua.exe"), (SQLITE_EXE, "mut_sq.exe")):
            mut = os.path.join(self.tmp_dir, name)
            with open(src_exe, "rb") as f:
                raw = bytearray(f.read())
            raw[len(raw) // 2] ^= 0xFF
            with open(mut, "wb") as f:
                f.write(raw)
            self.assertNotEqual(sha256_file(mut), sha256_file(src_exe))
            fc = run_fc_b(src_exe, mut)
            self.assertNotEqual(fc.returncode, 0, f"fc /b failed to catch 1-byte mutation on {name}")


if __name__ == "__main__":
    unittest.main(verbosity=2)
