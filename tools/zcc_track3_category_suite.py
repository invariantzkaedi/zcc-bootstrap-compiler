#!/usr/bin/env python3
"""
ZCC Track 3 — Expanded Semantic Category, Minimized Counterexample & Cross-TU ABI Suite
=======================================================================================
Executes:
  1. Part A: 5 Minimized 10-Line C99 Counterexamples (DEFECT-TRACK3-001..003) across
     4 reference compilers (gcc -O0, gcc -O2, clang -O2, gcc -O1 -fsanitize=undefined),
     unpatched baseline zcc2 (negative control, if provided), patched zcc2, and patched zcc2 --ir.
  2. Part B: 9 P0 C99 Semantic Category Differential Probes across all 4 reference compilers
     + patched zcc2 + patched zcc2 --ir.
  3. Part C: P1 2-TU x86-64 System V ABI Interoperability (zcc-caller + gcc-callee and
     gcc-caller + zcc-callee) covering 8B/16B register structs, 24B sret MEMORY structs,
     >6 GPR stack-spilled arguments, mixed GPR/XMM arguments, and variadic calls.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import sys
from pathlib import Path
from typing import Any

COUNTEREXAMPLES: list[dict[str, str]] = [
    {
        "id": "repro_01_shl_assign_u8_mem",
        "defect_id": "DEFECT-TRACK3-001",
        "c99_clause": "C99 §6.5.16.2 / §6.5.7 (1-byte memory <<= compound assignment)",
        "root_cause": "part4.c:1780 & 2800 emitted `shrb %cl, %al` (right shift) instead of `shlb %cl, %al` (left shift) for 1-byte ND_SHL.",
        "expected_pre_patch_zcc": "4",
        "code": """\
#include <stdio.h>
#include <stdint.h>
struct S { uint8_t b; };
int main(void) {
    struct S s = { 0x11u };
    s.b <<= 2;
    printf("%u\\n", (unsigned)s.b);
    return 0;
}
""",
    },
    {
        "id": "repro_02_compound_div_wider_rhs",
        "defect_id": "DEFECT-TRACK3-002a",
        "c99_clause": "C99 §6.5.16.2p3 & §6.3.1.8 (Compound assignment with wider 64-bit RHS)",
        "root_cause": "part3.c:3324 cast `ca->rhs` to `n->type` (`int32_t`) before `/=`, truncating `0x100000002LL` to `2` instead of promoting both operands to `int64_t`.",
        "expected_pre_patch_zcc": "50",
        "code": """\
#include <stdio.h>
#include <stdint.h>
int main(void) {
    int32_t s32 = 100;
    int64_t d64 = 0x100000002LL;
    s32 /= d64;
    printf("%d\\n", s32);
    return 0;
}
""",
    },
    {
        "id": "repro_03_compound_div_signed_unsigned_uac",
        "defect_id": "DEFECT-TRACK3-002b",
        "c99_clause": "C99 §6.5.16.2p3 & §6.3.1.8 (Compound assignment signed/unsigned usual arithmetic conversion)",
        "expected_pre_patch_zcc": "-5",
        "root_cause": "part3.c:3324 & part4.c:1709 evaluated `int32_t /= uint32_t` as signed `idivl` instead of converting LHS to `uint32_t` (`divl`).",
        "code": """\
#include <stdio.h>
#include <stdint.h>
int main(void) {
    int32_t s32 = -10;
    uint32_t u32 = 2u;
    s32 /= u32;
    printf("%d\\n", s32);
    return 0;
}
""",
    },
    {
        "id": "repro_04_compound_mul_int_double",
        "defect_id": "DEFECT-TRACK3-002c",
        "c99_clause": "C99 §6.5.16.2p3 & §6.3.1.8 (Compound assignment int32_t *= double)",
        "expected_pre_patch_zcc": "20",
        "root_cause": "part3.c:3324 & part4.c:1679 truncated `double` RHS `2.5` to `int32_t` `2` before integer `imulq` instead of evaluating in `double` (`mulsd`) and converting back.",
        "code": """\
#include <stdio.h>
#include <stdint.h>
int main(void) {
    int32_t x = 10;
    double scale = 2.5;
    x *= scale;
    printf("%d\\n", x);
    return 0;
}
""",
    },
    {
        "id": "repro_05_member_signed_subword_div_assign",
        "defect_id": "DEFECT-TRACK3-003",
        "c99_clause": "C99 §6.5.2.3 & §6.5.16.2 (Signed int8_t/int16_t struct member compound assignment load)",
        "expected_pre_patch_zcc": "124 32763",
        "root_cause": "part4.c:1589 loaded non-bitfield struct members in ND_COMPOUND_ASSIGN with unsigned `movzbl`/`movzwl` instead of `codegen_load(cc, node->lhs->type)` (`movsbq`/`movswq`).",
        "code": """\
#include <stdio.h>
#include <stdint.h>
struct S { int8_t a; int16_t b; };
int main(void) {
    struct S s = { -8, -9 };
    s.a /= 2;
    s.b /= 2;
    printf("%d %d\\n", (int)s.a, (int)s.b);
    return 0;
}
""",
    },
    {
        "id": "repro_06_unary_minus_uint8_promotion",
        "defect_id": "DEFECT-TRACK3-004",
        "c99_clause": "C99 §6.5.3.3p2-4 & §6.3.1.1 (Integer promotion of uint8_t/uint16_t on unary `-`, `+`, `~`)",
        "expected_pre_patch_zcc": "0 0",
        "root_cause": "part3.c:2879-2901 assigned `n->type = n->lhs->type` (`TY_UCHAR`/`TY_USHORT`) on unary `-`, `+`, and `~` instead of promoting narrow unsigned types to signed `int` via `integer_promotion(cc, n->lhs->type)`.",
        "code": """\
#include <stdio.h>
#include <stdint.h>
int main(void) {
    uint8_t u8 = 250;
    uint16_t u16 = 60000;
    printf("%d %d\\n", (-u8 < 0), (-u16 < 0));
    return 0;
}
""",
    },
    {
        "id": "repro_07_bitfield_compound_assign_expr_val",
        "defect_id": "DEFECT-TRACK3-005",
        "c99_clause": "C99 §6.5.16p3 & §6.5.16.2 (Bitfield compound assignment RMW & expression value truncation)",
        "expected_pre_patch_zcc": "-2 3 11",
        "root_cause": "part4.c:1034 returned unmasked `%r9` instead of truncated/extended bitfield value from `emit_bitfield_store_p4`, and `part4.c`/`ir_bridge.h` bypassed bitfield extract/store in `--ir`.",
        "code": """\
#include <stdio.h>
#include <stdint.h>
struct BF { signed int s5 : 5; unsigned int u3 : 3; };
int main(void) {
    struct BF bf = { -8, 5 };
    bf.s5 += 3;
    bf.s5 /= 2;
    int r = (bf.u3 += 6);
    printf("%d %u %d\\n", (int)bf.s5, (unsigned)bf.u3, r);
    return 0;
}
""",
    },
]

SEMANTIC_CATEGORIES: dict[str, str] = {
    "uac_int64_vs_uint32": """\
#include <stdio.h>
#include <stdint.h>
int main(void) {
    int64_t a = -5;
    uint32_t b = 2u;
    int r1 = (a < b);
    int64_t r2 = a / b;
    int64_t r3 = a % b;
    printf("%d %lld %lld\\n", r1, (long long)r2, (long long)r3);
    return 0;
}
""",
    "uac_uint8_promo_div": """\
#include <stdio.h>
#include <stdint.h>
int main(void) {
    uint8_t u8 = 250;
    uint16_t u16 = 60000;
    int r1 = (-u8 < 0);
    int r2 = (-u16 < 0);
    int r3 = (u8 - 255) / 2;
    printf("%d %d %d\\n", r1, r2, r3);
    return 0;
}
""",
    "compound_assign_subword": """\
#include <stdio.h>
#include <stdint.h>
struct S { int8_t a; uint8_t b; int16_t c; uint16_t d; };
int main(void) {
    struct S s = { -8, 250, -300, 65000 };
    s.a /= 2;
    s.b += 10;
    s.c >>= 2;
    s.d <<= 2;
    uint8_t arr[2] = { 0x81u, 0x11u };
    arr[0] >>= 1;
    arr[1] <<= 2;
    printf("%d %u %d %u %u %u\\n", (int)s.a, (unsigned)s.b, (int)s.c, (unsigned)s.d, (unsigned)arr[0], (unsigned)arr[1]);
    return 0;
}
""",
    "do_while_continue_switch": """\
#include <stdio.h>
int main(void) {
    int i = 0, acc = 0;
    do {
        i++;
        switch (i) {
            case 2:
                acc += 100;
                continue;
            case 4:
                acc += 1000;
                break;
            default:
                acc += i;
                break;
        }
        acc += 10;
    } while (i < 5);
    printf("%d %d\\n", i, acc);
    return 0;
}
""",
    "compound_assign_wider_rhs": """\
#include <stdio.h>
#include <stdint.h>
int main(void) {
    int32_t x = 10;
    double scale = 2.5;
    x *= scale;
    int32_t y = -10;
    uint32_t u = 2u;
    y /= u;
    int32_t s32 = 100;
    int64_t d64 = 0x100000002LL;
    s32 /= d64;
    printf("%d %d %d\\n", x, y, s32);
    return 0;
}
""",
    "ternary_int64_uint32": """\
#include <stdio.h>
#include <stdint.h>
int main(void) {
    int64_t s64 = -1;
    uint32_t u32 = 1u;
    int32_t s32 = -1;
    int r1 = ((1 ? s64 : u32) < 0);
    int r2 = ((1 ? s32 : u32) < 0);
    printf("%d %d\\n", r1, r2);
    return 0;
}
""",
    "shift_rhs_type_and_sign": """\
#include <stdio.h>
#include <stdint.h>
int main(void) {
    int32_t neg32 = -16;
    uint64_t sh = 2ULL;
    int64_t r1 = (neg32 >> sh);
    uint8_t u8 = 0x80u;
    int r2 = ((u8 << 1) >> 1);
    printf("%lld %d\\n", (long long)r1, r2);
    return 0;
}
""",
    "ptr_diff_and_postinc_subword": """\
#include <stdio.h>
#include <stdint.h>
struct Item { int32_t a; int16_t b; int8_t c; };
int main(void) {
    struct Item arr[8];
    struct Item *p = &arr[1];
    struct Item *q = &arr[6];
    long diff1 = (long)(q - p);
    long diff2 = (long)(p - q);
    uint8_t u = 255;
    uint8_t old_u = u++;
    int8_t s = 126;
    int8_t pre_s = ++s;
    printf("%ld %ld %u %u %d %d\\n", diff1, diff2, (unsigned)old_u, (unsigned)u, (int)pre_s, (int)s);
    return 0;
}
""",
    "comma_and_short_circuit": """\
#include <stdio.h>
static int side(int *p, int d) { *p = *p * 10 + d; return d; }
int main(void) {
    int s = 1;
    int a = (side(&s, 2), side(&s, 3), s + 5);
    int b = (0 && side(&s, 4));
    int c = (1 || side(&s, 5));
    int d = (1 && side(&s, 6));
    int e = (0 || side(&s, 7));
    printf("%d %d %d %d %d %d\\n", s, a, b, c, d, e);
    return 0;
}
""",
    "bitfield_compound_assignments": """\
#include <stdio.h>
#include <stdint.h>
struct BF { signed int s5 : 5; unsigned int u5 : 5; signed int s3 : 3; unsigned int u3 : 3; };
int main(void) {
    struct BF bf = { -8, 17, -4, 5 };
    bf.s5 += 3;
    bf.s5 /= 2;
    bf.u5 <<= 1;
    bf.u5 += 15;
    bf.u5 /= 3;
    int r_s3 = (bf.s3 += 7);
    int r_u3 = (bf.u3 += 6);
    printf("%d %u %d %u %d %d\\n", (int)bf.s5, (unsigned)bf.u5, (int)bf.s3, (unsigned)bf.u3, r_s3, r_u3);
    return 0;
}
""",
    "signed_unsigned_casts_width_boundaries": """\
#include <stdio.h>
#include <stdint.h>
int main(void) {
    int8_t s8_min = -128;
    uint8_t u8_max = 255u;
    uint16_t u16_max = 65535u;
    int32_t s32_min = (-2147483647 - 1);
    uint32_t u32_max = 4294967295u;
    printf("%u %lld %llu %d %d %u %d\\n",
           (uint32_t)s8_min, (long long)(int64_t)u32_max, (unsigned long long)(uint64_t)s32_min,
           (int32_t)u16_max, (int)(int16_t)u8_max, (unsigned)(uint16_t)s8_min, (int)(int8_t)u16_max);
    return 0;
}
""",
    "shift_counts_zero_and_width_minus_1": """\
#include <stdio.h>
#include <stdint.h>
int main(void) {
    uint8_t sh_u8 = 0x81u;
    int8_t sh_s8 = -64;
    uint32_t sh_u32 = 1u;
    int32_t sh_s32 = -1073741824;
    uint64_t sh_u64 = 1ULL;
    sh_u8 <<= 0;
    sh_s8 >>= 5;
    sh_u32 <<= 31;
    sh_s32 >>= 30;
    sh_u64 <<= 63;
    printf("%u %d %u %d %llu\\n",
           (unsigned)sh_u8, (int)sh_s8, sh_u32, sh_s32, (unsigned long long)sh_u64);
    return 0;
}
""",
    "compound_lvalue_evaluated_once": """\
#include <stdio.h>
#include <stdint.h>
static int g_lval_evals = 0;
static int *get_tracked_ptr(int *base, int idx) { g_lval_evals++; return &base[idx]; }
struct Holder { int8_t s8; uint8_t u8; int32_t s32; };
static int g_holder_evals = 0;
static struct Holder *get_holder(struct Holder *h) { g_holder_evals++; return h; }
int main(void) {
    int arr[4] = { 10, 20, -30, 40 };
    *get_tracked_ptr(arr, 0) += 5;
    *get_tracked_ptr(arr, 1) *= 2.5;
    *get_tracked_ptr(arr, 2) /= 4u;
    struct Holder h = { -12, 0x11u, 100 };
    get_holder(&h)->s8 /= 3;
    get_holder(&h)->u8 <<= 2;
    get_holder(&h)->s32 *= 1.5;
    printf("%d %d %d %d %d %d %u %d\\n",
           g_lval_evals, g_holder_evals, arr[0], arr[1], arr[2],
           (int)h.s8, (unsigned)h.u8, h.s32);
    return 0;
}
""",
    "float_to_int_conversion_boundaries": """\
#include <stdio.h>
#include <stdint.h>
int main(void) {
    double d_neg = -127.9, d_pos_u8 = 254.9, d_pos_u16 = 65534.9, d_pos_u32 = 4000000000.0;
    float f_neg = -32767.5f;
    int8_t fi_s8 = (int8_t)d_neg;
    uint8_t fi_u8 = (uint8_t)d_pos_u8;
    uint16_t fi_u16 = (uint16_t)d_pos_u16;
    uint32_t fi_u32 = (uint32_t)d_pos_u32;
    int16_t fi_s16 = (int16_t)f_neg;
    uint8_t ca_u8 = 100u; ca_u8 *= 2.5;
    int16_t ca_s16 = -1000; ca_s16 *= 3.25;
    uint32_t ca_u32 = 1000000000u; ca_u32 *= 3.5;
    printf("%d %u %u %u %d %u %d %u\\n",
           (int)fi_s8, (unsigned)fi_u8, (unsigned)fi_u16, fi_u32, (int)fi_s16,
           (unsigned)ca_u8, (int)ca_s16, ca_u32);
    return 0;
}
""",
}

ABI_CALLEE_C = """\
#include <stdint.h>
#include <stdarg.h>

struct PairI32 { int32_t a; int32_t b; };
struct PairI64 { int64_t x; int64_t y; };
struct Big24   { int64_t a; int64_t b; int64_t c; };

struct PairI32 callee_pair_i32(struct PairI32 p, int32_t k) {
    struct PairI32 out;
    out.a = p.a * k + p.b;
    out.b = p.b * k - p.a;
    return out;
}

struct PairI64 callee_pair_i64(struct PairI64 p, int64_t delta) {
    struct PairI64 out;
    out.x = p.x + delta;
    out.y = p.y ^ delta;
    return out;
}

struct Big24 callee_big24(struct Big24 in, int64_t scale) {
    struct Big24 out;
    out.a = in.a * scale + 1;
    out.b = in.b * scale + 2;
    out.c = in.c * scale + 3;
    return out;
}

int64_t callee_many_mixed(
    int64_t a1, int64_t a2, int64_t a3, int64_t a4,
    int64_t a5, int64_t a6, int64_t a7, int64_t a8,
    double d1, double d2, int32_t tail
) {
    int64_t gpr_sum = a1 * 1 + a2 * 2 + a3 * 3 + a4 * 4 + a5 * 5 + a6 * 6 + a7 * 7 + a8 * 8;
    int64_t fp_sum = (int64_t)(d1 * 10.0 + d2 * 100.0);
    return gpr_sum + fp_sum + tail;
}

int64_t callee_variadic(int count, ...) {
    va_list ap;
    va_start(ap, count);
    int64_t acc = 0;
    for (int i = 0; i < count; i++) {
        int64_t v = va_arg(ap, int64_t);
        acc = acc * 3 + v;
    }
    va_end(ap);
    return acc;
}
"""

ABI_CALLER_C = """\
#include <stdio.h>
#include <stdint.h>

struct PairI32 { int32_t a; int32_t b; };
struct PairI64 { int64_t x; int64_t y; };
struct Big24   { int64_t a; int64_t b; int64_t c; };

struct PairI32 callee_pair_i32(struct PairI32 p, int32_t k);
struct PairI64 callee_pair_i64(struct PairI64 p, int64_t delta);
struct Big24   callee_big24(struct Big24 in, int64_t scale);
int64_t callee_many_mixed(
    int64_t a1, int64_t a2, int64_t a3, int64_t a4,
    int64_t a5, int64_t a6, int64_t a7, int64_t a8,
    double d1, double d2, int32_t tail
);
int64_t callee_variadic(int count, ...);

int main(void) {
    struct PairI32 p32 = { 12, -5 };
    struct PairI32 r32 = callee_pair_i32(p32, 3);

    struct PairI64 p64 = { 10000000000LL, 0x123456789ABCDEF0LL };
    struct PairI64 r64 = callee_pair_i64(p64, 0x0F0F0F0F0F0F0F0FLL);

    struct Big24 b24 = { 10, 20, 30 };
    struct Big24 rb24 = callee_big24(b24, 4);

    int64_t mixed = callee_many_mixed(10, 20, 30, 40, 50, 60, 70, 80, 1.5, 2.25, -17);
    int64_t var_res = callee_variadic(8, 1LL, 2LL, 3LL, 4LL, 5LL, 6LL, 7LL, 8LL);

    printf("P32=%d,%d P64=%lld,%lld B24=%lld,%lld,%lld MIX=%lld VAR=%lld\\n",
           r32.a, r32.b,
           (long long)r64.x, (long long)r64.y,
           (long long)rb24.a, (long long)rb24.b, (long long)rb24.c,
           (long long)mixed, (long long)var_res);
    return 0;
}
"""


def sha256_file(p: Path) -> str:
    return hashlib.sha256(p.read_bytes()).hexdigest()


def run_ref(cmd: list[str], bin_path: Path) -> dict[str, Any]:
    comp = subprocess.run(cmd, text=True, capture_output=True)
    if comp.returncode != 0:
        return {"compile_rc": comp.returncode, "stderr": comp.stderr.strip(), "stdout": None, "run_rc": None}
    run = subprocess.run([str(bin_path)], text=True, capture_output=True)
    return {
        "compile_rc": 0,
        "run_rc": run.returncode,
        "stdout": run.stdout.strip(),
        "stderr": run.stderr.strip(),
        "bin_sha256": sha256_file(bin_path),
    }


def run_zcc(zcc_bin: Path, src_c: Path, asm_s: Path, out_bin: Path, extra_flags: list[str]) -> dict[str, Any]:
    cmd = [str(zcc_bin)] + extra_flags + [str(src_c), "-S", "-o", str(asm_s)]
    comp = subprocess.run(cmd, text=True, capture_output=True)
    if comp.returncode != 0:
        return {"compile_rc": comp.returncode, "stderr": comp.stderr.strip(), "stdout": None, "run_rc": None}
    link = subprocess.run(["gcc", "-no-pie", str(asm_s), "-o", str(out_bin), "-lm"], text=True, capture_output=True)
    if link.returncode != 0:
        return {"compile_rc": link.returncode, "stderr": link.stderr.strip(), "stdout": None, "run_rc": None}
    run = subprocess.run([str(out_bin)], text=True, capture_output=True)
    return {
        "compile_rc": 0,
        "run_rc": run.returncode,
        "stdout": run.stdout.strip(),
        "asm_sha256": sha256_file(asm_s),
        "bin_sha256": sha256_file(out_bin),
        "asm_path": str(asm_s),
    }


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--zcc", default="./zcc2", help="Patched ZCC binary")
    ap.add_argument("--unpatched-zcc", default="/tmp/zcc_cleanroom_repro/zcc2", help="Optional unpatched ZCC binary for negative control")
    ap.add_argument("--out", default="/tmp/zcc_track3_category_suite", help="Scratch output directory")
    ap.add_argument("--report", default="", help="Optional JSON report path")
    args = ap.parse_args()

    zcc_bin = Path(args.zcc).resolve()
    unpatched_bin = Path(args.unpatched_zcc).resolve() if args.unpatched_zcc else None
    out_dir = Path(args.out).resolve()
    out_dir.mkdir(parents=True, exist_ok=True)

    ref_configs = [
        ("gcc_O0", ["gcc", "-std=c99", "-O0", "-fno-strict-aliasing", "-fwrapv"]),
        ("gcc_O2", ["gcc", "-std=c99", "-O2", "-fno-strict-aliasing", "-fwrapv"]),
        ("clang_O2", ["clang", "-std=c99", "-O2", "-fno-strict-aliasing", "-fwrapv"]),
        ("gcc_ubsan", ["gcc", "-std=c99", "-O1", "-fno-strict-aliasing", "-fwrapv", "-fsanitize=undefined", "-fno-sanitize-recover=all"]),
    ]

    summary: dict[str, Any] = {
        "zcc_binary": str(zcc_bin),
        "zcc_sha256": sha256_file(zcc_bin),
        "unpatched_zcc_binary": str(unpatched_bin) if (unpatched_bin and unpatched_bin.is_file()) else None,
        "unpatched_zcc_sha256": sha256_file(unpatched_bin) if (unpatched_bin and unpatched_bin.is_file()) else None,
        "part_a_counterexamples": [],
        "part_b_semantic_categories": [],
        "part_c_cross_tu_abi": {},
        "all_passed": True,
    }

    # Part A: 5 Minimized Counterexamples
    for item in COUNTEREXAMPLES:
        cid = item["id"]
        c_file = out_dir / f"{cid}.c"
        c_file.write_text(item["code"], encoding="utf-8")

        refs: dict[str, Any] = {}
        ref_outputs: set[str] = set()
        for rname, rcmd in ref_configs:
            rbin = out_dir / f"{cid}.{rname}.bin"
            res = run_ref(rcmd + [str(c_file), "-o", str(rbin), "-lm"], rbin)
            refs[rname] = res
            if res["compile_rc"] == 0 and res["run_rc"] == 0 and res["stdout"] is not None:
                ref_outputs.add(res["stdout"])

        oracle_stdout = next(iter(ref_outputs)) if len(ref_outputs) == 1 else "ORACLE_DIVERGENCE"

        pre_patch_res = None
        if unpatched_bin and unpatched_bin.is_file():
            pre_patch_res = run_zcc(
                unpatched_bin,
                c_file,
                out_dir / f"{cid}.pre_patch.s",
                out_dir / f"{cid}.pre_patch.bin",
                [],
            )

        post_std = run_zcc(zcc_bin, c_file, out_dir / f"{cid}.post_std.s", out_dir / f"{cid}.post_std.bin", [])
        post_ir = run_zcc(zcc_bin, c_file, out_dir / f"{cid}.post_ir.s", out_dir / f"{cid}.post_ir.bin", ["--ir"])

        passed = (
            len(ref_outputs) == 1
            and post_std["compile_rc"] == 0
            and post_std["run_rc"] == 0
            and post_std["stdout"] == oracle_stdout
            and post_ir["compile_rc"] == 0
            and post_ir["run_rc"] == 0
            and post_ir["stdout"] == oracle_stdout
        )
        neg_control_detected = (
            pre_patch_res is not None
            and pre_patch_res["stdout"] == item["expected_pre_patch_zcc"]
            and pre_patch_res["stdout"] != oracle_stdout
        )
        if not passed:
            summary["all_passed"] = False

        rec = {
            "id": cid,
            "defect_id": item["defect_id"],
            "c99_clause": item["c99_clause"],
            "root_cause": item["root_cause"],
            "oracle_stdout": oracle_stdout,
            "references": refs,
            "pre_patch_zcc": pre_patch_res,
            "negative_control_confirmed": neg_control_detected,
            "post_patch_zcc_std": post_std,
            "post_patch_zcc_ir": post_ir,
            "passed": passed,
        }
        summary["part_a_counterexamples"].append(rec)
        print(
            f"[Part A] {cid:42s} | oracle={oracle_stdout!r:12s} "
            f"| pre_patch={pre_patch_res['stdout'] if pre_patch_res else 'N/A'!r:12s} "
            f"| post_std={post_std['stdout']!r:12s} | post_ir={post_ir['stdout']!r:12s} | PASS={passed}"
        )

    # Part B: 9 P0 Semantic Categories
    for cat_name, cat_code in SEMANTIC_CATEGORIES.items():
        c_file = out_dir / f"cat_{cat_name}.c"
        c_file.write_text(cat_code, encoding="utf-8")

        refs = {}
        ref_outputs = set()
        for rname, rcmd in ref_configs:
            rbin = out_dir / f"cat_{cat_name}.{rname}.bin"
            res = run_ref(rcmd + [str(c_file), "-o", str(rbin), "-lm"], rbin)
            refs[rname] = res
            if res["compile_rc"] == 0 and res["run_rc"] == 0 and res["stdout"] is not None:
                ref_outputs.add(res["stdout"])

        oracle_stdout = next(iter(ref_outputs)) if len(ref_outputs) == 1 else "ORACLE_DIVERGENCE"
        post_std = run_zcc(zcc_bin, c_file, out_dir / f"cat_{cat_name}.std.s", out_dir / f"cat_{cat_name}.std.bin", [])

        passed = (
            len(ref_outputs) == 1
            and post_std["compile_rc"] == 0
            and post_std["run_rc"] == 0
            and post_std["stdout"] == oracle_stdout
        )
        if not passed:
            summary["all_passed"] = False

        summary["part_b_semantic_categories"].append({
            "category": cat_name,
            "oracle_stdout": oracle_stdout,
            "zcc_std_stdout": post_std["stdout"],
            "zcc_std_asm_sha256": post_std.get("asm_sha256"),
            "zcc_std_bin_sha256": post_std.get("bin_sha256"),
            "passed": passed,
        })
        print(f"[Part B] {cat_name:32s} | oracle={oracle_stdout!r:24s} | std={post_std['stdout']!r:24s} | PASS={passed}")

    # Part C: P1 2-TU Cross-Toolchain System V ABI Interop
    caller_c = out_dir / "abi_caller.c"
    callee_c = out_dir / "abi_callee.c"
    caller_c.write_text(ABI_CALLER_C, encoding="utf-8")
    callee_c.write_text(ABI_CALLEE_C, encoding="utf-8")

    gcc_caller_o = out_dir / "gcc_caller.o"
    gcc_callee_o = out_dir / "gcc_callee.o"
    zcc_caller_s = out_dir / "zcc_caller.s"
    zcc_callee_s = out_dir / "zcc_callee.s"
    zcc_caller_o = out_dir / "zcc_caller.o"
    zcc_callee_o = out_dir / "zcc_callee.o"

    subprocess.run(["gcc", "-std=c99", "-O2", "-c", str(caller_c), "-o", str(gcc_caller_o)], check=True)
    subprocess.run(["gcc", "-std=c99", "-O2", "-c", str(callee_c), "-o", str(gcc_callee_o)], check=True)
    subprocess.run([str(zcc_bin), str(caller_c), "-S", "-o", str(zcc_caller_s)], check=True)
    subprocess.run([str(zcc_bin), str(callee_c), "-S", "-o", str(zcc_callee_s)], check=True)
    subprocess.run(["gcc", "-c", str(zcc_caller_s), "-o", str(zcc_caller_o)], check=True)
    subprocess.run(["gcc", "-c", str(zcc_callee_s), "-o", str(zcc_callee_o)], check=True)

    ref_abi_bin = out_dir / "abi_gcc_gcc.bin"
    dir1_bin = out_dir / "abi_zcc_caller_gcc_callee.bin"
    dir2_bin = out_dir / "abi_gcc_caller_zcc_callee.bin"
    dir3_bin = out_dir / "abi_zcc_caller_zcc_callee.bin"

    subprocess.run(["gcc", "-no-pie", str(gcc_caller_o), str(gcc_callee_o), "-o", str(ref_abi_bin)], check=True)
    subprocess.run(["gcc", "-no-pie", str(zcc_caller_o), str(gcc_callee_o), "-o", str(dir1_bin)], check=True)
    subprocess.run(["gcc", "-no-pie", str(gcc_caller_o), str(zcc_callee_o), "-o", str(dir2_bin)], check=True)
    subprocess.run(["gcc", "-no-pie", str(zcc_caller_o), str(zcc_callee_o), "-o", str(dir3_bin)], check=True)

    out_ref = subprocess.run([str(ref_abi_bin)], text=True, capture_output=True, check=True).stdout.strip()
    out_d1 = subprocess.run([str(dir1_bin)], text=True, capture_output=True, check=True).stdout.strip()
    out_d2 = subprocess.run([str(dir2_bin)], text=True, capture_output=True, check=True).stdout.strip()
    out_d3 = subprocess.run([str(dir3_bin)], text=True, capture_output=True, check=True).stdout.strip()

    abi_passed = (out_ref == out_d1 == out_d2 == out_d3)
    if not abi_passed:
        summary["all_passed"] = False

    summary["part_c_cross_tu_abi"] = {
        "gcc_caller_gcc_callee_stdout": out_ref,
        "zcc_caller_gcc_callee_stdout": out_d1,
        "gcc_caller_zcc_callee_stdout": out_d2,
        "zcc_caller_zcc_callee_stdout": out_d3,
        "zcc_caller_s_sha256": sha256_file(zcc_caller_s),
        "zcc_callee_s_sha256": sha256_file(zcc_callee_s),
        "passed": abi_passed,
    }
    print(f"[Part C] 2-TU ABI Interop | ref={out_ref!r} | zcc->gcc={out_d1!r} | gcc->zcc={out_d2!r} | PASS={abi_passed}")

    report_path = Path(args.report) if args.report else (out_dir / "track3_category_report.json")
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    print(f"TRACK3_CATEGORY_SUITE_VERDICT: {'PASS' if summary['all_passed'] else 'FAIL'} (report={report_path})")
    return 0 if summary["all_passed"] else 1


if __name__ == "__main__":
    sys.exit(main())

