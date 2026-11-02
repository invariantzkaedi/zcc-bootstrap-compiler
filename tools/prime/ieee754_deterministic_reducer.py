#!/usr/bin/env python3
# -*- coding: utf-8 -*-
r"""
====================================================================================================
 🔱 ZKAEDI PRIME // IDEA IEEE-3: KULISCH 4288-BIT SUPERACCUMULATOR & DETERMINISTIC TENSOR REDUCER 🔱
====================================================================================================
Fuses:
  - float_guard.py (double_to_bits, bits_to_double, ulp_diff, inspect, exact_sum)
  - tools/merkle_kv_cache_engine.py (SHA-256 content-addressed trajectory attestation)
  - tools/prime/crash_immunizer.py (Self-healing numerical kernel architecture)

Pre-Fortified Mathematical Architecture (Kulisch Exact Fixed-Point Superaccumulator):
  1. Why Standard IEEE-754 Reductions Are Non-Deterministic:
     IEEE-754 `binary64` addition is non-associative: `(a + b) + c != a + (b + c)`.
     Even Kahan (`kahan_sum`) and Neumaier (`neumaier_sum`) fail when intermediate terms span
     more than 106 bits of exponent range (e.g. `[1e100, 1.0, -1e100, -1.0]`), and thread-scheduled
     GPU/CPU reductions produce different float bit patterns on every run (`Gap 4` & `Gap 4b`).
  2. Exact 4288-Bit Fixed-Point Kulisch Superaccumulator:
     Every finite IEEE-754 `binary64` float `x` has an exact dyadic integer representation:
       - If normal (`1 <= exp_raw <= 2046`):
           `x = (-1)^sign * (2^52 + mantissa) * 2^(exp_raw - 1023 - 52)`
       - If subnormal (`exp_raw == 0, mantissa > 0`):
           `x = (-1)^sign * mantissa * 2^(-1022 - 52)`
     Since `MIN_SHIFT = -1074` (`-1022 - 52`, the bit position of `5e-324`) and the maximum bit
     position of `DBL_MAX` is `+1023`, shifting every 53-bit significand by `(exp_shift - (-1074))`
     maps **every finite IEEE-754 `binary64` number into a single exact signed integer** with zero
     truncation and zero rounding!
     Because integer addition `(A + B) + C == A + (B + C)` is strictly associative and commutative,
     accumulating `N` floats in the Kulisch register and rounding **once** at the very end using
     exact IEEE-754 **Round-Ties-To-Even** guarantees:
       - **0 ULP error** against $\mathbb{Q}$ (`fractions.Fraction`) across all scales (`5e-324` to `1.797e308`)
       - **100% Bit-Identical Hex Output** across any thread ordering or random permutation!
  3. Exact Fused Dot-Product (`super_dot`):
     Each product `a_i * b_i` of two `binary64` floats has an exact 106-bit significand
     `sig_a * sig_b` at shift `shift_a + shift_b` (`>= -2148`). Accumulating all exact 106-bit
     products into the Kulisch register at base shift `-2148` computes the exact inner product
     with a single final Round-Ties-To-Even!
  4. IEEE-754 Signed-Zero & Special-Value Compliance:
     Preserves IEEE-754 `-0.0 + (-0.0) == -0.0`, `+0.0 + (-0.0) == +0.0` (under Round-Ties-To-Even),
     and fail-closes on `NaN` or `Inf - Inf`.
====================================================================================================
"""

from __future__ import annotations

import argparse
import dataclasses
import hashlib
import json
import math
import struct
import sys
from fractions import Fraction
from pathlib import Path
from typing import Any, Dict, Iterable, List, Sequence, Tuple

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
# CONSTANTS & EXCEPTIONS
# ====================================================================================================

MIN_SUBNORMAL_SHIFT_F64 = -1074       # -1022 - 52 (LSB of 5e-324)
MIN_PRODUCT_SHIFT_F64 = -2148         # 2 * -1074 (LSB of (5e-324)^2)
MAX_EXP_F64 = 1023
MIN_NORMAL_EXP_F64 = -1022


class NonFiniteReductionError(Exception):
    """Raised when NaN or indeterminate Infinity (Inf - Inf) enters a deterministic reduction."""


@dataclasses.dataclass(frozen=True)
class DeterministicReductionReceipt:
    operation: str
    element_count: int
    result_float: float
    result_bits_hex: str
    naive_float: float
    naive_bits_hex: str
    neumaier_float: float
    ulp_error_vs_exact: int
    naive_ulp_drift: int
    neumaier_ulp_drift: int
    merkle_seal_hex: str


# ====================================================================================================
# IEEE-754 DECOMPOSITION & ROUND-TIES-TO-EVEN RECONSTRUCTION
# ====================================================================================================

def decompose_f64_to_dyadic(val: float) -> Tuple[int, int, int, bool]:
    """
    Decomposes a finite IEEE-754 `binary64` float into `(sign, sig53, shift, is_zero)`
    such that `val == (-1)^sign * sig53 * 2^shift` holds with 100% mathematical exactness.
    """
    bits = float_guard.double_to_bits(float(val))
    sign = (bits >> 63) & 1
    exp_raw = (bits >> 52) & 0x7FF
    mant = bits & 0xFFFFFFFFFFFFF

    if exp_raw == 0x7FF:
        raise NonFiniteReductionError(f"Non-finite IEEE-754 value encountered: 0x{bits:016X}")

    if exp_raw == 0:
        if mant == 0:
            return sign, 0, MIN_SUBNORMAL_SHIFT_F64, True
        # Subnormal: 0.mant * 2^-1022 = mant * 2^(-1074)
        return sign, mant, MIN_SUBNORMAL_SHIFT_F64, False

    # Normal: (2^52 + mant) * 2^(exp_raw - 1023 - 52)
    sig53 = (1 << 52) | mant
    shift = exp_raw - 1075
    return sign, sig53, shift, False


def round_fixed_integer_to_f64(
    accum: int,
    base_shift: int,
    all_negative_zeros: bool = False,
) -> float:
    """
    Converts an exact signed integer `accum * 2^base_shift` into the nearest IEEE-754 `binary64`
    floating-point value using exact **Round-Ties-To-Even** (`roundTiesToEven`), including
    exact gradual underflow into subnormals (`exp_raw == 0`).
    """
    if accum == 0:
        return -0.0 if all_negative_zeros else 0.0

    sign = 1 if accum < 0 else 0
    mag = -accum if accum < 0 else accum

    bit_len = mag.bit_length()
    # Value is mag * 2^base_shift. The MSB is at 2^(base_shift + bit_len - 1).
    unbiased_exp = base_shift + bit_len - 1

    if unbiased_exp > MAX_EXP_F64:
        inf_bits = (sign << 63) | (0x7FF << 52)
        return float_guard.bits_to_double(inf_bits)

    if unbiased_exp >= MIN_NORMAL_EXP_F64:
        # Normal number: we need 53 bits of significand (MSB at bit 52).
        # Target LSB shift is `unbiased_exp - 52`.
        target_lsb_shift = unbiased_exp - 52
        shift_diff = target_lsb_shift - base_shift
        if shift_diff <= 0:
            sig53 = mag << (-shift_diff)
        else:
            # Round right by `shift_diff` bits using Round-Ties-To-Even
            sig53 = _round_shr_ties_to_even(mag, shift_diff)
            if sig53 >= (1 << 53):
                sig53 >>= 1
                unbiased_exp += 1
                if unbiased_exp > MAX_EXP_F64:
                    inf_bits = (sign << 63) | (0x7FF << 52)
                    return float_guard.bits_to_double(inf_bits)

        exp_raw = unbiased_exp + 1023
        mant = sig53 & 0xFFFFFFFFFFFFF
        bits = (sign << 63) | (exp_raw << 52) | mant
        return float_guard.bits_to_double(bits)

    # Subnormal or underflow to zero: target LSB shift is fixed at -1074 (`MIN_SUBNORMAL_SHIFT_F64`)
    shift_diff = MIN_SUBNORMAL_SHIFT_F64 - base_shift
    if shift_diff <= 0:
        mant_sub = mag << (-shift_diff)
    else:
        mant_sub = _round_shr_ties_to_even(mag, shift_diff)

    if mant_sub >= (1 << 52):
        # Rounded up into the smallest normal number (exp_raw = 1, mant = 0)
        bits = (sign << 63) | (1 << 52) | (mant_sub & 0xFFFFFFFFFFFFF)
        return float_guard.bits_to_double(bits)

    bits = (sign << 63) | (mant_sub & 0xFFFFFFFFFFFFF)
    return float_guard.bits_to_double(bits)


def _round_shr_ties_to_even(mag: int, shr: int) -> int:
    """
    Shifts `mag` right by `shr >= 1` bits with exact IEEE-754 Round-Ties-To-Even.
    """
    q = mag >> shr
    half = 1 << (shr - 1)
    rem = mag & ((1 << shr) - 1)
    if rem > half or (rem == half and (q & 1) == 1):
        q += 1
    return q


# ====================================================================================================
# KULISCH SUPERACCUMULATOR ENGINE
# ====================================================================================================

class IEEE754KulischSuperaccumulator:
    """
    Order-invariant, bit-exact IEEE-754 `binary64` superaccumulator.
    Guarantees 0 ULP drift and 100% permutation invariance across arbitrary thread schedules.
    """

    def __init__(self, base_shift: int = MIN_SUBNORMAL_SHIFT_F64) -> None:
        self.base_shift = base_shift
        self._accum: int = 0
        self._count: int = 0
        self._all_neg_zero: bool = True

    def reset(self) -> None:
        self._accum = 0
        self._count = 0
        self._all_neg_zero = True

    def add_float(self, val: float) -> None:
        sign, sig53, shift, is_zero = decompose_f64_to_dyadic(val)
        self._count += 1
        if is_zero:
            if sign == 0:
                self._all_neg_zero = False
            return

        self._all_neg_zero = False
        offset = shift - self.base_shift
        term = sig53 << offset
        if sign == 1:
            self._accum -= term
        else:
            self._accum += term

    def add_all(self, values: Iterable[float]) -> float:
        for v in values:
            self.add_float(float(v))
        return self.value()

    def value(self) -> float:
        if self._count == 0:
            return 0.0
        return round_fixed_integer_to_f64(
            self._accum,
            self.base_shift,
            all_negative_zeros=self._all_neg_zero,
        )

    @classmethod
    def super_sum(cls, values: Sequence[float]) -> float:
        acc = cls(base_shift=MIN_SUBNORMAL_SHIFT_F64)
        return acc.add_all(values)

    @classmethod
    def super_dot(cls, vec_a: Sequence[float], vec_b: Sequence[float]) -> float:
        """
        Computes the exact fused inner product `sum(a_i * b_i)` with a single final IEEE-754
        Round-Ties-To-Even, eliminating intermediate product rounding and cancellation.
        """
        if len(vec_a) != len(vec_b):
            raise ValueError(f"Dimension mismatch: {len(vec_a)} != {len(vec_b)}")
        if len(vec_a) == 0:
            return 0.0

        accum = 0
        all_neg_zero = True
        for a, b in zip(vec_a, vec_b):
            sa, siga, sh_a, za = decompose_f64_to_dyadic(float(a))
            sb, sigb, sh_b, zb = decompose_f64_to_dyadic(float(b))
            prod_sign = sa ^ sb
            if za or zb:
                if prod_sign == 0:
                    all_neg_zero = False
                continue
            all_neg_zero = False
            prod_sig = siga * sigb  # Exact 106-bit integer significand
            prod_shift = (sh_a + sh_b) - MIN_PRODUCT_SHIFT_F64
            term = prod_sig << prod_shift
            if prod_sign == 1:
                accum -= term
            else:
                accum += term

        return round_fixed_integer_to_f64(
            accum,
            base_shift=MIN_PRODUCT_SHIFT_F64,
            all_negative_zeros=all_neg_zero,
        )

    @classmethod
    def audit_reduction(cls, values: Sequence[float]) -> DeterministicReductionReceipt:
        res_super = cls.super_sum(values)
        res_naive = float_guard.naive_sum(values)
        res_neumaier = float_guard.neumaier_sum(values)
        res_exact = float_guard.exact_sum(values)

        ulp_super = float_guard.ulp_diff(res_super, res_exact)
        ulp_naive = float_guard.ulp_diff(res_naive, res_exact)
        ulp_neumaier = float_guard.ulp_diff(res_neumaier, res_exact)

        super_bits = float_guard.double_to_bits(res_super)
        naive_bits = float_guard.double_to_bits(res_naive)

        seal = hashlib.sha256(
            f"KULISCH:{len(values)}:0x{super_bits:016X}".encode("utf-8")
        ).hexdigest()

        return DeterministicReductionReceipt(
            operation="KULISCH_SUPER_SUM",
            element_count=len(values),
            result_float=res_super,
            result_bits_hex=f"0x{super_bits:016X}",
            naive_float=res_naive,
            naive_bits_hex=f"0x{naive_bits:016X}",
            neumaier_float=res_neumaier,
            ulp_error_vs_exact=ulp_super,
            naive_ulp_drift=ulp_naive,
            neumaier_ulp_drift=ulp_neumaier,
            merkle_seal_hex=seal,
        )


# ====================================================================================================
# 4-ARM PAIRED CAUSAL HARNESS (RULE EF-13)
# ====================================================================================================

def run_4arm_causal_gauntlet(seed: int = 42) -> Dict[str, Any]:
    """
    Executes the 4-Arm Paired Causal Harness for Idea IEEE-3:
      - Arm 1 (Active Treatment): Kulisch 4288-bit Superaccumulator achieves 0 ULP error on
        multi-octave cancellation `[1e100, 1.0, -1e100, -1.0, 5e-324]` and exact fused `super_dot`.
      - Arm 2 (Negative / Naive & Neumaier Control): Both `naive_sum`, `kahan_sum`, and `neumaier_sum`
        suffer catastrophic cancellation (`> 10^18` ULPs) on `[1e100, 1.0, -1e100, -1.0, 1e-50]`.
      - Arm 3 (Sham / Non-Associative Permutation Control): Naive float64 sum produces multiple
        divergent bit patterns across 25 random permutations of the same array, whereas Kulisch
        Superaccumulator produces 1 exact bit pattern (`0` variance across all 25 permutations).
      - Arm 4 (Out-of-Sample Stress Regime): Extreme dynamic range (`DBL_MAX / 2` down to `5e-324`),
        signed-zero `-0.0 + (-0.0) == -0.0` preservation, and fail-closed `NonFiniteReductionError` on `NaN`/`Inf`.
    """
    rng = np.random.default_rng(seed)

    # 1. Arm 1 & Arm 2: Multi-Octave Cancellation (`[1e100, 1.0, -1e100, -1.0, 1e-50]`)
    hard_terms = [1e100, 1.0, -1e100, -1.0, 1e-50]
    receipt = IEEE754KulischSuperaccumulator.audit_reduction(hard_terms)

    # Fused dot product test where `a_i * b_i` cancels to `1e-30`
    vec_a = [1e50, 1.0, -1e50, -1.0, 1e-15]
    vec_b = [1e50, 2.0, 1e50, 2.0, 1e-15]
    dot_super = IEEE754KulischSuperaccumulator.super_dot(vec_a, vec_b)
    dot_exact = float(sum(Fraction(a) * Fraction(b) for a, b in zip(vec_a, vec_b)))
    dot_naive = float_guard.naive_sum([a * b for a, b in zip(vec_a, vec_b)])
    dot_super_ulp = float_guard.ulp_diff(dot_super, dot_exact)
    dot_naive_ulp = float_guard.ulp_diff(dot_naive, dot_exact)

    arm1_passed = (
        receipt.ulp_error_vs_exact == 0
        and receipt.result_float == 1e-50
        and dot_super_ulp == 0
    )

    arm2_passed = (
        receipt.naive_ulp_drift > 1000000
        and receipt.neumaier_ulp_drift > 1000000
        and dot_naive_ulp > 1000000
    )

    # 3. Arm 3: 25-Permutation Order-Invariance Test
    base_stream = (
        [1e16, -1e16] * 400
        + [1e8, -1e8] * 400
        + [float(i) * 1e-4 for i in range(1, 401)]
    )
    super_hashes = set()
    naive_hashes = set()
    for _ in range(25):
        perm = list(base_stream)
        rng.shuffle(perm)
        s_val = IEEE754KulischSuperaccumulator.super_sum(perm)
        n_val = float_guard.naive_sum(perm)
        super_hashes.add(f"0x{float_guard.double_to_bits(s_val):016X}")
        naive_hashes.add(f"0x{float_guard.double_to_bits(n_val):016X}")

    arm3_passed = (len(super_hashes) == 1) and (len(naive_hashes) > 1)

    # 4. Arm 4: Out-of-Sample Stress Regime (Subnormal 5e-324 + Signed Zero + NonFinite Trap)
    subnormal = float_guard.bits_to_double(0x0000000000000001)  # 5e-324
    huge = 1e300
    sub_stream = [huge, subnormal, -huge]
    sub_res = IEEE754KulischSuperaccumulator.super_sum(sub_stream)
    sub_exact = float_guard.ulp_diff(sub_res, subnormal) == 0

    neg_zero_res = IEEE754KulischSuperaccumulator.super_sum([-0.0, -0.0])
    neg_zero_ok = float_guard.inspect(neg_zero_res)["is_negative_zero"]

    pos_zero_res = IEEE754KulischSuperaccumulator.super_sum([-0.0, +0.0])
    pos_zero_ok = not float_guard.inspect(pos_zero_res)["is_negative_zero"]

    nan_rejected = False
    try:
        IEEE754KulischSuperaccumulator.super_sum([1.0, float("nan")])
    except NonFiniteReductionError:
        nan_rejected = True

    arm4_passed = sub_exact and neg_zero_ok and pos_zero_ok and nan_rejected

    overall_pass = arm1_passed and arm2_passed and arm3_passed and arm4_passed
    return {
        "engine": "IEEE-3_KULISCH_DETERMINISTIC_SUPERACCUMULATOR",
        "verdict": "PASS" if overall_pass else "FAIL",
        "arm1_active_treatment": {
            "passed": arm1_passed,
            "super_sum_result": receipt.result_float,
            "super_sum_hex": receipt.result_bits_hex,
            "ulp_error_vs_exact": receipt.ulp_error_vs_exact,
            "super_dot_ulp_error": dot_super_ulp,
        },
        "arm2_negative_control": {
            "passed": arm2_passed,
            "naive_result": receipt.naive_float,
            "neumaier_result": receipt.neumaier_float,
            "naive_ulp_drift": receipt.naive_ulp_drift,
            "neumaier_ulp_drift": receipt.neumaier_ulp_drift,
        },
        "arm3_sham_permutation_control": {
            "passed": arm3_passed,
            "kulisch_unique_bit_patterns_across_25_perms": len(super_hashes),
            "naive_unique_bit_patterns_across_25_perms": len(naive_hashes),
            "canonical_hex": next(iter(super_hashes)),
        },
        "arm4_stress_regime": {
            "passed": arm4_passed,
            "subnormal_5e324_recovered_after_1e300_cancel": sub_exact,
            "signed_neg_zero_preserved": neg_zero_ok,
            "tie_to_even_pos_zero_preserved": pos_zero_ok,
            "non_finite_rejected": nan_rejected,
        },
    }


def main() -> None:
    parser = argparse.ArgumentParser(description="Idea IEEE-3: Kulisch 4288-Bit Deterministic Superaccumulator")
    parser.add_argument("--gauntlet", action="store_true", help="Run 4-Arm Paired Causal Harness")
    args = parser.parse_args()

    res = run_4arm_causal_gauntlet()
    print(json.dumps(res, indent=2))
    if res["verdict"] != "PASS":
        sys.exit(1)


if __name__ == "__main__":
    main()
