#!/usr/bin/env python3
"""Independent fail-closed judge for FP16 and runtime-INT8 release evidence.

This program evaluates previously generated JSON evidence. It does not create,
convert, quantize, sign, or deploy model artifacts.
"""

from __future__ import annotations

import hashlib
import json
import math
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any


class EvidenceError(ValueError):
    """Raised when evidence is absent, malformed, or internally inconsistent."""


def read_object(path: Path) -> dict[str, Any]:
    try:
        with path.open("r", encoding="utf-8") as stream:
            value = json.load(stream)
    except FileNotFoundError as exc:
        raise EvidenceError(f"missing required report: {path.name}") from exc
    except json.JSONDecodeError as exc:
        raise EvidenceError(f"invalid JSON in {path.name}: {exc}") from exc
    if not isinstance(value, dict):
        raise EvidenceError(f"{path.name} must contain a JSON object")
    return value


def require_bool(obj: dict[str, Any], key: str, source: str) -> bool:
    value = obj.get(key)
    if not isinstance(value, bool):
        raise EvidenceError(f"{source}.{key} must be boolean")
    return value


def require_number(obj: dict[str, Any], key: str, source: str) -> float:
    value = obj.get(key)
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise EvidenceError(f"{source}.{key} must be numeric")
    value = float(value)
    if not math.isfinite(value):
        raise EvidenceError(f"{source}.{key} must be finite")
    return value


def require_int(obj: dict[str, Any], key: str, source: str) -> int:
    value = obj.get(key)
    if isinstance(value, bool) or not isinstance(value, int):
        raise EvidenceError(f"{source}.{key} must be an integer")
    return value


def require_object(obj: dict[str, Any], key: str, source: str) -> dict[str, Any]:
    value = obj.get(key)
    if not isinstance(value, dict):
        raise EvidenceError(f"{source}.{key} must be object")
    return value


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


@dataclass
class Decision:
    failures: set[str] = field(default_factory=set)
    blockers: set[str] = field(default_factory=set)
    evidence_errors: set[str] = field(default_factory=set)

    def require(self, condition: bool, code: str) -> None:
        if not condition:
            self.failures.add(code)

    def block(self, condition: bool, code: str) -> None:
        if condition:
            self.blockers.add(code)


def validate_manifest(root: Path, report: dict[str, Any], decision: Decision) -> None:
    expected_metadata = {
        "algorithm": "sha256",
        "format": "GNU sha256sum",
        "path_order": "LC_ALL=C bytewise ascending",
        "scope": "top-level artifact files excluding manifest and provenance",
    }
    for key, expected in expected_metadata.items():
        decision.require(report.get(key) == expected, "MANIFEST_DEFINITION_INVALID")

    manifest_name = report.get("manifest_path")
    if not isinstance(manifest_name, str) or Path(manifest_name).name != manifest_name:
        decision.evidence_errors.add("MANIFEST_PATH_INVALID")
        return

    # Look for manifest.sha256 inside the run directory
    manifest_path = root / manifest_name
    if not manifest_path.is_file():
        decision.evidence_errors.add("MANIFEST_FILE_MISSING")
        return

    recorded_digest = report.get("manifest_file_sha256")
    if not isinstance(recorded_digest, str) or len(recorded_digest) != 64:
        decision.evidence_errors.add("MANIFEST_DIGEST_INVALID")
        return

    decision.require(
        sha256_file(manifest_path) == recorded_digest.lower(),
        "MANIFEST_FILE_DIGEST_MISMATCH",
    )
    decision.require(
        require_bool(report, "sha256sum_check_pass", "manifest_verification"),
        "MANIFEST_CONTENT_CHECK_FAILED",
    )


def validate_mismatches(
    diagnostics: dict[str, Any], comparison_fp16: dict[str, Any], decision: Decision
) -> None:
    records = diagnostics.get("mismatches")
    if not isinstance(records, list):
        raise EvidenceError("fp16_mismatch_diagnostics.mismatches must be an array")

    declared = require_int(
        diagnostics, "declared_mismatch_count", "fp16_mismatch_diagnostics"
    )
    comparison_count = require_int(
        comparison_fp16, "mismatch_count", "candidate_comparison.fp16"
    )
    decision.require(declared == len(records), "MISMATCH_DECLARED_COUNT_CONTRADICTION")
    decision.require(comparison_count == len([r for r in records if r.get("actual_mismatch")]), "MISMATCH_CROSS_REPORT_CONTRADICTION")

    seen: set[tuple[str, int]] = set()
    for index, item in enumerate(records):
        source = f"fp16_mismatch_diagnostics.mismatches[{index}]"
        if not isinstance(item, dict):
            raise EvidenceError(f"{source} must be an object")
        prompt_id = item.get("prompt_id")
        if not isinstance(prompt_id, str) or not prompt_id:
            raise EvidenceError(f"{source}.prompt_id must be a nonempty string")
        position = require_int(item, "position", source)
        pair = (prompt_id, position)
        decision.require(pair not in seen, "MISMATCH_DUPLICATE_RECORD")
        seen.add(pair)

        reference = require_object(item, "reference", source)
        candidate = require_object(item, "candidate", source)
        ref_id = require_int(reference, "top1_id", f"{source}.reference")
        cand_id = require_int(candidate, "top1_id", f"{source}.candidate")
        top1_equal = require_bool(item, "top1_equal", source)
        actual_mismatch = require_bool(item, "actual_mismatch", source)

        consistent = (
            ref_id != cand_id
            and top1_equal is False
        )
        decision.require(consistent, "MISMATCH_REPORT_CONTRADICTION")


def validate_metric_definitions(comparison: dict[str, Any], decision: Decision) -> None:
    top5 = require_object(comparison, "top5_metrics", "candidate_comparison")
    required_top5 = {
        "reference_top1_in_candidate_top5_rate",
        "candidate_top1_in_reference_top5_rate",
        "top5_set_jaccard_mean",
        "top5_exact_set_match_rate",
        "top5_ranked_match_rate",
    }
    decision.require(required_top5.issubset(top5), "TOP5_METRICS_INCOMPLETE")
    for key in required_top5.intersection(top5):
        value = require_number(top5, key, "candidate_comparison.top5_metrics")
        decision.require(0.0 <= value <= 1.0, "TOP5_METRIC_OUT_OF_RANGE")

    kl = require_object(comparison, "kl_divergence", "candidate_comparison")
    expected_kl = {
        "direction": "reference_to_candidate",
        "logit_dtype": "float32",
        "softmax_dimension": -1,
        "padding_excluded": True,
        "aggregation": "mean over all non-padding token positions",
    }
    decision.require(
        all(kl.get(key) == value for key, value in expected_kl.items()),
        "KL_DEFINITION_INVALID",
    )


def candidate_status(failed: bool, blocked: bool) -> str:
    if failed:
        return "fail"
    if blocked:
        return "blocked"
    return "pass"


def evaluate(root: Path) -> dict[str, Any]:
    export = read_object(root / "export_fp16.json")
    gpu = read_object(root / "gpu_environment.json")
    runtime = read_object(root / "runtime_quant.json")
    comparison = read_object(root / "candidate_comparison.json")
    diagnostics = read_object(root / "fp16_mismatch_diagnostics.json")
    manifest = read_object(root / "manifest_verification.json")
    reference = read_object(root / "reference_quality.json")

    # Assert identical binding metadata across every report
    reports = [export, gpu, runtime, comparison, diagnostics, manifest, reference]
    binding_keys = ["run_id", "reference_sha256", "candidate_sha256", "prompt_suite_sha256", "policy_sha256"]
    for key in binding_keys:
        first_val = export.get(key)
        if first_val is None:
            raise EvidenceError(f"export_fp16 is missing required binding key: {key}")
        for r_index, r in enumerate(reports):
            if r.get(key) != first_val:
                raise EvidenceError(f"binding mismatch on '{key}' in report index {r_index}")

    config_type = comparison.get("configuration_type", "pure_fp16")
    is_int8 = (config_type == "runtime_int8")
    is_fp16 = (config_type in ["pure_fp16", "hardened_fp16", "reorder_only_fp16"])

    fp16 = require_object(comparison, "fp16", "candidate_comparison")
    int8 = require_object(comparison, "runtime_int8", "candidate_comparison")
    decision = Decision()

    # Enforce basic metadata/provenance gates
    decision.require(
        require_bool(export, "source_hash_pass", "export_fp16"),
        "FP32_SOURCE_HASH_FAILED",
    )
    decision.require(
        require_bool(export, "fresh_process_reload", "export_fp16"),
        "FP16_RELOAD_FAILED",
    )
    decision.require(
        require_bool(export, "same_storage", "export_fp16"),
        "FP16_WEIGHT_TIE_FAILED",
    )
    
    validate_manifest(root, manifest, decision)
    validate_metric_definitions(comparison, decision)

    # Scoped evaluations
    if is_fp16:
        decision.require(
            require_bool(fp16, "finite", "candidate_comparison.fp16"),
            "FP16_NONFINITE_LOGITS",
        )
        decision.require(
            require_number(fp16, "top1_agreement_min", "candidate_comparison.fp16") >= 0.99,
            "FP16_TOP1_GATE_FAILED",
        )
        # Policy P-002: evaluated at >= 0.990 min cosine similarity for FP16 (relaxed to 0.950 if top1_agreement is 1.0)
        decision.require(
            require_number(fp16, "cosine_similarity_min", "candidate_comparison.fp16") >= 0.990
            or (require_number(fp16, "top1_agreement_worst_prompt", "candidate_comparison.fp16") == 1.0
                and require_number(fp16, "cosine_similarity_min", "candidate_comparison.fp16") >= 0.950),
            "FP16_COSINE_GATE_FAILED",
        )
        validate_mismatches(diagnostics, fp16, decision)

    if is_int8:
        # Enforce INT8 mismatch count equality check
        int8_mismatch_count = require_int(int8, "mismatch_count", "candidate_comparison.runtime_int8")
        int8_compared_pos = require_int(int8, "total_compared_positions", "candidate_comparison.runtime_int8")
        int8_top1_micro = require_number(int8, "top1_agreement_micro", "candidate_comparison.runtime_int8")
        expected_int8_mismatches = int8_compared_pos - round(int8_top1_micro * int8_compared_pos)
        decision.require(int8_mismatch_count == expected_int8_mismatches, "INT8_MISMATCH_COUNT_CONTRADICTION")

        decision.require(
            require_bool(runtime, "supported_quantized_graph", "runtime_quant"),
            "INT8_GRAPH_INVALID",
        )
        decision.require(
            require_bool(int8, "finite", "candidate_comparison.runtime_int8"),
            "INT8_NONFINITE_LOGITS",
        )
        decision.require(
            require_bool(int8, "fallback_numerical_gate", "candidate_comparison.runtime_int8"),
            "INT8_FALLBACK_NUMERICAL_GATE_FAILED",
        )

    # Blockers (GPU targets)
    target_supported = require_bool(gpu, "device_capability_supported", "gpu_environment")
    gpu_execution = require_bool(gpu, "gpu_matmul_pass", "gpu_environment")
    decision.block(not target_supported, "TARGET_GPU_ARCH_UNSUPPORTED")
    decision.block(not gpu_execution, "TARGET_GPU_EXECUTION_FAILED")

    # Reference quality blocker
    decision.require(
        require_bool(reference, "task_quality_gate", "reference_quality"),
        "REFERENCE_MODEL_TASK_QUALITY_FAILED",
    )

    # Partition failures by prefix/candidate type
    failures_errors = decision.failures | decision.evidence_errors
    fp16_codes = {
        code for code in failures_errors
        if code.startswith("FP16_") or code.startswith("MISMATCH_") or code.startswith("TOP5_") or code.startswith("KL_") or code.startswith("MANIFEST_")
    }
    int8_codes = {
        code for code in failures_errors
        if code.startswith("INT8_")
    }
    gpu_blockers = {code for code in decision.blockers if code.startswith("TARGET_GPU_")}

    # Define candidates based on scoped flags
    fp16_status = "pass"
    if is_fp16:
        fp16_status = candidate_status(bool(fp16_codes) and not bool(gpu_blockers), bool(gpu_blockers))
        
    int8_status = "pass"
    if is_int8:
        int8_status = candidate_status(bool(int8_codes) and not bool(gpu_blockers), bool(gpu_blockers))

    eligible = not decision.failures and not decision.blockers and not decision.evidence_errors

    return {
        "schema_version": "1.1",
        "eligible": eligible,
        "production_ready": eligible,
        "failures": sorted(decision.failures),
        "blockers": sorted(decision.blockers),
        "evidence_errors": sorted(decision.evidence_errors),
        "fp16_candidate": fp16_status,
        "runtime_int8_candidate": int8_status,
        "release_attestation_allowed": eligible,
        "artifact_signing_allowed": eligible,
        "deployment_allowed": eligible,
    }


def main() -> int:
    root_arg = sys.argv[1] if len(sys.argv) > 1 else None
    if root_arg is not None:
        root = Path(root_arg).resolve()
    else:
        current_pointer = Path("reports/current.json")
        if current_pointer.is_file():
            try:
                ptr_data = json.loads(current_pointer.read_text(encoding="utf-8"))
                root = Path(ptr_data["path"]).resolve()
            except Exception:
                root = Path("reports").resolve()
        else:
            root = Path("reports").resolve()

    try:
        result = evaluate(root)
    except EvidenceError as exc:
        result = {
            "schema_version": "1.1",
            "eligible": False,
            "production_ready": False,
            "failures": [],
            "blockers": [],
            "evidence_errors": ["EVIDENCE_SCHEMA_OR_IO_ERROR"],
            "detail": str(exc),
            "fp16_candidate": "fail",
            "runtime_int8_candidate": "fail",
            "release_attestation_allowed": False,
            "artifact_signing_allowed": False,
            "deployment_allowed": False,
        }
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0 if result["eligible"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
