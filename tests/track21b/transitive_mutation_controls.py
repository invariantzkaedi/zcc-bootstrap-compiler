#!/usr/bin/env python3
"""ZCC Track 21B — 20 Transitive Allocator & Verdict Authority Mutation Controls.

Verifies ZIG-VK-TRANSITIVE-001..008 fail-closed enforcement by executing real
compiled C binaries and shared libraries under LD_PRELOAD=transitive_alloc_auditor.so
combined with tools/track21b_transitive_verifier.py.

Required 20 negative controls:
  1.  ZCC heap leak (TMC_01)
  2.  Vulkan resource / API leak (TMC_02)
  3.  Same-sized (128B) allocation from unrelated callsite (TMC_03)
  4.  Wrong Mesa/target ELF Build ID (TMC_04)
  5.  Unexpected allocation/orphan count mismatch (TMC_05)
  6.  Nonlinear leak scaling across cycles (TMC_06)
  7.  Forged audit/verdict evidence & EF-0 self-certification (TMC_07)
  8.  Missing mandatory gate or evidence block (TMC_08)
  9.  Concurrent lifecycle / RTLD_NODELETE DSO pinning violation in strict lane (TMC_09)
  10. Static DF_1_NODELETE or explicit RTLD_NODELETE in stock compat lane (TMC_10)
  11. Valid Build ID with mismatched binary SHA-256 (TMC_11)
  12. Positive unique symbol count with no runtime LD_DEBUG attribution (TMC_12)
  13. Missing or truncated probe LD_DEBUG log / digest (TMC_13)
  14. Stale/mismatched CI execution run identity (TMC_14)
  15. Unbalanced outstanding dlopen() reference handle (TMC_15)
  16. Pinned unrelated DSO instead of libvulkan_lvp.so (TMC_16)
  17. Missing fields or invalid JSON field types in pin provenance (TMC_17)
  18. Stock compatibility pin evidence injected into strict Lane 2 (TMC_18)
  19. Orphaned allocations hidden despite aggregate reconciliation (TMC_19)
  20. Probe non-zero exit code paired with claimed positive attribution (TMC_20)
Plus clean strict and attributed stock positive-control baselines (0 false positives).
"""

from __future__ import annotations

import json
import os
import subprocess
import sys
import tempfile
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Dict, List

REPO_ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO_ROOT))

from tools.track21b_transitive_verifier import (  # noqa: E402
    INSUFFICIENT_EVIDENCE,
    PINNED_DEBIAN_SHA256,
    PINNED_ORIG_SHA256,
    PINNED_PATCH_SHA256,
    PINNED_STOCK_NOBLE_LVP_BUILD_ID,
    PINNED_STOCK_NOBLE_LVP_SHA256,
    VERIFIED_FAIL,
    VERIFIED_PASS,
    UPSTREAM_DEPENDENCY_BLOCKED,
    evaluate_dependency_aware_verdicts,
    verify_verdict_artifact,
)


def extract_elf_build_id(path: Path) -> str:
    p = subprocess.run(["readelf", "-n", str(path)], capture_output=True, text=True, check=True)
    for line in p.stdout.splitlines():
        if "Build ID:" in line:
            return line.split("Build ID:", 1)[1].strip()
    return "none"


def make_clean_evidence_template(audit: Dict[str, Any], build_id: str) -> Dict[str, Any]:
    return {
        "lane": "corrected-mesa-strict-lsan",
        "mandatory_gates": {
            "spirv_val": "PASS",
            "ubsan": "PASS",
            "o2": "PASS",
            "differential_oracle": "PASS",
            "mutation_observatory": "PASS",
        },
        "zcc_c_runtime": {
            "pass": 262,
            "fail": 0,
            "live_total": 0,
            "seq_violations": 0,
            "faults_injected": 21,
            "teardown_mask": "0x7ffe",
            "vk_identity_ok": True,
        },
        "lsan_strict": {
            "detect_leaks": True,
            "suppression_used": False,
            "exit_code": 0,
            "leaked_bytes": 0,
            "leaked_allocs": 0,
        },
        "mesa_build_manifest": {
            "orig_sha256": PINNED_ORIG_SHA256,
            "debian_sha256": PINNED_DEBIAN_SHA256,
            "patch_sha256": PINNED_PATCH_SHA256,
            "libvulkan_lvp_build_id": build_id,
            "stb_gnu_unique_count": 0,
            "vk_icd_exports": 1,
        },
        "alloc_audit": audit,
        "transitive_mutations_killed": True,
    }


def run_audited_binary(
    auditor_so: Path,
    exe: Path,
    args: List[str],
    target_dso: str,
    audit_json: Path,
    fail_strict: bool = False,
) -> subprocess.CompletedProcess[str]:
    env = dict(os.environ)
    env["LD_PRELOAD"] = str(auditor_so)
    env["ZCC_AUDIT_TARGET_DSO"] = target_dso
    env["ZCC_AUDIT_JSON_OUT"] = str(audit_json)
    if fail_strict:
        env["ZCC_AUDIT_FAIL_ON_ORPHAN_OR_ZCC"] = "1"
    else:
        env.pop("ZCC_AUDIT_FAIL_ON_ORPHAN_OR_ZCC", None)
    return subprocess.run([str(exe), *args], env=env, capture_output=True, text=True)


def main() -> int:
    results: List[Dict[str, Any]] = []
    with tempfile.TemporaryDirectory(prefix="t21b_tmc_") as td:
        work = Path(td)
        auditor_so = work / "libauditor.so"
        subprocess.run(
            [
                "gcc", "-std=c99", "-O2", "-shared", "-fPIC", "-Wall", "-Wextra", "-Werror",
                str(REPO_ROOT / "tests" / "track21b" / "transitive_alloc_auditor.c"),
                "-ldl", "-lpthread", "-o", str(auditor_so),
            ],
            check=True,
        )

        # Build realistic dynamic DSOs modeling:
        #   - libvulkan_lvp.so clean (allocates 128B in .bss, frees via atexit on dlclose)
        #   - libvulkan_lvp.so nodelete pinned (-Wl,-z,nodelete)
        clean_dso_c = work / "lvp_clean_sim.c"
        clean_dso_c.write_text(
            """
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
static pthread_mutex_t g_mu = PTHREAD_MUTEX_INITIALIZER;
static void *g_l3_mask = NULL;
static int g_reg = 0;
static void cleanup_mask(void) {
    pthread_mutex_lock(&g_mu);
    free(g_l3_mask);
    g_l3_mask = NULL;
    pthread_mutex_unlock(&g_mu);
}
void sim_icd_probe(int mode_leak_api) {
    if (mode_leak_api) return;
    pthread_mutex_lock(&g_mu);
    if (!g_l3_mask) {
        g_l3_mask = realloc(g_l3_mask, 128);
        memset(g_l3_mask, 0, 128);
        if (!g_reg) { atexit(cleanup_mask); g_reg = 1; }
    }
    pthread_mutex_unlock(&g_mu);
}
""",
            encoding="utf-8",
        )
        clean_dir = work / "clean_lib"
        nodelete_dir = work / "nodelete_lib"
        clean_dir.mkdir()
        nodelete_dir.mkdir()
        clean_dso = clean_dir / "libvulkan_lvp.so"
        nodelete_dso = nodelete_dir / "libvulkan_lvp.so"
        subprocess.run(
            ["gcc", "-std=c99", "-O2", "-shared", "-fPIC", "-pthread", "-Wl,--build-id=sha1", str(clean_dso_c), "-o", str(clean_dso)],
            check=True,
        )
        subprocess.run(
            ["gcc", "-std=c99", "-O2", "-shared", "-fPIC", "-pthread", "-Wl,--build-id=sha1", "-Wl,-z,nodelete", str(clean_dso_c), "-o", str(nodelete_dso)],
            check=True,
        )
        clean_bid = extract_elf_build_id(clean_dso)

        # Host test harness (simulates ZCC driver lifecycle + selectable fault modes + allocator edge cases)
        harness_c = work / "zcc_harness_sim.c"
        harness_c.write_text(
            """
#define _GNU_SOURCE
#include <dlfcn.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

__attribute__((noinline))
static void leak_in_zcc_runtime(size_t sz) {
    void *p = realloc(NULL, sz);
    memset(p, 0xA5, sz);
}

__attribute__((noinline))
static void leak_same_sized_unrelated(size_t sz) {
    void *p = realloc(NULL, sz);
    memset(p, 0x5A, sz);
}

typedef struct { const char *so_path; int iters; int err; } th_arg_t;

static void *mt_fn(void *ap) {
    th_arg_t *a = (th_arg_t *)ap;
    for (int i = 0; i < a->iters; i++) {
        void *h = dlopen(a->so_path, RTLD_NOW | RTLD_LOCAL);
        if (!h) { a->err = 1; return NULL; }
        void (*fn)(int) = (void (*)(int))dlsym(h, "sim_icd_probe");
        if (fn) fn(0);
        dlclose(h);
    }
    return NULL;
}

int main(int argc, char **argv) {
    if (argc < 3) return 2;
    const char *mode = argv[1];
    const char *so_path = argv[2];
    int iters = (argc >= 4) ? atoi(argv[3]) : 5;

    /* Always exercise realloc OOM preservation & address reuse cleanly */
    void *r1 = malloc(64);
    free(r1);
    void *r2 = malloc(64);
    volatile size_t huge_sz = SIZE_MAX;
    void *oom = realloc(r2, huge_sz);
    if (oom == NULL) free(r2);

    if (strcmp(mode, "clean") == 0) {
        pthread_t th[4];
        th_arg_t args[4];
        for (int t = 0; t < 4; t++) {
            args[t].so_path = so_path; args[t].iters = iters; args[t].err = 0;
            pthread_create(&th[t], NULL, mt_fn, &args[t]);
        }
        for (int t = 0; t < 4; t++) {
            pthread_join(th[t], NULL);
            if (args[t].err) return 3;
        }
    } else if (strcmp(mode, "zcc_heap_leak") == 0) {
        leak_in_zcc_runtime(256);
    } else if (strcmp(mode, "unrelated_128b") == 0) {
        for (int i = 0; i < iters; i++) leak_same_sized_unrelated(128);
    } else if (strcmp(mode, "pin_nodelete") == 0) {
        void *h = dlopen(so_path, RTLD_NOW | RTLD_LOCAL);
        void (*fn)(int) = (void (*)(int))dlsym(h, "sim_icd_probe");
        if (fn) fn(0);
        dlclose(h);
    }
    return 0;
}
""",
            encoding="utf-8",
        )
        # Name binary with "zcc_" prefix so auditor classifies `(main)` / `*zcc*` accurately
        zcc_exe = work / "zcc_harness_sim"
        subprocess.run(
            ["gcc", "-std=c99", "-O0", "-fno-inline", "-pthread", str(harness_c), "-ldl", "-o", str(zcc_exe)],
            check=True,
        )

        # Clean baseline control (real auditor + real clean DSO MT unload)
        clean_audit_json = work / "clean_audit.json"
        p_clean = run_audited_binary(auditor_so, zcc_exe, ["clean", str(clean_dso), "5"], "libvulkan_lvp", clean_audit_json, fail_strict=True)
        clean_audit = json.loads(clean_audit_json.read_text(encoding="utf-8"))
        clean_ev = make_clean_evidence_template(clean_audit, clean_bid)
        clean_v = evaluate_dependency_aware_verdicts(clean_ev)
        clean_ok = (
            p_clean.returncode == 0
            and clean_audit["final_target_mapped"] == 0
            and clean_audit["unmapped_target_events"] >= 1
            and clean_audit.get("reconciliation", {}).get("live_count_reconciled") is True
            and clean_audit.get("reconciliation", {}).get("domain_partition_reconciled") is True
            and clean_audit.get("reconciliation", {}).get("domains", {}).get("other", {}).get("bytes") == 0
            and clean_v["complete_execution_stack_correctness"] == VERIFIED_PASS
        )
        if not clean_ok:
            print("CLEAN_DEBUG:", p_clean.returncode, clean_v, file=sys.stderr)
        false_positives = 0 if clean_ok else 1

        # TMC_01: ZCC heap leak
        j1 = work / "tmc1.json"
        p1 = run_audited_binary(auditor_so, zcc_exe, ["zcc_heap_leak", str(clean_dso), "1"], "libvulkan_lvp", j1, fail_strict=True)
        a1 = json.loads(j1.read_text(encoding="utf-8"))
        ev1 = make_clean_evidence_template(a1, clean_bid)
        ev1["lsan_strict"]["exit_code"] = 1
        ev1["lsan_strict"]["leaked_bytes"] = 256
        v1 = evaluate_dependency_aware_verdicts(ev1)
        k1 = (p1.returncode == 42 and a1["zcc_owned_live_bytes"] >= 256 and v1["zcc_owned_resource_correctness"] == VERIFIED_FAIL and v1["complete_execution_stack_correctness"] == VERIFIED_FAIL)
        results.append({"control_id": "TMC_01_ZCC_HEAP_LEAK", "killed": k1, "verdict": v1["complete_execution_stack_correctness"], "detail": f"rc={p1.returncode} zcc_live={a1['zcc_owned_live_bytes']}"})

        # TMC_02: Vulkan resource leak (live_total != 0, vk_api_live > 0)
        a2 = json.loads(json.dumps(clean_audit))
        a2["vulkan_api_live_count"] = 1
        a2["vulkan_api_live_bytes"] = 656
        ev2 = make_clean_evidence_template(a2, clean_bid)
        ev2["zcc_c_runtime"]["live_total"] = 1
        ev2["zcc_c_runtime"]["seq_violations"] = 1
        v2 = evaluate_dependency_aware_verdicts(ev2)
        k2 = (v2["vulkan_api_lifecycle_correctness"] == VERIFIED_FAIL and v2["complete_execution_stack_correctness"] == VERIFIED_FAIL)
        results.append({"control_id": "TMC_02_VULKAN_RESOURCE_LEAK", "killed": k2, "verdict": v2["complete_execution_stack_correctness"], "detail": f"vk_api={v2['vulkan_api_lifecycle_correctness']}"})

        # TMC_03: Same-sized (128 B * 10 = 1280 B) leak from unrelated non-Mesa callsite
        # MUST NOT be misattributed to upstream Mesa in stock-ubuntu2404-compat lane!
        j3 = work / "tmc3.json"
        p3 = run_audited_binary(auditor_so, zcc_exe, ["unrelated_128b", str(clean_dso), "10"], "libvulkan_lvp", j3, fail_strict=True)
        a3 = json.loads(j3.read_text(encoding="utf-8"))
        # Run a clean probe cycle first or simulate stock compat evaluation with the unrelated 128B leak present
        a3["unmapped_target_events"] = 1
        a3["final_target_mapped"] = 0
        ev3 = {
            "lane": "stock-ubuntu2404-compat",
            "mandatory_gates": clean_ev["mandatory_gates"],
            "zcc_c_runtime": clean_ev["zcc_c_runtime"],
            "lsan_strict": {"detect_leaks": True, "suppression_used": False, "exit_code": 1, "leaked_bytes": 1280, "leaked_allocs": 10},
            "alloc_audit": a3,
            "cpu_is_zen": True,
        }
        v3 = evaluate_dependency_aware_verdicts(ev3)
        k3 = (
            p3.returncode == 42
            and a3["zcc_owned_live_bytes"] == 1280
            and v3["transitive_dependency_memory_correctness"] == VERIFIED_FAIL
            and v3["complete_execution_stack_correctness"] == VERIFIED_FAIL
            and v3["complete_execution_stack_correctness"] != UPSTREAM_DEPENDENCY_BLOCKED
        )
        results.append({"control_id": "TMC_03_SAME_SIZED_UNRELATED_128B", "killed": k3, "verdict": v3["complete_execution_stack_correctness"], "detail": f"rc={p3.returncode} zcc_live={a3['zcc_owned_live_bytes']}"})

        # TMC_04: Wrong Mesa Build ID
        a4 = json.loads(json.dumps(clean_audit))
        a4["dso_build_ids"] = [{"module": "libvulkan_lvp.so", "build_id": "0000000000000000000000000000000000000000"}]
        ev4 = make_clean_evidence_template(a4, clean_bid)
        v4 = evaluate_dependency_aware_verdicts(ev4)
        k4 = (v4["transitive_dependency_memory_correctness"] == VERIFIED_FAIL and v4["complete_execution_stack_correctness"] == VERIFIED_FAIL)
        results.append({"control_id": "TMC_04_WRONG_MESA_BUILD_ID", "killed": k4, "verdict": v4["complete_execution_stack_correctness"], "detail": "rejected wrong ELF Build ID"})

        # TMC_05: Unexpected allocation/orphan count mismatch
        ev5 = {
            "lane": "stock-ubuntu2404-compat",
            "mandatory_gates": clean_ev["mandatory_gates"],
            "zcc_c_runtime": clean_ev["zcc_c_runtime"],
            "lsan_strict": {"detect_leaks": True, "suppression_used": False, "exit_code": 1, "leaked_bytes": 1280, "leaked_allocs": 10},
            "expected_stock_build_ids": [clean_bid],
            "expected_alloc_count": 77,
            "cpu_is_zen": True,
            "alloc_audit": {
                **clean_audit,
                "orphaned_on_dlclose_count": 10,
                "orphaned_on_dlclose_bytes": 1280,
                "buckets": [{
                    "module": "libvulkan_lvp.so", "build_id": clean_bid,
                    "count": 10, "bytes": 1280, "min_size": 128, "max_size": 128,
                    "orphaned_count": 10, "reachable_pre_dlclose": 10,
                    "stack": "libvulkan_lvp.so+0x150de4",
                }],
            },
        }
        v5 = evaluate_dependency_aware_verdicts(ev5)
        k5 = (v5["transitive_dependency_memory_correctness"] == VERIFIED_FAIL and v5["complete_execution_stack_correctness"] == VERIFIED_FAIL)
        results.append({"control_id": "TMC_05_UNEXPECTED_ALLOC_COUNT", "killed": k5, "verdict": v5["complete_execution_stack_correctness"], "detail": "expected 77 != observed 10 rejected"})

        # TMC_06: Nonlinear leak growth across 1 / 10 / 100
        ev6 = {
            **ev5,
            "expected_alloc_count": 10,
            "scaling_samples": [
                {"iters": 1, "leaked_allocs": 1, "leaked_bytes": 128, "lvp_build_id": clean_bid},
                {"iters": 10, "leaked_allocs": 10, "leaked_bytes": 1280, "lvp_build_id": clean_bid},
                {"iters": 100, "leaked_allocs": 100, "leaked_bytes": 25600, "lvp_build_id": clean_bid},
            ],
        }
        v6 = evaluate_dependency_aware_verdicts(ev6)
        k6 = (v6["transitive_dependency_memory_correctness"] == VERIFIED_FAIL and v6["complete_execution_stack_correctness"] == VERIFIED_FAIL)
        results.append({"control_id": "TMC_06_NONLINEAR_LEAK_GROWTH", "killed": k6, "verdict": v6["complete_execution_stack_correctness"], "detail": "rejected 1/10/100 nonlinear stride"})

        # TMC_07: Forged evidence / self-certification / tampered canonical SHA-256
        ev7 = json.loads(json.dumps(clean_ev))
        ev7["self_certified_override"] = True
        v7a = evaluate_dependency_aware_verdicts(ev7)
        tampered = json.loads(json.dumps(clean_v))
        tampered["transitive_dependency_memory_correctness"] = UPSTREAM_DEPENDENCY_BLOCKED
        tampered["complete_execution_stack_correctness"] = VERIFIED_PASS
        ok7b, _ = verify_verdict_artifact(tampered)
        ev7c = json.loads(json.dumps(clean_ev))
        ev7c["alloc_audit"]["reconciliation"]["domains"]["other"]["bytes"] = 64
        ev7c["alloc_audit"]["reconciliation"]["domains"]["other"]["count"] = 1
        v7c = evaluate_dependency_aware_verdicts(ev7c)
        k7 = (
            v7a["complete_execution_stack_correctness"] == VERIFIED_FAIL
            and not ok7b
            and v7c["complete_execution_stack_correctness"] == VERIFIED_FAIL
        )
        results.append({"control_id": "TMC_07_FORGED_EVIDENCE_SELF_CERT", "killed": k7, "verdict": v7a["complete_execution_stack_correctness"], "detail": "self_cert=VERIFIED_FAIL & lattice/reconciliation tamper rejected"})

        # TMC_08: Missing mandatory gate or evidence
        ev8 = json.loads(json.dumps(clean_ev))
        del ev8["mandatory_gates"]["spirv_val"]
        v8a = evaluate_dependency_aware_verdicts(ev8)
        ev8b = json.loads(json.dumps(clean_ev))
        del ev8b["mesa_build_manifest"]
        v8b = evaluate_dependency_aware_verdicts(ev8b)
        k8 = (
            v8a["complete_execution_stack_correctness"] == INSUFFICIENT_EVIDENCE
            and v8b["complete_execution_stack_correctness"] == INSUFFICIENT_EVIDENCE
            and not v8a["release_approved"]
            and not v8b["release_approved"]
        )
        results.append({"control_id": "TMC_08_MISSING_MANDATORY_GATE", "killed": k8, "verdict": v8a["complete_execution_stack_correctness"], "detail": "fail-closed INSUFFICIENT_EVIDENCE"})

        # TMC_09: Concurrent / RTLD_NODELETE DSO pinning violation
        j9 = work / "tmc9.json"
        p9 = run_audited_binary(auditor_so, zcc_exe, ["pin_nodelete", str(nodelete_dso), "1"], "libvulkan_lvp", j9, fail_strict=False)
        a9 = json.loads(j9.read_text(encoding="utf-8"))
        ev9 = make_clean_evidence_template(a9, clean_bid)
        v9 = evaluate_dependency_aware_verdicts(ev9)
        k9 = (
            p9.returncode == 0
            and a9["final_target_mapped"] >= 1
            and a9["unmapped_target_events"] == 0
            and v9["transitive_dependency_memory_correctness"] == VERIFIED_FAIL
            and v9["complete_execution_stack_correctness"] == VERIFIED_FAIL
        )
        results.append({"control_id": "TMC_09_CONCURRENT_NODELETE_PINNING", "killed": k9, "verdict": v9["complete_execution_stack_correctness"], "detail": f"final_mapped={a9['final_target_mapped']} unmapped_ev={a9['unmapped_target_events']}"})

        # Positive-control baseline for stock Ubuntu 24.04 runtime STB_GNU_UNIQUE pinning attribution
        stock_pinned_audit = json.loads(json.dumps(clean_audit))
        stock_pinned_audit["dso_build_ids"] = [{"module": "libvulkan_lvp.so", "build_id": PINNED_STOCK_NOBLE_LVP_BUILD_ID}]
        stock_pinned_audit["final_target_mapped"] = 1
        stock_pinned_audit["unmapped_target_events"] = 0
        stock_pinned_audit["retained_target_events"] = 110
        stock_pinned_audit["explicit_rtld_nodelete_calls"] = 0
        stock_pinned_audit["orphaned_on_dlclose_count"] = 0
        stock_pinned_audit["orphaned_on_dlclose_bytes"] = 0
        stock_pinned_audit["target_dso_live_count"] = 0
        stock_pinned_audit["target_dso_live_bytes"] = 0
        stock_pinned_audit["buckets"] = []
        stock_pinned_baseline_ev: Dict[str, Any] = {
            "lane": "stock-ubuntu2404-compat",
            "execution_run_id": "ci-run-46010671506-lane1",
            "cpu_is_zen": False,
            "mandatory_gates": json.loads(json.dumps(clean_ev["mandatory_gates"])),
            "zcc_c_runtime": json.loads(json.dumps(clean_ev["zcc_c_runtime"])),
            "lsan_strict": {"detect_leaks": True, "suppression_used": False, "exit_code": 0, "leaked_bytes": 0, "leaked_allocs": 0},
            "alloc_audit": stock_pinned_audit,
            "transitive_mutations_killed": True,
            "stock_dso_pin_provenance": {
                "lvp_path": "/usr/lib/x86_64-linux-gnu/libvulkan_lvp.so",
                "lvp_build_id": PINNED_STOCK_NOBLE_LVP_BUILD_ID,
                "lvp_sha256": PINNED_STOCK_NOBLE_LVP_SHA256,
                "lvp_device_inode": "2049:1048576",
                "glibc_version": "glibc 2.39",
                "static_df1_nodelete": False,
                "stb_gnu_unique_count": 5,
                "unique_symbol_names": [
                    "_ZGVZN8spvtools5utils8HexFloatINS0_10FloatProxyIfEENS0_14HexFloatTraitsIS3_EEE31getRoundedNormalizedSignificandINS1_INS2_INS0_7Float16EEENS4_IS9_EEEEEENT_9uint_typeENS0_15round_directionEPbE20last_significant_bit",
                    "_ZGVZN8spvtools5utils8HexFloatINS0_10FloatProxyIfEENS0_14HexFloatTraitsIS3_EEE31getRoundedNormalizedSignificandINS1_INS2_INS0_7Float16EEENS4_IS9_EEEEEENT_9uint_typeENS0_15round_directionEPbE17first_rounded_bit",
                    "_ZZN8spvtools5utils8HexFloatINS0_10FloatProxyIfEENS0_14HexFloatTraitsIS3_EEE31getRoundedNormalizedSignificandINS1_INS2_INS0_7Float16EEENS4_IS9_EEEEEENT_9uint_typeENS0_15round_directionEPbE20last_significant_bit",
                    "spvTargetEnvNameMap",
                    "_ZZN8spvtools5utils8HexFloatINS0_10FloatProxyIfEENS0_14HexFloatTraitsIS3_EEE31getRoundedNormalizedSignificandINS1_INS2_INS0_7Float16EEENS4_IS9_EEEEEENT_9uint_typeENS0_15round_directionEPbE17first_rounded_bit",
                ],
                "unique_relocation_count": 5,
                "runtime_unique_nodelete_observed": True,
                "runtime_nodelete_log_lines": [
                    "11667:\tmarking /usr/lib/x86_64-linux-gnu/libvulkan_lvp.so [0] as NODELETE due to unique symbol",
                    "11667:\tactivating NODELETE for /usr/lib/x86_64-linux-gnu/libvulkan_lvp.so [0]",
                ],
                "explicit_nodelete_requested": False,
                "outstanding_dlopen_handles": 0,
                "zero_zcc_probe_sha256": "1" * 64,
                "probe_log_sha256": "2" * 64,
                "probe_exit_code": 0,
                "zero_zcc_probe_lvp_mapped_end": 1,
                "icd_readdir_order": ["lvp_icd.json", "asahi_icd.json", "nouveau_icd.json"],
                "execution_run_id": "ci-run-46010671506-lane1",
            },
        }
        v_pos_stock = evaluate_dependency_aware_verdicts(stock_pinned_baseline_ev)
        ok_pos_stock, msg_pos_stock = verify_verdict_artifact(v_pos_stock)
        pos_stock_valid = (
            ok_pos_stock
            and v_pos_stock["transitive_dependency_memory_correctness"] == UPSTREAM_DEPENDENCY_BLOCKED
            and v_pos_stock["complete_execution_stack_correctness"] == UPSTREAM_DEPENDENCY_BLOCKED
            and v_pos_stock["verifier_execution_success"] is True
            and v_pos_stock["attribution_verified"] is True
            and v_pos_stock["stack_fully_verified"] is False
            and v_pos_stock["certification_passed"] is False
            and v_pos_stock["release_approved"] is False
        )
        if not pos_stock_valid:
            false_positives += 1
            print("STOCK_POS_DEBUG:", msg_pos_stock, v_pos_stock, file=sys.stderr)

        def check_stock_mutation(control_id: str, mut_fn, detail_msg: str) -> None:
            ev_m = json.loads(json.dumps(stock_pinned_baseline_ev))
            mut_fn(ev_m)
            vm = evaluate_dependency_aware_verdicts(ev_m)
            killed = (
                vm["transitive_dependency_memory_correctness"] == VERIFIED_FAIL
                and vm["complete_execution_stack_correctness"] == VERIFIED_FAIL
                and vm["stack_fully_verified"] is False
                and vm["certification_passed"] is False
                and vm["release_approved"] is False
            )
            results.append({"control_id": control_id, "killed": killed, "verdict": vm["complete_execution_stack_correctness"], "detail": detail_msg})

        # TMC_10: Static DF_1_NODELETE or explicit RTLD_NODELETE requested in stock compat
        ev10b = json.loads(json.dumps(stock_pinned_baseline_ev))
        ev10b["stock_dso_pin_provenance"]["explicit_nodelete_requested"] = True
        v10b = evaluate_dependency_aware_verdicts(ev10b)
        ev10a = json.loads(json.dumps(stock_pinned_baseline_ev))
        ev10a["stock_dso_pin_provenance"]["static_df1_nodelete"] = True
        v10a = evaluate_dependency_aware_verdicts(ev10a)
        k10 = (v10a["complete_execution_stack_correctness"] == VERIFIED_FAIL and v10b["complete_execution_stack_correctness"] == VERIFIED_FAIL)
        results.append({"control_id": "TMC_10_STATIC_OR_EXPLICIT_NODELETE", "killed": k10, "verdict": v10a["complete_execution_stack_correctness"], "detail": "rejected static DF_1_NODELETE & explicit RTLD_NODELETE"})

        # TMC_11: Valid Build ID with mismatched binary SHA-256
        check_stock_mutation(
            "TMC_11_VALID_BUILD_ID_MISMATCHED_SHA256",
            lambda d: d["stock_dso_pin_provenance"].__setitem__("lvp_sha256", "a" * 64),
            "rejected valid Build ID paired with wrong lvp_sha256",
        )

        # TMC_12: Positive unique symbol count without runtime LD_DEBUG attribution
        def mut12(d: Dict[str, Any]) -> None:
            d["stock_dso_pin_provenance"]["runtime_unique_nodelete_observed"] = False
            d["stock_dso_pin_provenance"]["runtime_nodelete_log_lines"] = ["11667:\tactivating NODELETE for /usr/lib/x86_64-linux-gnu/libvulkan_lvp.so [0]"]
        check_stock_mutation(
            "TMC_12_UNIQUE_SYMBOLS_NO_RUNTIME_LD_DEBUG",
            mut12,
            "rejected static UNIQUE=5 without runtime LD_DEBUG unique-symbol attribution",
        )

        # TMC_13: Missing or truncated probe LD_DEBUG log
        def mut13(d: Dict[str, Any]) -> None:
            d["stock_dso_pin_provenance"]["probe_log_sha256"] = ""
            d["stock_dso_pin_provenance"]["runtime_nodelete_log_lines"] = []
        check_stock_mutation(
            "TMC_13_MISSING_OR_TRUNCATED_PROBE_LOG",
            mut13,
            "rejected missing/truncated probe LD_DEBUG log & digest",
        )

        # TMC_14: Stale CI execution identity from previous run
        check_stock_mutation(
            "TMC_14_STALE_PREVIOUS_CI_EXECUTION_ID",
            lambda d: d["stock_dso_pin_provenance"].__setitem__("execution_run_id", "stale-ci-run-0001"),
            "rejected mismatched previous-CI execution_run_id",
        )

        # TMC_15: Unbalanced outstanding dlopen() reference handle
        check_stock_mutation(
            "TMC_15_OUTSTANDING_DLOPEN_HANDLE",
            lambda d: d["stock_dso_pin_provenance"].__setitem__("outstanding_dlopen_handles", 1),
            "rejected unbalanced outstanding dlopen handle (>0)",
        )

        # TMC_16: Pinned unrelated DSO instead of libvulkan_lvp.so
        check_stock_mutation(
            "TMC_16_PINNED_UNRELATED_DSO",
            lambda d: d["stock_dso_pin_provenance"].__setitem__(
                "runtime_nodelete_log_lines",
                [
                    "11691:\tmarking /usr/lib/x86_64-linux-gnu/libvulkan_asahi.so [0] as NODELETE due to unique symbol",
                    "11691:\tactivating NODELETE for /usr/lib/x86_64-linux-gnu/libvulkan_asahi.so [0]",
                ],
            ),
            "rejected LD_DEBUG pinning attributed to unrelated DSO (libvulkan_asahi.so)",
        )

        # TMC_17: Missing fields or invalid JSON field types
        ev17a = json.loads(json.dumps(stock_pinned_baseline_ev))
        ev17a["stock_dso_pin_provenance"]["stb_gnu_unique_count"] = "five"
        ev17b = json.loads(json.dumps(stock_pinned_baseline_ev))
        del ev17b["stock_dso_pin_provenance"]["glibc_version"]
        v17a = evaluate_dependency_aware_verdicts(ev17a)
        v17b = evaluate_dependency_aware_verdicts(ev17b)
        k17 = (v17a["complete_execution_stack_correctness"] == VERIFIED_FAIL and v17b["complete_execution_stack_correctness"] == VERIFIED_FAIL)
        results.append({"control_id": "TMC_17_MISSING_FIELDS_OR_INVALID_TYPES", "killed": k17, "verdict": v17a["complete_execution_stack_correctness"], "detail": "rejected missing field & invalid JSON type"})

        # TMC_18: Stock compatibility pin evidence injected into strict Lane 2
        ev18 = json.loads(json.dumps(stock_pinned_baseline_ev))
        ev18["lane"] = "corrected-mesa-strict-lsan"
        ev18["mesa_build_manifest"] = json.loads(json.dumps(clean_ev["mesa_build_manifest"]))
        v18 = evaluate_dependency_aware_verdicts(ev18)
        k18 = (v18["transitive_dependency_memory_correctness"] == VERIFIED_FAIL and v18["complete_execution_stack_correctness"] == VERIFIED_FAIL)
        results.append({"control_id": "TMC_18_STOCK_PIN_EVIDENCE_IN_LANE2", "killed": k18, "verdict": v18["complete_execution_stack_correctness"], "detail": "rejected stock pinning provenance injected into Lane 2"})

        # TMC_19: Orphaned allocations hidden despite aggregate reconciliation
        def mut19(d: Dict[str, Any]) -> None:
            d["alloc_audit"]["orphaned_on_dlclose_count"] = 1
            d["alloc_audit"]["orphaned_on_dlclose_bytes"] = 128
        check_stock_mutation(
            "TMC_19_ORPHAN_HIDDEN_BY_AGGREGATE",
            mut19,
            "rejected non-zero orphaned_on_dlclose despite aggregate reconciliation",
        )

        # TMC_20: Probe exits unsuccessfully while reporting positive attribution
        check_stock_mutation(
            "TMC_20_PROBE_NONZERO_EXIT_WITH_ATTRIBUTION",
            lambda d: d["stock_dso_pin_provenance"].__setitem__("probe_exit_code", 2),
            "rejected non-zero probe_exit_code paired with claimed attribution",
        )

    killed_count = sum(1 for r in results if r["killed"])
    total = len(results)
    verdict = "PASS" if (killed_count == total and false_positives == 0) else "FAIL"
    report = {
        "schema_version": "track21b.transitive_mutation_report.v1",
        "generated_at_utc": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "total_controls": total,
        "killed": killed_count,
        "survived": total - killed_count,
        "false_positives_on_clean_control": false_positives,
        "verdict": verdict,
        "controls": results,
    }
    (REPO_ROOT / "TRACK21B_TRANSITIVE_MUTATION_REPORT.json").write_text(
        json.dumps(report, indent=2) + "\n", encoding="utf-8"
    )
    print(
        f"TRACK21B_TRANSITIVE_MUTATION_CONTROLS total={total} killed={killed_count} "
        f"survived={total - killed_count} false_pos={false_positives} verdict={verdict}"
    )
    for r in results:
        print(f"  - {r['control_id']}: killed={r['killed']} verdict={r['verdict']} ({r['detail']})")
    return 0 if verdict == "PASS" else 1


if __name__ == "__main__":
    raise SystemExit(main())
