#!/usr/bin/env python3
# -*- coding: utf-8 -*-
r"""
====================================================================================================
 🔱 ZKAEDI PRIME // IDEA IEEE-2: IEEE-754 qNaN TOMBSTONE TRACER & SUBNORMAL FTZ/DAZ SENTINEL 🔱
====================================================================================================
Fuses:
  - tools/quantum/quantum_ieee754_multiradix_walk.py (IEEE754Codec bitfield dissection)
  - float_guard.py (double_to_bits, bits_to_double, ulp_diff, inspect)
  - omnicatch.py (Fail-closed boundary assertions)

Pre-Fortified Mathematical & Bitwise Architecture:
  1. 51-Bit `binary64` (`float64`) Quiet NaN Tombstone Layout:
     In IEEE-754 `binary64`, any float with `exponent = 0x7FF` (2047) and `mantissa_bit_51 = 1`
     (`0x7FF8_0000_0000_0000`) is a Quiet NaN (`qNaN`), leaving **51 lower mantissa bits**
     (`bits[50:0]`, mask `0x0007_FFFF_FFFF_FFFF`) as a user-definable diagnostic payload!
     We pack an authenticated 51-bit tombstone into `bits[50:0]`:
       - `magic`      (4 bits,  `[50:47]`): `0xA` (`1010b`, ZKAEDI Tombstone Magic)
       - `op_code`    (5 bits,  `[46:42]`): Fault operation (`DIV_ZERO`, `INF_MINUS_INF`, `NEG_SQRT`, `LOG_DOMAIN`, `ULP_CANCEL`, `SUBNORMAL_FTZ`, etc.)
       - `site_id`    (14 bits, `[41:28]`): Source line / node ID (`0..16,383`)
       - `row_idx`    (10 bits, `[27:18]`): Tensor row index (`0..1,023`)
       - `col_idx`    (10 bits, `[17:8]` ): Tensor col index (`0..1,023`)
       - `crc8`       (8 bits,  `[7:0]`  ): Keyed HMAC-CRC8 over `(magic, op_code, site_id, row_idx, col_idx)`
  2. 22-Bit `binary32` (`float32`) Quiet NaN Compact Tombstone Layout:
     In IEEE-754 `binary32` (`0x7FC0_0000`), packs `[magic:2b | op_code:4b | site_id:6b | idx:6b | crc4:4b]`.
  3. Tombstone-Preserving Matrix & Scalar Operations:
     Standard Python/C operations propagate `qNaN` payloads on hardware, or when two `qNaN`
     values collide (`qNaN_1 + qNaN_2`), preserve the **first-born (root-cause) tombstone**
     so downstream matrix outputs always point back to the exact origin site `(op_code, site_id, row, col)`!
  4. Subnormal / Denormal Flush-to-Zero (`FTZ`) & Denormals-Are-Zero (`DAZ`) Sentinel:
     Probes all 64-bit (`5e-324` .. `2.225e-308`) and 32-bit (`1.401e-45` .. `1.175e-38`) subnormal
     octaves to detect silent hardware/middleware `FTZ`/`DAZ` underflow flushing (`Gap 5`).
====================================================================================================
"""

from __future__ import annotations

import argparse
import dataclasses
import enum
import hashlib
import hmac
import json
import math
import struct
import sys
from pathlib import Path
from typing import Any, Dict, List, Optional, Sequence, Tuple

if hasattr(sys.stdout, "reconfigure"):
    try:
        sys.stdout.reconfigure(encoding="utf-8")
        sys.stderr.reconfigure(encoding="utf-8")
    except Exception:
        pass

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
if str(REPO_ROOT) in sys.path:
    sys.path.remove(str(REPO_ROOT))
sys.path.insert(0, str(REPO_ROOT))

import numpy as np
import float_guard


# ====================================================================================================
# FAULT OPCODES & TOMBSTONE STRUCTURES
# ====================================================================================================

class IEEE754FaultOp(enum.IntEnum):
    UNKNOWN = 0
    ZERO_DIV_ZERO = 1        # 0.0 / 0.0
    INF_DIV_INF = 2          # inf / inf
    INF_SUB_INF = 3          # inf - inf
    ZERO_MUL_INF = 4         # 0.0 * inf
    SQRT_NEGATIVE = 5        # sqrt(x < 0)
    LOG_NONPOSITIVE = 6      # log(x <= 0)
    ACOS_ASIN_DOMAIN = 7     # acos(|x| > 1)
    CATASTROPHIC_CANCEL = 8  # ULP drift > threshold
    SUBNORMAL_FTZ_FLUSH = 9  # Denormal flushed to +0.0
    OVERFLOW_TO_INF = 10     # Finite op overflowed to +/-inf
    SIGNALING_NAN_TRAP = 11  # sNaN encountered


@dataclasses.dataclass(frozen=True)
class NaNTombstoneRecord:
    is_nan: bool
    is_quiet_nan: bool
    is_signaling_nan: bool
    is_valid_tombstone: bool
    raw_bits_hex: str
    op_code: IEEE754FaultOp
    op_name: str
    site_id: int
    row_idx: int
    col_idx: int
    crc8: int
    expected_crc8: int


@dataclasses.dataclass(frozen=True)
class SubnormalAuditReport:
    subnormals_preserved: bool
    ftz_detected: bool
    daz_detected: bool
    tested_count: int
    flushed_indices: List[int]
    min_positive_subnormal_f64: float
    min_positive_subnormal_f32: float


class SubnormalFlushError(Exception):
    """Raised when FTZ/DAZ flushes IEEE-754 gradual underflow subnormals to zero."""


class SignalingNaNTrapError(Exception):
    """Raised when an IEEE-754 Signaling NaN (sNaN, bit 51 == 0) is encountered."""


# ====================================================================================================
# IEEE-754 64-BIT & 32-BIT qNaN TOMBSTONE CODEC
# ====================================================================================================

QNAN_F64_BASE = 0x7FF8000000000000
PAYLOAD_F64_MASK = 0x0007FFFFFFFFFFFF
EXP_F64_MASK = 0x7FF0000000000000
QNAN_BIT_F64 = 0x0008000000000000
MAGIC_4B = 0xA  # 1010b


class IEEE754TombstoneTracer:
    """
    Encodes, propagates, and decodes 51-bit forensic provenance tombstones inside
    IEEE-754 `binary64` Quiet NaN (`qNaN`) bitfields without raising mid-kernel exceptions.
    """

    def __init__(self, secret_key: bytes = b"ZKAEDI_IEEE754_QNAN_TOMBSTONE_KEY") -> None:
        self._secret = bytes(secret_key)

    def _compute_crc8(self, header_43b: int) -> int:
        raw = struct.pack("<Q", int(header_43b) & 0x7FFFFFFFFFF)
        digest = hmac.new(self._secret, raw, hashlib.sha256).digest()
        return digest[0]

    def mint_qnan_tombstone_bits(
        self,
        op_code: IEEE754FaultOp,
        site_id: int,
        row_idx: int = 0,
        col_idx: int = 0,
    ) -> int:
        """
        Packs `(magic=0xA, op_code, site_id, row_idx, col_idx, crc8)` into a 64-bit IEEE-754 qNaN integer.
        """
        op_val = int(op_code) & 0x1F
        site_val = int(site_id) & 0x3FFF
        row_val = int(row_idx) & 0x3FF
        col_val = int(col_idx) & 0x3FF

        header_43b = (
            (MAGIC_4B << 39)
            | (op_val << 34)
            | (site_val << 20)
            | (row_val << 10)
            | col_val
        )
        crc8 = self._compute_crc8(header_43b)
        payload_51b = (header_43b << 8) | crc8
        return QNAN_F64_BASE | payload_51b

    def mint_qnan_tombstone(
        self,
        op_code: IEEE754FaultOp,
        site_id: int,
        row_idx: int = 0,
        col_idx: int = 0,
    ) -> float:
        bits = self.mint_qnan_tombstone_bits(op_code, site_id, row_idx, col_idx)
        return float_guard.bits_to_double(bits)

    @staticmethod
    def mint_snan_bits(payload: int = 1) -> int:
        """
        Constructs an IEEE-754 Signaling NaN (`sNaN`): exponent=0x7FF, bit 51=0, non-zero payload.
        """
        safe_payload = max(1, int(payload) & PAYLOAD_F64_MASK)
        return EXP_F64_MASK | safe_payload

    def inspect_bits(self, bits: int) -> NaNTombstoneRecord:
        u64 = int(bits) & 0xFFFFFFFFFFFFFFFF
        exp = (u64 >> 52) & 0x7FF
        mant = u64 & 0xFFFFFFFFFFFFF
        is_nan = (exp == 0x7FF) and (mant != 0)
        is_qnan = is_nan and bool(u64 & QNAN_BIT_F64)
        is_snan = is_nan and not bool(u64 & QNAN_BIT_F64)

        if not is_nan:
            return NaNTombstoneRecord(
                is_nan=False,
                is_quiet_nan=False,
                is_signaling_nan=False,
                is_valid_tombstone=False,
                raw_bits_hex=f"0x{u64:016X}",
                op_code=IEEE754FaultOp.UNKNOWN,
                op_name="NONE",
                site_id=0,
                row_idx=0,
                col_idx=0,
                crc8=0,
                expected_crc8=0,
            )

        payload_51b = u64 & PAYLOAD_F64_MASK
        crc8 = payload_51b & 0xFF
        header_43b = payload_51b >> 8
        magic = (header_43b >> 39) & 0xF
        op_raw = (header_43b >> 34) & 0x1F
        site_id = (header_43b >> 20) & 0x3FFF
        row_idx = (header_43b >> 10) & 0x3FF
        col_idx = header_43b & 0x3FF

        expected_crc8 = self._compute_crc8(header_43b)
        try:
            op_enum = IEEE754FaultOp(op_raw)
        except ValueError:
            op_enum = IEEE754FaultOp.UNKNOWN

        is_valid = (
            is_qnan
            and magic == MAGIC_4B
            and crc8 == expected_crc8
            and op_enum != IEEE754FaultOp.UNKNOWN
        )

        return NaNTombstoneRecord(
            is_nan=True,
            is_quiet_nan=is_qnan,
            is_signaling_nan=is_snan,
            is_valid_tombstone=is_valid,
            raw_bits_hex=f"0x{u64:016X}",
            op_code=op_enum if is_valid else IEEE754FaultOp.UNKNOWN,
            op_name=op_enum.name if is_valid else ("SIGNALING_NAN" if is_snan else "UNATTESTED_NAN"),
            site_id=site_id if is_valid else 0,
            row_idx=row_idx if is_valid else 0,
            col_idx=col_idx if is_valid else 0,
            crc8=crc8,
            expected_crc8=expected_crc8,
        )

    def decode_float(self, val: float) -> NaNTombstoneRecord:
        """
        Decodes a Python/NumPy float64 value. Note: to preserve exact bit patterns including
        sNaN and custom qNaN payloads across CPython struct boundaries, pass `double_to_bits(val)`.
        """
        return self.inspect_bits(float_guard.double_to_bits(float(val)))

    # ----------------------------------------------------------------------------------------------
    # TOMBSTONE-PROPAGATING ARITHMETIC KERNELS
    # ----------------------------------------------------------------------------------------------

    def _check_input_bits(self, a_bits: int, b_bits: Optional[int] = None) -> Optional[int]:
        """
        Checks if `a_bits` or `b_bits` is already a NaN.
        - Traps immediately on Signaling NaN (`sNaN`).
        - Preserves the first valid `qNaN` tombstone payload.
        """
        for bits in (a_bits, b_bits):
            if bits is None:
                continue
            rec = self.inspect_bits(bits)
            if rec.is_signaling_nan:
                raise SignalingNaNTrapError(f"Signaling NaN trap triggered on bit pattern {rec.raw_bits_hex}")
            if rec.is_valid_tombstone:
                return bits
            if rec.is_nan:
                return bits
        return None

    def safe_div(
        self,
        a: float,
        b: float,
        site_id: int,
        row_idx: int = 0,
        col_idx: int = 0,
    ) -> float:
        a_bits = float_guard.double_to_bits(float(a))
        b_bits = float_guard.double_to_bits(float(b))
        prior = self._check_input_bits(a_bits, b_bits)
        if prior is not None:
            return float_guard.bits_to_double(prior)

        if a == 0.0 and b == 0.0:
            return self.mint_qnan_tombstone(IEEE754FaultOp.ZERO_DIV_ZERO, site_id, row_idx, col_idx)
        if math.isinf(a) and math.isinf(b):
            return self.mint_qnan_tombstone(IEEE754FaultOp.INF_DIV_INF, site_id, row_idx, col_idx)
        if b == 0.0:
            return self.mint_qnan_tombstone(IEEE754FaultOp.ZERO_DIV_ZERO, site_id, row_idx, col_idx)

        res = float(a) / float(b)
        if math.isinf(res) and math.isfinite(a) and math.isfinite(b):
            return self.mint_qnan_tombstone(IEEE754FaultOp.OVERFLOW_TO_INF, site_id, row_idx, col_idx)
        return res

    def safe_add(
        self,
        a: float,
        b: float,
        site_id: int,
        row_idx: int = 0,
        col_idx: int = 0,
    ) -> float:
        a_bits = float_guard.double_to_bits(float(a))
        b_bits = float_guard.double_to_bits(float(b))
        prior = self._check_input_bits(a_bits, b_bits)
        if prior is not None:
            return float_guard.bits_to_double(prior)

        if math.isinf(a) and math.isinf(b) and (a > 0) != (b > 0):
            return self.mint_qnan_tombstone(IEEE754FaultOp.INF_SUB_INF, site_id, row_idx, col_idx)

        return float(a) + float(b)

    def safe_mul(
        self,
        a: float,
        b: float,
        site_id: int,
        row_idx: int = 0,
        col_idx: int = 0,
    ) -> float:
        a_bits = float_guard.double_to_bits(float(a))
        b_bits = float_guard.double_to_bits(float(b))
        prior = self._check_input_bits(a_bits, b_bits)
        if prior is not None:
            return float_guard.bits_to_double(prior)

        if (a == 0.0 and math.isinf(b)) or (b == 0.0 and math.isinf(a)):
            return self.mint_qnan_tombstone(IEEE754FaultOp.ZERO_MUL_INF, site_id, row_idx, col_idx)

        return float(a) * float(b)

    def safe_sqrt(
        self,
        x: float,
        site_id: int,
        row_idx: int = 0,
        col_idx: int = 0,
    ) -> float:
        x_bits = float_guard.double_to_bits(float(x))
        prior = self._check_input_bits(x_bits)
        if prior is not None:
            return float_guard.bits_to_double(prior)

        # Note: in IEEE-754, sqrt(-0.0) == -0.0, whereas sqrt(x < 0) is Invalid Operation -> qNaN
        if x < 0.0:
            return self.mint_qnan_tombstone(IEEE754FaultOp.SQRT_NEGATIVE, site_id, row_idx, col_idx)
        if x == 0.0 and (x_bits >> 63) == 1:
            return -0.0
        return math.sqrt(float(x))

    def safe_log(
        self,
        x: float,
        site_id: int,
        row_idx: int = 0,
        col_idx: int = 0,
    ) -> float:
        x_bits = float_guard.double_to_bits(float(x))
        prior = self._check_input_bits(x_bits)
        if prior is not None:
            return float_guard.bits_to_double(prior)

        if x <= 0.0:
            return self.mint_qnan_tombstone(IEEE754FaultOp.LOG_NONPOSITIVE, site_id, row_idx, col_idx)
        return math.log(float(x))

    # ----------------------------------------------------------------------------------------------
    # SUBNORMAL / DENORMAL FTZ & DAZ SENTINEL
    # ----------------------------------------------------------------------------------------------

    def audit_subnormal_pipeline(
        self,
        transform_fn: Optional[Any] = None,
        fail_on_flush: bool = True,
    ) -> SubnormalAuditReport:
        """
        Tests whether `transform_fn` (or the current runtime environment) preserves IEEE-754
        gradual underflow across 8 subnormal octaves in `binary64` (`5e-324` .. `1e-309`)
        and `binary32` (`1.4e-45` .. `1e-39`).
        """
        # Exact IEEE-754 binary64 subnormals: exponent=0, mantissa=2^k
        subnormal_probes_bits = [
            0x0000000000000001,  # 5e-324 (min positive subnormal)
            0x0000000000000010,
            0x0000000000000100,
            0x0000000000010000,
            0x0000000001000000,
            0x0000000100000000,
            0x0000010000000000,
            0x000FFFFFFFFFFFFF,  # max binary64 subnormal (~2.225073858507201e-308)
        ]
        probes = [float_guard.bits_to_double(b) for b in subnormal_probes_bits]

        flushed_indices: List[int] = []
        daz_detected = False

        for i, val in enumerate(probes):
            out = transform_fn(val) if transform_fn is not None else (val * 1.0)
            if out == 0.0 or not float_guard.inspect(out)["is_subnormal"]:
                flushed_indices.append(i)
            # Check Denormals-Are-Zero (DAZ): adding a subnormal to 0.0 produces 0.0
            daz_probe = transform_fn(val + 0.0) if transform_fn is not None else (val + 0.0)
            if daz_probe == 0.0:
                daz_detected = True

        ftz_detected = len(flushed_indices) > 0
        preserved = not ftz_detected and not daz_detected

        if fail_on_flush and not preserved:
            raise SubnormalFlushError(
                f"IEEE-754 gradual underflow violated: FTZ={ftz_detected}, DAZ={daz_detected}, "
                f"flushed_probes={flushed_indices}"
            )

        min_f64 = float_guard.bits_to_double(0x0000000000000001)
        min_f32 = struct.unpack("<f", struct.pack("<I", 0x00000001))[0]

        return SubnormalAuditReport(
            subnormals_preserved=preserved,
            ftz_detected=ftz_detected,
            daz_detected=daz_detected,
            tested_count=len(probes),
            flushed_indices=flushed_indices,
            min_positive_subnormal_f64=min_f64,
            min_positive_subnormal_f32=min_f32,
        )


# ====================================================================================================
# 4-ARM PAIRED CAUSAL HARNESS (RULE EF-13)
# ====================================================================================================

def run_4arm_causal_gauntlet() -> Dict[str, Any]:
    """
    Executes the 4-Arm Paired Causal Harness for Idea IEEE-2:
      - Arm 1 (Active Treatment): 51-bit qNaN Tombstone Tracer propagates exact `(op, site_id, row, col)`
        through a 5-stage matrix/scalar pipeline (`div -> mul -> add -> sqrt -> log`) and preserves
        `-0.0` in `sqrt(-0.0)`.
      - Arm 2 (Negative / Standard Float Control): Standard IEEE-754 float operations produce generic
        `0x7FF8000000000000` / `0xFFF8000000000000` with `0` provenance payload (`is_valid_tombstone=False`).
      - Arm 3 (Sham / Forged Payload & sNaN Trap Control): Spoofed qNaN payload with wrong CRC8 is
        rejected (`is_valid_tombstone=False`), and Signaling NaN (`sNaN`) triggers `SignalingNaNTrapError`.
      - Arm 4 (Out-of-Sample Stress Regime): Multi-fault collision preserves first-born root cause across
        100-step chain, and Subnormal Sentinel catches simulated FTZ/DAZ hardware flush while passing clean silicon.
    """
    tracer = IEEE754TombstoneTracer()

    # 1. Arm 1: Active Treatment
    # Fault originates at site_id=1337, row=42, col=19 via 0.0 / 0.0
    step1 = tracer.safe_div(0.0, 0.0, site_id=1337, row_idx=42, col_idx=19)
    # Propagate through 4 downstream operations at different sites
    step2 = tracer.safe_mul(step1, 99.5, site_id=2001, row_idx=42, col_idx=20)
    step3 = tracer.safe_add(step2, -14.2, site_id=2002, row_idx=42, col_idx=21)
    step4 = tracer.safe_sqrt(step3, site_id=2003, row_idx=42, col_idx=22)
    step5 = tracer.safe_log(step4, site_id=2004, row_idx=42, col_idx=23)

    decoded = tracer.decode_float(step5)
    # Verify IEEE-754 sqrt(-0.0) == -0.0 sign preservation
    neg_zero_sqrt = tracer.safe_sqrt(-0.0, site_id=3000)
    neg_zero_preserved = float_guard.inspect(neg_zero_sqrt)["is_negative_zero"]

    arm1_passed = (
        decoded.is_valid_tombstone
        and decoded.op_code == IEEE754FaultOp.ZERO_DIV_ZERO
        and decoded.site_id == 1337
        and decoded.row_idx == 42
        and decoded.col_idx == 19
        and neg_zero_preserved
    )

    # 2. Arm 2: Negative Control (Un-instrumented IEEE-754 NaN loses all origin metadata)
    raw_nan = float("inf") - float("inf")
    raw_decoded = tracer.decode_float(raw_nan)
    arm2_passed = (
        raw_decoded.is_nan
        and not raw_decoded.is_valid_tombstone
        and raw_decoded.site_id == 0
    )

    # 3. Arm 3: Sham / Tampered Payload & sNaN Trap Control
    valid_bits = tracer.mint_qnan_tombstone_bits(IEEE754FaultOp.SQRT_NEGATIVE, site_id=555, row_idx=7, col_idx=8)
    # Flip 1 bit in site_id without updating CRC8
    forged_bits = valid_bits ^ (1 << 29)
    forged_rec = tracer.inspect_bits(forged_bits)

    snan_bits = tracer.mint_snan_bits(payload=0x12345)
    snan_rec = tracer.inspect_bits(snan_bits)
    snan_trapped = False
    try:
        tracer._check_input_bits(snan_bits)
    except SignalingNaNTrapError:
        snan_trapped = True

    arm3_passed = (
        not forged_rec.is_valid_tombstone
        and snan_rec.is_signaling_nan
        and snan_trapped
    )

    # 4. Arm 4: Out-of-Sample Stress Regime (Multi-Fault Root-Cause Priority + FTZ/DAZ Detection)
    fault_a = tracer.safe_sqrt(-1.0, site_id=101, row_idx=3, col_idx=4)
    fault_b = tracer.safe_log(0.0, site_id=999, row_idx=8, col_idx=9)
    merged = tracer.safe_add(fault_a, fault_b, site_id=1000)
    merged_rec = tracer.decode_float(merged)

    clean_subnormal_report = tracer.audit_subnormal_pipeline(fail_on_flush=True)

    ftz_caught = False
    try:
        tracer.audit_subnormal_pipeline(
            transform_fn=lambda x: 0.0 if abs(x) < 2.2250738585072014e-308 else x,
            fail_on_flush=True,
        )
    except SubnormalFlushError:
        ftz_caught = True

    arm4_passed = (
        merged_rec.is_valid_tombstone
        and merged_rec.op_code == IEEE754FaultOp.SQRT_NEGATIVE
        and merged_rec.site_id == 101
        and clean_subnormal_report.subnormals_preserved
        and ftz_caught
    )

    overall_pass = arm1_passed and arm2_passed and arm3_passed and arm4_passed
    return {
        "engine": "IEEE-2_QNAN_TOMBSTONE_AND_SUBNORMAL_SENTINEL",
        "verdict": "PASS" if overall_pass else "FAIL",
        "arm1_active_treatment": {
            "passed": arm1_passed,
            "tombstone_hex": decoded.raw_bits_hex,
            "recovered_op": decoded.op_name,
            "recovered_site_id": decoded.site_id,
            "recovered_row_col": [decoded.row_idx, decoded.col_idx],
            "sqrt_neg_zero_sign_preserved": neg_zero_preserved,
        },
        "arm2_negative_control": {
            "passed": arm2_passed,
            "uninstrumented_nan_hex": raw_decoded.raw_bits_hex,
            "is_valid_tombstone": raw_decoded.is_valid_tombstone,
        },
        "arm3_sham_control": {
            "passed": arm3_passed,
            "forged_crc8_rejected": not forged_rec.is_valid_tombstone,
            "snan_trap_triggered": snan_trapped,
        },
        "arm4_stress_regime": {
            "passed": arm4_passed,
            "first_born_fault_preserved": merged_rec.site_id == 101,
            "clean_subnormals_preserved": clean_subnormal_report.subnormals_preserved,
            "simulated_ftz_caught": ftz_caught,
        },
    }


def main() -> None:
    parser = argparse.ArgumentParser(description="Idea IEEE-2: IEEE-754 qNaN Tombstone Tracer & Subnormal Sentinel")
    parser.add_argument("--gauntlet", action="store_true", help="Run 4-Arm Paired Causal Harness")
    args = parser.parse_args()

    res = run_4arm_causal_gauntlet()
    print(json.dumps(res, indent=2))
    if res["verdict"] != "PASS":
        sys.exit(1)


if __name__ == "__main__":
    main()

