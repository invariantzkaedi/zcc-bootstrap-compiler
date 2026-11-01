#!/usr/bin/env python3
# -*- coding: utf-8 -*-
r"""
=======================================================================================================
 🔱 ZKAEDI PRIME // AUTONOMOUS CODE SCAR-MEMORY & CRASH IMMUNIZER (IDEA 6) 🔱
=======================================================================================================
 Fuses:
   1. omnicatch.py                — Tier 1-3 Runtime Fault Interception & Silent No-Op Traps
   2. zkaedi_prime/canonical_field.py — Two-Regime Hamiltonian Scar Field (H_base[node] += kick)
   3. infinimemory/evermind.py    — Holographic Parity-Sharded Engram Bank, EchoCore & LoreVault
   4. float_guard.py              — IEEE-754 ULP Distance & Compensated Summation Verification

 Governing Two-Regime Invariant:
   One equation, two regimes: eta shapes fields; scars + eps navigate.
   H_t(x,y) = H_base(x,y)
            + eta * H_{t-1}(x,y) * sigmoid(gamma * H_{t-1}(x,y))
            + eps * N(0, 1 + beta * |H_{t-1}(x,y)|)
=======================================================================================================
"""

from __future__ import annotations

import argparse
import ast
import copy
import hashlib
import inspect
import json
import math
import statistics
import sys
import tempfile
import textwrap
import threading
import time
from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import Any, Callable, Dict, List, Optional, Sequence, Tuple

if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8")
if hasattr(sys.stderr, "reconfigure"):
    sys.stderr.reconfigure(encoding="utf-8")

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
if str(REPO_ROOT) in sys.path:
    sys.path.remove(str(REPO_ROOT))
sys.path.insert(0, str(REPO_ROOT))
if "zkaedi_prime" in sys.modules and not hasattr(sys.modules["zkaedi_prime"], "__path__"):
    del sys.modules["zkaedi_prime"]

import numpy as np
import omnicatch
from float_guard import CancellationError, exact_sum, guard_sum, neumaier_sum, ulp_diff
from infinimemory import Evermind, LoreBlock, RememoryReconstructionError, sha256_bytes
from zkaedi_prime.canonical_field import ZkaediCanonicalField


# ======================================================================================
# 1. AST STRUCTURAL FINGERPRINTING (Alpha-Renaming Invariant)
# ======================================================================================

class _ASTAlphaNormalizer(ast.NodeTransformer):
    """Normalizes local variable and argument identifiers so structurally identical
    functions produce the same canonical AST signature regardless of variable naming."""

    def __init__(self) -> None:
        super().__init__()
        self._var_map: Dict[str, str] = {}
        self._counter: int = 0

    def _map_name(self, name: str) -> str:
        if name in {"True", "False", "None", "abs", "min", "max", "sum", "len", "float", "int", "dict", "list"}:
            return name
        if name not in self._var_map:
            self._var_map[name] = f"v{self._counter}"
            self._counter += 1
        return self._var_map[name]

    def visit_FunctionDef(self, node: ast.FunctionDef) -> ast.AST:
        node.name = "fn_norm"
        node.decorator_list = []
        return self.generic_visit(node)

    def visit_arg(self, node: ast.arg) -> ast.AST:
        node.arg = self._map_name(node.arg)
        node.annotation = None
        return node

    def visit_Name(self, node: ast.Name) -> ast.AST:
        if isinstance(node.ctx, (ast.Load, ast.Store, ast.Del)):
            node.id = self._map_name(node.id)
        return node


def compute_ast_signature(fn_or_source: Any) -> Tuple[str, str]:
    """Returns (ast_dump_normalized, sha256_hex_16) for a callable or source string."""
    try:
        src = fn_or_source if isinstance(fn_or_source, str) else textwrap.dedent(inspect.getsource(fn_or_source))
        tree = ast.parse(src)
        norm_tree = _ASTAlphaNormalizer().visit(tree)
        ast.fix_missing_locations(norm_tree)
        dumped = ast.dump(norm_tree, annotate_fields=True, include_attributes=False)
    except Exception:
        dumped = repr(fn_or_source)
    digest = hashlib.sha256(dumped.encode("utf-8")).hexdigest()[:16]
    return dumped, digest


# ======================================================================================
# 2. VACCINATION PATCH & IMMUNIZATION RECEIPT SCHEMAS
# ======================================================================================

@dataclass
class VaccinationPatch:
    """Verified self-healing patch stored inside an InfiniMemory Holographic Engram."""
    fault_key: str
    exc_type_name: str
    fault_description: str
    broken_ast_hash: str
    healed_fn_name: str
    healed_source: str
    test_vectors: List[Dict[str, Any]]
    source_sha256: str = ""

    def __post_init__(self) -> None:
        if not self.source_sha256:
            self.source_sha256 = sha256_bytes(self.healed_source.encode("utf-8"))

    def to_json(self) -> str:
        return json.dumps(asdict(self), sort_keys=True, separators=(",", ":"))

    @classmethod
    def from_json(cls, raw: str) -> "VaccinationPatch":
        data = json.loads(raw)
        return cls(**data)


@dataclass
class ImmunizationReceipt:
    """Cryptographic receipt emitted whenever a runtime crash is intercepted and healed."""
    fault_key: str
    exc_type: str
    exc_message: str
    engram_id: str
    recalled_via: str
    shard_healed: bool
    vectors_verified: int
    scar_energy_after: float
    ftle_lambda: float
    latency_ms: float
    patch_sha256: str
    receipt_sha256: str = ""

    def __post_init__(self) -> None:
        if not self.receipt_sha256:
            canonical = json.dumps(
                {k: v for k, v in asdict(self).items() if k != "receipt_sha256"},
                sort_keys=True,
                separators=(",", ":"),
            ).encode("utf-8")
            self.receipt_sha256 = hashlib.sha256(canonical).hexdigest()


# ======================================================================================
# 3. CRASH IMMUNIZER ENGINE
# ======================================================================================

class CrashImmunizer:
    """
    Autonomous Code Scar-Memory & Crash Immunizer.
    Connects omnicatch fault traps, ZkaediCanonicalField Two-Regime departure scars,
    and InfiniMemory Evermind holographic parity-sharded recall.
    """

    SAFE_BUILTINS: Dict[str, Any] = {
        "abs": abs,
        "all": all,
        "any": any,
        "bool": bool,
        "dict": dict,
        "enumerate": enumerate,
        "Exception": Exception,
        "float": float,
        "int": int,
        "isinstance": isinstance,
        "KeyError": KeyError,
        "len": len,
        "list": list,
        "max": max,
        "min": min,
        "range": range,
        "round": round,
        "set": set,
        "sorted": sorted,
        "str": str,
        "sum": sum,
        "tuple": tuple,
        "ValueError": ValueError,
        "ZeroDivisionError": ZeroDivisionError,
        "zip": zip,
        "math": math,
        "exact_sum": exact_sum,
        "neumaier_sum": neumaier_sum,
        "ulp_diff": ulp_diff,
    }

    def __init__(
        self,
        storage_root: Optional[Path] = None,
        field_size: int = 16,
        eta: float = 0.40,
        gamma: float = 0.30,
        beta: float = 0.10,
        eps: float = 0.05,
        kick: float = 2.00,
        seed: int = 1337,
        redundant_overlap: bool = True,
        enable_recall: bool = True,
    ) -> None:
        self.storage_root = storage_root or Path(tempfile.mkdtemp(prefix="zkaedi_immunizer_"))
        self.evermind = Evermind(storage_root=self.storage_root, redundant_overlap=redundant_overlap)
        self.field = ZkaediCanonicalField(
            size=field_size,
            eta=eta,
            gamma=gamma,
            beta=beta,
            eps=eps,
            kick=kick,
            seed=seed,
        )
        self.field.initialize_from_matrix(np.zeros((field_size, field_size), dtype=np.float64))
        self.enable_recall = enable_recall
        self._lock = threading.RLock()
        self._node_map: Dict[str, int] = {}
        self._compiled_cache: Dict[str, Callable[..., Any]] = {}
        self.receipts: List[ImmunizationReceipt] = []
        self.unhealed_crashes: int = 0
        self.rejected_sham_patches: int = 0

    FORBIDDEN_AST_CALLS = {
        "eval",
        "exec",
        "compile",
        "open",
        "getattr",
        "setattr",
        "delattr",
        "vars",
        "dir",
        "globals",
        "locals",
        "type",
        "input",
        "breakpoint",
        "help",
        "__import__",
    }

    @classmethod
    def verify_ast_safety(cls, source: str) -> Tuple[bool, str]:
        """Static AST security gate: blocks imports, dunder attribute/name access,
        unbounded while loops, exception swallowing (Try), and reflection calls."""
        if len(source) > 16_384:
            return False, "source exceeds 16KB safety bound"
        try:
            tree = ast.parse(source)
        except SyntaxError as exc:
            return False, f"syntax error: {exc}"

        fn_defs = [n for n in tree.body if isinstance(n, ast.FunctionDef)]
        if len(fn_defs) != 1 or len(tree.body) != 1:
            return False, "vaccine source must consist of exactly one top-level def"

        for node in ast.walk(tree):
            if isinstance(
                node,
                (
                    ast.Import,
                    ast.ImportFrom,
                    ast.While,
                    ast.Try,
                    ast.Global,
                    ast.Nonlocal,
                    ast.AsyncFunctionDef,
                    ast.ClassDef,
                    ast.Delete,
                    ast.Yield,
                    ast.YieldFrom,
                    ast.Await,
                ),
            ):
                return False, f"forbidden AST construct: {type(node).__name__}"
            if isinstance(node, ast.Attribute):
                if node.attr.startswith("_"):
                    return False, f"forbidden private/dunder attribute access: {node.attr}"
            if isinstance(node, ast.Name):
                if node.id.startswith("_") and node.id != "_":
                    return False, f"forbidden private/dunder identifier: {node.id}"
                if node.id in cls.FORBIDDEN_AST_CALLS:
                    return False, f"forbidden builtin reference: {node.id}"
            if isinstance(node, ast.Call) and isinstance(node.func, ast.Name):
                if node.func.id in cls.FORBIDDEN_AST_CALLS:
                    return False, f"forbidden call: {node.func.id}"
        return True, "ok"

    def _node_for_key(self, fault_key: str) -> int:
        if fault_key not in self._node_map:
            idx = len(self._node_map) % self.field.size
            self._node_map[fault_key] = idx
        return self._node_map[fault_key]

    def register_vaccine(
        self,
        fault_key: str,
        exc_type: type[BaseException],
        fault_description: str,
        broken_fn: Any,
        healed_source: str,
        healed_fn_name: str,
        test_vectors: List[Dict[str, Any]],
        tags: Optional[List[str]] = None,
    ) -> str:
        """Compiles, verifies, and stores a VaccinationPatch in Evermind with holographic parity shards."""
        _, broken_hash = compute_ast_signature(broken_fn)
        patch = VaccinationPatch(
            fault_key=fault_key,
            exc_type_name=exc_type.__name__,
            fault_description=fault_description,
            broken_ast_hash=broken_hash,
            healed_fn_name=healed_fn_name,
            healed_source=textwrap.dedent(healed_source).strip(),
            test_vectors=copy.deepcopy(test_vectors),
        )
        # Verify the vaccine passes AST safety and its deterministic test vectors before storing
        fn = self._compile_and_verify_patch(patch)
        if fn is None:
            raise ValueError(f"Refusing to register invalid or unsafe vaccine for {fault_key}")

        with self._lock:
            engram = self.evermind.learn(
                key=fault_key,
                content=patch.to_json(),
                tags=(tags or []) + ["vaccine", exc_type.__name__.lower(), broken_hash],
                metadata={
                    "exc_type": exc_type.__name__,
                    "broken_ast_hash": broken_hash,
                    "source_sha256": patch.source_sha256,
                },
                resonance=2.0,
            )
            # Capture a persistent EchoCore scar trace so even after epoch pruning the scar signature survives
            self.evermind.echo_core.capture_echo(engram, amplitude=1.0)
            return engram.id

    def seal_vaccines_into_lore(self, title: str = "Sovereign Crash Immunization Lore") -> LoreBlock:
        with self._lock:
            eids = list(self.evermind.engram_bank.keys())
            return self.evermind.seal_into_lore(title=title, engram_ids=eids, author="CRASH_IMMUNIZER")

    def _compile_and_verify_patch(
        self,
        patch: VaccinationPatch,
        expected_exc_type_name: Optional[str] = None,
    ) -> Optional[Callable[..., Any]]:
        """Compiles healed_source in a restricted namespace and runs all deterministic test_vectors."""
        if expected_exc_type_name is not None and patch.exc_type_name != expected_exc_type_name:
            self.rejected_sham_patches += 1
            return None
        actual_sha = sha256_bytes(patch.healed_source.encode("utf-8"))
        if actual_sha != patch.source_sha256:
            self.rejected_sham_patches += 1
            return None

        safe_ok, _ = self.verify_ast_safety(patch.healed_source)
        if not safe_ok:
            self.rejected_sham_patches += 1
            return None

        env: Dict[str, Any] = {"__builtins__": self.SAFE_BUILTINS}
        env.update(self.SAFE_BUILTINS)
        try:
            tree = ast.parse(patch.healed_source)
            code_obj = compile(tree, filename=f"<vaccine:{patch.fault_key}>", mode="exec")
            exec(code_obj, env, env)
            fn = env.get(patch.healed_fn_name)
            if not callable(fn):
                self.rejected_sham_patches += 1
                return None

            # Verify every deterministic test vector using deep-copied arguments
            if not patch.test_vectors:
                self.rejected_sham_patches += 1
                return None
            for vec in patch.test_vectors:
                args_copy = copy.deepcopy(vec.get("args", []))
                kwargs_copy = copy.deepcopy(vec.get("kwargs", {}))
                expected = vec.get("expected")
                out = fn(*args_copy, **kwargs_copy)
                if isinstance(expected, float) and isinstance(out, (int, float)):
                    if not math.isfinite(float(out)) or ulp_diff(float(out), expected) > vec.get("max_ulps", 2):
                        self.rejected_sham_patches += 1
                        return None
                elif out != expected:
                    self.rejected_sham_patches += 1
                    return None
        except Exception:
            self.rejected_sham_patches += 1
            return None

        return fn

    def _recall_and_heal_engram(
        self,
        fault_key: str,
        exc: BaseException,
        broken_ast_hash: str,
    ) -> Tuple[Optional[VaccinationPatch], Optional[str], str, bool]:
        """Queries Evermind across exact key, semantic/token search, and EchoCore traces.
        If the recalled engram is corrupted in memory, heals it via Rememory parity shards.
        Enforces strict structural identity (fault_key or broken_ast_hash match)."""
        if not self.enable_recall:
            return None, None, "disabled", False

        def _matches_identity(eng: Any) -> bool:
            return (
                eng.key == fault_key
                or eng.metadata.get("broken_ast_hash") == broken_ast_hash
            )

        engram = self.evermind.recall(fault_key)
        recalled_via = "exact_key"

        if engram is None:
            # Fallback 1: Tag search requiring BOTH broken_ast_hash AND exception type
            tag_hits = self.evermind.recall_engine.recall_by_tags(
                self.evermind.engram_bank,
                [broken_ast_hash, type(exc).__name__.lower()],
                match_all=True,
            )
            if tag_hits:
                engram = tag_hits[0]
                recalled_via = "ast_tag_recall"

        if engram is None:
            # Fallback 2: Semantic + Fast Token search filtered by structural identity
            query = f"{fault_key} {type(exc).__name__} {exc}"
            sem_hits = [
                pair[0]
                for pair in self.evermind.recall_engine.recall_semantic(
                    self.evermind.engram_bank, query, top_k=5
                )
                if _matches_identity(pair[0])
            ]
            if sem_hits:
                engram = sem_hits[0]
                recalled_via = "semantic_recall"
            else:
                tok_hits = [
                    pair[0]
                    for pair in self.evermind.hyper_recall.fast_token_search(
                        self.evermind.engram_bank, query, limit=5
                    )
                    if _matches_identity(pair[0])
                ]
                if tok_hits:
                    engram = tok_hits[0]
                    recalled_via = "hyper_token_recall"

        if engram is None:
            # Fallback 3: EchoCore scar trace recovery filtered by structural identity
            echo_hits = self.evermind.echo_core.find_echoes_for_key(fault_key)
            for eh in echo_hits:
                src_id = eh.source_engram_id
                cand = self.evermind.engram_bank.get(src_id)
                if cand is not None and _matches_identity(cand):
                    engram = cand
                    recalled_via = "echo_scar_trace"
                    break

        if engram is None:
            return None, None, "miss", False

        shard_healed = False
        if not self.evermind.rememory.verify_integrity(engram):
            healed_ok = self.evermind.heal(engram.id)
            if not healed_ok:
                return None, engram.id, recalled_via, False
            shard_healed = True

        try:
            patch = VaccinationPatch.from_json(engram.content)
        except Exception:
            self.rejected_sham_patches += 1
            return None, engram.id, recalled_via, shard_healed

        if patch.fault_key != fault_key and patch.broken_ast_hash != broken_ast_hash:
            self.rejected_sham_patches += 1
            return None, engram.id, "identity_mismatch", shard_healed

        return patch, engram.id, recalled_via, shard_healed

    def execute_immunized(
        self,
        fault_key: str,
        fn: Callable[..., Any],
        *args: Any,
        **kwargs: Any,
    ) -> Tuple[Any, Optional[ImmunizationReceipt]]:
        """Executes fn(*args, **kwargs). On failure, deposits a Hamiltonian departure scar,
        recalls and heals the corresponding VaccinationPatch from Evermind, verifies its
        test vectors, and executes the healed implementation."""
        with self._lock:
            node_idx = self._node_for_key(fault_key)
            cached_fn = (
                self._compiled_cache.get(fault_key)
                if self.field.H_base[node_idx, node_idx] > 0.0
                else None
            )

        if cached_fn is not None:
            t0 = time.perf_counter()
            res = cached_fn(*args, **kwargs)
            dt_ms = (time.perf_counter() - t0) * 1000.0
            with self._lock:
                receipt = ImmunizationReceipt(
                    fault_key=fault_key,
                    exc_type="CachedScarBypass",
                    exc_message="Bypassed known scarred fault node via cached vaccine",
                    engram_id="cached",
                    recalled_via="hot_scar_cache",
                    shard_healed=False,
                    vectors_verified=1,
                    scar_energy_after=float(self.field.H_base[node_idx, node_idx]),
                    ftle_lambda=float(self.field.lyapunov_sum / max(1, self.field.step_count)),
                    latency_ms=round(dt_ms, 4),
                    patch_sha256="cached",
                )
                self.receipts.append(receipt)
            return res, receipt

        t0 = time.perf_counter()
        try:
            return fn(*args, **kwargs), None
        except Exception as exc:
            with self._lock:
                # 1. Deposit departure scar into ZkaediCanonicalField (Two-Regime Canon: H_base += kick)
                self.field.inject_departure_scar(node_idx)
                _, _, ftle_lambda = self.field.step()
                scar_energy = float(self.field.H_base[node_idx, node_idx])

                # 2. Emit structured fault event to omnicatch
                omnicatch._emit(
                    omnicatch.OmniEvent(
                        kind="crash_immunizer_trap",
                        severity="WARN",
                        message=f"{type(exc).__name__}: {exc}",
                        where=fault_key,
                        data={"scar_energy": scar_energy, "ftle_lambda": ftle_lambda},
                    )
                )

                # 3. Recall & holographically heal vaccine from Evermind
                _, broken_hash = compute_ast_signature(fn)
                patch, engram_id, recalled_via, shard_healed = self._recall_and_heal_engram(
                    fault_key, exc, broken_hash
                )
                if patch is None or engram_id is None:
                    self.unhealed_crashes += 1
                    raise

                healed_fn = self._compile_and_verify_patch(patch, expected_exc_type_name=type(exc).__name__)
                if healed_fn is None:
                    self.unhealed_crashes += 1
                    raise

                # Cache verified callable for subsequent calls if scar energy is active
                if self.field.kick > 0.0:
                    self._compiled_cache[fault_key] = healed_fn

            healed_result = healed_fn(*args, **kwargs)
            dt_ms = (time.perf_counter() - t0) * 1000.0
            with self._lock:
                receipt = ImmunizationReceipt(
                    fault_key=fault_key,
                    exc_type=type(exc).__name__,
                    exc_message=str(exc),
                    engram_id=engram_id,
                    recalled_via=recalled_via,
                    shard_healed=shard_healed,
                    vectors_verified=len(patch.test_vectors),
                    scar_energy_after=round(scar_energy, 4),
                    ftle_lambda=round(ftle_lambda, 6),
                    latency_ms=round(dt_ms, 4),
                    patch_sha256=patch.source_sha256,
                )
                self.receipts.append(receipt)
            return healed_result, receipt

    def immunize(self, fault_key: str) -> Callable[[Callable[..., Any]], Callable[..., Any]]:
        """Decorator wrapping a function with autonomous scar-memory crash immunization."""
        def decorator(fn: Callable[..., Any]) -> Callable[..., Any]:
            def wrapper(*args: Any, **kwargs: Any) -> Any:
                res, _ = self.execute_immunized(fault_key, fn, *args, **kwargs)
                return res
            wrapper.__name__ = fn.__name__
            wrapper.__qualname__ = fn.__qualname__
            return wrapper
        return decorator


# ======================================================================================
# 4. BENCHMARK FAULT SUITE & VACCINE CORPUS (5 Fault Classes)
# ======================================================================================

def build_standard_fault_suite() -> List[Dict[str, Any]]:
    """Returns 5 canonical runtime bugs and their verified VaccinationPatches."""

    # Fault 1: Catastrophic IEEE-754 floating-point cancellation caught by float_guard
    def broken_ill_conditioned_sum(xs: Sequence[float]) -> float:
        return guard_sum(xs, max_ulps=10)

    healed_sum_src = """
def healed_ill_conditioned_sum(xs):
    return exact_sum(xs)
"""

    # Fault 2: ZeroDivisionError on financial ratio when denominator is 0.0
    def broken_margin_ratio(numerator: float, denominator: float) -> float:
        return (numerator / denominator) * 100.0

    healed_margin_src = """
def healed_margin_ratio(numerator, denominator):
    if denominator == 0 or abs(denominator) < 1e-12:
        return 0.0
    return (numerator / denominator) * 100.0
"""

    # Fault 3: KeyError on missing nested telemetry dictionary key
    def broken_extract_customer_count(payload: Dict[str, Any]) -> int:
        return int(payload["metrics"]["enterprise_customers"])

    healed_extract_src = """
def healed_extract_customer_count(payload):
    metrics = payload.get("metrics") if isinstance(payload, dict) else {}
    if not isinstance(metrics, dict):
        return 0
    return int(metrics.get("enterprise_customers", metrics.get("customers", 0)))
"""

    # Fault 4: ValueError on currency/comma-formatted numeric string parsing
    def broken_parse_currency(raw: str) -> float:
        return float(raw)

    healed_parse_src = """
def healed_parse_currency(raw):
    cleaned = str(raw).replace("$", "").replace(",", "").replace("%", "").strip()
    return float(cleaned) if cleaned else 0.0
"""

    # Fault 5: Silent no-op mutation caught by omnicatch.no_silent_noop
    def broken_normalize_weights(weights: List[float]) -> List[float]:
        with omnicatch.no_silent_noop("normalize_weights", lambda: tuple(weights)):
            # Bug: computes normalized list in local variable without mutating in-place
            total = sum(weights) or 1.0
            _ = [w / total for w in weights]
        return weights

    healed_normalize_src = """
def healed_normalize_weights(weights):
    total = exact_sum(weights)
    if abs(total) < 1e-15:
        return [0.0 for _ in weights]
    for i in range(len(weights)):
        weights[i] = weights[i] / total
    return weights
"""

    return [
        {
            "fault_key": "fault:float_cancellation:ill_conditioned_sum",
            "exc_type": CancellationError,
            "description": "Catastrophic IEEE-754 mantissa cancellation in naive float summation",
            "broken_fn": broken_ill_conditioned_sum,
            "healed_fn_name": "healed_ill_conditioned_sum",
            "healed_source": healed_sum_src,
            "test_vectors": [
                {
                    "args": [[1e16, 125_500_000_000.0, 0.4, 0.4, 0.4, 0.4, 0.4, -1e16]],
                    "kwargs": {},
                    "expected": 125_500_000_002.0,
                    "max_ulps": 0,
                }
            ],
            "probe_args": ([1e16, 125_500_000_000.0, 0.4, 0.4, 0.4, 0.4, 0.4, -1e16],),
            "expected_output": 125_500_000_002.0,
        },
        {
            "fault_key": "fault:zero_division:margin_ratio",
            "exc_type": ZeroDivisionError,
            "description": "ZeroDivisionError when computing margin ratio on zero denominator",
            "broken_fn": broken_margin_ratio,
            "healed_fn_name": "healed_margin_ratio",
            "healed_source": healed_margin_src,
            "test_vectors": [
                {"args": [500.0, 0.0], "kwargs": {}, "expected": 0.0, "max_ulps": 0},
                {"args": [250.0, 1000.0], "kwargs": {}, "expected": 25.0, "max_ulps": 0},
            ],
            "probe_args": (890_000_000.0, 0.0),
            "expected_output": 0.0,
        },
        {
            "fault_key": "fault:key_error:extract_customer_count",
            "exc_type": KeyError,
            "description": "KeyError when enterprise_customers key is aliased as customers",
            "broken_fn": broken_extract_customer_count,
            "healed_fn_name": "healed_extract_customer_count",
            "healed_source": healed_extract_src,
            "test_vectors": [
                {"args": [{"metrics": {"customers": 9437}}], "kwargs": {}, "expected": 9437},
                {"args": [{}], "kwargs": {}, "expected": 0},
            ],
            "probe_args": ({"metrics": {"customers": 9437}},),
            "expected_output": 9437,
        },
        {
            "fault_key": "fault:value_error:parse_currency",
            "exc_type": ValueError,
            "description": "ValueError when parsing USD formatted string with dollar sign and commas",
            "broken_fn": broken_parse_currency,
            "healed_fn_name": "healed_parse_currency",
            "healed_source": healed_parse_src,
            "test_vectors": [
                {"args": ["$3,628,000,000"], "kwargs": {}, "expected": 3_628_000_000.0, "max_ulps": 0},
                {"args": ["127.0%"], "kwargs": {}, "expected": 127.0, "max_ulps": 0},
            ],
            "probe_args": ("$3,628,000,000",),
            "expected_output": 3_628_000_000.0,
        },
        {
            "fault_key": "fault:silent_noop:normalize_weights",
            "exc_type": omnicatch.SilentNoOpError,
            "description": "SilentNoOpError when weight normalization fails to mutate target list",
            "broken_fn": broken_normalize_weights,
            "healed_fn_name": "healed_normalize_weights",
            "healed_source": healed_normalize_src,
            "test_vectors": [
                {"args": [[2.0, 6.0, 2.0]], "kwargs": {}, "expected": [0.2, 0.6, 0.2]},
            ],
            "probe_args": ([2.0, 6.0, 2.0],),
            "expected_output": [0.2, 0.6, 0.2],
        },
    ]


# ======================================================================================
# 5. 4-ARM PAIRED CAUSAL HARNESS (Rule EF-13 & NV-1..5)
# ======================================================================================

def _dist_stats(values: Sequence[float]) -> Dict[str, float]:
    if not values:
        return {"mean": 0.0, "median": 0.0, "std": 0.0, "max_linf": 0.0}
    arr = [float(x) for x in values]
    return {
        "mean": round(statistics.mean(arr), 4),
        "median": round(statistics.median(arr), 4),
        "std": round(statistics.pstdev(arr), 4),
        "max_linf": round(max(abs(x) for x in arr), 4),
    }


def run_4arm_gauntlet(rounds_per_fault: int = 4) -> Dict[str, Any]:
    """Executes the 4-Arm Paired Causal Harness for Idea 6 (CrashImmunizer)."""
    suite = build_standard_fault_suite()
    results: Dict[str, Any] = {"arms": {}}

    # ------------------------------------------------------------------
    # ARM 1: Active Treatment (Full CrashImmunizer + Scar Field + Overlapping Parity)
    # ------------------------------------------------------------------
    arm1_imm = CrashImmunizer(eta=0.40, gamma=0.30, beta=0.10, eps=0.05, kick=2.00, redundant_overlap=True)
    for item in suite:
        arm1_imm.register_vaccine(
            fault_key=item["fault_key"],
            exc_type=item["exc_type"],
            fault_description=item["description"],
            broken_fn=item["broken_fn"],
            healed_source=item["healed_source"],
            healed_fn_name=item["healed_fn_name"],
            test_vectors=item["test_vectors"],
        )
    lore_block = arm1_imm.seal_vaccines_into_lore()

    arm1_latencies: List[float] = []
    arm1_success = 0
    arm1_total = 0
    for _ in range(rounds_per_fault):
        for item in suite:
            arm1_total += 1
            # Copy mutable list argument for fault 5
            p_args = tuple(list(a) if isinstance(a, list) else a for a in item["probe_args"])
            out, receipt = arm1_imm.execute_immunized(item["fault_key"], item["broken_fn"], *p_args)
            if out == item["expected_output"] and receipt is not None:
                arm1_success += 1
                arm1_latencies.append(receipt.latency_ms)

    results["arms"]["arm1_active"] = {
        "total_trials": arm1_total,
        "healed_success": arm1_success,
        "recovery_rate_pct": round(100.0 * arm1_success / arm1_total, 2),
        "lore_merkle_root": lore_block.merkle_root,
        "ftle_lambda": round(arm1_imm.field.lyapunov_sum / max(1, arm1_imm.field.step_count), 6),
        "latency_ms_dist": _dist_stats(arm1_latencies),
    }

    # ------------------------------------------------------------------
    # ARM 2: Negative / Zeroed Control (kick=0, eps=0, recall disabled)
    # ------------------------------------------------------------------
    arm2_imm = CrashImmunizer(eta=0.0, gamma=0.30, beta=0.10, eps=0.0, kick=0.0, enable_recall=False)
    arm2_success = 0
    arm2_crashes = 0
    arm2_total = 0
    for _ in range(rounds_per_fault):
        for item in suite:
            arm2_total += 1
            p_args = tuple(list(a) if isinstance(a, list) else a for a in item["probe_args"])
            try:
                arm2_imm.execute_immunized(item["fault_key"], item["broken_fn"], *p_args)
                arm2_success += 1
            except Exception:
                arm2_crashes += 1

    results["arms"]["arm2_zeroed_control"] = {
        "total_trials": arm2_total,
        "healed_success": arm2_success,
        "unhealed_crashes": arm2_crashes,
        "recovery_rate_pct": round(100.0 * arm2_success / arm2_total, 2),
    }

    # ------------------------------------------------------------------
    # ARM 3: Sham / Permuted Control (Tampered / Mismatched Sham Patches in Memory)
    # ------------------------------------------------------------------
    arm3_imm = CrashImmunizer(eta=0.40, gamma=0.30, beta=0.10, eps=0.05, kick=2.00, redundant_overlap=True)
    for item in suite:
        arm3_imm.register_vaccine(
            fault_key=item["fault_key"],
            exc_type=item["exc_type"],
            fault_description=item["description"],
            broken_fn=item["broken_fn"],
            healed_source=item["healed_source"],
            healed_fn_name=item["healed_fn_name"],
            test_vectors=item["test_vectors"],
        )
    # Deliberately replace every stored engram's healed_source with a sham return value
    for engram in arm3_imm.evermind.engram_bank.values():
        patch_dict = json.loads(engram.content)
        patch_dict["healed_source"] = f"def {patch_dict['healed_fn_name']}(*a, **k): return -999999"
        # Update engram content and hashes to simulate a valid-looking sham engram with wrong test vector output
        new_raw = json.dumps(patch_dict, sort_keys=True, separators=(",", ":"))
        engram.content = new_raw
        engram.content_hash = sha256_bytes(new_raw.encode("utf-8"))

    arm3_unsafe_executions = 0
    arm3_rejected = 0
    for item in suite:
        p_args = tuple(list(a) if isinstance(a, list) else a for a in item["probe_args"])
        try:
            out, _ = arm3_imm.execute_immunized(item["fault_key"], item["broken_fn"], *p_args)
            if out == -999999:
                arm3_unsafe_executions += 1
        except Exception:
            arm3_rejected += 1

    results["arms"]["arm3_sham_permuted"] = {
        "total_trials": len(suite),
        "unsafe_sham_executions": arm3_unsafe_executions,
        "fail_closed_rejections": arm3_rejected,
        "sham_rejection_rate_pct": round(100.0 * arm3_rejected / len(suite), 2),
    }

    # ------------------------------------------------------------------
    # ARM 4: Out-of-Sample Stress Regime (Corrupted Engram Content + 2 Corrupted Primary Shards)
    # ------------------------------------------------------------------
    arm4_imm = CrashImmunizer(eta=0.40, gamma=0.30, beta=0.10, eps=0.05, kick=2.00, redundant_overlap=True)
    for item in suite:
        arm4_imm.register_vaccine(
            fault_key=item["fault_key"],
            exc_type=item["exc_type"],
            fault_description=item["description"],
            broken_fn=item["broken_fn"],
            healed_source=item["healed_source"],
            healed_fn_name=item["healed_fn_name"],
            test_vectors=item["test_vectors"],
        )

    # Inject severe adversarial corruption into EVERY engram in Evermind:
    # 1. Destroy engram.content
    # 2. Corrupt 2 of the 4 primary shards (shard_1 and shard_2)
    for engram in arm4_imm.evermind.engram_bank.values():
        engram.content = "CORRUPTED_BY_COSMIC_RAY_BITFLIP_0xDEAD"
        engram.parity_shards[1] = "shard_1:0_10:badhash00000:CORRUPT"
        engram.parity_shards[2] = "shard_2:10_20:badhash11111:CORRUPT"

    arm4_latencies: List[float] = []
    arm4_success = 0
    arm4_shard_heals = 0
    arm4_total = 0
    for _ in range(rounds_per_fault):
        for item in suite:
            arm4_total += 1
            p_args = tuple(list(a) if isinstance(a, list) else a for a in item["probe_args"])
            out, receipt = arm4_imm.execute_immunized(item["fault_key"], item["broken_fn"], *p_args)
            if out == item["expected_output"] and receipt is not None:
                arm4_success += 1
                arm4_latencies.append(receipt.latency_ms)
                if receipt.shard_healed:
                    arm4_shard_heals += 1

    results["arms"]["arm4_stress_corrupted"] = {
        "total_trials": arm4_total,
        "healed_success": arm4_success,
        "holographic_shard_heals": arm4_shard_heals,
        "recovery_rate_pct": round(100.0 * arm4_success / arm4_total, 2),
        "latency_ms_dist": _dist_stats(arm4_latencies),
    }

    results["verdict"] = (
        "PASS"
        if (
            results["arms"]["arm1_active"]["recovery_rate_pct"] == 100.0
            and results["arms"]["arm2_zeroed_control"]["recovery_rate_pct"] == 0.0
            and results["arms"]["arm3_sham_permuted"]["unsafe_sham_executions"] == 0
            and results["arms"]["arm4_stress_corrupted"]["recovery_rate_pct"] == 100.0
            and results["arms"]["arm4_stress_corrupted"]["holographic_shard_heals"] == len(suite)
        )
        else "FAIL"
    )
    return results


def main(argv: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(description="ZKAEDI PRIME Autonomous Code Scar-Memory & Crash Immunizer")
    parser.add_argument("--gauntlet", action="store_true", help="Run the 4-Arm Paired Causal Harness")
    parser.add_argument("--out", type=Path, help="Optional JSON output path for gauntlet report")
    args = parser.parse_args(argv)

    if args.gauntlet:
        report = run_4arm_gauntlet()
        if args.out:
            args.out.parent.mkdir(parents=True, exist_ok=True)
            args.out.write_text(json.dumps(report, indent=2), encoding="utf-8")
        print(json.dumps(report, indent=2))
        return 0 if report["verdict"] == "PASS" else 1

    report = run_4arm_gauntlet(rounds_per_fault=2)
    print(json.dumps(report, indent=2))
    return 0 if report["verdict"] == "PASS" else 1


if __name__ == "__main__":
    sys.exit(main())
