from __future__ import annotations

from datetime import datetime, timezone

import numpy as np
import pytest

from resonance_basin.core import ResonanceBasinEngine
from resonance_basin.errors import InvalidEventError
from resonance_basin.models import Config, Event


def test_config_validation() -> None:
    """Verify that the engine enforces strict hyperparameter bounds."""
    with pytest.raises(ValueError):
        Config(dim=0)
    with pytest.raises(ValueError):
        Config(gamma=0.0)
    with pytest.raises(ValueError):
        Config(gamma=1.1)
    with pytest.raises(ValueError):
        Config(alpha=-0.1)
    with pytest.raises(ValueError):
        Config(beta=0.0)
    with pytest.raises(ValueError):
        Config(history_size=0)

    cfg = Config(dim=128, gamma=0.5, alpha=0.1, beta=1.0, history_size=5)
    assert cfg.dim == 128


def test_encoding_arbitrary_dim_signature_shape() -> None:
    """Confirm that the augmented blake2b encoder correctly handles large dimensions."""
    engine = ResonanceBasinEngine(Config(dim=200, seed=42))
    sig = engine.update("test_signal")
    assert sig.vector.shape == (400,)


def test_determinism_same_seed_same_input() -> None:
    """Ensure that identical engines and inputs produce bit-identical checksums."""
    e1 = ResonanceBasinEngine(Config(dim=64, seed=42))
    e2 = ResonanceBasinEngine(Config(dim=64, seed=42))

    sig1 = e1.update("constant_signal")
    sig2 = e2.update("constant_signal")

    assert sig1.checksum == sig2.checksum
    assert np.allclose(sig1.vector, sig2.vector, atol=1e-12)


def test_graph_influence() -> None:
    """Prove that relational edges significantly influence the resulting state signatures."""
    cfg = Config(dim=32, seed=42)
    e1 = ResonanceBasinEngine(cfg)
    e2 = ResonanceBasinEngine(cfg)

    e2.add_edge("signal_source", "auth_module", weight=10.0)

    sig1 = e1.update("signal_source")
    sig2 = e2.update("signal_source")

    # Assert that the relational context induced a measurable change in the digest
    assert not np.allclose(sig1.vector, sig2.vector, atol=1e-6)


def test_history_bounded() -> None:
    """Verify that the engine maintains strict memory bounds via deque pruning."""
    engine = ResonanceBasinEngine(Config(dim=16, history_size=10, seed=42))
    for i in range(25):
        engine.update(f"event_{i}")
    assert len(engine.history) == 10


def test_timezone_awareness_utc() -> None:
    """Confirm that signatures use UTC-aware timestamps for production observability."""
    engine = ResonanceBasinEngine(Config(seed=42))
    sig = engine.update("ping")
    assert sig.timestamp.tzinfo == timezone.utc


def test_snapshot_structure() -> None:
    """Verify the integrity and structure of engine snapshots."""
    engine = ResonanceBasinEngine(Config(dim=16, seed=42))
    engine.update({"test": 123})
    snap = engine.snapshot()

    assert snap["signature_dim"] == 32
    assert snap["history_len"] == 1
    assert "last_signature" in snap
    assert snap["last_signature"] is not None
    assert "checksum" in snap["last_signature"]
    assert "energy" in snap["last_signature"]
    assert isinstance(snap["metrics"], dict)


def test_observability_error_counter_with_bad_encoder() -> None:
    """Prove that internal errors are correctly captured by the observability layer."""
    def bad_encoder(e: Event) -> np.ndarray:
        raise ValueError("boom")

    engine = ResonanceBasinEngine(Config(dim=8, seed=42), encoder=bad_encoder)

    with pytest.raises(InvalidEventError):
        engine.update("x")

    metrics = engine.snapshot()["metrics"]
    assert metrics["errors"] >= 1


def test_custom_encoder_injection() -> None:
    """Verify that the engine correctly integrates and invokes custom encoders."""
    seen = {"called": False}

    def custom_encoder(e: Event) -> np.ndarray:
        seen["called"] = True
        return np.ones(32, dtype=np.float64)

    engine = ResonanceBasinEngine(Config(dim=32, seed=42), encoder=custom_encoder)
    sig = engine.update("anything")

    assert seen["called"] is True
    assert sig.vector.shape == (64,)


def test_graph_determinism_across_instances() -> None:
    """Ensure that graph diffusion is stable across different engine instances."""
    cfg = Config(dim=24, seed=123)
    e1 = ResonanceBasinEngine(cfg)
    e2 = ResonanceBasinEngine(cfg)

    e1.add_edge("login", "auth", 5.0)
    e2.add_edge("login", "auth", 5.0)

    sig1 = e1.update({"action": "login"})
    sig2 = e2.update({"action": "login"})

    assert np.allclose(sig1.vector, sig2.vector, atol=1e-12)
    assert sig1.checksum == sig2.checksum


def test_reservoir_state_finite_after_burst() -> None:
    """Verify reservoir stability under high-frequency event bursts."""
    engine = ResonanceBasinEngine(Config(dim=32, history_size=50, seed=42))
    for i in range(200):
        engine.update({"burst": i, "kind": "hf"})
    state = engine.reservoir.get_state()
    assert np.isfinite(state).all()


def test_signature_dimension_scaling() -> None:
    """Confirm that the engine correctly scales signatures across multiple dimension targets."""
    for dim in (1, 8, 16, 64, 128):
        engine = ResonanceBasinEngine(Config(dim=dim, seed=42))
        sig = engine.update("scale-check")
        assert sig.vector.shape == (dim * 2,)

# New Hardening Tests

def test_reservoir_spectral_norm_bound() -> None:
    """Hardened: Verify that ||W||_2 <= 0.98 by construction."""
    from resonance_basin.reservoir import EchoStateReservoir
    res = EchoStateReservoir(dim=32, seed=42)
    sigma_max = np.linalg.svd(res.W, compute_uv=False)[0]
    assert sigma_max <= 0.98 + 1e-10

def test_reservoir_trajectory_convergence() -> None:
    """Hardened: Prove that twin reservoirs with same inputs converge exponentially."""
    from resonance_basin.reservoir import EchoStateReservoir
    r1 = EchoStateReservoir(dim=32, seed=42)
    r2 = EchoStateReservoir(dim=32, seed=42)
    
    # Diverge states manually
    r1.state = np.ones(32)
    r2.state = np.zeros(32)
    
    initial_dist = np.linalg.norm(r1.state - r2.state)
    
    # Drive with same inputs (seeded RNG for cross-environment determinism)
    rng = np.random.default_rng(123)
    for _ in range(100):
        x = rng.normal(0, 1, 32)
        r1.step(x)
        r2.step(x)
        
    final_dist = np.linalg.norm(r1.state - r2.state)
    # Convergence should be massive (exponential)
    assert final_dist < initial_dist * 1e-6


def test_update_many_and_full_snapshot() -> None:
    """Verify batch processing and the completeness of engine snapshots."""
    engine = ResonanceBasinEngine(Config(dim=16, seed=42))
    payloads = ["a", "b", "c"]
    sigs = engine.update_many(payloads)
    
    assert len(sigs) == 3
    snap = engine.snapshot()
    
    assert snap["history_len"] == 3
    assert snap["energy"] > 0
    assert snap["reservoir_norm"] > 0
    assert "metrics" in snap


def test_update_canonicalization_paths() -> None:
    """Exercise all input canonicalization paths in ResonanceBasinEngine.update()."""
    engine = ResonanceBasinEngine(Config(dim=16, seed=42))
    
    # Path 1: Event object
    evt = Event(payload="raw", source="test")
    sig1 = engine.update(evt)
    assert sig1.timestamp == evt.timestamp
    
    # Path 2: Dict with timestamp (float)
    ts_float = 1715000000.0
    sig2 = engine.update({"timestamp": ts_float, "data": 1})
    assert sig2.timestamp.timestamp() == ts_float
    
    # Path 3: Dict with timestamp (datetime)
    ts_dt = datetime.fromtimestamp(1715000000.0, tz=timezone.utc)
    sig3 = engine.update({"timestamp": ts_dt, "data": 2})
    assert sig3.timestamp == ts_dt
