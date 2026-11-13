#!/usr/bin/env python3
"""
tests/test_quad_isa_deep_regression.py

Independent differential regression harness for:
  - BUG-DEEP-02: Non-constant runtime power-of-2 / and % (signed & unsigned)
  - BUG-DEEP-04: Stack frames > 64 KB on AArch64 / RV64 / x86-64 (ldr x16, =imm)
"""

from __future__ import annotations

import os
import subprocess
import sys
import tempfile
from pathlib import Path

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")

REPO_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO_ROOT / "tools"))
from zcc_quad_isa_gate import AARCH64_SYSCALL_STUB, win_path_to_wsl  # noqa: E402

C_SOURCE = r"""int putchar(int c);

static void puthex32(unsigned int v) {
    int sh;
    for (sh = 28; sh >= 0; sh -= 4) {
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

volatile unsigned int g_reg_u[4] = {
    0x80000007u, 0xFEDCBA99u, 0x0001001Fu, 0xFFFFFFFFu
};
volatile int g_reg_s[4] = {
    -1023, -17, 1025, -2147483645
};

unsigned int reg_pow2_div_mod(unsigned int idx) {
    unsigned int u = g_reg_u[idx & 3u];
    int s = g_reg_s[idx & 3u];
    unsigned int q2 = u / 2u;
    unsigned int r2 = u % 2u;
    unsigned int q4 = u / 4u;
    unsigned int r4 = u % 4u;
    unsigned int q8 = u / 8u;
    unsigned int r8 = u % 8u;
    unsigned int q16 = u / 16u;
    unsigned int r16 = u % 16u;
    int sq2 = s / 2;
    int sq4 = s / 4;
    int sq8 = s / 8;
    unsigned int acc = q2 ^ (r2 << 31) ^ (q4 << 1) ^ (r4 << 28)
                     ^ (q8 << 2) ^ (r8 << 24) ^ (q16 << 3) ^ (r16 << 19);
    acc ^= (unsigned int)sq2 ^ ((unsigned int)sq4 << 5) ^ ((unsigned int)sq8 << 11);
    return acc;
}

unsigned int reg_huge_frame_gt64k(unsigned int v) {
    unsigned int arr[17000];
    arr[0] = v;
    arr[8191] = v ^ 0x13572468u;
    arr[16999] = v ^ 0x5A5A5A5Au;
    return arr[0] ^ (arr[8191] * 3u) ^ arr[16999];
}

int main(void) {
    unsigned int d_mix = 0x2468ACE1u;
    unsigned int i;
    for (i = 0; i < 4u; i++) {
        d_mix = (d_mix * 16777619u) ^ reg_pow2_div_mod(i);
    }
    unsigned int f_mix = reg_huge_frame_gt64k(g_reg_u[0]);
    puttag('D', '2', d_mix);
    puttag('F', '4', f_mix);
    putchar('\n');
    return 0;
}
"""


def run_cmd(cmd: list[str], timeout_sec: float = 30.0) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        cmd,
        cwd=str(REPO_ROOT),
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        timeout=timeout_sec,
    )


def run_suite() -> int:
    zcc_bin = REPO_ROOT / "zcc"
    if not zcc_bin.exists():
        print("[FAIL] Missing ./zcc binary")
        return 1

    with tempfile.TemporaryDirectory(prefix="zcc_deep_reg_") as td:
        work = Path(td)
        src = work / "deep_reg.c"
        src.write_text(C_SOURCE, encoding="utf-8")

        gcc_exe = work / "gcc.bin"
        clang_exe = work / "clang.bin"
        c_gcc = run_cmd(["gcc", "-O2", str(src), "-o", str(gcc_exe)])
        c_clang = run_cmd(["clang", "-O2", str(src), "-o", str(clang_exe)])
        if c_gcc.returncode != 0 or c_clang.returncode != 0:
            print(f"[FAIL] Reference compiler failed: gcc={c_gcc.returncode} clang={c_clang.returncode}")
            return 1

        r_gcc = run_cmd([str(gcc_exe)])
        r_clang = run_cmd([str(clang_exe)])
        oracle_out = r_gcc.stdout.strip()
        if r_gcc.returncode != 0 or r_clang.returncode != 0 or oracle_out != r_clang.stdout.strip():
            print(f"[FAIL] Reference mismatch: gcc={oracle_out!r} clang={r_clang.stdout.strip()!r}")
            return 1
        print(f"  [ORACLE] GCC/Clang -O2 : {oracle_out}")

        failures = 0

        # Lane 1: ZCC x86-64
        x64_s = work / "x64.s"
        x64_exe = work / "zcc_x64.bin"
        cx = run_cmd([str(zcc_bin), "-S", str(src), "-o", str(x64_s)])
        lx = run_cmd(["gcc", "-no-pie", str(x64_s), "-o", str(x64_exe)]) if cx.returncode == 0 else None
        if cx.returncode != 0 or lx is None or lx.returncode != 0:
            print(f"  [FAIL] zcc_x86_64 build error: {cx.stderr.strip()}")
            failures += 1
        else:
            rx = run_cmd([str(x64_exe)])
            ok = rx.returncode == 0 and rx.stdout.strip() == oracle_out
            print(f"  [{'PASS' if ok else 'FAIL'}] zcc_x86_64    : {rx.stdout.strip()}")
            if not ok:
                failures += 1

        # Lane 2: ZCC ARM64 QEMU
        a64_s = work / "arm64.s"
        a64_full = work / "arm64_full.s"
        a64_o = work / "arm64.o"
        a64_elf = work / "arm64.elf"
        ca = run_cmd([str(zcc_bin), "--target=arm64", "-S", str(src), "-o", str(a64_s)])
        if ca.returncode != 0:
            print(f"  [FAIL] zcc_arm64 compile error: {ca.stderr.strip()}")
            failures += 1
        else:
            a64_full.write_text(a64_s.read_text(encoding="utf-8") + "\n" + AARCH64_SYSCALL_STUB, encoding="utf-8")
            aa = run_cmd(["aarch64-linux-gnu-as", str(a64_full), "-o", str(a64_o)])
            la = run_cmd(["aarch64-linux-gnu-ld", "-static", str(a64_o), "-o", str(a64_elf)]) if aa.returncode == 0 else None
            if aa.returncode != 0 or la is None or la.returncode != 0:
                err_msg = aa.stderr.strip() if aa.returncode != 0 else (la.stderr.strip() if la else "")
                print(f"  [FAIL] zcc_arm64 as/ld error: {err_msg[:300]}")
                failures += 1
            else:
                qa = run_cmd(["qemu-aarch64-static", str(a64_elf)])
                ok = qa.returncode == 0 and qa.stdout.strip() == oracle_out
                print(f"  [{'PASS' if ok else 'FAIL'}] zcc_arm64     : {qa.stdout.strip()}")
                if not ok:
                    failures += 1

        # Lane 3: ZCC RISC-V 64 QEMU
        r64_s = work / "rv64.s"
        r64_elf = work / "rv64.elf"
        cr = run_cmd([str(zcc_bin), "--target=riscv64", "-S", str(src), "-o", str(r64_s)])
        lr = run_cmd(["riscv64-linux-gnu-gcc", "-static", str(r64_s), "-o", str(r64_elf)]) if cr.returncode == 0 else None
        if cr.returncode != 0 or lr is None or lr.returncode != 0:
            err_msg = cr.stderr.strip() if cr.returncode != 0 else (lr.stderr.strip() if lr else "")
            print(f"  [FAIL] zcc_riscv64 build error: {err_msg[:300]}")
            failures += 1
        else:
            qr = run_cmd(["qemu-riscv64-static", str(r64_elf)])
            ok = qr.returncode == 0 and qr.stdout.strip() == oracle_out
            print(f"  [{'PASS' if ok else 'FAIL'}] zcc_riscv64   : {qr.stdout.strip()}")
            if not ok:
                failures += 1

        return 1 if failures else 0


def main() -> int:
    if os.name == "nt":
        proc = subprocess.run(
            ["wsl", "-d", "Ubuntu-24.04", "--", "python3", win_path_to_wsl(Path(__file__))],
            capture_output=False,
            timeout=120,
        )
        return proc.returncode
    return run_suite()


if __name__ == "__main__":
    raise SystemExit(main())

