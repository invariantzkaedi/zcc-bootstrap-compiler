"""
infinimemory.evermind - Evermind: Apex Persistent Consciousness & Sovereign Memory Kernel
========================================================================================
Unifies all 10 memory layers into an unbroken, non-decaying cognitive substrate:
  ENGRAM → RECALL → REMEMORY → METAMEMORY → HYPERMEMORY → OMNIMEMORY → INFINIMEMORY → ETERNAMIND ∞
"""

import threading
from typing import Dict, List, Optional, Any
from pathlib import Path

from infinimemory.models import (
    Engram, RetentionPolicy, LoreBlock, MetaMetrics, atomic_save_json
)
from infinimemory.engram_core import EngramCore
from infinimemory.retention_engine import RetentionEngine
from infinimemory.recall_engine import RecallEngine
from infinimemory.rememory import Rememory
from infinimemory.echo_core import EchoCore
from infinimemory.lore_vault import LoreVault
from infinimemory.meta_memory import MetaMemory
from infinimemory.hyper_recall import HyperRecall
from infinimemory.omni_recall import OmniRecall
from infinimemory.legacy_core import LegacyCore


class Evermind:
    """Subsystem 11: Evermind (persistent intelligence)
    The sovereign coordinator orchestrating all memory planes.
    """
    def __init__(
        self,
        storage_root: Optional[Path] = None,
        policy: Optional[RetentionPolicy] = None,
        redundant_overlap: bool = False,
    ):
        self._lock = threading.RLock()
        self.storage_root = storage_root or Path(".infinimemory")
        self.storage_root.mkdir(parents=True, exist_ok=True)

        self.engram_bank: Dict[str, Engram] = {}
        self.total_recalls: int = 0
        self.reconstruction_count: int = 0

        # Instantiate all 10 foundational subsystems
        self.engram_core = EngramCore(redundant_overlap=redundant_overlap)
        self.retention_engine = RetentionEngine(policy=policy)
        self.recall_engine = RecallEngine()
        self.rememory = Rememory()
        self.echo_core = EchoCore()
        self.lore_vault = LoreVault(storage_dir=self.storage_root / "lore")
        self.meta_memory = MetaMemory()
        self.hyper_recall = HyperRecall()
        self.legacy_core = LegacyCore(archive_dir=self.storage_root / "legacy")

        self.omni_recall = OmniRecall(
            recall_engine=self.recall_engine,
            hyper_recall=self.hyper_recall,
            lore_vault=self.lore_vault,
            echo_core=self.echo_core
        )
        self._load_engrams()

    def _save_engrams(self):
        with self._lock:
            bank_path = self.storage_root / "engrams.json"
            data = {eid: e.to_dict() for eid, e in self.engram_bank.items()}
            atomic_save_json(bank_path, data)

    def _load_engrams(self):
        import json
        with self._lock:
            bank_path = self.storage_root / "engrams.json"
            if bank_path.exists():
                try:
                    with open(bank_path, "r", encoding="utf-8") as f:
                        data = json.load(f)
                    for eid, d in data.items():
                        eng = Engram.from_dict(d)
                        self.engram_bank[eid] = eng
                        self.hyper_recall.index_engram(eng)
                except Exception:
                    pass

    def learn(
        self,
        key: str,
        content: str,
        tags: Optional[List[str]] = None,
        metadata: Optional[Dict[str, Any]] = None,
        resonance: float = 1.0,
        resonance_weight: Optional[float] = None
    ) -> Engram:
        """Encodes experience, stores in engram bank, and indexes for hyper-recall."""
        actual_resonance = resonance_weight if resonance_weight is not None else resonance
        with self._lock:
            engram = self.engram_core.encode(
                key=key,
                content=content,
                tags=tags,
                metadata=metadata,
                resonance_weight=actual_resonance,
                epoch=self.retention_engine.current_epoch
            )
            self.engram_bank[engram.id] = engram
            self.hyper_recall.index_engram(engram)
            self.engram_bank = self.retention_engine.enforce_capacity(self.engram_bank)
            self._save_engrams()
            return engram

    def recall(self, key: str) -> Optional[Engram]:
        """Retrieves and reinforces an engram by key."""
        with self._lock:
            engram = self.recall_engine.recall_by_key(self.engram_bank, key)
            if engram:
                self.total_recalls += 1
                self.retention_engine.reinforce(engram)
            return engram

    def search_omni(self, query: str, top_k: int = 5) -> Dict[str, Any]:
        """Executes federated search across all tiers."""
        with self._lock:
            self.total_recalls += 1
            return self.omni_recall.query(self.engram_bank, query, top_k=top_k)

    def seal_into_lore(self, title: str, engram_ids: List[str], author: str = "EVERMIND") -> LoreBlock:
        """Promotes active memories to permanent immutable Lore."""
        with self._lock:
            members = [self.engram_bank[eid] for eid in engram_ids if eid in self.engram_bank]
            return self.lore_vault.seal_lore_block(title, members, author=author)

    def heal(self, engram_id: str) -> bool:
        """Heals damaged engram using Rememory holographic parity shards."""
        with self._lock:
            if engram_id not in self.engram_bank:
                return False
            engram = self.engram_bank[engram_id]
            success = self.rememory.heal_engram(engram)
            if success:
                self.reconstruction_count += 1
            return success

    def tick_epoch(self, steps: int = 1) -> MetaMetrics:
        """Advances temporal epoch, decays inactive memories, captures retiring echoes,
        and records metamemory telemetry."""
        with self._lock:
            # Capture echoes of low-resonance memories before pruning
            for engram in self.engram_bank.values():
                if engram.resonance_weight < 0.2:
                    self.echo_core.capture_echo(engram, amplitude=engram.resonance_weight)

            self.engram_bank = self.retention_engine.advance_epoch(self.engram_bank, step_count=steps)
            self.echo_core.step_echo_decay()
            self.hyper_recall.rebuild_index(self.engram_bank)
            self._save_engrams()

            metrics = self.meta_memory.analyze(
                self.engram_bank,
                total_recalls=self.total_recalls,
                reconstruction_count=self.reconstruction_count
            )
            return metrics
