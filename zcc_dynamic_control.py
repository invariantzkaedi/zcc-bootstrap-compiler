#!/usr/bin/env python3
"""
=============================================================================
ZCC G332+ Dynamic Control Systems (Dynamic Control Layer)
=============================================================================
Provides high-performance subsystems for:
  1. SchmittMomentumGate: Stateful Schmitt Trigger to prevent false plateaus
  2. DynamicParameterRegistry: Thread-safe parameter hot-swapping configuration
  3. BisectCFGIndex: O(log N) binary search lookup for basic blocks
  4. MAdaptController: Dynamic precision matrix adaptation for interaction matrix M
=============================================================================
"""

import bisect
import threading
import json
import logging
from typing import Optional, Tuple, Dict, List
import numpy as np

# Configure logging
logging.basicConfig(level=logging.INFO, format='[SYSTEMS-DYNAMIC-CONTROL] %(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger("ZccDynamicControl")


class SchmittMomentumGate:
    """
    Stateful Schmitt Trigger (two-threshold hysteresis loop) to prevent
    premature stabilization (false plateaus) during deep code optimization cascades.
    """
    def __init__(self, window_size: int = 5, trigger_threshold: float = -5.0, release_threshold: float = -1.0):
        self.window_size = window_size
        self.trigger_threshold = trigger_threshold
        self.release_threshold = release_threshold
        self.history: List[float] = []
        self._override_state = False

    def record_step(self, delta_score: float):
        """Record the score improvement/delta of the current cycle."""
        self.history.append(delta_score)
        if len(self.history) > self.window_size:
            self.history.pop(0)

    @property
    def override_active(self) -> bool:
        """
        Stateful check. Returns True if in an active exploitation/downhill streak.
        Transitions to True when rolling average drops below trigger_threshold.
        Transitions to False only when rolling average climbs above release_threshold.
        """
        if len(self.history) < self.window_size:
            return False
        
        rolling_gradient = sum(self.history) / self.window_size
        
        # State transitions
        if not self._override_state and rolling_gradient < self.trigger_threshold:
            self._override_state = True
            logger.info(f"Hysteresis Gate LOCKED (trigger): rolling_grad={rolling_gradient:.4f} < {self.trigger_threshold}")
        elif self._override_state and rolling_gradient > self.release_threshold:
            self._override_state = False
            logger.info(f"Hysteresis Gate RELEASED (release): rolling_grad={rolling_gradient:.4f} > {self.release_threshold}")
            
        return self._override_state


class DynamicParameterRegistry:
    """
    Thread-safe parameter hot-swapping configuration registry. Allows out-of-band
    Cold Path solvers to inject new dynamic parameters dynamically at cycle boundaries.
    """
    def __init__(self, eta_c: float, T_eff: float, mutation_rate: float):
        self._lock = threading.Lock()
        self._eta_c = eta_c
        self._T_eff = T_eff
        self._mutation_rate = mutation_rate
        self._updated = False

    def update(self, eta_c: float, T_eff: float, mutation_rate: float):
        """Called by background thread to push new parameters."""
        with self._lock:
            self._eta_c = eta_c
            self._T_eff = T_eff
            self._mutation_rate = mutation_rate
            self._updated = True
            logger.info(f"Registry updated: η_c={eta_c:.4f}, T_eff={T_eff:.4f}, μ={mutation_rate:.4f}")

    def retrieve(self) -> Tuple[float, float, float]:
        """Called by main orchestrator to fetch and clear the update flag."""
        with self._lock:
            self._updated = False
            return self._eta_c, self._T_eff, self._mutation_rate

    @property
    def has_update(self) -> bool:
        """Check if background update is pending."""
        with self._lock:
            return self._updated


class BisectCFGIndex:
    """
    O(log N) binary search lookup wrapper for basic blocks.
    Translates an assembly line number into the containing basic block label.
    """
    def __init__(self, label_to_lines: Dict[str, Tuple[int, int]]):
        self.sorted_starts: List[int] = []
        self.interval_map: Dict[int, Tuple[str, int]] = {}

        for label, (start, end) in label_to_lines.items():
            self.sorted_starts.append(start)
            self.interval_map[start] = (label, end)

        self.sorted_starts.sort()

    def get_node(self, m_start: int) -> str:
        """Find the basic block label that spans line m_start."""
        if not self.sorted_starts:
            return "__entry__"

        idx = bisect.bisect_right(self.sorted_starts, m_start) - 1
        if idx >= 0:
            start_line = self.sorted_starts[idx]
            label, end_line = self.interval_map[start_line]
            if m_start <= end_line:
                return label
        return "__entry__"


class MAdaptController:
    """
    Precision Matrix Adaptation (M-Adapt) controller for the Hamiltonian interaction matrix M.
    Regularizes and adjusts metric weights dynamically to adapt to changing density states.
    """
    def __init__(self, initial_M: np.ndarray, alpha: float = 0.05, 
                 min_diagonal_weights: Optional[np.ndarray] = None):
        self.M = np.copy(initial_M)
        self.alpha = alpha
        self.n = initial_M.shape[0]
        
        # Keep track of trace baseline for normalization
        self.base_trace = float(np.trace(initial_M))
        
        # Enforce minimum diagonals to anchor core selection pressure
        if min_diagonal_weights is not None:
            self.min_diag = np.copy(min_diagonal_weights)
        else:
            self.min_diag = np.diag(initial_M) * 0.5  # default floor

    def adapt(self, history_Z: List[np.ndarray]) -> np.ndarray:
        """
        Updates M based on the precision matrix (inverse covariance) of metric history.
        
        Args:
            history_Z: List of numpy arrays representing Z-vectors in the cohort.
            
        Returns:
            The evolved interaction matrix M.
        """
        if len(history_Z) < 10:
            logger.debug("Insufficient history for M-Adapt. Keeping M static.")
            return self.M

        # Shape of Z must match matrix dimension n
        Z_data = np.stack(history_Z, axis=0)  # Shape (k, n)
        
        # Compute empirical covariance matrix
        cov = np.cov(Z_data, rowvar=False)
        
        # Add small ridge regularization to prevent singular inversion crashes (Tikhonov regularization)
        ridge = 1e-4 * np.eye(self.n)
        cov_regularized = cov + ridge
        
        try:
            # Precision matrix P = inv(Cov)
            P = np.linalg.inv(cov_regularized)
        except np.linalg.LinAlgError:
            # Fall back to pseudo-inverse if inversion still fails
            logger.warning("Covariance matrix inversion failed, falling back to pseudo-inverse.")
            P = np.linalg.pinv(cov_regularized)

        # 1. Adapt toward the target precision matrix
        # Scale target to have the same norm or trace scaling
        target_M = P * (self.base_trace / max(1e-12, np.trace(P)))
        
        updated_M = (1.0 - self.alpha) * self.M + self.alpha * target_M

        # 2. Enforce Symmetry Constraint
        updated_M = 0.5 * (updated_M + updated_M.T)

        # 3. Enforce Diagonal Conservation (Selection Pressure Anchor)
        current_diag = np.diag(updated_M)
        clamped_diag = np.maximum(current_diag, self.min_diag)
        np.fill_diagonal(updated_M, clamped_diag)

        # 4. Enforce Trace Normalization (Scale Invariance)
        curr_trace = np.trace(updated_M)
        if curr_trace > 1e-12:
            updated_M = updated_M * (self.base_trace / curr_trace)

        self.M = updated_M
        logger.info("Interaction Matrix M adapted successfully.")
        return self.M
