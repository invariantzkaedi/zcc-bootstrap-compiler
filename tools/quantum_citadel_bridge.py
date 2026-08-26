#!/usr/bin/env python3
"""
tools/quantum_citadel_bridge.py
================================================================================
ZKAEDI QUANTUM TRINITY x ZERO-BURP VISUAL CITADEL TELEMETRY BRIDGE
================================================================================
Streams hardware-vectorized Quantum Walk (DTQW) states, 2D Hamiltonian fields,
and 296-byte cryptographic public commitments to the ZERO-BURP Visual Citadel.
================================================================================
"""

import os
import sys
import json
import time
import argparse
import numpy as np

sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))
from tools.pyavxzkd import PyAvxzkdField, PyAvxzkdQuantumWalk

def generate_telemetry_frame(step_count: int = 100, grid_dim: int = 32) -> dict:
    """Generates a synchronized quantum + classical Hamiltonian telemetry frame."""
    # 1. Quantum DTQW State
    qw = PyAvxzkdQuantumWalk()
    qw.step(steps=step_count)
    probs = qw.get_probabilities()
    phases = qw.get_phases()
    s_q0 = qw.get_entanglement_entropy()
    commitment_bytes, digest_hex = qw.get_public_commitment()

    # 2. 2D Hamiltonian Potential & Topological Curvature
    field = PyAvxzkdField(grid_dim, grid_dim)
    base_data = np.zeros((grid_dim, grid_dim), dtype=np.float32)
    # Modulate base potential with quantum Born probabilities
    for y in range(grid_dim):
        for x in range(grid_dim):
            q_idx = (x + y) % 16
            base_data[y, x] = float(probs[q_idx] * 20.0 - 5.0)
    field.init_field(base_data)
    field.step(eta=0.4, gamma=0.3, k_steps=20)
    field.compute_topology()

    curr_field = field.get_current()
    curvature = field.get_curvature()
    audit = field.audit(eta=0.4, gamma=0.3)

    return {
        "timestamp_ns": int(time.time() * 1e9),
        "quantum_dtqw": {
            "steps": step_count,
            "spatial_nodes": 16,
            "born_probabilities": [round(float(p), 6) for p in probs],
            "phase_angles": [round(float(phi), 6) for phi in phases],
            "entanglement_entropy_s_q0": round(float(s_q0), 6),
            "hilbert_norm": round(float(np.sum(probs)), 8),
            "commitment_sha256": digest_hex,
            "commitment_bytes_len": len(commitment_bytes)
        },
        "hamiltonian_field": {
            "grid_dim": grid_dim,
            "measured_gain": round(float(audit["measured_gain"]), 5),
            "floor_drift": round(float(audit["floor_drift"]), 8),
            "state_digest": audit["state_digest"],
            "pass_all_invariants": audit["pass_all_invariants"],
            "energy_sample": [round(float(curr_field[y, x]), 4) for y in range(0, grid_dim, 4) for x in range(0, grid_dim, 4)],
            "curvature_sample": [round(float(curvature[y, x]), 4) for y in range(0, grid_dim, 4) for x in range(0, grid_dim, 4)]
        }
    }

def main():
    parser = argparse.ArgumentParser(description="ZKAEDI Quantum Citadel Telemetry Bridge")
    parser.add_argument("--once", action="store_true", help="Generate single telemetry frame and write to artifacts")
    parser.add_argument("--out", default="artifacts/quantum_citadel_telemetry.json", help="Output path for frame")
    parser.add_argument("--steps", type=int, default=100, help="Quantum walk steps")
    args = parser.parse_args()

    os.makedirs(os.path.dirname(args.out), exist_ok=True)
    frame = generate_telemetry_frame(step_count=args.steps)

    with open(args.out, "w") as f:
        json.dump(frame, f, indent=2)

    print("=========================================================================================")
    print("      ZKAEDI QUANTUM CITADEL TELEMETRY BRIDGE — TELEMETRY FRAME GENERATED               ")
    print("=========================================================================================")
    print(f" Output File       : {args.out}")
    print(f" Quantum Walk Norm : {frame['quantum_dtqw']['hilbert_norm']:.8f}")
    print(f" Entanglement S(q0): {frame['quantum_dtqw']['entanglement_entropy_s_q0']:.6f} bits")
    print(f" Commitment Digest : {frame['quantum_dtqw']['commitment_sha256']}")
    print(f" Field State Digest: {frame['hamiltonian_field']['state_digest']}")
    print(f" Invariant Status  : {'ALL PASS' if frame['hamiltonian_field']['pass_all_invariants'] else 'FAIL'}")
    print("=========================================================================================")

if __name__ == "__main__":
    main()
