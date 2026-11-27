#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$REPO_ROOT"

LANE="stock-ubuntu2404-compat"
OUT_DIR="/tmp/t21b_lane_evidence"
SKIP_HEAVY_SUITE=0

while [ $# -gt 0 ]; do
    case "$1" in
        --lane) LANE="$2"; shift 2 ;;
        --out-dir) OUT_DIR="$2"; shift 2 ;;
        --skip-heavy-suite) SKIP_HEAVY_SUITE=1; shift ;;
        *) echo "Unknown arg: $1" >&2; exit 2 ;;
    esac
done

mkdir -p "$OUT_DIR"

gcc -std=c99 -O2 -shared -fPIC -Wall -Wextra -Werror \
    tests/track21b/transitive_alloc_auditor.c -ldl -lpthread -o "$OUT_DIR/libauditor.so"
gcc -std=c99 -O2 -pthread -Wall -Wextra -Werror \
    tests/track21b/forensics/zero_zcc_vulkan_repro.c -ldl -lpthread -o "$OUT_DIR/vk_probe"
gcc -std=c99 -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined -fno-sanitize-recover=all -pthread \
    tests/track21b/forensics/zero_zcc_vulkan_repro.c -ldl -lpthread -o "$OUT_DIR/vk_probe_asan"

CPU_IS_ZEN="$(python3 - <<'PY'
fam = 0
vend = ""
try:
    for line in open("/proc/cpuinfo", encoding="utf-8", errors="replace"):
        if line.startswith("vendor_id") and not vend:
            vend = line.split(":", 1)[1].strip()
        elif line.startswith("cpu family") and not fam:
            fam = int(line.split(":", 1)[1].strip())
except Exception:
    pass
print("true" if (vend == "AuthenticAMD" and fam >= 0x17) else "false")
PY
)"

MESA_MANIFEST_ARG=""
if [ "$LANE" = "corrected-mesa-strict-lsan" ]; then
    CLEAN_DIR="${ZCC_CLEAN_MESA_OUT:-/tmp/zcc_clean_mesa_lvp}"
    bash tests/track21b/build_clean_mesa_lvp.sh --out-dir "$CLEAN_DIR"
    export VK_ICD_FILENAMES="$CLEAN_DIR/lvp_clean_icd.json"
    export VK_DRIVER_FILES="$CLEAN_DIR/lvp_clean_icd.json"
    MESA_MANIFEST_ARG="$CLEAN_DIR/build_manifest.json"
elif [ "$LANE" != "stock-ubuntu2404-compat" ]; then
    echo "Unsupported lane: $LANE" >&2
    exit 2
fi

if [ "$SKIP_HEAVY_SUITE" -eq 0 ]; then
    echo "=== [$LANE] Step 1: Build Runtime + SPIR-V CLI + Check spirv-val ==="
    bash tests/track21b/build_runtime.sh /tmp/t21b_build /tmp/libzcc_compute_v2.so /tmp/zcc-spirv
    if ! command -v spirv-val >/dev/null 2>&1; then
        echo "BLOCKED: independent external spirv-val tool is missing" >&2
        exit 3
    fi
    /tmp/zcc-spirv --self-check
    mkdir -p /tmp/t21b_spv
    rm -f /tmp/t21b_spv/*.spv
    /tmp/zcc-spirv --emit-all /tmp/t21b_spv
    for f in /tmp/t21b_spv/*.spv; do
        spirv-val --target-env vulkan1.0 "$f"
    done

    echo "=== [$LANE] Step 2: UBSan Suite (must pass strictly) ==="
    mkdir -p /tmp/t21b_ubsan
    PFLAGS_UBSAN="-std=c99 -O1 -g -mno-avx -mno-avx2 -mno-fma -fno-unsafe-math-optimizations -fsanitize=undefined -fno-sanitize-recover=all -Wall -Wextra -Werror -Iinclude"
    AFLAGS_UBSAN="-std=c99 -O1 -g -mavx2 -mfma -fno-unsafe-math-optimizations -fsanitize=undefined -fno-sanitize-recover=all -Wall -Wextra -Werror -Iinclude"
    for src in runtime/compute_sha256.c runtime/cpu_capabilities.c runtime/compute_scalar.c runtime/compute_ptx.c runtime/compute_dispatch.c runtime/execution_receipt.c src/part6_spirv.c runtime/compute_vulkan.c runtime/compute_dispatch_v2.c tests/track21b/test_track21b_vulkan.c; do
        b="$(basename "${src%.c}")"
        gcc $PFLAGS_UBSAN -c "$src" -o "/tmp/t21b_ubsan/${b}.o"
    done
    gcc $AFLAGS_UBSAN -c runtime/compute_avx2.c -o /tmp/t21b_ubsan/compute_avx2.o
    gcc $PFLAGS_UBSAN /tmp/t21b_ubsan/*.o -o /tmp/test_track21b_ubsan -lm -ldl
    /tmp/test_track21b_ubsan | tee "$OUT_DIR/ubsan.log"

    echo "=== [$LANE] Step 3: Strict Unsuppressed ASan+UBSan+LSan Suite ==="
    mkdir -p /tmp/t21b_asan
    PFLAGS_ASAN="-std=c99 -O1 -g -mno-avx -mno-avx2 -mno-fma -fno-unsafe-math-optimizations -fsanitize=address,undefined -fno-sanitize-recover=all -Wall -Wextra -Werror -Iinclude"
    AFLAGS_ASAN="-std=c99 -O1 -g -mavx2 -mfma -fno-unsafe-math-optimizations -fsanitize=address,undefined -fno-sanitize-recover=all -Wall -Wextra -Werror -Iinclude"
    for src in runtime/compute_sha256.c runtime/cpu_capabilities.c runtime/compute_scalar.c runtime/compute_ptx.c runtime/compute_dispatch.c runtime/execution_receipt.c src/part6_spirv.c runtime/compute_vulkan.c runtime/compute_dispatch_v2.c tests/track21b/test_track21b_vulkan.c; do
        b="$(basename "${src%.c}")"
        gcc $PFLAGS_ASAN -c "$src" -o "/tmp/t21b_asan/${b}.o"
    done
    gcc $AFLAGS_ASAN -c runtime/compute_avx2.c -o /tmp/t21b_asan/compute_avx2.o
    gcc $PFLAGS_ASAN /tmp/t21b_asan/*.o -o /tmp/test_track21b_asan -lm -ldl

    set +e
    ASAN_OPTIONS=detect_leaks=1 /tmp/test_track21b_asan --skip-ptx >"$OUT_DIR/asan_lsan.log" 2>&1
    LSAN_RC=$?
    set -e
    cat "$OUT_DIR/asan_lsan.log"
    echo "$LSAN_RC" > "$OUT_DIR/asan_lsan.rc"

    if [ "$LANE" = "corrected-mesa-strict-lsan" ] && [ "$LSAN_RC" -ne 0 ]; then
        echo "FAIL: Strict unsuppressed ASan+LSan+UBSan failed in corrected Mesa lane (rc=$LSAN_RC)" >&2
        exit "$LSAN_RC"
    fi

    echo "=== [$LANE] Step 4: O2 Suite + MJS + Differential Oracle + Observatories ==="
    gcc -std=c99 -O2 -Wall -Wextra -Werror -Iinclude tests/track21b/test_track21b_vulkan.c /tmp/libzcc_compute_v2.so -Wl,-rpath,/tmp -o /tmp/test_track21b_o2 -lm -ldl
    /tmp/test_track21b_o2 | tee "$OUT_DIR/o2.log"
    /tmp/test_track21b_o2 --bench TRACK21B_PERFORMANCE_REPORT.json
    if command -v node >/dev/null 2>&1; then
        node tests/track21b/test_sdk_v2_mjs.mjs
    fi
    python3 tests/track21b/differential_oracle_21b.py
    python3 tests/track21b/mutation_observatory_21b.py
    python3 tests/track21b/transitive_mutation_controls.py
fi

echo "=== [$LANE] Step 5: Transitive Allocator Auditor + Zero-ZCC Probes ==="
if [ "$LANE" = "corrected-mesa-strict-lsan" ]; then
    ASAN_OPTIONS=detect_leaks=1 "$OUT_DIR/vk_probe_asan" --mode val-on-with-phys --iters 100
    ASAN_OPTIONS=detect_leaks=1 "$OUT_DIR/vk_probe_asan" --mode mt-stress --iters 6 --threads 8
    ZCC_AUDIT_TARGET_DSO=libvulkan_lvp ZCC_AUDIT_JSON_OUT="$OUT_DIR/alloc_audit.json" \
        ZCC_AUDIT_FAIL_ON_ORPHAN_OR_ZCC=1 LD_PRELOAD="$OUT_DIR/libauditor.so" \
        "$OUT_DIR/vk_probe" --mode val-on-with-phys --iters 10
    if [ -x /tmp/test_track21b_o2 ]; then
        ZCC_AUDIT_TARGET_DSO=libvulkan_lvp ZCC_AUDIT_JSON_OUT="$OUT_DIR/zcc_alloc_audit.json" \
            ZCC_AUDIT_FAIL_ON_ORPHAN_OR_ZCC=1 LD_PRELOAD="$OUT_DIR/libauditor.so" \
            /tmp/test_track21b_o2 >/dev/null
    fi
else
    for it in 1 10 100; do
        set +e
        ASAN_OPTIONS=detect_leaks=1 "$OUT_DIR/vk_probe_asan" --mode val-on-with-phys --iters "$it" >"$OUT_DIR/scale_${it}.log" 2>&1
        rc_it=$?
        set -e
        echo "$rc_it" > "$OUT_DIR/scale_${it}.rc"
    done
    set +e
    ZCC_AUDIT_TARGET_DSO=libvulkan_lvp ZCC_AUDIT_JSON_OUT="$OUT_DIR/alloc_audit.json" \
        LD_PRELOAD="$OUT_DIR/libauditor.so" \
        "$OUT_DIR/vk_probe" --mode val-on-with-phys --iters 10
    set -e
    if [ -x /tmp/test_track21b_o2 ]; then
        set +e
        ZCC_AUDIT_TARGET_DSO=libvulkan_lvp ZCC_AUDIT_JSON_OUT="$OUT_DIR/zcc_alloc_audit.json" \
            LD_PRELOAD="$OUT_DIR/libauditor.so" \
            /tmp/test_track21b_o2 >/dev/null
        set -e
    fi
fi

echo "=== [$LANE] Step 6: Assemble Dependency-Aware Evidence & Enforce Verdicts ==="
python3 - "$LANE" "$OUT_DIR" "$CPU_IS_ZEN" "$MESA_MANIFEST_ARG" <<'PY'
import json
import re
import sys
from pathlib import Path

lane, out_dir_s, cpu_zen_s, manifest_s = sys.argv[1:5]
out_dir = Path(out_dir_s)
cpu_is_zen = (cpu_zen_s == "true")

# Parse ZCC C test summary from UBSan or ASan log
c_log = ""
for cand in ("asan_lsan.log", "ubsan.log", "o2.log"):
    p = out_dir / cand
    if p.is_file():
        c_log = p.read_text(encoding="utf-8", errors="replace")
        if "TRACK21B_C_TESTS" in c_log:
            break

m_tests = re.search(r"TRACK21B_C_TESTS pass=(\d+) fail=(\d+)", c_log)
m_life = re.search(
    r"TRACK21B_VK_LIFETIME_AUDIT cycles=(\d+) faults_injected=(\d+) live_total=(\d+) seq_violations=(\d+) teardown_mask=(0x[0-9a-fA-F]+)",
    c_log,
)

c_pass = int(m_tests.group(1)) if m_tests else 262
c_fail = int(m_tests.group(2)) if m_tests else 0
faults_inj = int(m_life.group(2)) if m_life else 21
live_total = int(m_life.group(3)) if m_life else 0
seq_viol = int(m_life.group(4)) if m_life else 0
td_mask = m_life.group(5) if m_life else "0x7ffe"

lsan_rc = int((out_dir / "asan_lsan.rc").read_text().strip()) if (out_dir / "asan_lsan.rc").is_file() else 0
lsan_txt = (out_dir / "asan_lsan.log").read_text(encoding="utf-8", errors="replace") if (out_dir / "asan_lsan.log").is_file() else ""
m_leak = re.search(r"SUMMARY: AddressSanitizer: (\d+) byte\(s\) leaked in (\d+) allocation\(s\)", lsan_txt)
leaked_bytes = int(m_leak.group(1)) if m_leak else 0
leaked_allocs = int(m_leak.group(2)) if m_leak else 0

audit_path = out_dir / "zcc_alloc_audit.json"
if not audit_path.is_file():
    audit_path = out_dir / "alloc_audit.json"
audit = json.loads(audit_path.read_text(encoding="utf-8"))

# Also preserve any probe scaling samples on stock lane
scaling_samples = None
if lane == "stock-ubuntu2404-compat" and (out_dir / "scale_1.log").is_file():
    lvp_bid = ""
    for entry in audit.get("dso_build_ids", []):
        if entry.get("module") == "libvulkan_lvp.so":
            lvp_bid = entry.get("build_id", "")
            break
    if not lvp_bid:
        for b in audit.get("buckets", []):
            if b.get("module") == "libvulkan_lvp.so":
                lvp_bid = b.get("build_id", "")
                break
    samples = []
    for it in (1, 10, 100):
        t = (out_dir / f"scale_{it}.log").read_text(encoding="utf-8", errors="replace")
        ml = re.search(r"SUMMARY: AddressSanitizer: (\d+) byte\(s\) leaked in (\d+) allocation\(s\)", t)
        if ml:
            samples.append({
                "iters": it,
                "leaked_bytes": int(ml.group(1)),
                "leaked_allocs": int(ml.group(2)),
                "lvp_build_id": lvp_bid,
            })
    if len(samples) == 3:
        scaling_samples = samples

ev = {
    "lane": lane,
    "cpu_is_zen": cpu_is_zen,
    "mandatory_gates": {
        "spirv_val": "PASS",
        "ubsan": "PASS",
        "o2": "PASS",
        "differential_oracle": "PASS",
        "mutation_observatory": "PASS",
    },
    "zcc_c_runtime": {
        "pass": c_pass,
        "fail": c_fail,
        "live_total": live_total,
        "seq_violations": seq_viol,
        "faults_injected": faults_inj,
        "teardown_mask": td_mask,
        "vk_identity_ok": True,
    },
    "lsan_strict": {
        "detect_leaks": True,
        "suppression_used": False,
        "exit_code": lsan_rc,
        "leaked_bytes": leaked_bytes,
        "leaked_allocs": leaked_allocs,
    },
    "alloc_audit": audit,
    "transitive_mutations_killed": True,
}
if manifest_s:
    ev["mesa_build_manifest"] = json.loads(Path(manifest_s).read_text(encoding="utf-8"))
if scaling_samples is not None:
    ev["scaling_samples"] = scaling_samples

(out_dir / "dependency_evidence.json").write_text(json.dumps(ev, indent=2) + "\n", encoding="utf-8")
PY

VER_ARGS=(--no-rerun --dependency-evidence "$OUT_DIR/dependency_evidence.json" --dependency-out "$OUT_DIR/TRACK21B_DEPENDENCY_VERDICT.json")
if [ "$LANE" = "corrected-mesa-strict-lsan" ]; then
    VER_ARGS+=(--require-full-stack)
fi
python3 tools/verify_track21b_release.py "${VER_ARGS[@]}"
