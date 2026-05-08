from __future__ import annotations

import json
import os
import tempfile
from pathlib import Path

import numpy as np
from scripts.calibration_analysis import analyze_noise_floor, analyze_vortex_coupling


def test_analyze_noise_floor_stats() -> None:
    """Verify that the noise floor analyzer correctly computes energy statistics."""
    signatures = [
        {"energy": 0.1},
        {"energy": 0.2},
        {"energy": 0.3}
    ]
    stats = analyze_noise_floor(signatures)
    
    assert stats["n_samples"] == 3
    assert np.isclose(stats["mean_energy"], 0.2)
    assert stats["max_energy"] == 0.3
    assert stats["std_dev"] > 0


def test_analyze_vortex_coupling_correlation() -> None:
    """Verify that the vortex analyzer correctly identifies signal correlation."""
    events = [
        {"vortex_score": 0.0},
        {"vortex_score": 0.5},
        {"vortex_score": 1.0}
    ]
    signatures = [
        {"energy": 10.0},
        {"energy": 20.0},
        {"energy": 30.0}
    ]
    
    stats = analyze_vortex_coupling(events, signatures)
    # Perfect linear correlation
    assert np.isclose(stats["vortex_energy_correlation"], 1.0)
    assert stats["peak_energy"] == 30.0


def test_analyze_vortex_zero_variance_guard() -> None:
    """Ensure that the analyzer handles constant signals without crashing (no NaN)."""
    events = [{"vortex_score": 0.5}] * 5
    signatures = [{"energy": 10.0}] * 5
    
    stats = analyze_vortex_coupling(events, signatures)
    assert stats["vortex_energy_correlation"] == 0.0
    assert stats["peak_energy"] == 10.0


def test_analyze_empty_datasets() -> None:
    """Verify graceful handling of empty datasets."""
    assert analyze_noise_floor([])["n_samples"] == 0
    assert analyze_vortex_coupling([], [])["vortex_energy_correlation"] == 0.0
