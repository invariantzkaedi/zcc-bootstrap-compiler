"""
infinimemory.echo_core - EchoCore: Temporal Scars & Trace Recovery
==================================================================
Recovers faint experiential traces, historical echoes, and temporal scars
across system epochs, even after primary engrams undergo pruning.
"""

import time
from typing import Dict, List
from infinimemory.models import MemoryEcho, Engram, sha256_bytes


class EchoCore:
    """Subsystem 5: EchoCore (recover traces)
    Maintains low-amplitude experiential residues that survive full forgetting.
    """
    def __init__(self, max_echoes: int = 10_000, default_decay: float = 0.05):
        self.max_echoes = max_echoes
        self.default_decay = default_decay
        self.echoes: Dict[str, MemoryEcho] = {}

    def capture_echo(self, engram: Engram, amplitude: float = 1.0) -> MemoryEcho:
        """Captures an echo from an active or retiring engram."""
        echo_id = f"echo_{engram.id}_{int(time.time())}"
        digest = sha256_bytes(f"{engram.key}:{engram.content_hash}".encode("utf-8"))[:16]
        scar_sig = f"{engram.key}:{digest}"
        echo = MemoryEcho(
            echo_id=echo_id,
            source_engram_id=engram.id,
            timestamp=time.time(),
            amplitude=amplitude,
            scar_signature=scar_sig,
            decay_rate=self.default_decay
        )
        self.echoes[echo_id] = echo
        self._enforce_capacity()
        return echo

    def step_echo_decay(self, decay_multiplier: float = 1.0):
        """Gradually decays all echoes over time, pruning extinct signals."""
        extinct = []
        for eid, echo in self.echoes.items():
            echo.amplitude *= (1.0 - echo.decay_rate * decay_multiplier)
            if echo.amplitude < 1e-4:
                extinct.append(eid)
        for eid in extinct:
            del self.echoes[eid]

    def find_echoes_for_key(self, key_prefix: str) -> List[MemoryEcho]:
        """Finds active echoes matching a key signature prefix or hex digest prefix."""
        kp_low = key_prefix.lower()
        matches = [
            echo for echo in self.echoes.values()
            if echo.scar_signature.startswith(key_prefix)
            or echo.scar_signature.lower().startswith(kp_low)
            or echo.scar_signature.rsplit(":", 1)[-1].startswith(key_prefix)
        ]
        matches.sort(key=lambda e: e.amplitude, reverse=True)
        return matches

    def _enforce_capacity(self):
        if len(self.echoes) <= self.max_echoes:
            return
        sorted_echoes = sorted(self.echoes.values(), key=lambda e: e.amplitude)
        excess = len(self.echoes) - self.max_echoes
        for echo in sorted_echoes[:excess]:
            del self.echoes[echo.echo_id]
