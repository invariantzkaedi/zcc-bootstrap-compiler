# -*- coding: utf-8 -*-
"""
=======================================================================================================
 🔱 ZKAEDI PRIME // CANONICAL FIELD ENGINE & LYAPUNOV SENTRY (FORMS 1 & 3) 🔱
=======================================================================================================
 Canonical Equation:
   H_t(x, y) = H_base(x, y)
               + eta * H_{t-1}(x, y) * sigmoid(gamma * H_{t-1}(x, y))
               + eps * N(0, 1 + beta * |H_{t-1}(x, y)|)

 Departure Scarring:
   H_base[x, y] += kick

 Lyapunov Sentry (FTLE):
   lambda = (1/T) * sum(ln(d_t / delta_0))
   lambda < 0 for eta < 1.05 (Subcritical contractive stability)
=======================================================================================================
"""

import math
from typing import Tuple, Optional
import numpy as np

class ZkaediCanonicalField:
    """
    Form 1 & Form 3 of Zkaedi Prime.
    Evolves the continuous macro energy landscape while tracking dynamical chaos bounds.
    """
    def __init__(
        self,
        size: int,
        eta: float = 0.40,
        gamma: float = 0.30,
        beta: float = 0.10,
        eps: float = 0.05,
        kick: float = 2.00,
        delta0: float = 1e-8,
        seed: int = 1337
    ):
        self.size = size
        self.eta = eta
        self.gamma = gamma
        self.beta = beta
        self.eps = eps
        self.kick = kick
        self.delta0 = delta0
        self.rng = np.random.default_rng(seed)

        self.H_base = np.zeros((size, size), dtype=np.float64)
        self.H_prev = np.zeros((size, size), dtype=np.float64)
        self.H_shadow = np.zeros((size, size), dtype=np.float64)
        self.step_count = 0
        self.lyapunov_sum = 0.0

    def initialize_from_matrix(self, W: np.ndarray):
        """Initializes the base field from an empirical coupling matrix W."""
        assert W.shape == (self.size, self.size), "Matrix shape mismatch"
        self.H_base = W.astype(np.float64).copy()
        self.H_prev = self.H_base.copy()
        self.H_shadow = self.H_prev + (self.delta0 / math.sqrt(self.size * self.size)) * self.rng.normal(size=self.H_prev.shape)
        self.step_count = 0
        self.lyapunov_sum = 0.0

    def inject_departure_scar(self, node: int, custom_kick: Optional[float] = None):
        """Applies a departure kick scar: H_base[node, :] += kick."""
        k = self.kick if custom_kick is None else custom_kick
        self.H_base[node, :] += k
        self.H_base[:, node] += k

    def step(self) -> Tuple[np.ndarray, float, float]:
        """
        Executes one canonical time-step.
        Returns: (H_t, trace_energy, ftle_lambda)
        """
        self.step_count += 1

        # 1. State-dependent non-linear recursive term
        sig = 1.0 / (1.0 + np.exp(-np.clip(self.gamma * self.H_prev, -30.0, 30.0)))
        rec = self.eta * self.H_prev * sig

        # 2. Heteroscedastic noise
        noise_std = np.sqrt(1.0 + self.beta * np.abs(self.H_prev))
        noise = self.eps * self.rng.normal(0.0, noise_std, size=self.H_prev.shape)

        # 3. Canonical field update
        H_t = self.H_base + rec + noise
        H_t = 0.5 * (H_t + H_t.T) # Symmetrize

        # 4. Shadow trajectory update for Benettin Lyapunov tracking
        sig_s = 1.0 / (1.0 + np.exp(-np.clip(self.gamma * self.H_shadow, -30.0, 30.0)))
        rec_s = self.eta * self.H_shadow * sig_s
        H_shadow_t = self.H_base + rec_s + noise
        H_shadow_t = 0.5 * (H_shadow_t + H_shadow_t.T)

        d_t = float(np.linalg.norm(H_shadow_t - H_t))
        if d_t > 1e-14:
            self.lyapunov_sum += math.log(d_t / self.delta0)
            self.H_shadow = H_t + (self.delta0 / d_t) * (H_shadow_t - H_t)

        self.H_prev = H_t
        trace_energy = float(np.trace(H_t @ H_t))
        current_ftle = self.lyapunov_sum / self.step_count

        return H_t, trace_energy, current_ftle
