# -*- coding: utf-8 -*-
"""
=======================================================================================================
 🔱 ZKAEDI PRIME // QUANTUM UNITARY & DYNAMIC RECURSIVE REVEAL (FORM 2) 🔱
=======================================================================================================
 Continuous-Time Quantum Walk (CTQW):
   |psi(t)> = exp(-i * L * t / hbar) |psi(0)>

 Non-Linear Gross-Pitaevskii Self-Interaction:
   i * d|psi>/dt = [ L + kappa * diag(|psi|^2) ] |psi>

 Topological Invariants:
   - Spectral Mass Gap: Delta E = E_1 - E_0
   - Participation Ratio: PR = 1 / sum(P_k^2)
   - Von Neumann Entropy: S = -sum(P_k * ln(P_k))
=======================================================================================================
"""

import math
from typing import Dict, List, Tuple, Any
import numpy as np
import scipy.linalg

class QuantumWalkEngine:
    """
    Continuous-Time Quantum Walk Engine on Graph Laplacian.
    Form 2 of Zkaedi Prime.
    """
    def __init__(self, adjacency_matrix: np.ndarray):
        self.W = adjacency_matrix.astype(np.float64)
        self.size = self.W.shape[0]
        self.D = np.diag(np.sum(np.abs(self.W), axis=1))
        self.L = self.D - self.W

        evals, evecs = scipy.linalg.eigh(self.L)
        self.eigenvalues = evals
        self.eigenvectors = evecs
        self.mass_gap = float(evals[1] - evals[0])

    def propagate(self, psi_0: np.ndarray, t: float) -> Tuple[np.ndarray, np.ndarray, float]:
        """
        Propagates initial wavepacket psi_0 through time t.
        Returns: (psi_t, probabilities, entropy)
        """
        U_t = scipy.linalg.expm(-1j * self.L * t)
        psi_t = U_t @ psi_0
        probs = np.abs(psi_t) ** 2
        probs /= np.sum(probs)
        entropy = float(-np.sum([p * math.log(max(p, 1e-14)) for p in probs]))
        return psi_t, probs, entropy

class EmergentRevealEngine:
    """
    Dynamic Recursive Emergent Quantum Walk Engine with Non-Linear Gross-Pitaevskii
    self-interaction and recursive field feedback.
    """
    def __init__(
        self,
        base_coupling: np.ndarray,
        kappa: float = 0.85,
        eta: float = 0.40,
        gamma: float = 0.30,
        beta: float = 0.10,
        eps: float = 0.05,
        kick: float = 1.50
    ):
        self.size = base_coupling.shape[0]
        self.H_m = base_coupling.astype(np.float64).copy()
        self.kappa = kappa
        self.eta = eta
        self.gamma = gamma
        self.beta = beta
        self.eps = eps
        self.kick = kick
        self.rng = np.random.default_rng(42)

    def execute_recursive_cycle(
        self,
        psi_in: np.ndarray,
        tau: float = 1.25,
        sub_steps: int = 50
    ) -> Dict[str, Any]:
        """Executes a single non-linear recursive feedback cycle."""
        D_m = np.diag(np.sum(np.abs(self.H_m), axis=1))
        L_m = D_m - self.H_m

        dt = tau / sub_steps
        psi_curr = psi_in.copy()

        for _ in range(sub_steps):
            def d_psi_dt(psi):
                density = np.abs(psi) ** 2
                H_eff = L_m + np.diag(self.kappa * density)
                return -1j * (H_eff @ psi)

            k1 = d_psi_dt(psi_curr)
            k2 = d_psi_dt(psi_curr + 0.5 * dt * k1)
            k3 = d_psi_dt(psi_curr + 0.5 * dt * k2)
            k4 = d_psi_dt(psi_curr + dt * k3)

            psi_curr += (dt / 6.0) * (k1 + 2 * k2 + 2 * k3 + k4)
            psi_curr /= np.linalg.norm(psi_curr)

        psi_out = psi_curr
        probs = np.abs(psi_out) ** 2
        probs /= np.sum(probs)

        pr = float(1.0 / np.sum(probs ** 2))
        entropy = float(-np.sum([p * math.log(max(p, 1e-14)) for p in probs]))
        fidelity = float(np.abs(np.vdot(psi_in, psi_out)) ** 2)

        # Recursive field feedback
        rho_m = np.outer(psi_out, np.conj(psi_out))
        sig = 1.0 / (1.0 + np.exp(-np.clip(self.gamma * self.H_m, -20.0, 20.0)))
        rec = self.eta * self.H_m * sig
        scar = self.kick * np.real(rho_m)
        noise = self.eps * self.rng.normal(0.0, np.sqrt(1.0 + self.beta * np.abs(self.H_m)), size=(self.size, self.size))

        H_next = self.H_m + rec + scar + noise
        self.H_m = 0.5 * (H_next + H_next.T)

        evals = scipy.linalg.eigvalsh(self.H_m)
        gap = float(evals[1] - evals[0])

        return {
            "psi": psi_out,
            "probabilities": probs,
            "participation_ratio": pr,
            "entropy": entropy,
            "fidelity": fidelity,
            "spectral_mass_gap": gap,
            "dominant_node": int(np.argmax(probs))
        }
