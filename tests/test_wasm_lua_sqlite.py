#!/usr/bin/env python3
"""
Standalone Lua 5.4.6 & SQLite 3.45.0 WASI Dual-Runtime Test Suite
=================================================================
Independently verifies `build_wasm/lua.wasm` and `build_wasm/sqlite3.wasm` across:
  1. V8 `WebAssembly.validate()` + `new WebAssembly.Module()` + zero `env.*` imports
  2. Lua 5.4.6 execution (if/else, recursive fib, closures, metatables, WASI file I/O)
     on BOTH Node 22 `node:wasi` and pure-JS `tools/browser_wasi_shim.js`
  3. SQLite 3.45.0 `:memory:` DDL/DML/JOIN/GROUP BY/CTE/Transaction workload
     differentially checked against Python's stdlib `sqlite3` module on BOTH hosts
  4. SQLite 3.45.0 persistent WASI file DB across separate instantiations on BOTH hosts
  5. EF-0 negative controls (Lua syntax/assert errors, SQL errors, WASM byte mutation)
"""

import json
import os
import shutil
import sqlite3
import subprocess
import tempfile
import unittest

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD_WASM = os.path.join(REPO_ROOT, "build_wasm")
LUA_WASM = os.path.join(BUILD_WASM, "lua.wasm")
LUA_NATIVE_WASM = os.path.join(BUILD_WASM, "lua_native_zcc.wasm")
SQLITE_WASM = os.path.join(BUILD_WASM, "sqlite3.wasm")
SHIM_JS = os.path.join(REPO_ROOT, "tools", "browser_wasi_shim.js")


def find_node_bin() -> str:
    found = shutil.which("node")
    if found:
        return found
    candidate = "/home/ZKAEDI/.local/bin/node"
    if os.path.isfile(candidate) and os.access(candidate, os.X_OK):
        return candidate
    raise RuntimeError("node binary not found on PATH or at /home/ZKAEDI/.local/bin/node")


def inspect_wasm_v8(node_bin: str, wasm_path: str) -> dict:
    js = r"""
const fs = require('fs');
const b = fs.readFileSync(process.argv[1]);
const valid = WebAssembly.validate(b);
let compiled = false;
let imports = [];
let err = null;
if (valid) {
  try {
    const mod = new WebAssembly.Module(b);
    compiled = true;
    imports = WebAssembly.Module.imports(mod).map(i => `${i.module}.${i.name}`);
  } catch (e) { err = String(e); }
}
console.log(JSON.stringify({valid, compiled, bytes: b.length, imports, err}));
"""
    res = subprocess.run([node_bin, "-e", js, wasm_path], cwd=REPO_ROOT, capture_output=True, text=True, check=True)
    return json.loads(res.stdout.strip().splitlines()[-1])


def run_node_wasi(node_bin: str, wasm_path: str, args, preopen_dir: str = None, stdin_str: str = ""):
    work_dir = preopen_dir or REPO_ROOT
    js = r"""
const fs = require('fs');
const { WASI } = require('node:wasi');
const cfg = JSON.parse(fs.readFileSync(process.argv[1], 'utf8'));
const w = new WASI({
  version: 'preview1',
  args: cfg.args,
  env: {},
  preopens: { '.': cfg.preopen },
  returnOnExit: true,
});
const imp = w.getImportObject();
const wasi = imp.wasi_snapshot_preview1;
for (const k of Object.keys(wasi)) {
  const orig = wasi[k];
  wasi[k] = (...a) => orig.apply(wasi, a);
}
(async () => {
  const bytes = fs.readFileSync(cfg.wasm);
  const mod = await WebAssembly.compile(bytes);
  const inst = await WebAssembly.instantiate(mod, imp);
  const rc = w.start(inst);
  process.exit(rc | 0);
})().catch((e) => { console.error(e); process.exit(99); });
"""
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as tf:
        json.dump({"wasm": os.path.abspath(wasm_path), "args": args, "preopen": "."}, tf)
        cfg_path = tf.name
    try:
        return subprocess.run([node_bin, "-e", js, cfg_path], cwd=work_dir, input=stdin_str, capture_output=True, text=True)
    finally:
        if os.path.exists(cfg_path):
            os.remove(cfg_path)


def run_browser_shim(node_bin: str, wasm_path: str, args, files=None, out_files=None, stdin_str: str = "") -> dict:
    js = r"""
const fs = require('fs');
const { BrowserWasiShim } = require(process.argv[1]);
const cfg = JSON.parse(fs.readFileSync(process.argv[2], 'utf8'));
(async () => {
  const bytes = fs.readFileSync(cfg.wasm);
  const vfsFiles = {};
  for (const [k, b64] of Object.entries(cfg.filesB64 || {})) {
    vfsFiles[k] = Buffer.from(b64, 'base64');
  }
  const shim = new BrowserWasiShim({
    args: cfg.args,
    files: vfsFiles,
    stdin: cfg.stdin || '',
  });
  let rc = -1;
  let trap = null;
  try {
    const res = await shim.run(bytes);
    rc = res.exitCode;
  } catch (e) {
    trap = String(e);
  }
  const exported = {};
  for (const k of (cfg.outFiles || [])) {
    if (shim.vfs.has(k)) {
      exported[k] = Buffer.from(shim.vfs.get(k)).toString('base64');
    }
  }
  console.log('__RESULT__' + JSON.stringify({
    exitCode: rc,
    stdout: shim.getStdout(),
    stderr: shim.getStderr(),
    trap,
    vfsKeys: Array.from(shim.vfs.keys()),
    exported,
  }));
})();
"""
    import base64
    files_b64 = {}
    if files:
        for k, v in files.items():
            raw = v.encode("utf-8") if isinstance(v, str) else bytes(v)
            files_b64[k] = base64.b64encode(raw).decode("ascii")
    with tempfile.NamedTemporaryFile("w", suffix=".json", delete=False) as tf:
        json.dump(
            {
                "wasm": os.path.abspath(wasm_path),
                "args": args,
                "filesB64": files_b64,
                "outFiles": list(out_files or []),
                "stdin": stdin_str,
            },
            tf,
        )
        cfg_path = tf.name
    try:
        res = subprocess.run([node_bin, "-e", js, SHIM_JS, cfg_path], cwd=REPO_ROOT, capture_output=True, text=True, check=True)
        for line in reversed(res.stdout.splitlines()):
            if line.startswith("__RESULT__"):
                return json.loads(line[len("__RESULT__"):])
        raise RuntimeError(f"Missing __RESULT__ marker in shim output: {res.stdout}\n{res.stderr}")
    finally:
        if os.path.exists(cfg_path):
            os.remove(cfg_path)


LUA_WORKLOAD = r"""
local function fib(n)
  if n <= 1 then return n end
  return fib(n - 1) + fib(n - 2)
end

local function make_adder(x)
  return function(y) return x + y end
end

local add10 = make_adder(10)
local mt = {
  __add = function(a, b)
    return setmetatable({v = a.v + b.v}, getmetatable(a))
  end
}
local t1 = setmetatable({v = 14}, mt)
local t2 = setmetatable({v = 28}, mt)
local t3 = t1 + t2

local sum = 0
local arr = {2, 4, 6, 8, 10}
for _, v in ipairs(arr) do
  sum = sum + v
end

local branch = "bad"
if fib(15) == 610 and sum == 30 then
  branch = "branch_ok"
elseif sum > 0 then
  branch = "elseif_wrong"
end

assert(fib(15) == 610, "fib failed")
assert(t3.v == 42, "metatable failed")
print("LUA_OK", fib(15), add10(32), t3.v, sum, branch, tostring(3.5 * 2))
"""

SQL_WORKLOAD = """
CREATE TABLE dept(id INTEGER PRIMARY KEY, dname TEXT);
CREATE TABLE emp(id INTEGER PRIMARY KEY, dept_id INT, name TEXT, salary INT);
INSERT INTO dept VALUES (1, 'Eng'), (2, 'Ops'), (3, 'Research');
INSERT INTO emp VALUES
  (10, 1, 'Ada', 150),
  (11, 1, 'Grace', 175),
  (12, 2, 'Linus', 140),
  (13, 3, 'Alan', 200);
BEGIN;
INSERT INTO emp VALUES (99, 2, 'Temp', 1);
ROLLBACK;
BEGIN;
INSERT INTO emp VALUES (14, 2, 'Ken', 160);
COMMIT;
SELECT d.dname, COUNT(e.id), SUM(e.salary)
  FROM dept d INNER JOIN emp e ON d.id = e.dept_id
  GROUP BY d.dname
  ORDER BY SUM(e.salary) DESC;
WITH RECURSIVE seq(n) AS (SELECT 1 UNION ALL SELECT n + 1 FROM seq WHERE n < 6)
  SELECT SUM(n), COUNT(*) FROM seq;
"""


def sqlite_python_oracle(sql_text: str) -> str:
    conn = sqlite3.connect(":memory:", isolation_level=None)
    cur = conn.cursor()
    out_lines = []
    # Execute statement by statement matching sqlite3_exec pipe format
    buf = ""
    for line in sql_text.splitlines():
        s = line.strip()
        if not s:
            continue
        buf += (" " if buf else "") + s
        if sqlite3.complete_statement(buf):
            cur.execute(buf)
            if cur.description is not None:
                for row in cur.fetchall():
                    out_lines.append("|".join("NULL" if v is None else str(v) for v in row))
            buf = ""
    conn.close()
    return "\n".join(out_lines) + ("\n" if out_lines else "")


class TestWasmLuaSqlite(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.node_bin = find_node_bin()
        if not os.path.isfile(LUA_WASM):
            raise RuntimeError(f"Missing {LUA_WASM}; run `python3 tools/build_lua_wasm.py` first.")
        if not os.path.isfile(SQLITE_WASM):
            raise RuntimeError(f"Missing {SQLITE_WASM}; run `python3 tools/build_sqlite_wasm.py` first.")

    def test_01_v8_validation_zero_env_imports(self):
        for label, path in (("lua.wasm", LUA_WASM), ("sqlite3.wasm", SQLITE_WASM)):
            info = inspect_wasm_v8(self.node_bin, path)
            self.assertTrue(info["valid"], f"{label} failed V8 WebAssembly.validate()")
            self.assertTrue(info["compiled"], f"{label} failed V8 WebAssembly.Module(): {info['err']}")
            non_wasi = [i for i in info["imports"] if not i.startswith("wasi_snapshot_preview1.")]
            self.assertEqual(non_wasi, [], f"{label} leaked non-WASI imports: {non_wasi}")
            self.assertGreaterEqual(len(info["imports"]), 10)

    def test_02_lua54_dual_host_execution_and_file_vfs(self):
        expected = "LUA_OK\t610\t42\t42\t30\tbranch_ok\t7.0\n"
        with tempfile.TemporaryDirectory() as tmpd:
            # 1. Node 22 node:wasi (-e)
            r_node = run_node_wasi(self.node_bin, LUA_WASM, ["lua.wasm", "-e", LUA_WORKLOAD], preopen_dir=tmpd)
            self.assertEqual(r_node.returncode, 0, r_node.stderr)
            self.assertEqual(r_node.stdout, expected)

            # 2. Pure-JS BrowserWasiShim (-e)
            r_shim = run_browser_shim(self.node_bin, LUA_WASM, ["lua.wasm", "-e", LUA_WORKLOAD])
            self.assertIsNone(r_shim["trap"])
            self.assertEqual(r_shim["exitCode"], 0, r_shim["stderr"])
            self.assertEqual(r_shim["stdout"], expected)

            # 3. Native-compiled vs in-WASI-compiled parity (if lua_native_zcc.wasm exists)
            if os.path.isfile(LUA_NATIVE_WASM):
                r_nat = run_browser_shim(self.node_bin, LUA_NATIVE_WASM, ["lua.wasm", "-e", LUA_WORKLOAD])
                self.assertEqual(r_nat["exitCode"], 0, r_nat["stderr"])
                self.assertEqual(r_nat["stdout"], r_shim["stdout"])

            # 4. WASI file script execution across both runtimes
            script_txt = "local s = 0; for i = 1, 10 do s = s + i end; print('FILE_OK', s)\n"
            script_path = os.path.join(tmpd, "hello.lua")
            with open(script_path, "w", encoding="utf-8") as f:
                f.write(script_txt)
            r_fnode = run_node_wasi(self.node_bin, LUA_WASM, ["lua.wasm", "hello.lua"], preopen_dir=tmpd)
            self.assertEqual(r_fnode.returncode, 0, r_fnode.stderr)
            self.assertEqual(r_fnode.stdout, "FILE_OK\t55\n")

            r_fshim = run_browser_shim(self.node_bin, LUA_WASM, ["lua.wasm", "hello.lua"], files={"hello.lua": script_txt})
            self.assertEqual(r_fshim["exitCode"], 0, r_fshim["stderr"])
            self.assertEqual(r_fshim["stdout"], "FILE_OK\t55\n")

    def test_03_sqlite3_memory_differential_dual_host(self):
        expected_rows = sqlite_python_oracle(SQL_WORKLOAD)
        self.assertEqual(expected_rows, "Eng|2|325\nOps|2|300\nResearch|1|200\n21|6\n")

        with tempfile.TemporaryDirectory() as tmpd:
            # Version query
            r_v = run_browser_shim(self.node_bin, SQLITE_WASM, ["sqlite3.wasm", ":memory:", "SELECT sqlite_version();"])
            self.assertEqual(r_v["exitCode"], 0, r_v["stderr"])
            self.assertEqual(r_v["stdout"], "3.45.0\n")

            # Node 22 node:wasi differential check
            r_node = run_node_wasi(self.node_bin, SQLITE_WASM, ["sqlite3.wasm", ":memory:", SQL_WORKLOAD], preopen_dir=tmpd)
            self.assertEqual(r_node.returncode, 0, r_node.stderr)
            self.assertEqual(r_node.stdout, expected_rows)

            # Pure-JS BrowserWasiShim differential check
            r_shim = run_browser_shim(self.node_bin, SQLITE_WASM, ["sqlite3.wasm", ":memory:", SQL_WORKLOAD])
            self.assertIsNone(r_shim["trap"])
            self.assertEqual(r_shim["exitCode"], 0, r_shim["stderr"])
            self.assertEqual(r_shim["stdout"], expected_rows)

    def test_04_sqlite3_persistent_wasi_file_db(self):
        import base64
        create_sql = "CREATE TABLE kv(k TEXT PRIMARY KEY, v INT); INSERT INTO kv VALUES('alpha', 100), ('beta', 200);"
        query_sql = "SELECT k, v FROM kv ORDER BY v;"

        # 1. Node 22 node:wasi disk persistence
        with tempfile.TemporaryDirectory() as tmpd:
            r1 = run_node_wasi(self.node_bin, SQLITE_WASM, ["sqlite3.wasm", "persist.db", create_sql], preopen_dir=tmpd)
            self.assertEqual(r1.returncode, 0, r1.stderr)
            db_file = os.path.join(tmpd, "persist.db")
            self.assertTrue(os.path.isfile(db_file))
            with open(db_file, "rb") as f:
                hdr = f.read(16)
            self.assertEqual(hdr, b"SQLite format 3\x00")
            r2 = run_node_wasi(self.node_bin, SQLITE_WASM, ["sqlite3.wasm", "persist.db", query_sql], preopen_dir=tmpd)
            self.assertEqual(r2.returncode, 0, r2.stderr)
            self.assertEqual(r2.stdout, "alpha|100\nbeta|200\n")

        # 2. Pure-JS BrowserWasiShim in-memory VFS persistence
        s1 = run_browser_shim(
            self.node_bin,
            SQLITE_WASM,
            ["sqlite3.wasm", "persist.db", create_sql],
            out_files=["persist.db"],
        )
        self.assertEqual(s1["exitCode"], 0, s1["stderr"])
        self.assertIn("persist.db", s1["exported"])
        db_bytes = base64.b64decode(s1["exported"]["persist.db"])
        self.assertTrue(db_bytes.startswith(b"SQLite format 3\x00"))

        s2 = run_browser_shim(
            self.node_bin,
            SQLITE_WASM,
            ["sqlite3.wasm", "persist.db", query_sql],
            files={"persist.db": db_bytes},
        )
        self.assertEqual(s2["exitCode"], 0, s2["stderr"])
        self.assertEqual(s2["stdout"], "alpha|100\nbeta|200\n")

    def test_05_ef0_negative_controls(self):
        # Negative Control 1: Lua syntax error exits non-zero
        bad_lua = run_browser_shim(self.node_bin, LUA_WASM, ["lua.wasm", "-e", "local x = = 1"])
        self.assertNotEqual(bad_lua["exitCode"], 0)

        # Negative Control 2: Failed Lua assert exits non-zero
        fail_lua = run_browser_shim(self.node_bin, LUA_WASM, ["lua.wasm", "-e", "assert(false, 'EF0_FAIL')"])
        self.assertNotEqual(fail_lua["exitCode"], 0)

        # Negative Control 3: SQLite invalid SQL exits non-zero with diagnostic
        bad_sql = run_browser_shim(self.node_bin, SQLITE_WASM, ["sqlite3.wasm", ":memory:", "SELECT * FROM missing_tbl_ef0;"])
        self.assertNotEqual(bad_sql["exitCode"], 0)
        self.assertIn("SQLITE_ERROR:", bad_sql["stderr"])

        # Negative Control 4: 1-byte header mutation fails WebAssembly.validate()
        with open(LUA_WASM, "rb") as f:
            mutated = bytearray(f.read())
        mutated[0] ^= 0xFF
        with tempfile.NamedTemporaryFile(suffix=".wasm", delete=False) as tf:
            tf.write(bytes(mutated))
            mut_path = tf.name
        try:
            info = inspect_wasm_v8(self.node_bin, mut_path)
            self.assertFalse(info["valid"])
        finally:
            if os.path.exists(mut_path):
                os.remove(mut_path)


if __name__ == "__main__":
    unittest.main(verbosity=2)
