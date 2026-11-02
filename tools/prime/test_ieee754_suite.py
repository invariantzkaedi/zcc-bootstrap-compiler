#!/usr/bin/env python3
# -*- coding: utf-8 -*-
r"""
====================================================================================================
 🔱 ZKAEDI PRIME // DETERMINISTIC PYTEST GAUNTLET FOR IDEAS IEEE-1, IEEE-2, IEEE-3, IEEE-4 🔱
====================================================================================================
Verifies all 16 mathematical, cryptographic, and IEEE-754 bit-level fortification invariants across:
  - float_guard.py (signed-zero ULP parity & inspect negative-zero detection)
  - tools/prime/ieee754_vector_firewall.py (Idea IEEE-1)
  - tools/prime/ieee754_nan_tombstone_tracer.py (Idea IEEE-2)
  - tools/prime/ieee754_deterministic_reducer.py (Idea IEEE-3)
  - tools/prime/ieee754_weight_watermarker.py (Idea IEEE-4)
====================================================================================================
"""

from __future__ import annotations

import math
import sys
from pathlib import Path

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
if "zkaedi_prime" in sys.modules and not hasattr(sys.modules["zkaedi_prime"], "__path__"):
    del sys.modules["zkaedi_prime"]

import numpy as np
import pytest

import float_guard
from tools.prime.ieee754_vector_firewall import (
    IEEE754VectorFirewall,
    QuantizationFraudError,
    TenantIsolationError,
    VectorPoisoningError,
    float32_to_u32,
    u32_to_float32,
    run_4arm_causal_gauntlet as run_gauntlet_ieee1,
)
from tools.prime.ieee754_nan_tombstone_tracer import (
    IEEE754FaultOp,
    IEEE754TombstoneTracer,
    SignalingNaNTrapError,
    SubnormalFlushError,
    run_4arm_causal_gauntlet as run_gauntlet_ieee2,
)
from tools.prime.ieee754_deterministic_reducer import (
    IEEE754KulischSuperaccumulator,
    NonFiniteReductionError,
    run_4arm_causal_gauntlet as run_gauntlet_ieee3,
)
from tools.prime.ieee754_weight_watermarker import (
    IEEE754HamiltonianWeightWatermarker,
    OwnershipVerificationError,
    run_4arm_causal_gauntlet as run_gauntlet_ieee4,
)


# ====================================================================================================
# 1. FLOAT_GUARD SIGNED-ZERO & SUBNORMAL FORTIFICATION TESTS
# ====================================================================================================

def test_float_guard_signed_zero_ulp_and_inspect():
    """Gap 1 Fix: ulp_diff(-0.0, +0.0) == 0 while signed_zero_bits_differ(-0.0, +0.0) == True."""
    assert float_guard.ulp_diff(-0.0, 0.0) == 0
    assert float_guard.ulp_diff(0.0, -0.0) == 0
    assert float_guard.signed_zero_bits_differ(-0.0, 0.0) is True
    assert float_guard.signed_zero_bits_differ(0.0, 0.0) is False

    info_neg = float_guard.inspect(-0.0)
    info_pos = float_guard.inspect(0.0)
    assert info_neg["is_zero"] is True
    assert info_neg["is_negative_zero"] is True
    assert info_pos["is_zero"] is True
    assert info_pos["is_negative_zero"] is False


# ====================================================================================================
# 2. IDEA IEEE-1: SELF-AUTHENTICATING VECTOR FIREWALL TESTS
# ====================================================================================================

def test_ieee1_dense_keyed_ecc_and_wots_seal():
    rng = np.random.default_rng(101)
    fw = IEEE754VectorFirewall(redundancy=5)
    vec = rng.standard_normal(1536).astype(np.float32)
    vec /= np.linalg.norm(vec)

    rec = fw.seal_and_ingest(
        doc_id="doc_alpha",
        tenant_id="TENANT_01",
        text="Confidential sovereign balance sheet entry.",
        vector=vec,
        carrier_mode="DENSE_LSB_ECC",
    )
    assert rec.cosine_drift < 1e-6
    assert rec.max_ulp_drift <= 1

    # Flip 50 prefix coordinates (simulating prefix LSB scrub)
    attacked = rec.vector.copy()
    for i in range(50):
        attacked[i] = u32_to_float32(float32_to_u32(float(attacked[i])) ^ 1)

    report = fw.verify_retrieved_chunk(rec, "TENANT_01", candidate_vector=attacked)
    assert report.is_authentic_fp32 is True
    assert report.canary_ber == 0.0


def test_ieee1_sparse_sign_of_zero_zero_drift():
    rng = np.random.default_rng(102)
    fw = IEEE754VectorFirewall(redundancy=5)
    sparse = np.zeros(4096, dtype=np.float32)
    nz_idx = rng.choice(4096, size=256, replace=False)
    sparse[nz_idx] = rng.uniform(0.1, 2.0, size=256).astype(np.float32)

    rec = fw.seal_and_ingest(
        doc_id="doc_sparse",
        tenant_id="TENANT_01",
        text="Sparse monosemantic feature.",
        vector=sparse,
        carrier_mode="SPARSE_SIGN_OF_ZERO",
    )
    assert bool(np.all(sparse == rec.vector)) is True
    query = rng.standard_normal(4096).astype(np.float32)
    assert float(np.dot(sparse, query)) == float(np.dot(rec.vector, query))

    # Algebraic canonicalization (+ 0.0f) strips -0.0f and must raise QuantizationFraudError
    stripped = rec.vector + np.float32(0.0)
    with pytest.raises(QuantizationFraudError):
        fw.verify_retrieved_chunk(rec, "TENANT_01", candidate_vector=stripped)


def test_ieee1_tenant_swap_and_quantization_fraud_rejected():
    rng = np.random.default_rng(103)
    fw = IEEE754VectorFirewall(redundancy=5)
    vec = rng.standard_normal(1536).astype(np.float32)
    vec /= np.linalg.norm(vec)
    rec = fw.seal_and_ingest("doc_q", "TENANT_A", "Authentic text.", vec)

    with pytest.raises(TenantIsolationError):
        fw.verify_retrieved_chunk(rec, "TENANT_B")

    with pytest.raises(VectorPoisoningError):
        fw.verify_retrieved_chunk(rec, "TENANT_A", candidate_text="Injected adversarial prompt.")

    fp16_cast = rec.vector.astype(np.float16).astype(np.float32)
    with pytest.raises(QuantizationFraudError):
        fw.verify_retrieved_chunk(rec, "TENANT_A", candidate_vector=fp16_cast)


def test_ieee1_4arm_gauntlet():
    res = run_gauntlet_ieee1()
    assert res["verdict"] == "PASS"


# ====================================================================================================
# 3. IDEA IEEE-2: qNaN TOMBSTONE TRACER & SUBNORMAL SENTINEL TESTS
# ====================================================================================================

def test_ieee2_qnan_51bit_tombstone_propagation_and_crc8():
    tracer = IEEE754TombstoneTracer()
    qnan = tracer.safe_div(0.0, 0.0, site_id=4095, row_idx=511, col_idx=255)
    assert math.isnan(qnan)

    # Propagate through downstream ops
    res = tracer.safe_log(tracer.safe_sqrt(tracer.safe_add(qnan, 100.0, 10), 11), 12)
    rec = tracer.decode_float(res)
    assert rec.is_valid_tombstone is True
    assert rec.op_code == IEEE754FaultOp.ZERO_DIV_ZERO
    assert rec.site_id == 4095
    assert rec.row_idx == 511
    assert rec.col_idx == 255

    # Tamper 1 bit of payload -> CRC8 rejects forged tombstone
    bits = float_guard.double_to_bits(res) ^ (1 << 15)
    bad_rec = tracer.inspect_bits(bits)
    assert bad_rec.is_valid_tombstone is False


def test_ieee2_snan_trap_and_subnormal_ftz_sentinel():
    tracer = IEEE754TombstoneTracer()
    snan_bits = tracer.mint_snan_bits(0xABC)
    assert tracer.inspect_bits(snan_bits).is_signaling_nan is True
    with pytest.raises(SignalingNaNTrapError):
        tracer._check_input_bits(snan_bits)

    clean_rep = tracer.audit_subnormal_pipeline(fail_on_flush=True)
    assert clean_rep.subnormals_preserved is True

    with pytest.raises(SubnormalFlushError):
        tracer.audit_subnormal_pipeline(
            transform_fn=lambda x: 0.0 if abs(x) < 1e-308 else x,
            fail_on_flush=True,
        )


def test_ieee2_4arm_gauntlet():
    res = run_gauntlet_ieee2()
    assert res["verdict"] == "PASS"


# ====================================================================================================
# 4. IDEA IEEE-3: KULISCH 4288-BIT SUPERACCUMULATOR TESTS
# ====================================================================================================

def test_ieee3_kulisch_multi_octave_cancellation_and_subnormal():
    subnormal = float_guard.bits_to_double(0x0000000000000001)  # 5e-324
    terms = [1e100, 1.0, -1e100, -1.0, subnormal]
    res = IEEE754KulischSuperaccumulator.super_sum(terms)
    assert float_guard.ulp_diff(res, subnormal) == 0


def test_ieee3_permutation_bit_identity_and_ties_to_even():
    rng = np.random.default_rng(99)
    arr = [1e20, -1e20] * 250 + [1e-5 * i for i in range(1, 251)]
    expected_bits = float_guard.double_to_bits(IEEE754KulischSuperaccumulator.super_sum(arr))

    for _ in range(15):
        shuffled = list(arr)
        rng.shuffle(shuffled)
        bits = float_guard.double_to_bits(IEEE754KulischSuperaccumulator.super_sum(shuffled))
        assert bits == expected_bits

    # Round-Ties-To-Even check at exact halfway boundary: 1.0 + 2^-53 rounds to 1.0 (even LSB=0),
    # whereas (1.0 + 2^-52) + 2^-53 rounds up to 1.0 + 2^-51 (even LSB=0).
    half_ulp = 2.0**-53
    full_ulp = 2.0**-52
    r_even = IEEE754KulischSuperaccumulator.super_sum([1.0, half_ulp])
    assert float_guard.double_to_bits(r_even) == float_guard.double_to_bits(1.0)

    r_odd_up = IEEE754KulischSuperaccumulator.super_sum([1.0 + full_ulp, half_ulp])
    assert float_guard.double_to_bits(r_odd_up) == float_guard.double_to_bits(1.0 + 2.0 * full_ulp)


def test_ieee3_signed_zero_and_nonfinite_rejection():
    neg_z = IEEE754KulischSuperaccumulator.super_sum([-0.0, -0.0])
    assert float_guard.inspect(neg_z)["is_negative_zero"] is True

    pos_z = IEEE754KulischSuperaccumulator.super_sum([-0.0, 0.0])
    assert float_guard.inspect(pos_z)["is_negative_zero"] is False

    with pytest.raises(NonFiniteReductionError):
        IEEE754KulischSuperaccumulator.super_sum([1.0, float("inf")])


def test_ieee3_4arm_gauntlet():
    res = run_gauntlet_ieee3()
    assert res["verdict"] == "PASS"


# ====================================================================================================
# 5. IDEA IEEE-4: HAMILTONIAN NEURAL WEIGHT WATERMARKER TESTS
# ====================================================================================================

def test_ieee4_hamiltonian_watermark_survival_and_wots():
    rng = np.random.default_rng(77)
    wm = IEEE754HamiltonianWeightWatermarker(redundancy=7)
    w = rng.normal(0.0, 0.5, size=(128, 64)).astype(np.float32)
    w[np.abs(w) < 0.05] = 0.0  # Include pruned zero weights

    bundle = wm.embed_watermark(w, owner_id="ZKAEDI_LABS", model_id="model_v1")
    assert bundle.max_weight_ulp_drift <= 1
    assert bundle.max_forward_logit_drift < 1e-6
    assert bundle.hamiltonian_ftle < 0.0

    # Corrupt 300 random weights (both pruned zero sign bits and non-zero mantissa LSBs)
    corrupted = bundle.weights.flatten().copy()
    idx_flip = rng.choice(len(corrupted), size=300, replace=False)
    for idx in idx_flip:
        val = float(corrupted[idx])
        u = float32_to_u32(val)
        if val == 0.0:
            corrupted[idx] = u32_to_float32(u ^ 0x80000000)
        else:
            corrupted[idx] = u32_to_float32(u ^ 1)

    res = wm.verify_watermark(bundle, candidate_weights=corrupted.reshape(w.shape))
    assert res.verified is True
    assert res.bit_error_rate == 0.0

    with pytest.raises(OwnershipVerificationError):
        wm.verify_watermark(bundle, claimed_owner_id="IMPOSTOR_INC")


def test_ieee4_4arm_gauntlet():
    res = run_gauntlet_ieee4()
    assert res["verdict"] == "PASS"
