#!/usr/bin/env python3
"""
Native Zero-Toolchain Win64 Verification Suite for QuickJS 2024-01-13 & MicroPython v1.24.1
============================================================================================
Verifies standalone native Windows PE32+ executables (`build_win64/qjs.exe` and
`build_win64/micropython.exe`) built with zero MSVC/MinGW/binutils by BOTH host `./zcc`
and native Windows sovereign `build_win64/zcc_sov_stage3.exe`:
  1. PE32+ structural audit (.text RX, .data RW, .pdata, .reloc, StackReserve>=16MB,
     TimeDateStamp==0) + `msvcrt.dll`-only imports verified against real Windows
     `C:\\Windows\\System32\\msvcrt.dll` exports, plus unmodified upstream sources.
  2. Byte-for-byte convergence between `./zcc` and `zcc_sov_stage3.exe` via SHA-256 + `fc /b`.
  3. QuickJS 2024-01-13 native Windows ES2023 differential parity vs host Node 22 V8
     (IEEE-754 math/hyperbolics, -0.0, 128-bit BigInt, classes/closures/`fib(18)` `alloca`,
     64-bit `Date`, `JSON`, `RegExp`, `Promise`/`async`/`await`, GC stress, file & stdin).
  4. MicroPython v1.24.1 native Windows Python 3 differential parity vs host CPython 3
     (`mpz` 200-bit int, `try/except/finally/raise` NLR `setjmp`/`longjmp` unwinding,
     generators, comprehensions, closures, `@property`, `json`/`re`/`struct`/`collections`/
     `heapq`, register-spill GC stress, file/import/stdin).
  5. ZCC-EF-0 negative controls (corrupted PE magic, bogus DLL/import symbol, 1-byte PE
     mutation, JS & Python syntax/runtime errors, non-zero `sys.exit`, missing scripts).
"""

import os
import shutil
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

QJS_EXE = os.path.join(BUILD_DIR, "qjs.exe")
QJS_HOST_EXE = os.path.join(BUILD_DIR, "qjs_host_zcc.exe")
MPY_EXE = os.path.join(BUILD_DIR, "micropython.exe")
MPY_HOST_EXE = os.path.join(BUILD_DIR, "micropython_host_zcc.exe")


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


ES2023_WIN64_DIFF_SCRIPT = r"""
const out = [];

// 1. Arithmetic & safe integers
out.push("arith:" + ((17 * 23 - 5) ^ 0x5a) + "," + ((-17) >> 1) + "," + ((-17) >>> 1));
out.push("safe:" + Number.MAX_SAFE_INTEGER + "," + ((9007199254740991).toString(16)));

// 2. IEEE-754 & C99 Math.*
const f6 = (x) => Number.isNaN(x) ? "NaN" : (Object.is(x, -0) ? "-0" : x.toFixed(6));
out.push("trig:" + [
  f6(Math.sin(0.5)), f6(Math.cos(0.5)), f6(Math.tan(0.5)),
  f6(Math.acos(0.25)), f6(Math.atan2(-3, -4))
].join(","));
out.push("exp:" + [
  f6(Math.exp(1.5)), f6(Math.log(12.5)), f6(Math.pow(2.5, 3.25)),
  f6(Math.cbrt(-27.0)), f6(Math.hypot(3, 4, 12)),
  f6(Math.expm1(0.001)), f6(Math.log1p(0.001)),
  f6(Math.log2(1024)), f6(Math.exp2 ? Math.exp2(5) : 32)
].join(","));
out.push("hyp:" + [
  f6(Math.sinh(0.75)), f6(Math.cosh(0.75)), f6(Math.tanh(0.75)),
  f6(Math.asinh(1.5)), f6(Math.acosh(2.5)), f6(Math.atanh(0.5))
].join(","));
out.push("round:" + [
  f6(Math.trunc(-3.75)), f6(Math.round(-3.5)), f6(Math.fround(1.337)),
  Math.clz32(1), Math.imul(0x7fffffff, 3)
].join(","));

// 3. Signed -0.0 & Float64Array sort
const arrZ = new Float64Array([0, -0, 1, -1]);
arrZ.sort((a, b) => (a - b) || ((1 / a) - (1 / b)));
out.push("neg0:" + Object.is(-0.0, -0) + "," + Object.is(-0.0, +0.0) + "," +
         Array.from(arrZ).map(x => Object.is(x, -0) ? "-0" : String(x)).join(","));

// 4. JSON
const parsed = JSON.parse('{"a":[1,true,null,"z\\u0043"],"b":{"x":42.5}}');
parsed.a.push(JSON.stringify(parsed.b).length);
out.push("json:" + JSON.stringify(parsed));

// 5. Closures, recursion + stack probing (`alloca` inside JS_CallInternal)
const makeAcc = (base) => (delta) => (base += delta);
const acc = makeAcc(10);
function fib(n) { return n <= 1 ? n : fib(n - 1) + fib(n - 2); }
function sumArgs(...xs) { return xs.reduce((a, b) => a + b, 0); }
const bigArgs = [];
for (let i = 1; i <= 150; i++) bigArgs.push(i);
out.push("cls:" + acc(5) + "," + acc(7) + ",fib18=" + fib(18) + ",vararg=" + sumArgs(...bigArgs));

// 6. RegExp & Unicode
const re = /([a-z]+)-(\d+)/g;
const rep = "x-12 y-345".replace(re, (_, w, n) => w.toUpperCase() + ":" + (Number(n) * 2));
out.push("re:" + rep + ",u=" + /^\p{Letter}+$/u.test("Alpha"));

// 7. ES6 classes + inheritance + generators
class Vec {
  constructor(x, y) { this.x = x; this.y = y; }
  norm2() { return this.x * this.x + this.y * this.y; }
}
class Vec3 extends Vec {
  constructor(x, y, z) { super(x, y); this.z = z; }
  norm2() { return super.norm2() + this.z * this.z; }
}
function* squares(k) { for (let i = 1; i <= k; i++) yield i * i; }
out.push("cls3:" + new Vec3(3, 4, 12).norm2() + ",gen=" + Array.from(squares(5)).join("-"));

// 8. 64-bit Date & 128-bit BigInt
out.push("date:" + new Date(1700000000000).toISOString() + "," + new Date(4102444800000).toISOString());
const b1 = (2n ** 64n) * 1000000000000000003n + 7n;
const b2 = BigInt.asUintN(64, (1n << 65n) - 1n);
out.push("big:" + b1.toString() + "," + b2.toString(16));

// 9. Map, Set, Proxy, Symbol & GC stress
const s = new Set([3, 1, 4, 1, 5, 9]);
const m = new Map([["k1", 10], ["k2", 20]]);
const sym = Symbol("tok");
const px = new Proxy({ val: 21, [sym]: "sec" }, { get(t, k) { return k === "val" ? t.val * 2 : t[k]; } });
let gcAcc = 0;
for (let i = 0; i < 4000; i++) {
  const tmp = { k: i, arr: [i, i + 1, "v_" + (i % 13)] };
  if (i % 997 === 0) gcAcc += tmp.arr[1];
}
out.push("col:" + Array.from(s).join("") + "," + m.get("k2") + "," + px.val + "," + px[sym] + ",gc=" + gcAcc);

// 10. Promise / async / await microtasks
async function asyncFlow() {
  const step1 = await Promise.resolve(20);
  const step2 = await new Promise((res) => Promise.resolve().then(() => res(step1 + 22)));
  out.push("async:" + step2);
  const printer = (typeof console === "object" && console && typeof console.log === "function") ? console.log : print;
  for (const line of out) printer(line);
}
asyncFlow();
""".strip()


PY_WIN64_DIFF_SCRIPT = r"""
import math
import json
import re
import struct
import heapq
from collections import OrderedDict, deque, namedtuple

lines = []

# 1. Arbitrary-precision mpz integers & modular exponentiation
big_a = (2 ** 256) + 12345678901234567890123456789
big_b = 98765432109876543210987654321
fact60 = 1
for k in range(2, 61):
    fact60 *= k
lines.append("mpz:%d|%d|%d|%d" % (big_a // big_b, big_a % big_b, pow(3, 100, 10**9 + 7), fact60))

# 2. IEEE-754 floats, math & complex
fvals = [math.sin(0.5), math.cos(0.5), math.sqrt(2.0), math.exp(1.25), math.log(12.5), math.floor(-2.3), math.ceil(-2.3)]
lines.append("math:" + ",".join("%.6f" % x for x in fvals))
z = (3 + 4j) * (1 - 2j)
lines.append("cplx:%d,%d,%.1f" % (int(z.real), int(z.imag), abs(3 + 4j)))

# 3. Closures, decorators, comprehensions, f-strings, sorting
def trace_wrap(fn):
    def inner(x):
        return fn(x) * 10
    return inner

@trace_wrap
def square(n):
    return n * n

def make_acc(base):
    def add(d):
        nonlocal base
        base += d
        return base
    return add

acc = make_acc(10)
comp = [x * x for x in range(10) if x % 2 == 1]
dcomp = {str(k): k * 3 for k in range(4)}
scomp = sorted({x % 4 for x in range(10)})
words = sorted(["pear", "fig", "apple", "kiwi"], key=lambda s: (len(s), s))
lines.append(f"fn:{square(3)}|{acc(5)},{acc(7)}|{comp}|{sorted(dcomp.items())}|{scomp}|{words}")

# 4. Classes, inheritance, @property, dunder methods
class Shape:
    def __init__(self, scale):
        self._scale = scale
    def area(self):
        return 0

class Rect(Shape):
    def __init__(self, w, h, scale=1):
        super().__init__(scale)
        self.w = w
        self.h = h
    @property
    def perim(self):
        return 2 * (self.w + self.h)
    def area(self):
        return self.w * self.h * self._scale
    def __repr__(self):
        return f"Rect({self.w},{self.h})"

r = Rect(3, 4, 2)
lines.append(f"cls:{r}|{r.area()}|{r.perim}|{isinstance(r, Shape)}")

# 5. Deep Win64 setjmp/longjmp NLR unwinding (`try/except/else/finally`, re-raise, generators)
class CustomErr(Exception):
    def __init__(self, msg, code):
        super().__init__(msg)
        self.code = code

eh_trace = []
def level3():
    raise CustomErr("deep_fail", 503)

def level2():
    try:
        level3()
    finally:
        eh_trace.append("lvl2_fin")

def level1():
    try:
        try:
            level2()
        except CustomErr as e:
            eh_trace.append(f"caught:{e.code}")
            raise ValueError("rethrown")
        finally:
            eh_trace.append("inner_fin")
    except ValueError as v:
        eh_trace.append(f"outer:{v}")
    else:
        eh_trace.append("BAD_ELSE")
    finally:
        eh_trace.append("outer_fin")

level1()

try:
    _ = 1 // 0
except ZeroDivisionError:
    eh_trace.append("zdiv")

class SeqGetItem:
    def __getitem__(self, idx):
        if idx >= 4:
            raise IndexError
        return idx * 10

lines.append("eh:" + ",".join(eh_trace) + "|" + str([v for v in SeqGetItem()]))

# 6. Generators, yield from, gen.throw
def inner_gen():
    try:
        yield 10
        yield 20
    except RuntimeError:
        yield 99

def outer_gen():
    yield from inner_gen()
    yield 30

g = outer_gen()
g_out = [next(g), g.throw(RuntimeError("g_boom")), next(g)]
lines.append("gen:" + str(g_out))

# 7. Stdlib: json, re, struct, collections, heapq, range
payload = {"k": [1, True, None, "zcc"], "n": 42}
dec = json.loads(json.dumps(payload))
m = re.search(r"([a-z]+)-([0-9]+)", "id=alpha-2026!")
sub = re.sub(r"[0-9]+", "NUM", "a1b23c456")
pk = struct.pack("<BHi", 0x7A, 0x1234, -99999)
unpk = struct.unpack("<BHi", pk)
od = OrderedDict([("b", 2), ("a", 1)])
dq = deque([1, 2, 3], 10)
dq.appendleft(0)
Pt = namedtuple("Pt", ("x", "y"))
pt = Pt(7, 9)
h = [5, 1, 8, 3, 2]
heapq.heapify(h)
hsorted = [heapq.heappop(h) for _ in range(len(h))]
lines.append(f"std:{dec['k']}|{dec['n']}|{m.group(1)}:{m.group(2)}|{sub}|{len(pk)}:{unpk}|{list(od.keys())}|{list(dq)}|{pt.x + pt.y}|{hsorted}|{list(range(2, 10, 3))}")

for ln in lines:
    print(ln)
""".strip()


class TestWin64QuickJsMicroPython(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        for req in (QJS_EXE, QJS_HOST_EXE, MPY_EXE, MPY_HOST_EXE):
            if not os.path.isfile(req):
                raise RuntimeError(f"Missing {req}; run build_quickjs_win64.py and build_micropython_win64.py first.")
        cls.tmp_dir = tempfile.mkdtemp(prefix="_test_qjs_mpy_", dir=BUILD_DIR)
        cls.node_bin = find_node_bin()

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp_dir, ignore_errors=True)

    def test_01_pe32plus_audit_system32_msvcrt_and_clean_upstream(self):
        q_audit = audit_pe32plus_msvcrt(QJS_EXE)
        m_audit = audit_pe32plus_msvcrt(MPY_EXE)
        self.assertGreater(len(q_audit["msvcrt_imports"]), 40)
        self.assertGreater(len(m_audit["msvcrt_imports"]), 40)
        if os.path.isfile(MSVCRT_PATH):
            exps = parse_pe_exports(MSVCRT_PATH)
            for sym in q_audit["msvcrt_imports"] + m_audit["msvcrt_imports"]:
                self.assertIn(sym, exps, f"Missing import {sym} in {MSVCRT_PATH}")

        for up_dir in ("tests/embedded_quickjs/", "tests/embedded_micropython/"):
            gd = subprocess.run(["git", "diff", "--name-only", up_dir], cwd=REPO_ROOT, capture_output=True, text=True, check=True)
            self.assertEqual(gd.stdout.strip(), "", f"Upstream files modified in {up_dir}: {gd.stdout}")

    def test_02_sovereign_cross_vs_native_byte_identity(self):
        for host_p, win_p, label in (
            (QJS_HOST_EXE, QJS_EXE, "qjs.exe"),
            (MPY_HOST_EXE, MPY_EXE, "micropython.exe"),
        ):
            self.assertEqual(
                sha256_file(host_p),
                sha256_file(win_p),
                f"{label} SHA-256 mismatch between ./zcc and zcc_sov_stage3.exe",
            )
            fc = run_fc_b(host_p, win_p)
            self.assertEqual(fc.returncode, 0, f"fc /b failed for {label}: {fc.stdout}\n{fc.stderr}")
            self.assertIn("no differences encountered", fc.stdout.lower())

    def test_03_quickjs_es2023_differential_vs_node_v8_and_io(self):
        r_v8 = subprocess.run([self.node_bin, "-e", ES2023_WIN64_DIFF_SCRIPT], cwd=REPO_ROOT, capture_output=True, text=True, check=True)
        v8_lines = r_v8.stdout.replace("\r\n", "\n").strip().splitlines()

        r_qjs = run_win64_exe(QJS_EXE, ["-e", ES2023_WIN64_DIFF_SCRIPT])
        self.assertEqual(r_qjs.returncode, 0, f"qjs.exe -e failed:\n{r_qjs.stdout}\n{r_qjs.stderr}")
        qjs_lines = r_qjs.stdout.replace("\r\n", "\n").strip().splitlines()
        self.assertEqual(qjs_lines, v8_lines, f"QuickJS Win64 mismatch vs Node V8:\ngot={qjs_lines}\nexp={v8_lines}")

        # Script file + argv + -p + stdin checks
        js_path = os.path.join(self.tmp_dir, "qjs_file_test.js")
        with open(js_path, "w", encoding="utf-8", newline="\n") as f:
            f.write('print("SCRIPT:" + JSON.stringify(scriptArgs.slice(1)) + ":" + ((1n << 80n) + 9n).toString());\n')
        rf = run_win64_exe(QJS_EXE, [to_rel_win(js_path), "alpha", "beta"])
        self.assertEqual(rf.returncode, 0, f"qjs.exe file mode failed:\n{rf.stderr}")
        self.assertEqual(rf.stdout.replace("\r\n", "\n").strip(), 'SCRIPT:["alpha","beta"]:1208925819614629174706185')

        rp = run_win64_exe(QJS_EXE, ["-p", "(2n**64n) + 1n"])
        self.assertEqual(rp.returncode, 0, rp.stderr)
        self.assertEqual(rp.stdout.strip(), "18446744073709551617")

        rs = run_win64_exe(QJS_EXE, ["-"], stdin_text='console.log("QJS_STDIN:" + (6 * 7));\n')
        self.assertEqual(rs.returncode, 0, rs.stderr)
        self.assertEqual(rs.stdout.strip(), "QJS_STDIN:42")

    def test_04_micropython_differential_vs_cpython_gc_vfs_and_nlr(self):
        r_py = subprocess.run([sys.executable, "-c", PY_WIN64_DIFF_SCRIPT], cwd=REPO_ROOT, capture_output=True, text=True, check=True)
        py_lines = r_py.stdout.replace("\r\n", "\n").strip().splitlines()

        r_mpy = run_win64_exe(MPY_EXE, ["-c", PY_WIN64_DIFF_SCRIPT])
        self.assertEqual(r_mpy.returncode, 0, f"micropython.exe -c failed:\n{r_mpy.stdout}\n{r_mpy.stderr}")
        mpy_lines = r_mpy.stdout.replace("\r\n", "\n").strip().splitlines()
        self.assertEqual(mpy_lines, py_lines, f"MicroPython Win64 mismatch vs CPython:\ngot={mpy_lines}\nexp={py_lines}")

        # GC stress with callee-saved register-held roots across gc.collect() + temp module import + file I/O
        mod_path = os.path.join(self.tmp_dir, "win64_helper_mod.py")
        main_path = os.path.join(self.tmp_dir, "win64_main_test.py")
        io_path = os.path.join(self.tmp_dir, "win64_io_roundtrip.txt")
        with open(mod_path, "w", encoding="utf-8", newline="\n") as f:
            f.write("def triple_plus_one(x):\n    return x * 3 + 1\n")
        tmp_rel = to_rel_win(self.tmp_dir).replace("\\", "/")
        io_rel = to_rel_win(io_path).replace("\\", "/")
        with open(main_path, "w", encoding="utf-8", newline="\n") as f:
            f.write(
                f"import sys, gc\n"
                f"sys.path.insert(0, '{tmp_rel}')\n"
                f"import win64_helper_mod\n"
                f"keeper = [win64_helper_mod.triple_plus_one(i) for i in range(6)]\n"
                f"for k in range(3000):\n"
                f"    _junk = (k, [k, k + 1], {{'v': k}})\n"
                f"    if k % 500 == 0:\n"
                f"        gc.collect()\n"
                f"with open('{io_rel}', 'w') as wf:\n"
                f"    wf.write('WIN64_OK:' + str(sum(keeper)))\n"
                f"with open('{io_rel}', 'r') as rf:\n"
                f"    txt = rf.read()\n"
                f"print('MPY_IO', txt, sys.argv[1:])\n"
            )
        rf = run_win64_exe(MPY_EXE, [to_rel_win(main_path), "argA", "argB"])
        self.assertEqual(rf.returncode, 0, f"micropython.exe script failed:\n{rf.stdout}\n{rf.stderr}")
        self.assertEqual(rf.stdout.replace("\r\n", "\n").strip(), "MPY_IO WIN64_OK:51 ['argA', 'argB']")

        rs = run_win64_exe(MPY_EXE, ["-"], stdin_text="print('MPY_STDIN', sum(x * x for x in range(5)))\n")
        self.assertEqual(rs.returncode, 0, rs.stderr)
        self.assertEqual(rs.stdout.strip(), "MPY_STDIN 30")

    def test_05_ef0_negative_controls(self):
        # NC-1: Corrupted PE header rejected by audit_pe32plus_msvcrt
        bad_hdr = os.path.join(self.tmp_dir, "bad_pe_hdr.exe")
        with open(QJS_EXE, "rb") as f:
            raw = bytearray(f.read())
        pe_off = int.from_bytes(raw[0x3C:0x40], "little")
        raw[pe_off : pe_off + 4] = b"BAD!"
        with open(bad_hdr, "wb") as f:
            f.write(raw)
        with self.assertRaises(AssertionError):
            audit_pe32plus_msvcrt(bad_hdr)

        # NC-2: Foreign DLL name injected into IAT rejected by audit_pe32plus_msvcrt
        bad_dll = os.path.join(self.tmp_dir, "bad_dll_imp.exe")
        with open(MPY_EXE, "rb") as f:
            raw = bytearray(f.read())
        idx = raw.find(b"msvcrt.dll\x00")
        self.assertGreater(idx, 0)
        raw[idx : idx + 10] = b"bogus1.dll"
        with open(bad_dll, "wb") as f:
            f.write(raw)
        with self.assertRaises(AssertionError):
            audit_pe32plus_msvcrt(bad_dll)

        # NC-3: Bogus imported function symbol rejected against System32\msvcrt.dll exports
        bad_sym = os.path.join(self.tmp_dir, "bad_sym_imp.exe")
        with open(MPY_EXE, "rb") as f:
            raw = bytearray(f.read())
        sidx = raw.find(b"fopen\x00")
        self.assertGreater(sidx, 0)
        raw[sidx : sidx + 5] = b"zfake"
        with open(bad_sym, "wb") as f:
            f.write(raw)
        with self.assertRaises(AssertionError):
            audit_pe32plus_msvcrt(bad_sym)

        # NC-4: QuickJS syntax / runtime / missing file -> nonzero exit
        self.assertNotEqual(run_win64_exe(QJS_EXE, ["-e", "function broken("]).returncode, 0)
        self.assertNotEqual(run_win64_exe(QJS_EXE, ["-e", "throw new Error('ef0_qjs');"]).returncode, 0)
        self.assertNotEqual(run_win64_exe(QJS_EXE, [r"build_win64\_missing_ef0.js"]).returncode, 0)

        # NC-5: MicroPython syntax / runtime / sys.exit(7) / missing file -> nonzero exit
        self.assertNotEqual(run_win64_exe(MPY_EXE, ["-c", "def broken(:"]).returncode, 0)
        self.assertNotEqual(run_win64_exe(MPY_EXE, ["-c", "raise ValueError('ef0_mpy')"]).returncode, 0)
        self.assertEqual(run_win64_exe(MPY_EXE, ["-c", "import sys; sys.exit(7)"]).returncode, 7)
        self.assertNotEqual(run_win64_exe(MPY_EXE, [r"build_win64\_missing_ef0.py"]).returncode, 0)

        # NC-6: 1-byte PE mutation caught by SHA-256 and fc /b
        for src_exe, name in ((QJS_EXE, "mut_qjs.exe"), (MPY_EXE, "mut_mpy.exe")):
            mut = os.path.join(self.tmp_dir, name)
            with open(src_exe, "rb") as f:
                mraw = bytearray(f.read())
            mraw[len(mraw) // 2] ^= 0xFF
            with open(mut, "wb") as f:
                f.write(mraw)
            self.assertNotEqual(sha256_file(mut), sha256_file(src_exe))
            self.assertNotEqual(run_fc_b(src_exe, mut).returncode, 0)


if __name__ == "__main__":
    unittest.main(verbosity=2)
