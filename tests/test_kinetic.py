from __future__ import annotations

import numpy as np
from resonance_basin.kinetic import KineticEncoder
from resonance_basin.models import Event


def test_kinetic_encoder_basic_projection() -> None:
    """Verify projection dimensionality and normalization invariants."""
    encoder = KineticEncoder(dim=128)
    event = Event(payload={
        "accel": [0.1, 0.2, 0.9],
        "gyro": [0.01, -0.02, 0.05],
        "vortex_score": 0.5
    })
    
    vector = encoder(event)
    assert vector.shape == (128,)
    assert np.allclose(np.linalg.norm(vector), 1.0, atol=1e-8)
    assert np.abs(np.mean(vector)) < 1e-8


def test_kinetic_high_pass_filtering() -> None:
    """Ensure that the high-pass filter isolates motion from static bias."""
    encoder = KineticEncoder(dim=64, alpha=0.9)
    static_event = Event(payload={"accel": [0.0, 0.0, 1.0], "gyro": [0, 0, 0], "vortex_score": 0})
    
    # Warm up filter
    for _ in range(20):
        encoder(static_event)
        
    motion_event = Event(payload={"accel": [0.1, 0.0, 1.0], "gyro": [0, 0, 0], "vortex_score": 0})
    
    v_static = encoder(static_event)
    v_motion = encoder(motion_event)
    
    # Static should produce significantly different vector than motion after warmup
    assert not np.allclose(v_static, v_motion, atol=1e-3)


def test_kinetic_malformed_input() -> None:
    """Verify zero-vector fallback for malformed or incorrect shapes."""
    encoder = KineticEncoder(dim=64)
    
    # Wrong types
    assert np.allclose(encoder(Event(payload="fail")), np.zeros(64))
    
    # Wrong shapes (2D accel instead of 3D)
    assert np.allclose(
        encoder(Event(payload={"accel": [1, 2], "gyro": [1, 2, 3]})), 
        np.zeros(64)
    )
    
    # Missing fields
    assert np.allclose(encoder(Event(payload={"gyro": [1, 2, 3]})), np.zeros(64))


def test_kinetic_projection_determinism() -> None:
    """Prove that encoders with same seed/dim produce identical projections."""
    e1 = KineticEncoder(dim=32, seed=123)
    e2 = KineticEncoder(dim=32, seed=123)
    e3 = KineticEncoder(dim=32, seed=456)
    
    event = Event(payload={"accel": [0, 0, 1], "gyro": [0, 0, 0], "vortex_score": 0.5})
    
    v1 = e1(event)
    v2 = e2(event)
    v3 = e3(event)
    
    assert np.array_equal(v1, v2)
    assert not np.array_equal(v1, v3)


def test_twin_encoder_identity() -> None:
    """Verify that independent instances produce identical results for the same event."""
    cfg_dim = 128
    cfg_seed = 42
    enc1 = KineticEncoder(dim=cfg_dim, seed=cfg_seed)
    enc2 = KineticEncoder(dim=cfg_dim, seed=cfg_seed)
    
    event = Event(payload={"accel": [0.1, -0.2, 0.9], "gyro": [0.01, 0.02, 0.03], "vortex_score": 0.8})
    
    res1 = enc1(event)
    res2 = enc2(event)
    
    assert np.allclose(res1, res2, atol=1e-15)
