#!/usr/bin/env python3
"""ZCC Track 21B Dependency-Aware Transitive Memory & Release Authority Engine.

Evaluates four independent axes under ZCC-EF-0 and ZCC-EF-7:
  1. zcc_owned_resource_correctness
  2. vulkan_api_lifecycle_correctness
  3. transitive_dependency_memory_correctness
  4. complete_execution_stack_correctness

Permitted closed verdicts:
  - VERIFIED_PASS
  - VERIFIED_FAIL
  - UPSTREAM_DEPENDENCY_BLOCKED
  - INSUFFICIENT_EVIDENCE

Invariant: UPSTREAM_DEPENDENCY_BLOCKED or INSUFFICIENT_EVIDENCE never promotes
complete_execution_stack_correctness to VERIFIED_PASS.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

VERIFIED_PASS = "VERIFIED_PASS"
VERIFIED_FAIL = "VERIFIED_FAIL"
UPSTREAM_DEPENDENCY_BLOCKED = "UPSTREAM_DEPENDENCY_BLOCKED"
INSUFFICIENT_EVIDENCE = "INSUFFICIENT_EVIDENCE"

PERMITTED_VERDICTS = {
    VERIFIED_PASS,
    VERIFIED_FAIL,
    UPSTREAM_DEPENDENCY_BLOCKED,
    INSUFFICIENT_EVIDENCE,
}

PINNED_PATCH_SHA256 = "65a58ad9d9bad78aeee362304235cb30e80b65d45a0ae6b7571f9debcfea1ac6"
PINNED_ORIG_SHA256 = "097842f3e49d996868b38688db87b006f7d4541e93ce86d2f341d8b3e7be7c93"
PINNED_DEBIAN_SHA256 = "edda95cf4f8cf4378a0e6cefc4a62b9db811e2e0d94cb498ea077357dda41ed5"
PINNED_STOCK_NOBLE_LVP_BUILD_ID = "8494c48f5f2c2e0ff01872de64b5f9b40867e7a0"
PINNED_STOCK_NOBLE_LVP_SHA256 = "bd9a708b927aff0b327725af4816c54c53107017e443c72de26e65c310ec67ca"
PINNED_HOST_REF_LVP_BUILD_ID = "1dab537bd9d19186a0a1e8aef43781f0fa90dbb6"
PINNED_HOST_REF_LVP_SHA256 = "d9a8bc5d4b3ec0e065562d362541c2a4cb3f3d6208ac715f7ff1dcf01289d164"
PINNED_STOCK_LVP_SHA256_BY_BUILD_ID = {
    PINNED_STOCK_NOBLE_LVP_BUILD_ID: PINNED_STOCK_NOBLE_LVP_SHA256,
    PINNED_HOST_REF_LVP_BUILD_ID: PINNED_HOST_REF_LVP_SHA256,
}

_SHA256_RE = re.compile(r"^[0-9a-f]{64}$")
_DEV_INODE_RE = re.compile(r"^\d+:\d+$")
_MARKING_UNIQUE_RE = re.compile(r"marking\s+\S*libvulkan_lvp\.so\b.*as\s+NODELETE\s+due\s+to\s+unique\s+symbol")
_ACTIVATING_NODELETE_RE = re.compile(r"activating\s+NODELETE\s+for\s+\S*libvulkan_lvp\.so\b")


def _is_strict_int(val: Any) -> bool:
    return isinstance(val, int) and not isinstance(val, bool)


def verify_stock_unique_pin_provenance(
    evidence: Dict[str, Any],
    audit: Dict[str, Any],
    expected_bids: List[str],
) -> Tuple[bool, str]:
    """Fail-closed attribution gate for stock Ubuntu Mesa STB_GNU_UNIQUE NODELETE pinning."""
    if str(evidence.get("lane", "")) != "stock-ubuntu2404-compat":
        return False, "stock pinning provenance forbidden outside stock-ubuntu2404-compat lane"

    prov = evidence.get("stock_dso_pin_provenance")
    if not isinstance(prov, dict):
        return False, "missing or malformed stock_dso_pin_provenance block"

    required_keys = (
        "lvp_path",
        "lvp_build_id",
        "lvp_sha256",
        "lvp_device_inode",
        "glibc_version",
        "static_df1_nodelete",
        "stb_gnu_unique_count",
        "unique_symbol_names",
        "unique_relocation_count",
        "runtime_unique_nodelete_observed",
        "runtime_nodelete_log_lines",
        "explicit_nodelete_requested",
        "outstanding_dlopen_handles",
        "zero_zcc_probe_sha256",
        "probe_log_sha256",
        "probe_exit_code",
        "zero_zcc_probe_lvp_mapped_end",
        "execution_run_id",
    )
    missing = [k for k in required_keys if k not in prov]
    if missing:
        return False, f"stock_dso_pin_provenance missing required field(s): {missing}"

    lvp_path = prov["lvp_path"]
    if not isinstance(lvp_path, str) or not lvp_path.strip() or not lvp_path.strip().endswith("libvulkan_lvp.so"):
        return False, f"invalid lvp_path: {lvp_path!r}"

    lvp_bid = prov["lvp_build_id"]
    dso_bids = [str(x.get("build_id", "")) for x in audit.get("dso_build_ids", [])]
    if not isinstance(lvp_bid, str) or not lvp_bid or lvp_bid not in expected_bids or lvp_bid not in dso_bids:
        return False, f"lvp_build_id {lvp_bid!r} missing from expected {expected_bids} or audit {dso_bids}"

    sha_map = evidence.get("expected_stock_sha256_by_build_id", PINNED_STOCK_LVP_SHA256_BY_BUILD_ID)
    if not isinstance(sha_map, dict):
        return False, "invalid expected_stock_sha256_by_build_id mapping"
    lvp_sha = prov["lvp_sha256"]
    expected_sha = sha_map.get(lvp_bid)
    if (
        not isinstance(lvp_sha, str)
        or not _SHA256_RE.match(lvp_sha)
        or not isinstance(expected_sha, str)
        or lvp_sha != expected_sha
    ):
        return False, f"lvp_sha256 mismatch for Build ID {lvp_bid}: got={lvp_sha!r} expected={expected_sha!r}"

    dev_ino = prov["lvp_device_inode"]
    if not isinstance(dev_ino, str) or not _DEV_INODE_RE.match(dev_ino):
        return False, f"invalid lvp_device_inode: {dev_ino!r}"

    glibc_ver = prov["glibc_version"]
    if not isinstance(glibc_ver, str) or not glibc_ver.strip():
        return False, f"invalid glibc_version: {glibc_ver!r}"

    if prov["static_df1_nodelete"] is not False:
        return False, f"static DF_1_NODELETE flag disallowed for STB_GNU_UNIQUE attribution: {prov['static_df1_nodelete']!r}"

    uniq_cnt = prov["stb_gnu_unique_count"]
    if not _is_strict_int(uniq_cnt) or uniq_cnt <= 0:
        return False, f"invalid stb_gnu_unique_count: {uniq_cnt!r}"

    uniq_syms = prov["unique_symbol_names"]
    if (
        not isinstance(uniq_syms, list)
        or len(uniq_syms) != uniq_cnt
        or any(not isinstance(s, str) or not s.strip() for s in uniq_syms)
        or not any(("spvTargetEnvNameMap" in s or "HexFloat" in s) for s in uniq_syms)
    ):
        return False, "unique_symbol_names invalid or missing Mesa SPIRV-Tools STB_GNU_UNIQUE symbols"

    reloc_cnt = prov["unique_relocation_count"]
    if not _is_strict_int(reloc_cnt) or reloc_cnt <= 0:
        return False, f"invalid unique_relocation_count: {reloc_cnt!r}"

    if prov["runtime_unique_nodelete_observed"] is not True:
        return False, "runtime LD_DEBUG unique-symbol NODELETE attribution not observed"

    log_lines = prov["runtime_nodelete_log_lines"]
    if not isinstance(log_lines, list) or not log_lines or any(not isinstance(ln, str) for ln in log_lines):
        return False, "missing or invalid runtime_nodelete_log_lines"
    has_mark = any(_MARKING_UNIQUE_RE.search(ln) for ln in log_lines)
    has_act = any(_ACTIVATING_NODELETE_RE.search(ln) for ln in log_lines)
    if not (has_mark and has_act):
        return False, f"runtime LD_DEBUG lacks marking+activating NODELETE lines for libvulkan_lvp.so (mark={has_mark}, act={has_act})"

    if prov["explicit_nodelete_requested"] is not False:
        return False, f"explicit RTLD_NODELETE requested in probe: {prov['explicit_nodelete_requested']!r}"
    if int(audit.get("explicit_rtld_nodelete_calls", 0)) != 0:
        return False, f"auditor observed explicit RTLD_NODELETE dlopen call(s): {audit.get('explicit_rtld_nodelete_calls')}"

    out_dl = prov["outstanding_dlopen_handles"]
    if not _is_strict_int(out_dl) or out_dl != 0:
        return False, f"unbalanced dlopen/dlclose handles in probe: outstanding={out_dl!r}"

    for key in ("zero_zcc_probe_sha256", "probe_log_sha256"):
        if not isinstance(prov[key], str) or not _SHA256_RE.match(prov[key]):
            return False, f"invalid digest in {key}: {prov[key]!r}"

    if not _is_strict_int(prov["probe_exit_code"]) or prov["probe_exit_code"] != 0:
        return False, f"zero-ZCC probe failed or exited non-zero: rc={prov['probe_exit_code']!r}"

    mapped_end = prov["zero_zcc_probe_lvp_mapped_end"]
    if not _is_strict_int(mapped_end) or mapped_end < 1:
        return False, f"zero-ZCC probe did not reproduce lvp retention: mapped_end={mapped_end!r}"

    ev_run_id = evidence.get("execution_run_id")
    prov_run_id = prov["execution_run_id"]
    if (
        not isinstance(ev_run_id, str)
        or not ev_run_id.strip()
        or not isinstance(prov_run_id, str)
        or not prov_run_id.strip()
        or prov_run_id != ev_run_id
    ):
        return False, f"stale or mismatched execution_run_id: prov={prov_run_id!r} vs ev={ev_run_id!r}"

    orphaned_bytes = int(audit.get("orphaned_on_dlclose_bytes", -1))
    orphaned_count = int(audit.get("orphaned_on_dlclose_count", -1))
    if orphaned_bytes != 0 or orphaned_count != 0:
        return False, f"orphaned_on_dlclose non-zero under pinned DSO attribution: {orphaned_count}/{orphaned_bytes} B"

    buckets = audit.get("buckets", [])
    if any(int(b.get("orphaned_count", 0)) > 0 for b in buckets):
        return False, "bucket orphaned_count > 0 disallowed under pinned attribution"

    tgt_bytes = int(audit.get("target_dso_live_bytes", -1))
    tgt_count = int(audit.get("target_dso_live_count", -1))
    if tgt_bytes < 0 or tgt_count < 0:
        return False, f"invalid target_dso_live metrics: {tgt_count}/{tgt_bytes} B"
    if tgt_bytes > 0:
        if evidence.get("cpu_is_zen") is not True or tgt_count != 1 or tgt_bytes % 128 != 0:
            return False, (
                f"unexpected pinned target_dso_live state: cpu_is_zen={evidence.get('cpu_is_zen')} "
                f"count={tgt_count} bytes={tgt_bytes}"
            )
        lvp_buckets = [b for b in buckets if str(b.get("module", "")) == "libvulkan_lvp.so"]
        if not lvp_buckets or any(
            str(b.get("build_id", "")) != lvp_bid
            or int(b.get("orphaned_count", 0)) != 0
            or int(b.get("min_size", 0)) <= 0
            or int(b.get("min_size", 0)) != int(b.get("max_size", -1))
            or int(b.get("min_size", 0)) % 128 != 0
            for b in lvp_buckets
        ):
            return False, f"pinned libvulkan_lvp.so bucket failed single-init L3 invariant: {lvp_buckets}"

    return True, (
        f"runtime STB_GNU_UNIQUE NODELETE confirmed for {lvp_path} "
        f"(BuildID={lvp_bid}, sha256={lvp_sha[:12]}..., unique={uniq_cnt}, relocs={reloc_cnt}, "
        f"glibc={glibc_ver}, probe_rc=0, outstanding_dlopen=0)"
    )


def canonical_json_sha256(obj: Dict[str, Any]) -> str:
    payload = {k: v for k, v in obj.items() if k != "canonical_sha256"}
    raw = json.dumps(payload, sort_keys=True, separators=(",", ":")).encode("utf-8")
    return hashlib.sha256(raw).hexdigest()


def verify_scaling_linearity(
    scaling_samples: List[Dict[str, Any]],
    expected_build_ids: Optional[List[str]] = None,
) -> Tuple[bool, str, Dict[str, Any]]:
    """Verify exact per-cycle linear leak proportionality across >= 3 cycle counts (e.g. 1/10/100)."""
    if len(scaling_samples) < 3:
        return False, "insufficient scaling points (need >= 3, e.g. 1/10/100)", {}

    samples = sorted(scaling_samples, key=lambda x: int(x.get("iters", 0)))
    parsed: List[Tuple[int, int, int]] = []
    for s in samples:
        iters = int(s.get("iters", 0))
        leaked_bytes = int(s.get("leaked_bytes", -1))
        leaked_allocs = int(s.get("leaked_allocs", -1))
        if iters <= 0 or leaked_bytes <= 0 or leaked_allocs != iters:
            return (
                False,
                f"invalid scaling sample iters={iters} allocs={leaked_allocs} bytes={leaked_bytes}",
                {},
            )
        if leaked_bytes % iters != 0:
            return (
                False,
                f"non-integer bytes/cycle: {leaked_bytes} / {iters}",
                {},
            )
        stride = leaked_bytes // iters
        if stride <= 0 or stride % 128 != 0:
            return (
                False,
                f"unexpected L3_affinity_masks stride={stride} (must be 128 * num_L3_caches)",
                {},
            )
        if expected_build_ids is not None:
            bid = str(s.get("lvp_build_id", "")).strip()
            if not bid or bid not in expected_build_ids:
                return (
                    False,
                    f"scaling sample Build ID {bid!r} not in expected {expected_build_ids}",
                    {},
                )
        parsed.append((iters, leaked_allocs, stride))

    strides = {p[2] for p in parsed}
    if len(strides) != 1:
        return False, f"nonlinear scaling stride variation across cycles: {parsed}", {}

    stride_val = next(iter(strides))
    return (
        True,
        f"linear scaling verified ({[p[0] for p in parsed]}, stride={stride_val} B/cycle)",
        {"stride_bytes_per_cycle": stride_val, "num_l3_caches_inferred": stride_val // 128},
    )


def verify_audit_reconciliation(audit: Dict[str, Any]) -> Tuple[bool, str]:
    """Verify exact allocator accounting reconciliation: total_allocs - total_frees - reuses == live_count."""
    if all(k in audit for k in ("total_allocs", "total_frees", "live_count")):
        allocs = int(audit["total_allocs"])
        frees = int(audit["total_frees"])
        reuses = int(audit.get("address_reuses", 0))
        live_c = int(audit["live_count"])
        if allocs - frees - reuses != live_c:
            return (
                False,
                f"allocator count mismatch: {allocs} - {frees} - {reuses} = {allocs - frees - reuses} != live_count={live_c}",
            )
    rec = audit.get("reconciliation")
    if rec is None:
        return True, "no reconciliation block present"
    if not isinstance(rec, dict):
        return False, "malformed non-dict reconciliation block"
    if rec.get("live_count_reconciled") is not True or rec.get("domain_partition_reconciled") is not True:
        return False, "auditor reported reconciled=false"
    live_c = int(audit.get("live_count", -1))
    live_b = int(audit.get("live_bytes", -1))
    if int(rec.get("allocs_minus_frees_minus_reuses", -1)) != live_c:
        return False, "reconciliation allocs_minus_frees_minus_reuses != live_count"
    if int(rec.get("unpinned_non_orphan_count", -1)) != 0 or int(rec.get("unpinned_non_orphan_bytes", -1)) != 0:
        return False, f"unpinned non-orphan allocations remain: {rec.get('unpinned_non_orphan_bytes')} B"
    domains = rec.get("domains")
    if not isinstance(domains, dict):
        return False, "reconciliation missing domains object"
    req_domains = ("zcc_owned", "vulkan_api", "target_dso", "system_runtime", "other")
    if any(not isinstance(domains.get(k), dict) for k in req_domains):
        return False, "reconciliation missing required domain counters"
    sum_c = sum(int(domains[k].get("count", -1)) for k in req_domains)
    sum_b = sum(int(domains[k].get("bytes", -1)) for k in req_domains)
    if sum_c != live_c or sum_b != live_b:
        return False, f"domain partition sum mismatch ({sum_c}/{sum_b}B != {live_c}/{live_b}B)"
    if int(domains["other"].get("bytes", -1)) != 0 or int(domains["other"].get("count", -1)) != 0:
        return False, f"unaccounted non-system live allocations detected: domains.other={domains['other']}"
    mods = rec.get("live_by_module", [])
    if not isinstance(mods, list):
        return False, "reconciliation live_by_module is not a list"
    mod_c = sum(int(m.get("count", 0)) for m in mods)
    mod_b = sum(int(m.get("bytes", 0)) for m in mods)
    if mod_c != live_c or mod_b != live_b:
        return False, f"live_by_module sum mismatch ({mod_c}/{mod_b}B != {live_c}/{live_b}B)"
    return True, "reconciled"


def evaluate_dependency_aware_verdicts(evidence: Dict[str, Any]) -> Dict[str, Any]:
    """Evaluate all four correctness axes cleanly from structured evidence dict."""
    lane = str(evidence.get("lane", ""))
    reasons: Dict[str, List[str]] = {
        "zcc": [],
        "vulkan_api": [],
        "transitive": [],
        "stack": [],
    }
    failures_detected: List[str] = []

    if lane not in ("corrected-mesa-strict-lsan", "stock-ubuntu2404-compat"):
        out = {
            "schema_version": "track21b.dependency_verdict.v1",
            "generated_at_utc": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
            "lane": lane or "unknown",
            "zcc_owned_resource_correctness": INSUFFICIENT_EVIDENCE,
            "vulkan_api_lifecycle_correctness": INSUFFICIENT_EVIDENCE,
            "transitive_dependency_memory_correctness": INSUFFICIENT_EVIDENCE,
            "complete_execution_stack_correctness": INSUFFICIENT_EVIDENCE,
            "verifier_execution_success": True,
            "attribution_verified": False,
            "dependency_status": INSUFFICIENT_EVIDENCE,
            "stack_fully_verified": False,
            "certification_passed": False,
            "failures_detected": [f"unrecognized lane {lane!r}"],
            "reasons": reasons,
            "release_approved": False,
        }
        out["canonical_sha256"] = canonical_json_sha256(out)
        return out

    # --- Tamper / self-certification check ---
    if evidence.get("self_certified_override") is True:
        failures_detected.append("ZCC-EF-0 self-certification override attempted")
        for axis in ("zcc", "vulkan_api", "transitive", "stack"):
            reasons[axis].append("rejected self-certified verdict override")
        out = {
            "schema_version": "track21b.dependency_verdict.v1",
            "generated_at_utc": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
            "lane": lane,
            "zcc_owned_resource_correctness": VERIFIED_FAIL,
            "vulkan_api_lifecycle_correctness": VERIFIED_FAIL,
            "transitive_dependency_memory_correctness": VERIFIED_FAIL,
            "complete_execution_stack_correctness": VERIFIED_FAIL,
            "verifier_execution_success": True,
            "attribution_verified": False,
            "dependency_status": VERIFIED_FAIL,
            "stack_fully_verified": False,
            "certification_passed": False,
            "failures_detected": failures_detected,
            "reasons": reasons,
            "release_approved": False,
        }
        out["canonical_sha256"] = canonical_json_sha256(out)
        return out

    # --- Mandatory gates check ---
    required_gates = evidence.get("mandatory_gates", {})
    req_names = ("spirv_val", "ubsan", "o2", "differential_oracle", "mutation_observatory")
    missing_gates = [g for g in req_names if g not in required_gates or required_gates[g] is None]
    failed_gates = [g for g in req_names if g in required_gates and required_gates[g] not in ("PASS", True)]

    audit = evidence.get("alloc_audit")
    lsan = evidence.get("lsan_strict")
    c_runtime = evidence.get("zcc_c_runtime")

    if not isinstance(c_runtime, dict) or not isinstance(audit, dict) or not isinstance(lsan, dict):
        for k, v in (("zcc_c_runtime", c_runtime), ("alloc_audit", audit), ("lsan_strict", lsan)):
            if not isinstance(v, dict):
                failures_detected.append(f"missing mandatory evidence block: {k}")
        zcc_verdict = INSUFFICIENT_EVIDENCE
        vk_verdict = INSUFFICIENT_EVIDENCE
        trans_verdict = INSUFFICIENT_EVIDENCE
        reasons["zcc"].append("missing mandatory runtime/audit/lsan evidence")
        reasons["vulkan_api"].append("missing mandatory runtime/audit/lsan evidence")
        reasons["transitive"].append("missing mandatory runtime/audit/lsan evidence")
    else:
        # 1. ZCC-Owned Resource Correctness
        c_pass = int(c_runtime.get("pass", -1))
        c_fail = int(c_runtime.get("fail", -1))
        live_total = int(c_runtime.get("live_total", -1))
        seq_viol = int(c_runtime.get("seq_violations", -1))
        faults_inj = int(c_runtime.get("faults_injected", 0))
        zcc_heap_live = int(audit.get("zcc_owned_live_bytes", -1))

        if missing_gates:
            zcc_verdict = INSUFFICIENT_EVIDENCE
            failures_detected.append(f"missing mandatory gates: {missing_gates}")
            reasons["zcc"].append(f"missing mandatory gates: {missing_gates}")
        elif (
            c_fail != 0
            or c_pass <= 0
            or live_total != 0
            or seq_viol != 0
            or faults_inj < 21
            or zcc_heap_live != 0
            or bool(failed_gates)
        ):
            zcc_verdict = VERIFIED_FAIL
            if c_fail != 0 or c_pass <= 0:
                failures_detected.append(f"ZCC C tests failed/incomplete: pass={c_pass} fail={c_fail}")
            if live_total != 0 or seq_viol != 0:
                failures_detected.append(f"ZCC tracked live_total={live_total} seq_violations={seq_viol}")
            if zcc_heap_live != 0:
                failures_detected.append(f"ZCC-owned heap leak detected by auditor: {zcc_heap_live} B")
            if failed_gates:
                failures_detected.append(f"failed mandatory gates: {failed_gates}")
            reasons["zcc"].extend(failures_detected)
        else:
            zcc_verdict = VERIFIED_PASS
            reasons["zcc"].append(
                f"pass={c_pass} fail=0 live_total=0 seq_violations=0 zcc_owned_live_bytes=0 faults_injected={faults_inj}"
            )

        # 2. Vulkan API Lifecycle Correctness
        vk_api_live = int(audit.get("vulkan_api_live_bytes", -1))
        teardown_mask = str(c_runtime.get("teardown_mask", "0x0"))
        vk_id_ok = bool(c_runtime.get("vk_identity_ok", True))
        if missing_gates:
            vk_verdict = INSUFFICIENT_EVIDENCE
            reasons["vulkan_api"].append("missing mandatory validation gates")
        elif live_total != 0 or seq_viol != 0 or vk_api_live != 0 or not vk_id_ok or teardown_mask == "0x0":
            vk_verdict = VERIFIED_FAIL
            msg = (
                f"Vulkan API lifecycle failed: live_total={live_total} seq_viol={seq_viol} "
                f"vk_api_live={vk_api_live} vk_id_ok={vk_id_ok} mask={teardown_mask}"
            )
            failures_detected.append(msg)
            reasons["vulkan_api"].append(msg)
        else:
            vk_verdict = VERIFIED_PASS
            reasons["vulkan_api"].append(
                f"live_total=0 seq_violations=0 vulkan_api_live_bytes=0 teardown_mask={teardown_mask}"
            )

        # 3. Transitive Dependency Memory Correctness
        detect_leaks = bool(lsan.get("detect_leaks", False))
        suppressed = bool(lsan.get("suppression_used", True))
        lsan_rc = int(lsan.get("exit_code", -1))
        lsan_bytes = int(lsan.get("leaked_bytes", -1))

        orphaned_bytes = int(audit.get("orphaned_on_dlclose_bytes", -1))
        orphaned_count = int(audit.get("orphaned_on_dlclose_count", -1))
        target_dso_live = int(audit.get("target_dso_live_bytes", -1))
        final_mapped = int(audit.get("final_target_mapped", -1))
        unmapped_events = int(audit.get("unmapped_target_events", 0))
        dso_bids = [str(x.get("build_id", "")) for x in audit.get("dso_build_ids", [])]
        buckets = audit.get("buckets", [])

        expected_bid = evidence.get("expected_target_build_id")
        expected_orphans = evidence.get("expected_alloc_count")
        rec_ok, rec_msg = verify_audit_reconciliation(audit)

        if not detect_leaks or suppressed:
            trans_verdict = VERIFIED_FAIL
            msg = f"weakened LSan configuration forbidden: detect_leaks={detect_leaks} suppression_used={suppressed}"
            failures_detected.append(msg)
            reasons["transitive"].append(msg)
        elif zcc_heap_live != 0 or vk_api_live != 0:
            trans_verdict = VERIFIED_FAIL
            msg = f"non-transitive leak present: zcc={zcc_heap_live} B vk_api={vk_api_live} B"
            failures_detected.append(msg)
            reasons["transitive"].append(msg)
        elif not rec_ok:
            trans_verdict = VERIFIED_FAIL
            msg = f"allocator reconciliation failed: {rec_msg}"
            failures_detected.append(msg)
            reasons["transitive"].append(msg)
        elif lane == "corrected-mesa-strict-lsan":
            if "stock_dso_pin_provenance" in evidence:
                trans_verdict = VERIFIED_FAIL
                msg = "stock_dso_pin_provenance forbidden in strict corrected-mesa-strict-lsan lane"
                failures_detected.append(msg)
                reasons["transitive"].append(msg)
            elif final_mapped != 0 or unmapped_events < 1:
                trans_verdict = VERIFIED_FAIL
                msg = (
                    f"target DSO did not genuinely unload in strict lane (possible NODELETE pinning): "
                    f"final_target_mapped={final_mapped} unmapped_target_events={unmapped_events}"
                )
                failures_detected.append(msg)
                reasons["transitive"].append(msg)
            else:
                manifest = evidence.get("mesa_build_manifest")
                if not isinstance(manifest, dict):
                    trans_verdict = INSUFFICIENT_EVIDENCE
                    msg = "corrected-mesa-strict-lsan lane requires mesa_build_manifest provenance"
                    failures_detected.append(msg)
                    reasons["transitive"].append(msg)
                else:
                    pin_ok = (
                        manifest.get("patch_sha256") == PINNED_PATCH_SHA256
                        and manifest.get("orig_sha256") == PINNED_ORIG_SHA256
                        and manifest.get("debian_sha256") == PINNED_DEBIAN_SHA256
                        and int(manifest.get("stb_gnu_unique_count", -1)) == 0
                        and int(manifest.get("vk_icd_exports", 0)) >= 1
                    )
                    man_bid = str(manifest.get("libvulkan_lvp_build_id", ""))
                    bid_ok = bool(man_bid) and (man_bid in dso_bids)
                    if expected_bid and expected_bid != man_bid:
                        bid_ok = False
                    if not pin_ok or not bid_ok:
                        trans_verdict = VERIFIED_FAIL
                        msg = f"patched Mesa provenance/Build ID mismatch: pin_ok={pin_ok} bid_ok={bid_ok} man_bid={man_bid}"
                        failures_detected.append(msg)
                        reasons["transitive"].append(msg)
                    elif lsan_rc != 0 or lsan_bytes != 0 or orphaned_bytes != 0 or target_dso_live != 0:
                        trans_verdict = VERIFIED_FAIL
                        msg = (
                            f"leak detected in corrected Mesa lane: lsan_rc={lsan_rc} lsan_bytes={lsan_bytes} "
                            f"orphaned={orphaned_bytes} target_dso_live={target_dso_live}"
                        )
                        failures_detected.append(msg)
                        reasons["transitive"].append(msg)
                    elif missing_gates:
                        trans_verdict = INSUFFICIENT_EVIDENCE
                        reasons["transitive"].append("missing mandatory validation gates")
                    else:
                        trans_verdict = VERIFIED_PASS
                        reasons["transitive"].append(
                            f"patched Mesa BuildID={man_bid} unloaded cleanly (unmapped={unmapped_events}, final_mapped=0), "
                            f"orphaned=0 B, target_dso_live=0 B, strict LSan=0 B"
                        )
        else:
            # stock-ubuntu2404-compat lane
            expected_bids = evidence.get("expected_stock_build_ids") or [PINNED_STOCK_NOBLE_LVP_BUILD_ID]
            if expected_bid:
                expected_bids = [str(expected_bid)]

            if final_mapped != 0 or unmapped_events < 1:
                pin_attr_ok, pin_attr_msg = verify_stock_unique_pin_provenance(evidence, audit, expected_bids)
                if not pin_attr_ok:
                    trans_verdict = VERIFIED_FAIL
                    msg = (
                        f"target DSO retention without valid runtime STB_GNU_UNIQUE attribution: "
                        f"final_target_mapped={final_mapped} unmapped_target_events={unmapped_events} ({pin_attr_msg})"
                    )
                    failures_detected.append(msg)
                    reasons["transitive"].append(msg)
                elif missing_gates:
                    trans_verdict = INSUFFICIENT_EVIDENCE
                    reasons["transitive"].append("missing mandatory validation gates")
                else:
                    trans_verdict = UPSTREAM_DEPENDENCY_BLOCKED
                    reasons["transitive"].append(pin_attr_msg)
            elif lsan_rc == 0 and lsan_bytes == 0 and orphaned_bytes == 0 and target_dso_live == 0:
                if evidence.get("cpu_is_zen") is True and PINNED_STOCK_NOBLE_LVP_BUILD_ID in dso_bids:
                    # On Zen CPU, unpatched 25.2.8-.5 Build ID without NODELETE MUST leak; 0 leak indicates silent bypass
                    trans_verdict = VERIFIED_FAIL
                    msg = "stock Mesa LVP (8494c48f...) on Zen reported 0 orphaned/leaked bytes despite unmapped_target_events > 0"
                    failures_detected.append(msg)
                    reasons["transitive"].append(msg)
                else:
                    trans_verdict = VERIFIED_PASS
                    reasons["transitive"].append(
                        "0 B leaked & 0 orphaned with genuine module unload (non-Zen SKU or patched distro Mesa)"
                    )
            else:
                # Leak observed — must strictly prove upstream attribution (never exonerate mismatches!)
                leaked_target_buckets = [
                    b for b in buckets
                    if int(b.get("orphaned_count", 0)) > 0 or str(b.get("module", "")) == "libvulkan_lvp.so"
                ]
                non_lvp_orphan_buckets = [
                    b for b in buckets
                    if int(b.get("orphaned_count", 0)) > 0 and str(b.get("module", "")) != "libvulkan_lvp.so"
                ]
                if non_lvp_orphan_buckets or not leaked_target_buckets:
                    trans_verdict = VERIFIED_FAIL
                    msg = (
                        f"unattributed or non-Mesa orphaned allocations: non_lvp={non_lvp_orphan_buckets}, "
                        f"lvp_buckets={len(leaked_target_buckets)}"
                    )
                    failures_detected.append(msg)
                    reasons["transitive"].append(msg)
                else:
                    bid_match = all(str(b.get("build_id", "")) in expected_bids for b in leaked_target_buckets)
                    stride_ok = all(
                        int(b.get("min_size", 0)) > 0
                        and int(b.get("min_size", 0)) == int(b.get("max_size", -1))
                        and int(b.get("min_size", 0)) % 128 == 0
                        and int(b.get("orphaned_count", 0)) == int(b.get("count", -1))
                        and int(b.get("reachable_pre_dlclose", 0)) == int(b.get("count", -1))
                        for b in leaked_target_buckets
                    )
                    count_ok = True
                    if expected_orphans is not None and orphaned_count != int(expected_orphans):
                        count_ok = False
                    scaling_samples = evidence.get("scaling_samples")
                    scaling_ok = True
                    scaling_msg = "not provided"
                    if scaling_samples is not None:
                        scaling_ok, scaling_msg, _ = verify_scaling_linearity(scaling_samples, expected_bids)

                    if not bid_match or not stride_ok or not count_ok or not scaling_ok:
                        trans_verdict = VERIFIED_FAIL
                        msg = (
                            f"upstream attribution invariant mismatch: bid_match={bid_match} stride_ok={stride_ok} "
                            f"count_ok={count_ok} scaling_ok={scaling_ok} ({scaling_msg})"
                        )
                        failures_detected.append(msg)
                        reasons["transitive"].append(msg)
                    else:
                        trans_verdict = UPSTREAM_DEPENDENCY_BLOCKED
                        reasons["transitive"].append(
                            f"strict upstream attribution verified: BuildID={[b.get('build_id') for b in leaked_target_buckets]}, "
                            f"orphaned={orphaned_count} ({orphaned_bytes} B), pre-dlclose reachable=100%, scaling={scaling_msg}"
                        )

    # 4. Complete Execution Stack Correctness (strict fail-closed lattice)
    axes = (zcc_verdict, vk_verdict, trans_verdict)
    if VERIFIED_FAIL in axes:
        stack_verdict = VERIFIED_FAIL
        reasons["stack"].append("at least one correctness axis failed verification")
    elif INSUFFICIENT_EVIDENCE in axes:
        stack_verdict = INSUFFICIENT_EVIDENCE
        reasons["stack"].append("incomplete or missing mandatory verification evidence")
    elif UPSTREAM_DEPENDENCY_BLOCKED in axes:
        stack_verdict = UPSTREAM_DEPENDENCY_BLOCKED
        reasons["stack"].append("blocked by verified upstream Mesa dependency defect (never VERIFIED_PASS)")
    else:
        mut_ok = evidence.get("transitive_mutations_killed", True)
        if mut_ok is not True:
            stack_verdict = VERIFIED_FAIL
            failures_detected.append("transitive mutation controls failed or incomplete")
            reasons["stack"].append("transitive mutation controls not 9/9 KILLED")
        else:
            stack_verdict = VERIFIED_PASS
            reasons["stack"].append("all axes independently verified PASS under strict unsuppressed gates")

    out = {
        "schema_version": "track21b.dependency_verdict.v1",
        "generated_at_utc": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "lane": lane,
        "zcc_owned_resource_correctness": zcc_verdict,
        "vulkan_api_lifecycle_correctness": vk_verdict,
        "transitive_dependency_memory_correctness": trans_verdict,
        "complete_execution_stack_correctness": stack_verdict,
        "verifier_execution_success": True,
        "attribution_verified": stack_verdict in (VERIFIED_PASS, UPSTREAM_DEPENDENCY_BLOCKED),
        "dependency_status": trans_verdict,
        "stack_fully_verified": stack_verdict == VERIFIED_PASS,
        "certification_passed": stack_verdict == VERIFIED_PASS,
        "failures_detected": failures_detected,
        "reasons": reasons,
        "release_approved": stack_verdict == VERIFIED_PASS,
    }
    out["canonical_sha256"] = canonical_json_sha256(out)
    return out


def verify_verdict_artifact(artifact: Dict[str, Any]) -> Tuple[bool, str]:
    """Independently validate serialized verdict integrity and lattice invariants (ZCC-EF-0)."""
    for field in (
        "zcc_owned_resource_correctness",
        "vulkan_api_lifecycle_correctness",
        "transitive_dependency_memory_correctness",
        "complete_execution_stack_correctness",
    ):
        if artifact.get(field) not in PERMITTED_VERDICTS:
            return False, f"invalid verdict state in {field}: {artifact.get(field)!r}"
    if artifact.get("canonical_sha256") != canonical_json_sha256(artifact):
        return False, "canonical_sha256 mismatch (tampered verdict artifact)"
    axes = (
        artifact["zcc_owned_resource_correctness"],
        artifact["vulkan_api_lifecycle_correctness"],
        artifact["transitive_dependency_memory_correctness"],
    )
    stack = artifact["complete_execution_stack_correctness"]
    if any(x != VERIFIED_PASS for x in axes) and stack == VERIFIED_PASS:
        return False, "invariant violation: complete_execution_stack_correctness=VERIFIED_PASS with non-PASS dependency axis"
    if artifact.get("release_approved") is True and stack != VERIFIED_PASS:
        return False, "invariant violation: release_approved=True when stack != VERIFIED_PASS"
    if artifact.get("verifier_execution_success") is not True:
        return False, "invariant violation: verifier_execution_success must be True"
    if artifact.get("dependency_status") != artifact["transitive_dependency_memory_correctness"]:
        return False, "invariant violation: dependency_status mismatch"
    if artifact.get("attribution_verified") is not (stack in (VERIFIED_PASS, UPSTREAM_DEPENDENCY_BLOCKED)):
        return False, "invariant violation: attribution_verified inconsistency"
    if stack != VERIFIED_PASS and (
        artifact.get("stack_fully_verified") is True or artifact.get("certification_passed") is True
    ):
        return False, "invariant violation: non-VERIFIED_PASS stack promoted to stack_fully_verified/certification_passed"
    if artifact.get("stack_fully_verified") is not (stack == VERIFIED_PASS) or artifact.get("certification_passed") is not (stack == VERIFIED_PASS):
        return False, "invariant violation: stack_fully_verified/certification_passed mismatch"
    return True, "ok"


def main(argv: Optional[List[str]] = None) -> int:
    ap = argparse.ArgumentParser(description="Evaluate dependency-aware Track 21B verdicts")
    ap.add_argument("--evidence-json", help="Input assembled JSON evidence payload")
    ap.add_argument("--out-json", help="Write verdict JSON artifact")
    ap.add_argument(
        "--require-full-stack",
        action="store_true",
        help="Exit non-zero unless complete_execution_stack_correctness == VERIFIED_PASS",
    )
    args = ap.parse_args(argv)

    if not args.evidence_json:
        ap.error("--evidence-json is required")

    ev = json.loads(Path(args.evidence_json).read_text(encoding="utf-8"))
    verdict = evaluate_dependency_aware_verdicts(ev)
    if args.out_json:
        Path(args.out_json).write_text(json.dumps(verdict, indent=2) + "\n", encoding="utf-8")

    print(
        f"TRACK21B_DEPENDENCY_VERDICT lane={verdict['lane']} "
        f"zcc={verdict['zcc_owned_resource_correctness']} "
        f"vk_api={verdict['vulkan_api_lifecycle_correctness']} "
        f"transitive={verdict['transitive_dependency_memory_correctness']} "
        f"stack={verdict['complete_execution_stack_correctness']}"
    )

    stack = verdict["complete_execution_stack_correctness"]
    if stack == VERIFIED_PASS:
        return 0
    if stack == UPSTREAM_DEPENDENCY_BLOCKED:
        if args.require_full_stack:
            return 4
        # Allow non-release compatibility lane to report zcc+vk_api PASS while marking stack UPSTREAM_DEPENDENCY_BLOCKED
        if (
            verdict["zcc_owned_resource_correctness"] == VERIFIED_PASS
            and verdict["vulkan_api_lifecycle_correctness"] == VERIFIED_PASS
        ):
            return 0
        return 4
    if stack == INSUFFICIENT_EVIDENCE:
        return 3
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
