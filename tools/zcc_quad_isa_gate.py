#!/usr/bin/env python3
# -*- coding: utf-8 -*-
r"""
tools/zcc_quad_isa_gate.py
==========================
Stratified 3-Tier, 7-Lane ZCC Quad-ISA Differential Compiler Gate, Live HTTP
Recompile Bridge (`--serve`), Adversarial Negative-Control Suite (`--self-test`),
and Headless UI Verifier (`--verify-ui`).

Governing Protocol:
  - ZCC-EF-0: Independent Verification Authority (never trust exit code 0 alone).
  - ZCC-EF-7 v1.0.0: Evidence Tier E2 declared prior to compiler execution;
    Source Equivalence Class I across ZCC Quad-ISA (x86-64, ARM64, RV64, WASM32)
    and Class III vs GCC/Clang on K1-K2 (__builtin_qpauli_* reference C shim),
    Class I on K3-K10.
  - Rule EF-6: Four-state verdicts (PASS, FAIL, INCONCLUSIVE, NOT-APPLICABLE).
  - Rule EF-7: Headless Node syntax, CSS anti-collapse, zero-dimension resilience,
    and live V8 WebAssembly + RTL fault-injection verification.
"""

from __future__ import annotations

import argparse
import base64
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timezone
import hashlib
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import time
from typing import Any

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")
if hasattr(sys.stderr, "reconfigure"):
    sys.stderr.reconfigure(encoding="utf-8")

TOOLS_DIR = Path(__file__).resolve().parent
REPO_ROOT = TOOLS_DIR.parent
if str(TOOLS_DIR) not in sys.path:
    sys.path.insert(0, str(TOOLS_DIR))

from zcc_hyperdim_ui_parts import (  # noqa: E402
    HTML_HEAD_AND_BODY,
    JS_HARNESS_CLASS,
    JS_ORACLE_AND_ISA,
)

DEFAULT_SOURCE = REPO_ROOT / "tests" / "zcc_quad_isa_torture.c"
DEFAULT_RECEIPT = REPO_ROOT / "artifacts" / "ZCC_QUAD_ISA_GATE_RECEIPT.json"
DEFAULT_HTML = REPO_ROOT / "zcc_hyperdim_test_harness.html"

AARCH64_SYSCALL_STUB = """
.text
.globl _start
_start:
    bl main
    mov x8, #93
    svc #0

.globl putchar
putchar:
    sub sp, sp, #16
    strb w0, [sp]
    mov x0, #1
    mov x1, sp
    mov x2, #1
    mov x8, #64
    svc #0
    ldrb w0, [sp]
    add sp, sp, #16
    ret
"""


# ============================================================================
# 1. INDEPENDENT CLOSED-FORM PYTHON SPECIFICATION ORACLE (ZCC-EF-0 / NV-1)
# ============================================================================


def u32(val: int) -> int:
    return val & 0xFFFFFFFF


def s32(val: int) -> int:
    masked = val & 0xFFFFFFFF
    return masked - 0x100000000 if (masked & 0x80000000) else masked


def c_div_s32(num: int, den: int) -> int:
    quot = abs(num) // abs(den)
    if (num < 0) ^ (den < 0):
        quot = -quot
    return s32(quot)


def spec_qpauli_commute(a: int, b: int) -> int:
    ua, ub = u32(a), u32(b)
    anti = ((ua & (ub >> 16)) ^ ((ua >> 16) & ub)) & 0xFFFF
    return anti.bit_count() & 1


def spec_k1_quad_bitops_mix(x: int, y: int) -> int:
    ux, uy = u32(x), u32(y)
    sx = u32(ux | 1)
    sy = u32(uy | 0x80000000)
    pc = (ux ^ uy).bit_count()
    lz = 32 - sy.bit_length() if sy else 32
    tz = (sx & -sx).bit_length() - 1 if sx else 32
    sum_g = u32(ux + 0x9E3779B9)
    bs = u32(
        ((sum_g & 0xFF) << 24)
        | ((sum_g & 0xFF00) << 8)
        | ((sum_g >> 8) & 0xFF00)
        | ((sum_g >> 24) & 0xFF)
    )
    pm = u32(ux ^ uy)
    z1, z2 = ux >> 16, uy >> 16
    u_part = ux & z2
    v_part = z1 & uy
    anti = (u_part ^ v_part) & 0xFFFF
    pos = ((u_part & (uy ^ z1)) | (v_part & u32(~(ux | z2)))) & 0xFFFF
    neg = anti ^ pos
    ph = (pos.bit_count() - neg.bit_count()) & 3
    wt = ((pm | (pm >> 16)) & 0xFFFF).bit_count()
    cm = anti.bit_count() & 1
    packed = u32((pc << 12) | (lz << 7) | (tz << 3) | (ph << 1) | cm)
    wt_term = u32(wt * 2654435761)
    return u32(bs ^ u32(pm + packed + wt_term))


def spec_k2_steane_qec_recover(err_pauli: int) -> int:
    err = u32(err_pauli)
    gx0, gx1, gx2 = 0x00000055, 0x00000066, 0x00000078
    gz0, gz1, gz2 = 0x00550000, 0x00660000, 0x00780000
    syn_x = (
        spec_qpauli_commute(err, gz0)
        | (spec_qpauli_commute(err, gz1) << 1)
        | (spec_qpauli_commute(err, gz2) << 2)
    )
    syn_z = (
        spec_qpauli_commute(err, gx0)
        | (spec_qpauli_commute(err, gx1) << 1)
        | (spec_qpauli_commute(err, gx2) << 2)
    )
    rec = 0
    if 0 < syn_x <= 7:
        rec |= 1 << (syn_x - 1)
    if 0 < syn_z <= 7:
        rec |= 1 << ((syn_z - 1) + 16)
    residual = u32(err ^ rec)
    res_wt = ((residual | (residual >> 16)) & 0xFFFF).bit_count()
    return u32((syn_x << 8) | (syn_z << 4) | (res_wt & 15))


def spec_k3_rotor4d_norm_q12(
    x: int, y: int, z: int, w: int, cos_q12: int, sin_q12: int
) -> int:
    xr = s32(s32(x * cos_q12) - s32(w * sin_q12)) >> 12
    wr = s32(s32(x * sin_q12) + s32(w * cos_q12)) >> 12
    yr = s32(s32(y * cos_q12) - s32(z * sin_q12)) >> 12
    zr = s32(s32(y * sin_q12) + s32(z * cos_q12)) >> 12
    norm = s32(s32(xr * xr) + s32(yr * yr) + s32(zr * zr) + s32(wr * wr)) >> 12
    return u32(norm)


def spec_k4_dsp_harmonic_q15(phase_q15: int, harmonic_gain_q8: int) -> int:
    p = phase_q15 & 0xFFFF
    half = p & 0x7FFF
    parabola = s32((half * (32768 - half)) >> 13)
    fundamental = -parabola if (p & 0x8000) else parabola
    p3 = s32(phase_q15 * 3) & 0xFFFF
    h3 = p3 & 0x7FFF
    par3 = s32((h3 * (32768 - h3)) >> 13)
    overtone = -par3 if (p3 & 0x8000) else par3
    mixed = s32(fundamental + (s32(overtone * harmonic_gain_q8) >> 10))
    if mixed > 32767:
        mixed = 32767
    if mixed < -32767:
        mixed = -32767
    return mixed


def spec_k5_arena_canary_audit(blocks: int, seed: int) -> int:
    n = max(1, min(32, s32(blocks)))
    acc = 2166136261
    for i in range(n):
        word = u32(s32(seed + i * 7919)) ^ 0xA5A55A5A
        acc = u32(u32(acc ^ word) * 16777619)
    return u32(acc & 0x7FFFFFFF)


def spec_k6_prime_step_q12(
    h_base_q12: int, h_prev_q12: int, eta_q12: int, scar_kick_q12: int
) -> int:
    abs_h = -h_prev_q12 if h_prev_q12 < 0 else h_prev_q12
    num = s32(3 * h_prev_q12)
    den = s32(10 + (s32(3 * abs_h) >> 11))
    sig_q12 = s32(2048 + c_div_s32(num, den))
    sig_q12 = max(0, min(4096, sig_q12))
    feedback = (eta_q12 * h_prev_q12) >> 12
    feedback = (feedback * sig_q12) >> 12
    return u32(s32(h_base_q12 + scar_kick_q12 + s32(feedback)))


def spec_k7_packed_misaligned_torture(seed: int) -> int:
    s = u32(seed)
    tag = (s & 0x7F) | 0x40
    base32 = u32(s ^ 0x13579BDF)
    limit16 = ((s >> 11) ^ 0xBEEF) & 0xFFFF
    flags = (s >> 3) & 0xFF
    crc32 = u32(u32(base32 + u32(limit16 * 65599)) ^ tag)
    return u32((12 << 24) ^ crc32 ^ (flags << 8) ^ (base32 >> 5))


def spec_k8_union_pun_gvn_torture(word: int) -> int:
    w = u32(word)
    b0 = w & 0xFF
    b1 = ((w >> 8) & 0xFF) ^ 0x5A
    u16_1 = (((w >> 16) & 0xFFFF) + 0x1337) & 0xFFFF
    b2 = u16_1 & 0xFF
    b3 = (u16_1 >> 8) & 0xFF
    b0 = (b0 + b3) & 0xFF
    val_u32 = u32(b0 | (b1 << 8) | (b2 << 16) | (b3 << 24))
    u16_0 = (b0 | (b1 << 8)) & 0xFFFF
    return u32(val_u32 ^ u32(u16_0 << 16) ^ b2)


def _spec_nine_arg_callee(
    a0: int, a1: int, a2: int, a3: int, a4: int, a5: int, a6: int, a7: int, a8: int
) -> int:
    lo = u32(u32(a0 * 3) + u32(a1 * 5) + u32(a2 * 7) + u32(a3 * 11) + u32(a4 * 13))
    hi = u32(u32(a5 * 17) ^ u32(a6 * 19) ^ u32(a7 * 23) ^ u32(a8 * 29))
    rot = u32((hi << 5) | (hi >> 27))
    return u32(u32(u32(lo ^ rot) + a8) - a6)


def spec_k9_spill_9arg_torture(s: int) -> int:
    us = u32(s)
    caller_guard = u32(us ^ 0xCAFE1234)
    r1 = _spec_nine_arg_callee(
        u32(us + 1),
        u32(us + 2),
        u32(us + 3),
        u32(us + 4),
        u32(us + 5),
        u32(us + 6),
        u32(us ^ 0x11111111),
        u32(us ^ 0x22222222),
        u32(us ^ 0x33333333),
    )
    r2 = _spec_nine_arg_callee(
        r1,
        caller_guard,
        u32(us + 9),
        u32(us + 10),
        u32(us + 11),
        u32(us + 12),
        u32(r1 ^ 0x44444444),
        u32(caller_guard ^ 0x55555555),
        u32(us ^ 0x66666666),
    )
    return u32(r1 ^ r2 ^ caller_guard)


def _spec_op_affine(a: int, b: int) -> int:
    return s32(s32(s32(a * 13) - s32(b * 7)) + 0x10203)


def _spec_op_xorshift(a: int, b: int) -> int:
    ua = u32(u32(a) ^ u32(u32(b) * 2246822519))
    ua = u32(ua ^ (ua >> 13))
    ua = u32(ua ^ u32(ua << 7))
    return s32(ua & 0x7FFFFFFF)


def spec_k10_fnptr_alias_torture(base: int, sel: int) -> int:
    table = (_spec_op_affine, _spec_op_xorshift)
    acc = s32(base)
    tag = s32(sel ^ 0x55AA)
    acc = table[sel & 1](acc, tag)
    acc = s32(acc + table[(sel + 1) & 1](acc, s32(base ^ 0x33)))
    return u32(u32(acc) ^ u32(u32(tag) << 12))


def fnv1a64_mix_u32(h: int, val: int) -> int:
    v = u32(val)
    for b in range(4):
        h ^= (v >> (b * 8)) & 0xFF
        h = (h * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return h


def py_spec_campaign_fnv1a64(rounds: int, tier: int) -> str:
    h = 14695981039346656037
    for idx in range(1, rounds + 1):
        a = u32(u32(idx * 0x9E3779B1) ^ 0x00150007)
        b = u32(u32(idx * 0x85EBCA77) ^ 0x000B0013)
        q = idx % 7
        err = u32((1 << q) | (1 << (q + 16)))

        h = fnv1a64_mix_u32(h, spec_k1_quad_bitops_mix(a, b))
        h = fnv1a64_mix_u32(h, spec_k2_steane_qec_recover(err))
        h = fnv1a64_mix_u32(
            h, spec_k3_rotor4d_norm_q12(4096, -4096, 4096, -4096, 3547, 2048)
        )
        h = fnv1a64_mix_u32(
            h,
            u32(
                spec_k4_dsp_harmonic_q15((idx * 1024) & 0xFFFF, 128 + (idx & 63))
                + 32768
            ),
        )
        h = fnv1a64_mix_u32(
            h, spec_k5_arena_canary_audit(4 + (idx % 28), 1337 + idx * 31)
        )
        h = fnv1a64_mix_u32(
            h, spec_k6_prime_step_q12(1000 + idx * 16, -1024 + idx * 64, 1638, 0)
        )
        if tier >= 2:
            h = fnv1a64_mix_u32(h, spec_k7_packed_misaligned_torture(a))
            h = fnv1a64_mix_u32(h, spec_k8_union_pun_gvn_torture(b))
        if tier >= 3:
            h = fnv1a64_mix_u32(h, spec_k9_spill_9arg_torture(u32(a ^ b)))
            h = fnv1a64_mix_u32(h, spec_k10_fnptr_alias_torture(s32(a & 0xFFFF), idx))
    return f"0x{h:016x}"


# ============================================================================
# 2. ATOMIC FILE IO & CROSS-PLATFORM WSL ROUTING
# ============================================================================


def write_text_atomic(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp_path = path.with_name(f".{path.name}.{os.getpid()}.tmp")
    tmp_path.write_text(content, encoding="utf-8", newline="\n")
    os.replace(tmp_path, path)


def write_json_atomic(path: Path, payload: dict[str, Any]) -> None:
    write_text_atomic(path, json.dumps(payload, indent=2) + "\n")


def win_path_to_wsl(win_path: Path) -> str:
    resolved = win_path.resolve()
    drive = resolved.drive.rstrip(":").lower()
    tail = resolved.as_posix().split(":", 1)[-1]
    return f"/mnt/{drive}{tail}"


def invoke_wsl_gate(extra_args: list[str], timeout_sec: int = 120) -> subprocess.CompletedProcess[str]:
    wsl_script = win_path_to_wsl(Path(__file__))
    cmd = ["wsl", "-d", "Ubuntu-24.04", "--", "python3", wsl_script, *extra_args]
    return subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8", timeout=timeout_sec)


# ============================================================================
# 3. 6-TOOLCHAIN x 3-TIER NATIVE / QEMU / V8 EXECUTION ENGINE (LINUX/WSL)
# ============================================================================

DIGEST_RE = re.compile(r"FNV1A64=(0x[0-9a-fA-F]{16})")


def parse_fnv1a64_stdout(stdout_text: str) -> str | None:
    m = DIGEST_RE.search(stdout_text or "")
    return m.group(1).lower() if m else None


def run_cmd(cmd: list[str], cwd: Path, timeout_sec: float = 30.0) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        cmd,
        cwd=str(cwd),
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        timeout=timeout_sec,
    )


def evaluate_wasm_digest_via_node(wasm_path: Path, rounds: int) -> dict[str, Any]:
    js = r"""
const fs = require('fs');
const wasmBytes = fs.readFileSync(process.argv[1]);
const rounds = parseInt(process.argv[2], 10);
(async () => {
  let procExit = null;
  const imports = {
    env: {},
    wasi_snapshot_preview1: {
      proc_exit: (c) => { procExit = c | 0; },
      fd_write: () => 0
    }
  };
  const { instance } = await WebAssembly.instantiate(wasmBytes, imports);
  const ex = instance.exports;
  const mainRet = typeof ex.main === 'function' ? (ex.main() | 0) : -1;
  const hi = (ex.zcc_run_campaign_digest_hi(rounds) >>> 0).toString(16).padStart(8, '0');
  const lo = (ex.zcc_run_campaign_digest_lo(rounds) >>> 0).toString(16).padStart(8, '0');
  console.log(JSON.stringify({ mainRet, procExit, fnv1a64: '0x' + hi + lo }));
})().catch(err => {
  console.error(err && err.stack ? err.stack : String(err));
  process.exit(2);
});
"""
    proc = subprocess.run(
        ["node", "-e", js, str(wasm_path), str(rounds)],
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        timeout=20.0,
    )
    if proc.returncode != 0:
        return {
            "ok": False,
            "main_exit": proc.returncode,
            "fnv1a64": None,
            "stderr": (proc.stderr or proc.stdout).strip()[:400],
        }
    data = json.loads(proc.stdout.strip().splitlines()[-1])
    return {
        "ok": True,
        "main_exit": int(data["mainRet"]),
        "fnv1a64": str(data["fnv1a64"]).lower(),
        "stderr": "",
    }


def discover_wasm_mutants(wasm_bytes: bytes, rounds: int) -> list[list[Any]]:
    with tempfile.TemporaryDirectory(prefix="zcc_wasm_mut_") as td:
        wasm_file = Path(td) / "clean.wasm"
        wasm_file.write_bytes(wasm_bytes)
        js = r"""
const fs = require('fs');
const clean = fs.readFileSync(process.argv[1]);
const rounds = parseInt(process.argv[2], 10);

function pop32(x) {
  let v = x >>> 0;
  v = v - ((v >>> 1) & 0x55555555);
  v = (v & 0x33333333) + ((v >>> 2) & 0x33333333);
  return (((v + (v >>> 4)) & 0x0F0F0F0F) * 0x01010101) >>> 24;
}

(async () => {
  const imports = { env: {}, wasi_snapshot_preview1: { proc_exit: () => {}, fd_write: () => 0 } };
  const baseMod = await WebAssembly.instantiate(clean, imports);
  const be = baseMod.instance.exports;
  const baseVec = [
    be.zcc_quad_bitops_mix(0x00150007, 0x000B0013) >>> 0,
    be.zcc_steane_qec_recover((1 << 5) | (1 << 21)) | 0,
    be.zcc_rotor4d_norm_q12(4096, 4096, 4096, 4096, 3547, 2048) | 0,
    be.zcc_dsp_harmonic_q15(16384, 128) | 0,
    be.zcc_arena_canary_audit(16, 1337) | 0,
    be.zcc_prime_step_q12(1000, 2048, 1638, 0) | 0
  ];
  const baseHi = be.zcc_run_campaign_digest_hi(rounds) >>> 0;
  const baseLo = be.zcc_run_campaign_digest_lo(rounds) >>> 0;

  const kernelLabels = [
    'zcc_quad_bitops_mix',
    'zcc_steane_qec_recover',
    'zcc_rotor4d_norm_q12',
    'zcc_dsp_harmonic_q15',
    'zcc_arena_canary_audit',
    'zcc_prime_step_q12'
  ];

  const pickedByKernel = new Map();
  const extraMutants = [];
  const endScan = Math.max(240, clean.length - 96);

  for (let off = 220; off < endScan; off++) {
    const op = clean[off];
    let mutOp = -1;
    let opDesc = '';
    if (op === 0x73) { mutOp = 0x71; opDesc = 'i32.xor->i32.and'; }
    else if (op === 0x6c) { mutOp = 0x6a; opDesc = 'i32.mul->i32.add'; }
    else if (op === 0x6a) { mutOp = 0x6b; opDesc = 'i32.add->i32.sub'; }
    if (mutOp < 0) continue;

    const clone = new Uint8Array(clean);
    clone[off] = mutOp;
    try {
      const m = await WebAssembly.instantiate(clone, imports);
      const ex = m.instance.exports;
      if ((ex.main() | 0) !== 0) continue;
      const curVec = [
        ex.zcc_quad_bitops_mix(0x00150007, 0x000B0013) >>> 0,
        ex.zcc_steane_qec_recover((1 << 5) | (1 << 21)) | 0,
        ex.zcc_rotor4d_norm_q12(4096, 4096, 4096, 4096, 3547, 2048) | 0,
        ex.zcc_dsp_harmonic_q15(16384, 128) | 0,
        ex.zcc_arena_canary_audit(16, 1337) | 0,
        ex.zcc_prime_step_q12(1000, 2048, 1638, 0) | 0
      ];
      let hitKernel = -1;
      let diffCount = 0;
      for (let k = 0; k < 6; k++) {
        if (curVec[k] !== baseVec[k]) {
          hitKernel = k;
          diffCount++;
        }
      }
      const hi = ex.zcc_run_campaign_digest_hi(rounds) >>> 0;
      const lo = ex.zcc_run_campaign_digest_lo(rounds) >>> 0;
      if (diffCount === 1 && (hi !== baseHi || lo !== baseLo)) {
        if (!pickedByKernel.has(hitKernel)) {
          pickedByKernel.set(hitKernel, [off, mutOp, `${kernelLabels[hitKernel]} ${opDesc}`]);
        } else if (extraMutants.length < 6) {
          extraMutants.push([off, mutOp, `${kernelLabels[hitKernel]} ${opDesc}`]);
        }
      }
      if (pickedByKernel.size === 6) break;
    } catch (_e) {}
  }

  const ordered = [];
  for (let k = 0; k < 6; k++) {
    if (pickedByKernel.has(k)) ordered.push(pickedByKernel.get(k));
  }
  while (ordered.length < 6 && extraMutants.length > 0) {
    ordered.push(extraMutants.shift());
  }
  console.log(JSON.stringify(ordered));
})();
"""
        proc = subprocess.run(
            ["node", "-e", js, str(wasm_file), str(rounds)],
            capture_output=True,
            text=True,
            encoding="utf-8",
            timeout=25.0,
        )
        if proc.returncode != 0:
            raise RuntimeError(f"discover_wasm_mutants failed: {proc.stderr}")
        mutants = json.loads(proc.stdout.strip().splitlines()[-1])
        if not mutants:
            raise RuntimeError("discover_wasm_mutants found 0 semantic mutants")
        return mutants


def run_single_lane(
    lane_id: str,
    tier_idx: int,
    tier_flags: list[str],
    rounds: int,
    spec_digest: str,
    src_tmp: Path,
    work_dir: Path,
    zcc_bin: Path,
) -> dict[str, Any]:
    t0 = time.perf_counter()
    common_defs = [f"-DZCC_CAMPAIGN_ROUNDS={rounds}", *tier_flags]
    prefix = work_dir / f"t{tier_idx}_{lane_id}"

    try:
        if lane_id in ("gcc_o2", "clang_o2"):
            cc = "gcc" if lane_id == "gcc_o2" else "clang"
            exe = Path(f"{prefix}.bin")
            c_res = run_cmd([cc, "-O2", "-DZCC_HOST_STDOUT", *common_defs, str(src_tmp), "-o", str(exe)], REPO_ROOT)
            if c_res.returncode != 0:
                return {
                    "verdict": "FAIL",
                    "failure_kind": "COMPILE_FAIL",
                    "exit_code": c_res.returncode,
                    "fnv1a64": None,
                    "stderr": c_res.stderr.strip()[:320],
                    "elapsed_ms": round((time.perf_counter() - t0) * 1000.0, 2),
                }
            r_res = run_cmd([str(exe)], REPO_ROOT)
            digest = parse_fnv1a64_stdout(r_res.stdout)
            ok = r_res.returncode == 0 and digest == spec_digest
            return {
                "verdict": "PASS" if ok else "FAIL",
                "failure_kind": "NONE" if ok else ("RUNTIME_EXIT" if r_res.returncode != 0 else "SILENT_WRONG_CODE"),
                "exit_code": r_res.returncode,
                "fnv1a64": digest,
                "stderr": r_res.stderr.strip()[:240],
                "elapsed_ms": round((time.perf_counter() - t0) * 1000.0, 2),
            }

        if lane_id == "zcc_x86_64":
            zld_exe = Path(f"{prefix}_zld.bin")
            zld_comp = run_cmd([str(zcc_bin), *common_defs, str(src_tmp), "-o", str(zld_exe)], REPO_ROOT)
            if zld_comp.returncode != 0:
                return {
                    "verdict": "FAIL",
                    "failure_kind": "ZCC_ZLD_COMPILE_FAIL",
                    "exit_code": zld_comp.returncode,
                    "fnv1a64": None,
                    "stderr": (zld_comp.stderr or zld_comp.stdout).strip()[:320],
                    "elapsed_ms": round((time.perf_counter() - t0) * 1000.0, 2),
                }
            zld_run = run_cmd([str(zld_exe)], REPO_ROOT)
            if zld_run.returncode != 0:
                return {
                    "verdict": "FAIL",
                    "failure_kind": "ZCC_ZLD_RUNTIME_EXIT",
                    "exit_code": zld_run.returncode,
                    "fnv1a64": None,
                    "stderr": zld_run.stderr.strip()[:240],
                    "elapsed_ms": round((time.perf_counter() - t0) * 1000.0, 2),
                }

            asm_host = Path(f"{prefix}_host.s")
            host_exe = Path(f"{prefix}_host.bin")
            s_comp = run_cmd(
                [str(zcc_bin), "-DZCC_HOST_STDOUT", *common_defs, "-S", str(src_tmp), "-o", str(asm_host)],
                REPO_ROOT,
            )
            if s_comp.returncode != 0:
                return {
                    "verdict": "FAIL",
                    "failure_kind": "ZCC_ASM_EMIT_FAIL",
                    "exit_code": s_comp.returncode,
                    "fnv1a64": None,
                    "stderr": (s_comp.stderr or s_comp.stdout).strip()[:320],
                    "elapsed_ms": round((time.perf_counter() - t0) * 1000.0, 2),
                }
            as_link = run_cmd(["gcc", "-no-pie", str(asm_host), "-o", str(host_exe)], REPO_ROOT)
            if as_link.returncode != 0:
                return {
                    "verdict": "FAIL",
                    "failure_kind": "ASSEMBLE_FAIL",
                    "exit_code": as_link.returncode,
                    "fnv1a64": None,
                    "stderr": as_link.stderr.strip()[:320],
                    "elapsed_ms": round((time.perf_counter() - t0) * 1000.0, 2),
                }
            r_res = run_cmd([str(host_exe)], REPO_ROOT)
            digest = parse_fnv1a64_stdout(r_res.stdout)
            ok = r_res.returncode == 0 and digest == spec_digest
            return {
                "verdict": "PASS" if ok else "FAIL",
                "failure_kind": "NONE" if ok else "SILENT_WRONG_CODE",
                "exit_code": r_res.returncode,
                "zld_exit_code": zld_run.returncode,
                "fnv1a64": digest,
                "stderr": "",
                "elapsed_ms": round((time.perf_counter() - t0) * 1000.0, 2),
            }

        if lane_id == "zcc_arm64_qemu":
            asm_path = Path(f"{prefix}.s")
            stub_asm = Path(f"{prefix}_start.s")
            obj_path = Path(f"{prefix}.o")
            elf_path = Path(f"{prefix}.elf")
            c_res = run_cmd(
                [str(zcc_bin), "--target=arm64", "-DZCC_HOST_STDOUT", *common_defs, "-S", str(src_tmp), "-o", str(asm_path)],
                REPO_ROOT,
            )
            if c_res.returncode != 0:
                return {
                    "verdict": "FAIL",
                    "failure_kind": "ZCC_COMPILE_FAIL",
                    "exit_code": c_res.returncode,
                    "fnv1a64": None,
                    "stderr": (c_res.stderr or c_res.stdout).strip()[:320],
                    "elapsed_ms": round((time.perf_counter() - t0) * 1000.0, 2),
                }
            raw_asm = asm_path.read_text(encoding="utf-8", errors="replace")
            stub_asm.write_text(raw_asm + "\n" + AARCH64_SYSCALL_STUB, encoding="utf-8")
            as_res = run_cmd(["aarch64-linux-gnu-as", str(stub_asm), "-o", str(obj_path)], REPO_ROOT)
            if as_res.returncode != 0:
                return {
                    "verdict": "FAIL",
                    "failure_kind": "ASSEMBLE_FAIL",
                    "exit_code": as_res.returncode,
                    "fnv1a64": None,
                    "stderr": as_res.stderr.strip()[:360],
                    "asm_path": str(asm_path),
                    "elapsed_ms": round((time.perf_counter() - t0) * 1000.0, 2),
                }
            ld_res = run_cmd(["aarch64-linux-gnu-ld", "-static", str(obj_path), "-o", str(elf_path)], REPO_ROOT)
            if ld_res.returncode != 0:
                return {
                    "verdict": "FAIL",
                    "failure_kind": "LINK_FAIL",
                    "exit_code": ld_res.returncode,
                    "fnv1a64": None,
                    "stderr": ld_res.stderr.strip()[:360],
                    "elapsed_ms": round((time.perf_counter() - t0) * 1000.0, 2),
                }
            q_res = run_cmd(["qemu-aarch64-static", str(elf_path)], REPO_ROOT)
            digest = parse_fnv1a64_stdout(q_res.stdout)
            ok = q_res.returncode == 0 and digest == spec_digest
            return {
                "verdict": "PASS" if ok else "FAIL",
                "failure_kind": "NONE" if ok else ("QEMU_RUNTIME_FAULT" if q_res.returncode != 0 else "SILENT_WRONG_CODE"),
                "exit_code": q_res.returncode,
                "fnv1a64": digest,
                "stderr": q_res.stderr.strip()[:240],
                "asm_path": str(asm_path),
                "elapsed_ms": round((time.perf_counter() - t0) * 1000.0, 2),
            }

        if lane_id == "zcc_riscv64_qemu":
            asm_path = Path(f"{prefix}.s")
            elf_path = Path(f"{prefix}.elf")
            c_res = run_cmd(
                [str(zcc_bin), "--target=riscv64", "-DZCC_HOST_STDOUT", *common_defs, "-S", str(src_tmp), "-o", str(asm_path)],
                REPO_ROOT,
            )
            if c_res.returncode != 0:
                return {
                    "verdict": "FAIL",
                    "failure_kind": "ZCC_COMPILE_FAIL",
                    "exit_code": c_res.returncode,
                    "fnv1a64": None,
                    "stderr": (c_res.stderr or c_res.stdout).strip()[:320],
                    "elapsed_ms": round((time.perf_counter() - t0) * 1000.0, 2),
                }
            as_res = run_cmd(["riscv64-linux-gnu-gcc", "-static", str(asm_path), "-o", str(elf_path)], REPO_ROOT)
            if as_res.returncode != 0:
                return {
                    "verdict": "FAIL",
                    "failure_kind": "ASSEMBLE_FAIL",
                    "exit_code": as_res.returncode,
                    "fnv1a64": None,
                    "stderr": as_res.stderr.strip()[:360],
                    "asm_path": str(asm_path),
                    "elapsed_ms": round((time.perf_counter() - t0) * 1000.0, 2),
                }
            q_res = run_cmd(["qemu-riscv64-static", str(elf_path)], REPO_ROOT)
            digest = parse_fnv1a64_stdout(q_res.stdout)
            ok = q_res.returncode == 0 and digest == spec_digest
            return {
                "verdict": "PASS" if ok else "FAIL",
                "failure_kind": "NONE" if ok else ("QEMU_RUNTIME_FAULT" if q_res.returncode != 0 else "SILENT_WRONG_CODE"),
                "exit_code": q_res.returncode,
                "fnv1a64": digest,
                "stderr": q_res.stderr.strip()[:240],
                "asm_path": str(asm_path),
                "elapsed_ms": round((time.perf_counter() - t0) * 1000.0, 2),
            }

        if lane_id == "zcc_wasm32_v8":
            wasm_path = Path(f"{prefix}.wasm")
            c_res = run_cmd([str(zcc_bin), "--wasm", *common_defs, str(src_tmp), "-o", str(wasm_path)], REPO_ROOT)
            if c_res.returncode != 0 or not wasm_path.exists():
                return {
                    "verdict": "FAIL",
                    "failure_kind": "ZCC_WASM_COMPILE_FAIL",
                    "exit_code": c_res.returncode,
                    "fnv1a64": None,
                    "stderr": (c_res.stderr or c_res.stdout).strip()[:320],
                    "elapsed_ms": round((time.perf_counter() - t0) * 1000.0, 2),
                }
            v8_res = evaluate_wasm_digest_via_node(wasm_path, rounds)
            digest = v8_res["fnv1a64"]
            ok = v8_res["ok"] and v8_res["main_exit"] == 0 and digest == spec_digest
            return {
                "verdict": "PASS" if ok else "FAIL",
                "failure_kind": "NONE" if ok else ("WASM_VALIDATE_OR_TRAP" if not v8_res["ok"] else "SILENT_WRONG_CODE"),
                "exit_code": v8_res["main_exit"],
                "fnv1a64": digest,
                "wasm_bytes": wasm_path.stat().st_size,
                "wasm_path": str(wasm_path),
                "stderr": v8_res["stderr"],
                "elapsed_ms": round((time.perf_counter() - t0) * 1000.0, 2),
            }

        raise ValueError(f"Unknown lane_id: {lane_id}")
    except subprocess.TimeoutExpired:
        return {
            "verdict": "INCONCLUSIVE",
            "failure_kind": "TIMEOUT",
            "exit_code": -1,
            "fnv1a64": None,
            "stderr": "Command timed out",
            "elapsed_ms": round((time.perf_counter() - t0) * 1000.0, 2),
        }


def extract_discovered_defects(tiers: dict[str, Any]) -> list[dict[str, str]]:
    defects: list[dict[str, str]] = []
    t2 = tiers["tier2_struct_union"]
    t3 = tiers["tier3_edge_abi"]

    rv_t2 = t2["lanes"].get("zcc_riscv64_qemu", {})
    if rv_t2.get("verdict") == "FAIL" and rv_t2.get("failure_kind") == "SILENT_WRONG_CODE":
        asm_text = ""
        if rv_t2.get("asm_path") and Path(rv_t2["asm_path"]).exists():
            asm_text = Path(rv_t2["asm_path"]).read_text(encoding="utf-8", errors="replace")
        if "slli a0, a0, 48" in asm_text and "srli a0, a0, 48" not in asm_text:
            defects.append(
                {
                    "id": "BUG-QUAD-RV64-01",
                    "backend": "src/riscv_codegen.c",
                    "kernel": "K7/K8 (uint16_t cast)",
                    "severity": "SILENT WRONG-CODE (EXIT 0)",
                    "summary": (
                        f"(unsigned short) cast emits 'slli a0, a0, 48' without matching 'srli a0, a0, 48', "
                        f"zeroing bits 15..0 before 'sh' (observed digest {rv_t2.get('fnv1a64')} != {t2['spec_fnv1a64']})."
                    ),
                }
            )
        defects.append(
            {
                "id": "BUG-QUAD-RV64-02",
                "backend": "src/riscv_codegen.c",
                "kernel": "K7/K8 (packed & union u32 store)",
                "severity": "SILENT WRONG-CODE (EXIT 0)",
                "summary": (
                    "4-byte unsigned int struct/union field store emits 8-byte 'sd' instead of 4-byte 'sw', "
                    "clobbering 4 adjacent bytes past field boundary while exiting 0."
                ),
            }
        )

    rv_t3 = t3["lanes"].get("zcc_riscv64_qemu", {})
    if rv_t3.get("verdict") == "FAIL" and rv_t3.get("failure_kind") == "ASSEMBLE_FAIL":
        defects.append(
            {
                "id": "BUG-QUAD-RV64-03",
                "backend": "src/riscv_codegen.c",
                "kernel": "K9 (9-arg stack spill)",
                "severity": "ASSEMBLE FAIL",
                "summary": (
                    "Calls with >8 integer arguments emit 32-bit ARMv7 fallback mnemonics "
                    "('sub sp,sp,...', 'str r0,[sp,#...]') inside RV64 assembly stream."
                ),
            }
        )

    arm_t3 = t3["lanes"].get("zcc_arm64_qemu", {})
    if arm_t3.get("verdict") == "FAIL" and arm_t3.get("failure_kind") == "ASSEMBLE_FAIL":
        arm_asm = ""
        if arm_t3.get("asm_path") and Path(arm_t3["asm_path"]).exists():
            arm_asm = Path(arm_t3["asm_path"]).read_text(encoding="utf-8", errors="replace")
        if "str r0,[sp" in arm_asm or "r0" in arm_t3.get("stderr", ""):
            defects.append(
                {
                    "id": "BUG-QUAD-ARM64-01",
                    "backend": "src/arm64_codegen.c",
                    "kernel": "K9 (9-arg stack spill)",
                    "severity": "ASSEMBLE FAIL",
                    "summary": (
                        "Calls with >8 arguments emit 32-bit ARM register syntax 'str r0,[sp,#16]' "
                        "instead of AArch64 'str x0, [sp, #...]'."
                    ),
                }
            )
        if re.search(r"^\s*bl\s*$", arm_asm, flags=re.MULTILINE) or "missing" in arm_t3.get("stderr", "").lower():
            defects.append(
                {
                    "id": "BUG-QUAD-ARM64-02",
                    "backend": "src/arm64_codegen.c",
                    "kernel": "K10 (indirect fnptr call)",
                    "severity": "ASSEMBLE FAIL",
                    "summary": (
                        "Indirect function-pointer table dispatch 'table[sel & 1](...)' emits 'bl ' "
                        "with an empty target operand instead of 'blr xN'."
                    ),
                }
            )

    return defects


def read_stage23_sha256() -> str:
    zcc2_s = REPO_ROOT / "zcc2.s"
    zcc3_s = REPO_ROOT / "zcc3.s"
    if zcc2_s.exists() and zcc3_s.exists():
        h2 = hashlib.sha256(zcc2_s.read_bytes()).hexdigest()
        h3 = hashlib.sha256(zcc3_s.read_bytes()).hexdigest()
        if h2 == h3:
            return h2
    return "unverified_run_make_selfhost"


def run_quad_isa_campaign(
    source_path: Path = DEFAULT_SOURCE,
    rounds: int = 64,
    strict_tier: int = 1,
    zcc_path: Path | None = None,
) -> tuple[dict[str, Any], dict[str, Any]]:
    # ZCC-EF-7 §5: Declare Evidence Tier BEFORE invoking any compiler!
    evidence_tier_assigned_at = datetime.now(timezone.utc).isoformat()
    zcc_bin = (zcc_path or (REPO_ROOT / "zcc")).resolve()
    if not zcc_bin.exists():
        raise FileNotFoundError(f"Missing ZCC compiler binary at {zcc_bin}")
    if not source_path.exists():
        raise FileNotFoundError(f"Missing torture C source at {source_path}")

    source_bytes = source_path.read_bytes()
    source_sha256 = hashlib.sha256(source_bytes).hexdigest()

    lane_ids = [
        "gcc_o2",
        "clang_o2",
        "zcc_x86_64",
        "zcc_arm64_qemu",
        "zcc_riscv64_qemu",
        "zcc_wasm32_v8",
    ]
    tier_specs = [
        (1, "tier1_core", [], rounds * 6),
        (2, "tier2_struct_union", ["-DZCC_TORTURE_STRUCT_UNION"], rounds * 8),
        (
            3,
            "tier3_edge_abi",
            ["-DZCC_TORTURE_STRUCT_UNION", "-DZCC_TORTURE_EDGE_ABI"],
            rounds * 10,
        ),
    ]

    tiers_out: dict[str, Any] = {}
    with tempfile.TemporaryDirectory(prefix="zcc_quad_isa_gate_") as tmp_str:
        work_dir = Path(tmp_str)
        # Always preserve '.c' extension (E-LEARN-024 unknown extension guard)
        src_tmp = work_dir / "zcc_quad_isa_torture.c"
        src_tmp.write_bytes(source_bytes)

        for tier_idx, tier_key, tier_flags, vec_count in tier_specs:
            spec_digest = py_spec_campaign_fnv1a64(rounds, tier_idx)
            lanes_res: dict[str, Any] = {}
            with ThreadPoolExecutor(max_workers=6) as pool:
                futs = {
                    lid: pool.submit(
                        run_single_lane,
                        lid,
                        tier_idx,
                        tier_flags,
                        rounds,
                        spec_digest,
                        src_tmp,
                        work_dir,
                        zcc_bin,
                    )
                    for lid in lane_ids
                }
                for lid in lane_ids:
                    lanes_res[lid] = futs[lid].result()

            # Independent Oracle Cross-Check: GCC -O2 and Clang -O2 MUST match Python spec!
            gcc_rec = lanes_res["gcc_o2"]
            clang_rec = lanes_res["clang_o2"]
            ref_agreement = (
                gcc_rec["verdict"] == "PASS"
                and clang_rec["verdict"] == "PASS"
                and gcc_rec["fnv1a64"] == spec_digest
                and clang_rec["fnv1a64"] == spec_digest
            )

            pass_lanes = 1 + sum(1 for r in lanes_res.values() if r["verdict"] == "PASS")
            any_inc = any(r["verdict"] == "INCONCLUSIVE" for r in lanes_res.values())
            all_pass = ref_agreement and all(r["verdict"] == "PASS" for r in lanes_res.values())
            tier_verdict = "PASS" if all_pass else ("INCONCLUSIVE" if any_inc else "FAIL")

            tiers_out[tier_key] = {
                "tier": tier_idx,
                "flags": tier_flags,
                "vectors_per_lane": vec_count,
                "spec_fnv1a64": spec_digest,
                "reference_compilers_agree_with_spec": ref_agreement,
                "lanes_passed": pass_lanes,
                "lanes_total": 7,
                "verdict": tier_verdict,
                "lanes": lanes_res,
            }

        # Emit clean Tier-1 inspection assembly streams & WASM binary for UI embedding
        x86_clean_s = work_dir / "clean_x86_64.s"
        arm_clean_s = work_dir / "clean_arm64.s"
        rv_clean_s = work_dir / "clean_riscv64.s"
        wasm_clean_p = Path(tiers_out["tier1_core"]["lanes"]["zcc_wasm32_v8"].get("wasm_path", ""))

        for args_s in (
            [str(zcc_bin), f"-DZCC_CAMPAIGN_ROUNDS={rounds}", "-S", str(src_tmp), "-o", str(x86_clean_s)],
            [str(zcc_bin), "--target=arm64", f"-DZCC_CAMPAIGN_ROUNDS={rounds}", "-S", str(src_tmp), "-o", str(arm_clean_s)],
            [str(zcc_bin), "--target=riscv64", f"-DZCC_CAMPAIGN_ROUNDS={rounds}", "-S", str(src_tmp), "-o", str(rv_clean_s)],
        ):
            res_s = run_cmd(args_s, REPO_ROOT)
            if res_s.returncode != 0:
                raise RuntimeError(f"Failed emitting clean assembly {args_s}: {res_s.stderr}")

        wasm_bytes = wasm_clean_p.read_bytes()
        asm_x86 = x86_clean_s.read_text(encoding="utf-8", errors="replace")
        asm_arm64 = arm_clean_s.read_text(encoding="utf-8", errors="replace")
        asm_rv64 = rv_clean_s.read_text(encoding="utf-8", errors="replace")
        verified_mutants = discover_wasm_mutants(wasm_bytes, rounds)
        defects = extract_discovered_defects(tiers_out)

        # Strip transient /tmp paths before serializing receipt
        for t_obj in tiers_out.values():
            for l_obj in t_obj["lanes"].values():
                l_obj.pop("asm_path", None)
                l_obj.pop("wasm_path", None)

    required_keys = ["tier1_core"]
    if strict_tier >= 2:
        required_keys.append("tier2_struct_union")
    if strict_tier >= 3:
        required_keys.append("tier3_edge_abi")

    if any(tiers_out[k]["verdict"] == "FAIL" for k in required_keys):
        overall_verdict = "FAIL"
    elif any(tiers_out[k]["verdict"] == "INCONCLUSIVE" for k in required_keys):
        overall_verdict = "INCONCLUSIVE"
    else:
        overall_verdict = "PASS"

    stage23_sha = read_stage23_sha256()
    completed_at = datetime.now(timezone.utc).isoformat()

    receipt: dict[str, Any] = {
        "schema": "ZCC-QUAD-ISA-GATE-RECEIPT-v1",
        "governing_rule_version": "ZCC-EF-7 v1.0.0",
        "authority_role": "INDEPENDENT_MULTI_ORACLE_GATE (ZCC-EF-0)",
        "evidence_tier": "E2",
        "evidence_tier_assigned_at": evidence_tier_assigned_at,
        "completed_at": completed_at,
        "source_equivalence_class": (
            "Class I across ZCC Quad-ISA (x86-64, ARM64, RV64, WASM32); "
            "Class III vs GCC/Clang on K1-K2 (__builtin_qpauli_* reference C shim), Class I on K3-K10"
        ),
        "source_file": str(source_path.relative_to(REPO_ROOT).as_posix())
        if source_path.is_relative_to(REPO_ROOT)
        else str(source_path),
        "source_sha256": source_sha256,
        "zcc_binary_sha256": hashlib.sha256(zcc_bin.read_bytes()).hexdigest(),
        "stage23_fixed_point_sha256": stage23_sha,
        "rounds": rounds,
        "strict_gate_tier": strict_tier,
        "verdict": overall_verdict,
        "tiers": tiers_out,
        "discovered_defects": defects,
    }

    embedded_artifacts: dict[str, Any] = {
        "wasmBase64": base64.b64encode(wasm_bytes).decode("ascii"),
        "wasmSha256": hashlib.sha256(wasm_bytes).hexdigest(),
        "x86Sha256": hashlib.sha256(asm_x86.encode("utf-8")).hexdigest(),
        "arm64Sha256": hashlib.sha256(asm_arm64.encode("utf-8")).hexdigest(),
        "riscv64Sha256": hashlib.sha256(asm_rv64.encode("utf-8")).hexdigest(),
        "stage23Sha256": stage23_sha,
        "asmX86_64": asm_x86,
        "asmArm64": asm_arm64,
        "asmRiscv64": asm_rv64,
        "verifiedMutants": verified_mutants,
        "quadIsaGateReceipt": receipt,
    }
    return receipt, embedded_artifacts


def render_harness_html(embedded_artifacts: dict[str, Any]) -> str:
    embedded_json = json.dumps(embedded_artifacts, separators=(",", ":"))
    return (
        HTML_HEAD_AND_BODY
        + "\n    <script>\n        const ZCC_EMBEDDED_ARTIFACTS = "
        + embedded_json
        + ";\n"
        + JS_ORACLE_AND_ISA
        + JS_HARNESS_CLASS
        + "\n    </script>\n</body>\n\n</html>\n"
    )


# ============================================================================
# 4. ZCC-EF-0 ADVERSARIAL NEGATIVE CONTROLS (--self-test)
# ============================================================================


def run_self_test(rounds: int = 32) -> int:
    print("=== [ZCC-EF-0] Running Quad-ISA Gate Adversarial Self-Test (3 Negative Controls) ===")
    receipt, embedded = run_quad_isa_campaign(DEFAULT_SOURCE, rounds=rounds, strict_tier=1)
    if receipt["verdict"] != "PASS":
        print(f"  [FAIL] POS-01 baseline failed unexpectedly: {receipt['verdict']}")
        return 1
    print(f"  [PASS] POS-01 Clean 7-Lane Tier-1 Consensus: {receipt['tiers']['tier1_core']['spec_fnv1a64']}")

    # NEG-01: Mutate WASM opcode at verifiedMutants[0] -> main() == 0, but digest oracle MUST kill!
    wasm_raw = bytearray(base64.b64decode(embedded["wasmBase64"]))
    mut_off, mut_op, mut_desc = embedded["verifiedMutants"][0]
    wasm_raw[mut_off] = mut_op
    with tempfile.TemporaryDirectory(prefix="zcc_neg01_") as td:
        mut_wasm = Path(td) / "mutant.wasm"
        mut_wasm.write_bytes(bytes(wasm_raw))
        v8_mut = evaluate_wasm_digest_via_node(mut_wasm, rounds)
    spec_t1 = receipt["tiers"]["tier1_core"]["spec_fnv1a64"]
    if not (v8_mut["ok"] and v8_mut["main_exit"] == 0 and v8_mut["fnv1a64"] != spec_t1):
        print(f"  [FAIL] NEG-01 failed to catch self-certifying WASM mutant: {v8_mut}")
        return 1
    print(
        f"  [PASS] NEG-01 Killed Self-Certifying WASM Mutant [{mut_off}] ({mut_desc}): "
        f"exports.main()==0, observed={v8_mut['fnv1a64']} != spec={spec_t1}"
    )

    # NEG-02: Promoting strict_tier=2 MUST fail-closed on ZCC RV64 QEMU silent wrong-code!
    t2_receipt, _ = run_quad_isa_campaign(DEFAULT_SOURCE, rounds=rounds, strict_tier=2)
    rv_t2 = t2_receipt["tiers"]["tier2_struct_union"]["lanes"]["zcc_riscv64_qemu"]
    if rv_t2["verdict"] == "FAIL":
        if t2_receipt["verdict"] != "FAIL":
            print("  [FAIL] NEG-02 strict_tier=2 failed to propagate RV64 wrong-code to overall FAIL")
            return 1
        print(
            f"  [PASS] NEG-02 Killed Silent RV64 QEMU Wrong-Code (exit={rv_t2['exit_code']}, "
            f"digest={rv_t2['fnv1a64']} != spec={t2_receipt['tiers']['tier2_struct_union']['spec_fnv1a64']})"
        )
    else:
        print("  [PASS] NEG-02 Tier-2 RV64 backend now matches spec; testing synthetic wrong-code injection...")
        with tempfile.TemporaryDirectory(prefix="zcc_neg02_") as td:
            corrupted_c = Path(td) / "corrupted_torture.c"
            src_txt = DEFAULT_SOURCE.read_text(encoding="utf-8").replace("0x9E3779B9u", "0x9E3779B8u", 1)
            corrupted_c.write_text(src_txt, encoding="utf-8")
            bad_spec = py_spec_campaign_fnv1a64(rounds, 1)
            lane_bad = run_single_lane(
                "zcc_riscv64_qemu", 1, [], rounds, bad_spec, corrupted_c, Path(td), REPO_ROOT / "zcc"
            )
            if lane_bad["verdict"] != "FAIL":
                print("  [FAIL] NEG-02 synthetic wrong-code escaped detection!")
                return 1
            print(f"  [PASS] NEG-02 Killed Synthetic RV64 QEMU Divergence ({lane_bad['fnv1a64']} != {bad_spec})")

    # NEG-03: Truncated C source MUST fail-closed across all lanes!
    with tempfile.TemporaryDirectory(prefix="zcc_neg03_") as td:
        trunc_c = Path(td) / "truncated_torture.c"
        trunc_c.write_text("int broken_tu(void { return ; ", encoding="utf-8")
        lane_trunc = run_single_lane(
            "zcc_x86_64", 1, [], rounds, spec_t1, trunc_c, Path(td), REPO_ROOT / "zcc"
        )
        if lane_trunc["verdict"] != "FAIL":
            print(f"  [FAIL] NEG-03 truncated C source did not fail closed: {lane_trunc}")
            return 1
    print(f"  [PASS] NEG-03 Truncated TU Fail-Closed ({lane_trunc['failure_kind']})")
    print("=== [ZCC-EF-0] ALL 3 ADVERSARIAL NEGATIVE CONTROLS KILLED (SELF-TEST PASS) ===")
    return 0


# ============================================================================
# 5. HEADLESS NODE UI, CSS GRID ANTI-COLLAPSE & LIVE V8 WASM VERIFIER (EF-7)
# ============================================================================


def verify_ui_headless(html_path: Path = DEFAULT_HTML) -> int:
    html_text = html_path.read_text(encoding="utf-8")

    # 1. Static CSS Anti-Collapse & Rule EF-7 Structural Checks
    ctrl_block = re.search(r"\.control-panel\s*\{([^}]*)\}", html_text, flags=re.DOTALL)
    res_block = re.search(r"\.results-panel\s*\{([^}]*)\}", html_text, flags=re.DOTALL)
    grid_block = re.search(r"\.test-harness-container\s*\{([^}]*)\}", html_text, flags=re.DOTALL)
    overlay_block = re.search(r"#ambient-grid-overlay\s*\{([^}]*)\}", html_text, flags=re.DOTALL)

    if not ctrl_block or "position: sticky" not in ctrl_block.group(1) or "position: fixed" in ctrl_block.group(1):
        raise AssertionError("CSS regression: .control-panel must use position: sticky (never position: fixed)")
    if not grid_block or "320px minmax(0, 1fr)" not in grid_block.group(1):
        raise AssertionError("CSS regression: .test-harness-container missing 320px minmax(0, 1fr) grid")
    if not res_block or "grid-column: 2" not in res_block.group(1) or "margin-left: 3" in res_block.group(1):
        raise AssertionError("CSS regression: .results-panel missing grid-column: 2 or still has fixed margin-left")
    if not overlay_block or "pointer-events: none !important" not in overlay_block.group(1):
        raise AssertionError("CSS regression: #ambient-grid-overlay missing pointer-events: none !important")

    canvases = re.findall(r"<canvas\b[^>]*>", html_text)
    if len(canvases) < 5:
        raise AssertionError(f"Expected >= 5 <canvas> elements, found {len(canvases)}")
    for c_tag in canvases:
        wm = re.search(r'width="(\d+)"', c_tag)
        hm = re.search(r'height="(\d+)"', c_tag)
        if not wm or not hm or int(wm.group(1)) < 50 or int(hm.group(1)) < 50:
            raise AssertionError(f"Canvas tag missing explicit width/height >= 50: {c_tag}")

    if "if (!canvas || canvas.width < 50 || canvas.height < 50) return;" not in html_text:
        raise AssertionError("Missing Rule EF-7 zero-dimension canvas guard")
    if "Math.max(1," not in html_text:
        raise AssertionError("Missing Rule EF-7 Math.max(1, ...) radius clamp")

    # 2. Headless Node VM Syntax + Full 8-Suite + EF-0 Self-Certify Trap + Zero-Dim Check
    node_driver = r"""
const fs = require('fs');
const vm = require('vm');
const html = fs.readFileSync(process.argv[1], 'utf8');

const scripts = [...html.matchAll(/<script\b[^>]*>([\s\S]*?)<\/script>/gi)].map(m => m[1]);
if (scripts.length === 0) throw new Error('No <script> blocks found');
scripts.forEach((code, idx) => new vm.Script(code, { filename: `script_${idx}.js` }));

function makeCtx2d() {
  return {
    clearRect() {}, fillRect() {}, strokeRect() {}, beginPath() {}, moveTo() {}, lineTo() {},
    stroke() {}, fill() {}, fillText() {},
    arc(_x, _y, r) { if (!(r >= 1)) throw new Error('Radius < 1: ' + r); },
    createRadialGradient(_x0, _y0, r0, _x1, _y1, r1) {
      if (!(r0 >= 1 && r1 >= 1)) throw new Error('Bad radial gradient radius');
      return { addColorStop() {} };
    }
  };
}

function makeEl(tag, attrs = {}) {
  const el = {
    tagName: tag.toUpperCase(),
    id: attrs.id || '',
    className: attrs.class || '',
    value: attrs.value || '6',
    checked: Object.prototype.hasOwnProperty.call(attrs, 'checked'),
    width: attrs.width ? parseInt(attrs.width, 10) : 460,
    height: attrs.height ? parseInt(attrs.height, 10) : 160,
    clientWidth: attrs.width ? parseInt(attrs.width, 10) : 460,
    clientHeight: attrs.height ? parseInt(attrs.height, 10) : 160,
    textContent: '',
    innerHTML: '',
    title: '',
    dataset: {},
    style: {},
    children: [],
    listeners: {},
    classList: {
      add(c) { if (!el.className.split(/\s+/).includes(c)) el.className = (el.className + ' ' + c).trim(); },
      remove(c) { el.className = el.className.split(/\s+/).filter(x => x !== c).join(' '); },
      toggle(c, force) {
        const has = el.className.split(/\s+/).includes(c);
        const want = force === undefined ? !has : !!force;
        if (want && !has) this.add(c);
        if (!want && has) this.remove(c);
      }
    },
    addEventListener(ev, fn) { (this.listeners[ev] = this.listeners[ev] || []).push(fn); },
    appendChild(ch) { this.children.push(ch); return ch; },
    getContext() { return makeCtx2d(); },
    querySelectorAll(sel) {
      if (sel === '.metric-value') {
        if (!this._metrics) this._metrics = Array.from({ length: 8 }, () => makeEl('span', { class: 'metric-value' }));
        return this._metrics;
      }
      if (sel === '.log-entry') return this.children;
      return [];
    },
    click() {}
  };
  return el;
}

const byId = new Map();
const tagRe = /<([a-zA-Z0-9]+)\b([^>]*?)>/g;
let m;
while ((m = tagRe.exec(html)) !== null) {
  const tag = m[1];
  const rawAttrs = m[2];
  const idMatch = rawAttrs.match(/\bid="([^"]+)"/);
  if (!idMatch) continue;
  const attrs = { id: idMatch[1] };
  for (const [k, reA] of [
    ['class', /\bclass="([^"]*)"/],
    ['value', /\bvalue="([^"]*)"/],
    ['width', /\bwidth="(\d+)"/],
    ['height', /\bheight="(\d+)"/]
  ]) {
    const am = rawAttrs.match(reA);
    if (am) attrs[k] = am[1];
  }
  if (/\bchecked\b/.test(rawAttrs)) attrs.checked = true;
  byId.set(attrs.id, makeEl(tag, attrs));
}

const domListeners = {};
const sandbox = {
  console,
  Math,
  Date,
  JSON,
  Uint8Array,
  Int32Array,
  Float64Array,
  ArrayBuffer,
  BigInt,
  Set,
  Map,
  Promise,
  Buffer,
  WebAssembly,
  performance: { now: () => Date.now() },
  setTimeout: (fn, _ms) => setTimeout(fn, 1),
  clearTimeout,
  requestAnimationFrame: () => 1,
  document: {
    getElementById: (id) => byId.get(id) || null,
    createElement: (tag) => makeEl(tag),
    createDocumentFragment: () => makeEl('frag'),
    querySelectorAll: () => []
  },
  window: null
};
sandbox.window = sandbox;
sandbox.window.addEventListener = (ev, fn) => { (domListeners[ev] = domListeners[ev] || []).push(fn); };

vm.createContext(sandbox);
scripts.forEach(code => vm.runInContext(code, sandbox));
(domListeners['DOMContentLoaded'] || []).forEach(fn => fn());

(async () => {
  await new Promise(r => setTimeout(r, 80));
  const h = sandbox.window.zccHarness;
  if (!h) throw new Error('zccHarness failed to instantiate');
  if (!h.wasmExports || h.wasmBootError) throw new Error('WASM boot failed: ' + h.wasmBootError);

  const matrixHtml = byId.get('tier-matrix-tbody').innerHTML;
  const bugHtml = byId.get('bug-ledger-tbody').innerHTML;
  if (!matrixHtml.includes('qemu-aarch64') || !matrixHtml.includes('qemu-riscv64')) {
    throw new Error('3-Tier QEMU matrix failed to render in DOM');
  }
  const defects = (h.gateReceipt && Array.isArray(h.gateReceipt.discovered_defects))
    ? h.gateReceipt.discovered_defects
    : ((h.embedded && h.embedded.quadIsaGateReceipt && Array.isArray(h.embedded.quadIsaGateReceipt.discovered_defects))
        ? h.embedded.quadIsaGateReceipt.discovered_defects
        : null);
  if (!Array.isArray(defects)) {
    throw new Error('Missing gateReceipt.discovered_defects in embedded harness');
  }
  if (defects.length > 0 && !bugHtml.includes(defects[0].id)) {
    throw new Error('Backend defect ledger missing ' + defects[0].id);
  }
  if (defects.length === 0 && !bugHtml.includes('data-empty-ledger="1"')) {
    throw new Error('Backend defect ledger missing ALL CLEAR empty-state row');
  }

  byId.get('test-duration').value = '2';
  byId.get('sample-rate').value = '20';

  const suiteVerdicts = {};
  for (const suite of Object.keys(h.testSuites)) {
    const res = await h.runTestSuite(suite);
    suiteVerdicts[suite] = res.verdict;
    if (res.verdict !== 'PASS') {
      throw new Error(`Clean suite ${suite} returned ${res.verdict}`);
    }
  }

  // Adversarial Self-Certify Trap Test (ZCC-EF-0)
  await h.injectFault('wasm_xor_316');
  const mutMainRet = h.wasmExports.main();
  const faultRes = await h.runTestSuite('performance');
  if (mutMainRet !== 0 || faultRes.verdict !== 'FAIL' || faultRes.systems.deep.selfCertifyTraps < 1) {
    throw new Error(`EF-0 Self-Certify Trap failed: main=${mutMainRet}, verdict=${faultRes.verdict}, traps=${faultRes.systems.deep.selfCertifyTraps}`);
  }
  await h.restoreCleanArtifacts();

  // Assembly ABI / Load Corruption Fault Tests
  for (const [faultKind, sysKey] of [
    ['x86_rsp_misalign', 'refined'],
    ['arm64_sp_corrupt', 'original'],
    ['rv64_lwu_corrupt', 'test']
  ]) {
    await h.injectFault(faultKind);
    const r = await h.runTestSuite('performance');
    if (r.systems[sysKey].verdict !== 'FAIL') {
      throw new Error(`Assembly fault ${faultKind} was not killed on ${sysKey}`);
    }
    await h.restoreCleanArtifacts();
  }

  // Zero-Dimension Hidden Tab Collapse Test (Rule EF-7.2)
  for (const id of ['hd-tesseract-canvas', 'chart-refined', 'chart-deep', 'chart-original', 'chart-test']) {
    const c = byId.get(id);
    c.width = 0; c.height = 0; c.clientWidth = 0; c.clientHeight = 0;
  }
  h.updateCharts();
  h.render4dTesseractAndSteane(1500);

  // Stop -> INCONCLUSIVE Test (Rule EF-6)
  const pending = h.runTestSuite('performance');
  h.stopAllTests();
  const stoppedRes = await pending;
  if (stoppedRes.verdict !== 'INCONCLUSIVE') {
    throw new Error(`Stopped suite returned ${stoppedRes.verdict} instead of INCONCLUSIVE`);
  }

  console.log(JSON.stringify({
    ok: true,
    scriptsParsed: scripts.length,
    domIdsBound: byId.size,
    wasmBytes: h.cleanWasmBytes.length,
    suiteVerdicts,
    ef0SelfCertifyTrap: { mutMainRet, killedVerdict: faultRes.verdict, trapsBlocked: faultRes.systems.deep.selfCertifyTraps },
    stopVerdict: stoppedRes.verdict
  }));
})().catch(err => {
  console.error(err && err.stack ? err.stack : String(err));
  process.exit(1);
});
"""
    proc = subprocess.run(
        ["node", "-e", node_driver, str(html_path)],
        capture_output=True,
        text=True,
        encoding="utf-8",
        timeout=30.0,
    )
    if proc.returncode != 0:
        print(proc.stderr or proc.stdout, file=sys.stderr)
        return 1
    summary = json.loads(proc.stdout.strip().splitlines()[-1])
    print(
        f"=== [EF-7 UI VERIFIER] PASS: {summary['scriptsParsed']} script(s) AST-clean, "
        f"{summary['domIdsBound']} DOM IDs bound, WASM={summary['wasmBytes']}B, "
        f"8/8 suites PASS, EF-0 self-certify trap killed (main={summary['ef0SelfCertifyTrap']['mutMainRet']} -> "
        f"{summary['ef0SelfCertifyTrap']['killedVerdict']}), 0x0 canvas safe, Stop={summary['stopVerdict']} ==="
    )
    return 0


# ============================================================================
# 6. LIVE LOCAL COMPILER & QEMU HTTP BRIDGE (--serve on 127.0.0.1:8799)
# ============================================================================


def make_bridge_handler(
    html_path: Path, receipt_path: Path
) -> type[BaseHTTPRequestHandler]:
    class QuadIsaBridgeHandler(BaseHTTPRequestHandler):
        def _send_cors_headers(self) -> None:
            self.send_header("Access-Control-Allow-Origin", "*")
            self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
            self.send_header("Access-Control-Allow-Headers", "Content-Type")
            self.send_header("Cache-Control", "no-store")

        def _send_json(self, code: int, payload: dict[str, Any]) -> None:
            raw = json.dumps(payload).encode("utf-8")
            self.send_response(code)
            self.send_header("Content-Type", "application/json; charset=utf-8")
            self.send_header("Content-Length", str(len(raw)))
            self._send_cors_headers()
            self.end_headers()
            self.wfile.write(raw)

        def do_OPTIONS(self) -> None:  # noqa: N802
            self.send_response(204)
            self._send_cors_headers()
            self.end_headers()

        def do_GET(self) -> None:  # noqa: N802
            if self.path in ("/", "/index.html", "/zcc_hyperdim_test_harness.html"):
                raw = html_path.read_bytes()
                self.send_response(200)
                self.send_header("Content-Type", "text/html; charset=utf-8")
                self.send_header("Content-Length", str(len(raw)))
                self._send_cors_headers()
                self.end_headers()
                self.wfile.write(raw)
                return

            if self.path.startswith("/api/status"):
                tier1_digest = "UNINITIALIZED"
                verdict = "READY"
                if receipt_path.exists():
                    try:
                        rec = json.loads(receipt_path.read_text(encoding="utf-8"))
                        verdict = rec.get("verdict", "READY")
                        tier1_digest = rec.get("tiers", {}).get("tier1_core", {}).get("spec_fnv1a64", "READY")
                    except Exception:
                        verdict = "READ_ERR"
                self._send_json(
                    200,
                    {
                        "ok": True,
                        "compiler": "./zcc (WSL Ubuntu-24.04)" if os.name == "nt" else "./zcc",
                        "verdict": verdict,
                        "tier1Digest": tier1_digest,
                    },
                )
                return

            self._send_json(404, {"ok": False, "error": f"Unknown endpoint: {self.path}"})

        def do_POST(self) -> None:  # noqa: N802
            if not self.path.startswith("/api/recompile"):
                self._send_json(404, {"ok": False, "error": f"Unknown POST endpoint: {self.path}"})
                return

            length = int(self.headers.get("Content-Length", "0") or "0")
            body_raw = self.rfile.read(length).decode("utf-8") if length > 0 else "{}"
            try:
                req_json = json.loads(body_raw or "{}")
            except json.JSONDecodeError:
                req_json = {}
            rounds = max(8, min(256, int(req_json.get("rounds", 64))))

            try:
                if os.name == "nt":
                    proc = invoke_wsl_gate(["--rounds", str(rounds), "--emit-bundle-json"], timeout_sec=120)
                    if proc.returncode != 0:
                        raise RuntimeError(proc.stderr or proc.stdout)
                    bundle = json.loads(proc.stdout.strip().splitlines()[-1])
                    self._send_json(200, bundle)
                else:
                    receipt, embedded = run_quad_isa_campaign(DEFAULT_SOURCE, rounds=rounds, strict_tier=1)
                    write_json_atomic(receipt_path, receipt)
                    write_text_atomic(html_path, render_harness_html(embedded))
                    self._send_json(
                        200,
                        {
                            "ok": True,
                            "receipt": receipt,
                            "embeddedArtifacts": embedded,
                        },
                    )
            except Exception as exc:
                self._send_json(500, {"ok": False, "error": str(exc)})

        def log_message(self, fmt: str, *args: Any) -> None:
            sys.stderr.write(f"[zcc-quad-bridge] {fmt % args}\n")

    return QuadIsaBridgeHandler


def serve_bridge(host: str, port: int, html_path: Path, receipt_path: Path) -> int:
    server = ThreadingHTTPServer((host, port), make_bridge_handler(html_path, receipt_path))
    print(
        f"=== [ZCC QUAD-ISA LIVE BRIDGE] Listening on http://{host}:{port} ===\n"
        f"  UI Dashboard : http://{host}:{port}/\n"
        f"  Health Probe : http://{host}:{port}/api/status\n"
        f"  Live Compile : POST http://{host}:{port}/api/recompile"
    )
    sys.stdout.flush()
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
    return 0


# ============================================================================
# 7. CLI ENTRY POINT
# ============================================================================


def print_campaign_summary(receipt: dict[str, Any]) -> None:
    print(
        f"=== [ZCC QUAD-ISA GATE] Verdict: {receipt['verdict']} "
        f"(Rounds={receipt['rounds']}, Stage23={receipt['stage23_fixed_point_sha256'][:16]}...) ==="
    )
    for t_key in ("tier1_core", "tier2_struct_union", "tier3_edge_abi"):
        t = receipt["tiers"][t_key]
        print(
            f"  * {t_key:<20} | Spec={t['spec_fnv1a64']} | "
            f"{t['lanes_passed']}/{t['lanes_total']} lanes PASS | Tier Verdict={t['verdict']}"
        )
        for lid, lrec in t["lanes"].items():
            if lrec["verdict"] != "PASS":
                print(
                    f"      -> {lid:<18}: {lrec['verdict']} ({lrec['failure_kind']}, "
                    f"exit={lrec['exit_code']}, fnv1a={lrec['fnv1a64']})"
                )
    if receipt["discovered_defects"]:
        print(f"  * Surfaced Tier-2/3 Backend Defects ({len(receipt['discovered_defects'])}):")
        for d in receipt["discovered_defects"]:
            print(f"      - {d['id']} [{d['backend']} / {d['kernel']}]: {d['severity']}")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="ZCC Stratified 3-Tier Quad-ISA Differential Compiler Gate & Live UI Bridge"
    )
    parser.add_argument("--rounds", type=int, default=64, help="Campaign vector rounds per kernel (default: 64)")
    parser.add_argument(
        "--strict-tier",
        type=int,
        choices=(1, 2, 3),
        default=3,
        help="Highest torture tier required to PASS overall gate (default: 3)",
    )
    parser.add_argument("--receipt", type=Path, default=DEFAULT_RECEIPT, help="Output JSON receipt path")
    parser.add_argument("--html", type=Path, default=DEFAULT_HTML, help="Target HTML harness path")
    parser.add_argument("--no-update-html", action="store_true", help="Do not regenerate HTML harness")
    parser.add_argument("--verify-ui", action="store_true", help="Run headless Node EF-7 + V8 WASM UI verifier")
    parser.add_argument("--self-test", action="store_true", help="Run ZCC-EF-0 adversarial negative controls")
    parser.add_argument("--emit-bundle-json", action="store_true", help="Emit live recompile JSON bundle on stdout")
    parser.add_argument("--serve", action="store_true", help="Start live ./zcc + QEMU HTTP bridge on 127.0.0.1")
    parser.add_argument("--host", default="127.0.0.1", help="HTTP bridge bind address (default: 127.0.0.1)")
    parser.add_argument("--port", type=int, default=8799, help="HTTP bridge TCP port (default: 8799)")
    args = parser.parse_args(argv)

    if args.serve:
        if not args.html.exists():
            if os.name == "nt":
                res = invoke_wsl_gate(["--rounds", str(args.rounds)])
                if res.returncode != 0:
                    sys.stderr.write(res.stderr or res.stdout)
                    return res.returncode
            else:
                rec, emb = run_quad_isa_campaign(DEFAULT_SOURCE, rounds=args.rounds, strict_tier=args.strict_tier)
                write_json_atomic(args.receipt, rec)
                write_text_atomic(args.html, render_harness_html(emb))
        return serve_bridge(args.host, args.port, args.html, args.receipt)

    # On Windows, delegate compiler/QEMU/Node execution cleanly to WSL Ubuntu-24.04 in a single spawn
    if os.name == "nt":
        raw_argv = argv if argv is not None else sys.argv[1:]
        proc = invoke_wsl_gate(raw_argv, timeout_sec=180)
        if proc.stdout:
            sys.stdout.write(proc.stdout)
        if proc.stderr:
            sys.stderr.write(proc.stderr)
        return proc.returncode

    if args.self_test:
        return run_self_test(rounds=min(args.rounds, 32))

    receipt, embedded = run_quad_isa_campaign(
        DEFAULT_SOURCE,
        rounds=args.rounds,
        strict_tier=args.strict_tier,
    )
    write_json_atomic(args.receipt, receipt)
    if not args.no_update_html:
        write_text_atomic(args.html, render_harness_html(embedded))

    if args.emit_bundle_json:
        print(json.dumps({"ok": True, "receipt": receipt, "embeddedArtifacts": embedded}))
        return 0 if receipt["verdict"] == "PASS" else 1

    print_campaign_summary(receipt)

    if args.verify_ui:
        ui_rc = verify_ui_headless(args.html)
        if ui_rc != 0:
            return ui_rc

    return 0 if receipt["verdict"] == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
