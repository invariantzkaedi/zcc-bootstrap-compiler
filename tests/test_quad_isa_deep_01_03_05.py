#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Differential + EF-2 assembly regression test for BUG-DEEP-01, BUG-DEEP-03, BUG-DEEP-05, and 32-bit wrap fixes.

Compares ZCC across x86-64, ARM64 (qemu-aarch64-static), and RV64 (qemu-riscv64-static)
against independent GCC -O2 and Clang -O2 oracles on:
  M1: Nested 2/3-level, chained-getter, and >8-arg indirect calls (BUG-DEEP-01)
  M2: Struct adjacent-field preservation, pre/post ++/--, uc += 2u, int* ++/-- (BUG-DEEP-03 + L1822)
  M3: Bitwise NOT (~), unary negation (-), and 32/64-bit casts (BUG-DEEP-05 + siblings)
  M4: 32-bit arithmetic & left-shift upper-bit wrap + mixed signed/unsigned promotion
"""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import subprocess
import sys
import tempfile
from typing import Dict, List, Tuple

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")
if hasattr(sys.stderr, "reconfigure"):
    sys.stderr.reconfigure(encoding="utf-8")

REPO_ROOT = Path(__file__).resolve().parent.parent
TOOLS_DIR = REPO_ROOT / "tools"
if str(TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(TOOLS_DIR))

from zcc_quad_isa_gate import AARCH64_SYSCALL_STUB  # noqa: E402


PROBE_C = r"""int putchar(int c);

static void puthex32(unsigned int v) {
    for (int sh = 28; sh >= 0; sh -= 4) {
        unsigned int d = (v >> sh) & 15u;
        putchar((int)(d < 10u ? ('0' + d) : ('a' + (d - 10u))));
    }
}

static void puttag(char a, char b, unsigned int v) {
    putchar((int)a);
    putchar((int)b);
    putchar('=');
    puthex32(v);
    putchar(' ');
}

typedef int (*binfn_t)(int, int);
typedef unsigned int (*manyfn_t)(
    unsigned int, unsigned int, unsigned int, unsigned int,
    unsigned int, unsigned int, unsigned int, unsigned int,
    unsigned int, unsigned int
);

static int fn_a(int x, int y) { return (x * 3 + y) ^ 0x1357; }
static int fn_b(int x, int y) { return ((x ^ 0x55) + y * 5) - 11; }
static int fn_c(int x, int y) { return (x + y * 7) ^ 0x2468; }

static binfn_t g_tbl[3] = { fn_a, fn_b, fn_c };
static binfn_t get_fn(int idx) { return g_tbl[idx % 3]; }

static unsigned int many_add(
    unsigned int a0, unsigned int a1, unsigned int a2, unsigned int a3,
    unsigned int a4, unsigned int a5, unsigned int a6, unsigned int a7,
    unsigned int a8, unsigned int a9
) {
    return a0 + a1 * 3u + a2 * 5u + a3 * 7u + a4 * 11u +
           a5 * 13u + a6 * 17u + a7 * 19u + a8 * 23u + a9 * 29u;
}

static unsigned int many_xor(
    unsigned int a0, unsigned int a1, unsigned int a2, unsigned int a3,
    unsigned int a4, unsigned int a5, unsigned int a6, unsigned int a7,
    unsigned int a8, unsigned int a9
) {
    return (a0 ^ 0x11u) + a1 * 2u + (a2 ^ a9) + a3 * 3u + a4 + a5 + a6 + a7 + (a8 * 31u);
}

volatile int g_sel = 1;
volatile unsigned int g_vu = 7u;
volatile int g_vi = -7;
volatile long long g_vll = -17LL;
volatile unsigned int g_umax = 0xffffffffu;
volatile unsigned char g_uc = 255u;

unsigned int test_m1_nested_ind(void) {
    int s = g_sel;
    binfn_t f0 = g_tbl[s & 1];
    binfn_t f1 = g_tbl[(s + 1) & 1];
    binfn_t f2 = g_tbl[(s + 2) % 3];
    int r1 = f0(f1(f2(9, 4), f0(3, 5)), f2(11, 2));
    int r2 = get_fn(s + 1)(get_fn(s)(17, 6), 13);
    manyfn_t mf0 = (s & 1) ? many_add : many_xor;
    manyfn_t mf1 = (s & 1) ? many_xor : many_add;
    unsigned int r3 = mf0(
        mf1(1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, (unsigned int)f0(2, 3)),
        2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u,
        mf1(10u, 9u, 8u, 7u, 6u, 5u, 4u, 3u, 2u, (unsigned int)f1(4, 5))
    );
    return (unsigned int)r1 ^ ((unsigned int)r2 << 5) ^ r3;
}

struct PackedFields {
    unsigned int a;
    unsigned int b;
    unsigned short s0;
    unsigned short s1;
    unsigned char c0;
    unsigned char c1;
    unsigned short pad;
    int arr[3];
    int *ptr;
};

unsigned int test_m2_inc_dec(void) {
    struct PackedFields p;
    p.a = g_umax;
    p.b = 0xDEADBEEFu;
    p.s0 = 0xffffu;
    p.s1 = 0x1234u;
    p.c0 = g_uc;
    p.c1 = 0xA5u;
    p.pad = 0x5A5Au;
    p.arr[0] = 11;
    p.arr[1] = 22;
    p.arr[2] = 33;
    p.ptr = &p.arr[0];

    unsigned int r0 = (++p.a) >> 1;
    unsigned int r1 = p.b;
    unsigned int r2 = (p.a--) >> 1;
    unsigned int r3 = p.a;
    unsigned int r4 = p.b;
    unsigned int r5 = (unsigned int)(++p.s0) >> 1;
    unsigned int r6 = p.s1;
    unsigned int r7 = (unsigned int)(++p.c0) >> 1;
    unsigned int r8 = p.c1;
    p.c0 = g_uc;
    unsigned int r9 = (unsigned int)(p.c0 += 2u);
    int *q0 = p.ptr++;
    int r10 = (*q0 * 100) + *(--p.ptr);
    unsigned int r11 = p.pad;
    return r0 ^ r1 ^ (r2 << 3) ^ r3 ^ (r4 * 3u) ^ (r5 << 7) ^ r6 ^
           (r7 << 11) ^ (r8 << 16) ^ (r9 << 4) ^ ((unsigned int)r10 << 9) ^ r11;
}

unsigned int test_m3_bnot_neg_cast(void) {
    unsigned int u = g_vu;
    int s = g_vi;
    long long ll = g_vll;

    unsigned int a = (~u) >> 1;
    unsigned int b = (~u) / 3u;
    unsigned long long c = (unsigned long long)(~u);
    unsigned int d = ((unsigned int)(-s)) >> 2;
    unsigned int e = (-u) >> 1;
    unsigned int f = ((unsigned int)ll) >> 2;
    int g = (~s);
    unsigned long long h = (unsigned long long)(long long)g;
    long long j = (long long)(int)(~u);
    return a ^ (b << 3) ^ (unsigned int)(c ^ (c >> 32)) ^ d ^ e ^ f ^
           (unsigned int)(h >> 32) ^ (unsigned int)(j >> 32);
}

unsigned int test_m4_arith32_wrap(void) {
    unsigned int u = g_vu;
    unsigned int umax = g_umax;
    int pos = g_sel;
    unsigned int a = (u - 9u) >> 1;
    unsigned int b = (umax + 2u) >> 1;
    unsigned int c = (umax * umax) >> 1;
    unsigned int d = ((1u << 31) << 1) >> 1;
    unsigned int e = ((unsigned int)(-pos) | 1u) >> 1;
    return a ^ (b << 3) ^ (c << 7) ^ (d << 11) ^ (e << 13);
}

int main(void) {
    puttag('M', '1', test_m1_nested_ind());
    puttag('M', '2', test_m2_inc_dec());
    puttag('M', '3', test_m3_bnot_neg_cast());
    puttag('M', '4', test_m4_arith32_wrap());
    putchar('\n');
    return 0;
}
"""

EF2_REQUIRED_MARKERS: Dict[str, List[str]] = {
    "x86_64": ["popq %r10"],
    "arm64": ["ldr x16, [sp], #16", "str w1, [x0]", "mov w0, w0"],
    "riscv64": ["ld t6, 0(sp)", "sw a1, 0(a0)", "slli a0, a0, 32", "srli a0, a0, 32"],
    "wasm32": ["wasm_magic_v1", "call_indirect_0x11"],
}


def run_step(cmd: List[str], timeout: int = 30) -> subprocess.CompletedProcess[str]:
    return subprocess.run(cmd, capture_output=True, text=True, errors="replace", timeout=timeout)


def compile_and_run_oracle(cc: str, src: Path, out_bin: Path) -> Tuple[bool, str]:
    comp = run_step([cc, "-O2", str(src), "-o", str(out_bin)])
    if comp.returncode != 0:
        return False, f"{cc} compile failed ({comp.returncode}): {comp.stderr.strip()}"
    exe = run_step([str(out_bin)])
    if exe.returncode != 0:
        return False, f"{cc} runtime failed ({exe.returncode}): {exe.stderr.strip()}"
    return True, exe.stdout.strip()


def compile_and_run_zcc_target(zcc: Path, src: Path, work: Path, arch: str) -> Tuple[bool, str, str]:
    asm_path = work / f"probe_{arch}.s"
    if arch == "x86_64":
        comp = run_step([str(zcc), "-S", str(src), "-o", str(asm_path)])
        if comp.returncode != 0:
            return False, f"zcc x86_64 compile failed: {comp.stderr.strip()}", ""
        bin_path = work / "probe_x86_64.bin"
        lnk = run_step(["gcc", "-no-pie", str(asm_path), "-o", str(bin_path)])
        if lnk.returncode != 0:
            return False, f"x86_64 link failed: {lnk.stderr.strip()}", ""
        exe = run_step([str(bin_path)])
    elif arch == "arm64":
        comp = run_step([str(zcc), "--target=arm64", "-S", str(src), "-o", str(asm_path)])
        if comp.returncode != 0:
            return False, f"zcc arm64 compile failed: {comp.stderr.strip()}", ""
        full_asm = work / "probe_arm64_full.s"
        full_asm.write_text(asm_path.read_text(encoding="utf-8") + "\n" + AARCH64_SYSCALL_STUB, encoding="utf-8")
        obj_path = work / "probe_arm64.o"
        asm_proc = run_step(["aarch64-linux-gnu-as", str(full_asm), "-o", str(obj_path)])
        if asm_proc.returncode != 0:
            return False, f"arm64 assemble failed: {asm_proc.stderr.strip()}", ""
        elf_path = work / "probe_arm64.elf"
        lnk = run_step(["aarch64-linux-gnu-ld", "-static", str(obj_path), "-o", str(elf_path)])
        if lnk.returncode != 0:
            return False, f"arm64 link failed: {lnk.stderr.strip()}", ""
        exe = run_step(["qemu-aarch64-static", str(elf_path)])
    elif arch == "riscv64":
        comp = run_step([str(zcc), "--target=riscv64", "-S", str(src), "-o", str(asm_path)])
        if comp.returncode != 0:
            return False, f"zcc riscv64 compile failed: {comp.stderr.strip()}", ""
        elf_path = work / "probe_riscv64.elf"
        lnk = run_step(["riscv64-linux-gnu-gcc", "-static", str(asm_path), "-o", str(elf_path)])
        if lnk.returncode != 0:
            return False, f"rv64 link failed: {lnk.stderr.strip()}", ""
        exe = run_step(["qemu-riscv64-static", str(elf_path)])
    elif arch == "wasm32":
        wasm_path = work / "probe_wasm32.wasm"
        comp = run_step([str(zcc), "--target=wasm32", str(src), "-o", str(wasm_path)])
        if comp.returncode != 0 or not wasm_path.is_file():
            return False, f"zcc wasm32 compile failed: {comp.stderr.strip()}", ""
        raw_wasm = wasm_path.read_bytes()
        markers = []
        if raw_wasm[:8] == b"\x00asm\x01\x00\x00\x00":
            markers.append("wasm_magic_v1")
        if b"\x11" in raw_wasm:
            markers.append("call_indirect_0x11")
        asm_path.write_text(" ".join(markers), encoding="utf-8")
        runner_js = work / "wasm_runner.js"
        runner_js.write_text(
            "const fs = require('fs');\n"
            "const buf = fs.readFileSync(process.argv[2]);\n"
            "let mem = null; let out = '';\n"
            "const wasi = {\n"
            "  fd_write: (fd, iovs, iovs_len, nwritten) => {\n"
            "    const v = new DataView(mem.buffer);\n"
            "    let total = 0;\n"
            "    for (let i = 0; i < iovs_len; i++) {\n"
            "      const ptr = v.getUint32(iovs + i * 8, true);\n"
            "      const len = v.getUint32(iovs + i * 8 + 4, true);\n"
            "      out += Buffer.from(mem.buffer, ptr, len).toString('utf8');\n"
            "      total += len;\n"
            "    }\n"
            "    if (nwritten) v.setUint32(nwritten, total, true);\n"
            "    return 0;\n"
            "  },\n"
            "  fd_read: () => 0,\n"
            "  proc_exit: (c) => { process.stdout.write(out); process.exit(c); }\n"
            "};\n"
            "WebAssembly.instantiate(buf, { wasi_snapshot_preview1: wasi }).then(({ instance }) => {\n"
            "  mem = instance.exports.memory;\n"
            "  if (instance.exports._start) instance.exports._start();\n"
            "  else if (instance.exports.main) instance.exports.main();\n"
            "  process.stdout.write(out);\n"
            "}).catch(e => { console.error(e); process.exit(1); });\n",
            encoding="utf-8",
        )
        exe = run_step(["node", str(runner_js), str(wasm_path)])
    else:
        return False, f"Unknown architecture: {arch}", ""

    asm_text = asm_path.read_text(encoding="utf-8") if asm_path.is_file() else ""
    if exe.returncode != 0:
        return False, f"{arch} runtime failed ({exe.returncode}): {exe.stderr.strip()}", asm_text
    return True, exe.stdout.strip(), asm_text


def run_suite(zcc_path: Path) -> int:
    if not zcc_path.is_file() or not os.access(zcc_path, os.X_OK):
        print(f"[FAIL] Compiler binary missing or not executable: {zcc_path}")
        return 2

    with tempfile.TemporaryDirectory(prefix="zcc_deep_010305_") as td:
        work = Path(td)
        src = work / "probe_deep_01_03_05.c"
        src.write_text(PROBE_C, encoding="utf-8")

        oracle_outputs: Dict[str, str] = {}
        for cc in ("gcc", "clang"):
            ok, out = compile_and_run_oracle(cc, src, work / f"{cc}_oracle.bin")
            if not ok:
                print(f"[FAIL] Oracle {out}")
                return 2
            oracle_outputs[cc] = out

        if oracle_outputs["gcc"] != oracle_outputs["clang"]:
            print(f"[FAIL] Oracle disagreement: gcc={oracle_outputs['gcc']} clang={oracle_outputs['clang']}")
            return 2

        expected = oracle_outputs["gcc"]
        print(f"[ORACLE] GCC/Clang -O2 : {expected}")

        all_ok = True
        for arch in ("x86_64", "arm64", "riscv64", "wasm32"):
            ok, out, asm_text = compile_and_run_zcc_target(zcc_path, src, work, arch)
            missing = [m for m in EF2_REQUIRED_MARKERS[arch] if m not in asm_text]
            rt_match = ok and (out == expected)
            ef2_ok = len(missing) == 0
            status = "PASS" if (rt_match and ef2_ok) else "FAIL"
            if status != "PASS":
                all_ok = False
            print(
                f"  [{status}] zcc_{arch:8s}: {out} | expected={expected} | EF2_missing={missing}"
            )

        return 0 if all_ok else 1


def main() -> int:
    parser = argparse.ArgumentParser(description="Differential regression suite for BUG-DEEP-01/03/05.")
    parser.add_argument(
        "compiler",
        nargs="?",
        default=str(REPO_ROOT / "zcc"),
        help="Path to zcc compiler binary (default: ./zcc)",
    )
    args = parser.parse_args()
    return run_suite(Path(args.compiler).resolve())


if __name__ == "__main__":
    sys.exit(main())

