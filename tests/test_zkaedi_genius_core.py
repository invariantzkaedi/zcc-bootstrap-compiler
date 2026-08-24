"""
test_zkaedi_genius_core.py — Unit Tests for Zkaedi Genius Core & Exceptions
==========================================================================
Verifies 100% coverage and diagnostic correctness across ZkaediException,
monadic SovereignResult[T, E], CognitiveVector, and SymplecticState.
"""

import sys
import pytest
from pathlib import Path

# Add paths
REPO_ROOT = Path(__file__).resolve().parent.parent
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from src.zkaedi_genius_core import (
    ZkaediException,
    RemediationStrategy,
    NumericalSingularityException,
    HamiltonianDivergenceError,
    DenormalLeakError,
    ULPDesyncLimitExceededError,
    FormalVerificationException,
    SMTCounterexampleDetectedError,
    R1CSConstraintViolationError,
    CryptographicThresholdException,
    ByzantineQuorumCollapseError,
    NonMonotonicEpochRollbackError,
    SovereignResult,
    CognitiveVector,
    SymplecticState,
    FormalTheoremCertificate,
)


# ==============================================================================
# 1. Exception Hierarchy & Formatting Tests
# ==============================================================================

def test_zkaedi_exception_diagnostics_and_formatting():
    ex = ZkaediException("Test fault", error_code="ERR_TEST", remediation=RemediationStrategy.HOT_RELOAD_IN_MEMORY_KERNEL, telemetry={"node": 1})
    d = ex.to_dict()
    assert d["error_code"] == "ERR_TEST"
    assert d["message"] == "Test fault"
    assert d["remediation"] == "HOT_RELOAD_IN_MEMORY_KERNEL"
    assert d["telemetry"]["node"] == 1
    assert d["trace_digest"].startswith("0x")

    box = ex.format_diagnostic_box()
    assert "ERR_TEST" in box
    assert "Test fault" in box

    # Subclasses
    h_err = HamiltonianDivergenceError(energy=150.0, step=42)
    assert h_err.error_code == "ERR_HAMILTONIAN_DIVERGENCE"
    assert h_err.remediation == RemediationStrategy.RECALIBRATE_ETA_SUBCLIPPING

    d_err = DenormalLeakError(lane_index=3, bit_pattern="0x0000000000000001")
    assert d_err.error_code == "ERR_DENORMAL_FLOAT_LEAK"

    u_err = ULPDesyncLimitExceededError(observed_ulp=1.5, max_allowed=0.0)
    assert u_err.error_code == "ERR_ULP_DESYNC_LIMIT_EXCEEDED"

    s_err = SMTCounterexampleDetectedError("thm_1", {"x": 42})
    assert s_err.error_code == "ERR_SMT_COUNTEREXAMPLE_FOUND"

    r_err = R1CSConstraintViolationError(gate_index=5, left=2, right=3, out=7)
    assert r_err.error_code == "ERR_R1CS_GATE_UNSATISFIED"

    q_err = ByzantineQuorumCollapseError(qualified_count=2, threshold=3)
    assert q_err.error_code == "ERR_BYZANTINE_QUORUM_COLLAPSE"

    e_err = NonMonotonicEpochRollbackError(target_epoch=1, current_epoch=2)
    assert e_err.error_code == "ERR_EPOCH_ROLLBACK_ATTEMPT"


# ==============================================================================
# 2. Monadic SovereignResult[T, E] Tests
# ==============================================================================

def test_sovereign_result_monadic_operations():
    # Ok path
    res_ok = SovereignResult.ok(42)
    assert res_ok.is_ok
    assert not res_ok.is_err
    assert res_ok.unwrap() == 42
    assert res_ok.unwrap_or(0) == 42
    assert res_ok.unwrap_or_else(lambda e: 0) == 42

    # Map on Ok
    mapped = res_ok.map(lambda x: x * 2)
    assert mapped.unwrap() == 84

    # Map with exception
    def faulty_map(x):
        raise ZkaediException("Map failure", error_code="ERR_MAP")
    failed_map = res_ok.map(faulty_map)
    assert failed_map.is_err

    def generic_faulty_map(x):
        raise ValueError("Generic map failure")
    failed_generic_map = res_ok.map(generic_faulty_map)
    assert failed_generic_map.is_err

    # And_then on Ok
    and_then_res = res_ok.and_then(lambda x: SovereignResult.ok(f"Val: {x}"))
    assert and_then_res.unwrap() == "Val: 42"

    # Recover on Ok (no-op)
    recovered_ok = res_ok.recover(lambda e: SovereignResult.ok(100))
    assert recovered_ok.unwrap() == 42

    # Err path
    err_inst = ZkaediException("Failed computation")
    res_err = SovereignResult.err(err_inst)
    assert res_err.is_err
    assert not res_err.is_ok
    assert res_err.unwrap_or(999) == 999
    assert res_err.unwrap_or_else(lambda e: 777) == 777

    with pytest.raises(ZkaediException):
        res_err.unwrap()

    # Map and and_then on Err
    assert res_err.map(lambda x: x * 2).is_err
    assert res_err.and_then(lambda x: SovereignResult.ok(x)).is_err

    # Recover on Err
    recovered = res_err.recover(lambda e: SovereignResult.ok(555))
    assert recovered.unwrap() == 555


# ==============================================================================
# 3. Cognitive Vector & Symplectic State Tests
# ==============================================================================

def test_cognitive_vector_and_similarity():
    v1 = CognitiveVector(dimension=4, features=(1.0, 0.0, 1.0, 0.0), symbol_name="sym_a")
    v2 = CognitiveVector(dimension=4, features=(1.0, 0.0, 1.0, 0.0), symbol_name="sym_b")
    v3 = CognitiveVector(dimension=4, features=(0.0, 1.0, 0.0, 1.0), symbol_name="sym_c")
    v_zero = CognitiveVector(dimension=4, features=(0.0, 0.0, 0.0, 0.0), symbol_name="sym_zero")

    assert pytest.approx(v1.cosine_similarity(v2), 1e-6) == 1.0
    assert pytest.approx(v1.cosine_similarity(v3), 1e-6) == 0.0
    assert v1.cosine_similarity(v_zero) == 0.0

    # Dimension mismatch check
    with pytest.raises(NumericalSingularityException):
        CognitiveVector(dimension=4, features=(1.0, 2.0), symbol_name="bad")

    v_diff_dim = CognitiveVector(dimension=2, features=(1.0, 2.0), symbol_name="diff")
    with pytest.raises(NumericalSingularityException):
        v1.cosine_similarity(v_diff_dim)

    # Hadamard product
    had = v1.hadamard_product(v2)
    assert had.features == (1.0, 0.0, 1.0, 0.0)
    assert "sym_a⊗sym_b" in had.symbol_name

    with pytest.raises(NumericalSingularityException):
        v1.hadamard_product(v_diff_dim)


def test_symplectic_state_and_certificates():
    # Conserved state
    state = SymplecticState(step=1, q=1.0, p=0.0, hamiltonian=10.0)
    res_cons = state.verify_conservation(h0=10.0, tol=1e-5)
    assert res_cons.is_ok

    # Diverged state
    state_div = SymplecticState(step=2, q=5.0, p=5.0, hamiltonian=50.0)
    res_div = state_div.verify_conservation(h0=10.0, tol=1e-5)
    assert res_div.is_err
    assert res_div._error.error_code == "ERR_HAMILTONIAN_DIVERGENCE"

    # Certificate validation
    cert_valid = FormalTheoremCertificate(
        theorem_id="thm_ok",
        logic_theory="QF_BV",
        is_sound=True,
        r1cs_hash="0x1234",
        signature=b"\x01" * 64
    )
    assert cert_valid.verify_validity().is_ok

    # Unsound certificate
    cert_unsound = FormalTheoremCertificate(
        theorem_id="thm_bad",
        logic_theory="QF_BV",
        is_sound=False,
        r1cs_hash="0x1234",
        signature=b"\x01" * 64
    )
    assert cert_unsound.verify_validity().is_err

    # Bad signature length
    cert_bad_sig = FormalTheoremCertificate(
        theorem_id="thm_bad_sig",
        logic_theory="QF_BV",
        is_sound=True,
        r1cs_hash="0x1234",
        signature=b"\x01" * 32
    )
    assert cert_bad_sig.verify_validity().is_err
