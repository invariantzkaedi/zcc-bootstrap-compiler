#!/usr/bin/env python3
"""
CWE-121 / Specialist-7 Error Analysis
Answers:
  1. Positive/negative counts per specialist
  2. CWE-121 TP/FP/FN function names
  3. Feature means for TP vs FP vs FN (CWE-121)
  4. Why Specialist 7 (CWE-API) has 0 positives

Run from WSL:
  cd /mnt/h/__DOWNLOADS/zcc_github_upload
  python3 c_source_scanner/diagnose.py              # full corpus
  python3 c_source_scanner/diagnose.py --limit 980  # match training distribution
"""
import os, sys, glob, random, json, argparse
import torch
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from extractor import ASTFeatureExtractor

SPECIALIST_NAMES = [
    "CWE-704 (Type Cast)",
    "CWE-121 (Stack Buf Overflow)",
    "CWE-476 (NULL Deref)",
    "CWE-682 (Numeric)",
    "CWE-839 (Range Check)",
    "CWE-ASM (Inline Asm)",
    "CWE-131 (Buffer Size)",
    "CWE-API (API Misuse)",
]

FEATURE_NAMES = [
    "node_count/100",
    "memcpy_size_gt_dest",
    "call/stmt",
    "unsafe_str_call",
    "stack_buf_with_unsafe_copy",
    "arith/cmp",
    "int_arith_no_bounds",
    "cmp_ratio",
    "sizeof_ptr_as_buf",
    "array_idx_no_bounds",
    "deref/if",
    "malloc_lt_copy",
    "cast/node",
    "ptr_vars/vars",
    "if_ratio",
    "signed_unsigned_cmp",
    "null_checked_deref",
    "num_lit_ratio",
    "ptr_arith_ratio",
    "float_lit_ratio",
    "max_depth/10",
    "num_params/5",
    "var_ratio",
    "unsigned_type_ratio",
    "long_int_ratio",
    "ptr_to_ptr_ratio",
    "unsigned_vars/vars",
    "stmt/50",
    "loop_ratio",
    "switch_ratio",
    "bitwise_ratio",
    "member_ratio",
]

def get_target_specialist(file_path):
    fn = os.path.basename(file_path)
    path_upper = file_path.upper()
    if "CWE131" in path_upper: return 6
    if "CWE843" in path_upper: return 0
    if "CWE121" in path_upper: return 1
    if "CWE476" in path_upper: return 2
    if "CWE190" in path_upper: return 3
    if "CWE839" in path_upper or "CWE127" in path_upper: return 4
    return -1

def main():
    parser = argparse.ArgumentParser(description="CWE-121 / Specialist-7 Error Analysis")
    parser.add_argument("--limit", type=int, default=None,
                        help="Max files to process. When set, mirrors training's "
                             "stratified per-specialist cap (default: all files).")
    args = parser.parse_args()

    workspace_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    juliet_dir = os.path.join(workspace_dir, "juliet_train_subset")
    zcc_path = "/tmp/zcc"

    # Copy ZCC
    import shutil
    zcc_src = os.path.join(workspace_dir, "zcc")
    if os.path.exists(zcc_path):
        try: os.remove(zcc_path)
        except: pass
    shutil.copy2(zcc_src, zcc_path)
    os.chmod(zcc_path, 0o755)

    c_files = glob.glob(os.path.join(juliet_dir, "**", "*.c"), recursive=True)
    c_files = [f for f in c_files if not any(x in os.path.basename(f)
                for x in ["main.c", "main_linux.c", "std_testcases.h"])]
    c_files = [f for f in c_files if "wchar" not in os.path.basename(f).lower()
               and "socket" not in os.path.basename(f).lower()]

    # Apply stratified per-specialist limit (mirrors scan_juliet_subset in train.py)
    if args.limit is not None:
        per_spec_cap = max(1, args.limit // 8)  # ~180 per specialist for --limit 1440
        groups = {i: [] for i in range(8)}
        unmatched = []
        for f in c_files:
            s = get_target_specialist(f)
            if s != -1:
                groups[s].append(f)
            else:
                unmatched.append(f)
        sampled = []
        random.seed(42)
        for s, files in groups.items():
            if len(files) > per_spec_cap:
                sampled.extend(random.sample(files, per_spec_cap))
            else:
                sampled.extend(files)
        # Fill remaining slots from unmatched
        remaining = args.limit - len(sampled)
        if remaining > 0:
            sampled.extend(unmatched[:remaining])
        c_files = sampled
        print(f"[Diagnose] --limit {args.limit}: using {len(c_files)} files "
              f"(~{per_spec_cap} per specialist)")

    extractor = ASTFeatureExtractor()
    support_dir = os.path.join(juliet_dir, "C", "testcasesupport")

    # Collect all labeled samples
    pos_counts = [0]*8
    neg_counts = [0]*8
    # For CWE-121 deep analysis: store (fn_name, file, features, label)
    cwe121_samples = []

    print(f"[Diagnose] Parsing {len(c_files)} files...")
    for i, c_file in enumerate(c_files):
        if (i+1) % 100 == 0:
            print(f"  [{i+1}/{len(c_files)}]")
        target_spec = get_target_specialist(c_file)
        try:
            functions = extractor.extract_functions_from_file(zcc_path, c_file, ["-I", support_dir])
        except Exception:
            continue
        if not functions:
            continue

        for fn_name, features in functions.items():
            labels = [0.0]*8
            if features[18] > 0.0:   # pointer_arith_ratio → asm signal
                labels[5] = 1.0
            fn_lower = fn_name.lower()
            is_bad = "bad" in fn_lower
            is_good = "good" in fn_lower

            if is_bad and target_spec != -1:
                labels[target_spec] = 1.0
            elif is_bad and target_spec == -1:
                labels[7] = 1.0

            for s in range(8):
                if labels[s] == 1.0:
                    pos_counts[s] += 1
                    break
            else:
                for s in range(8):
                    neg_counts[s] += 1

            if target_spec == 1:
                cwe121_samples.append({
                    "fn": fn_name,
                    "file": os.path.basename(c_file),
                    "features": features,
                    "label": labels[1],
                })

    # ── 1. Per-specialist sample counts ──────────────────────────────────────
    print("\n" + "="*70)
    print("SPECIALIST SAMPLE COUNTS")
    print("="*70)
    print(f"{'#':<3} {'Specialist':<30} {'Pos':>6} {'Neg':>8} {'Ratio':>8}")
    print("-"*70)
    for i, name in enumerate(SPECIALIST_NAMES):
        total = pos_counts[i] + neg_counts[i]
        ratio = pos_counts[i] / total if total > 0 else 0.0
        flag = " ← EMPTY!" if pos_counts[i] == 0 else ""
        print(f"{i:<3} {name:<30} {pos_counts[i]:>6} {neg_counts[i]:>8} {ratio:>8.3f}{flag}")

    # ── 2. CWE-121 feature distribution ──────────────────────────────────────
    print("\n" + "="*70)
    print("CWE-121 FEATURE MEANS (positives vs negatives in CWE-121 files)")
    print("="*70)
    pos = [s for s in cwe121_samples if s["label"] == 1.0]
    neg = [s for s in cwe121_samples if s["label"] == 0.0]
    print(f"  CWE-121 positives (bad functions):  {len(pos)}")
    print(f"  CWE-121 negatives (good functions): {len(neg)}")
    print(f"\n  {'Feature':<32} {'POS mean':>10} {'NEG mean':>10} {'Delta':>10}")
    print("  " + "-"*65)

    if pos and neg:
        import statistics
        for fi, fname in enumerate(FEATURE_NAMES):
            pos_vals = [s["features"][fi] for s in pos]
            neg_vals = [s["features"][fi] for s in neg]
            pm = statistics.mean(pos_vals)
            nm = statistics.mean(neg_vals)
            delta = pm - nm
            star = " ★" if abs(delta) > 0.05 else ""
            print(f"  {fname:<32} {pm:>10.4f} {nm:>10.4f} {delta:>+10.4f}{star}")

    # ── 3. Inspect unsafe_str_call / stack_buf for CWE-121 ──────────────────
    print("\n" + "="*70)
    print("CWE-121 NEW FEATURE COVERAGE")
    print("="*70)
    feat3_pos = sum(1 for s in pos if s["features"][3] > 0)
    feat4_pos = sum(1 for s in pos if s["features"][4] > 0)
    feat3_neg = sum(1 for s in neg if s["features"][3] > 0)
    feat4_neg = sum(1 for s in neg if s["features"][4] > 0)
    print(f"  unsafe_str_call     fires in POS: {feat3_pos}/{len(pos)}  NEG: {feat3_neg}/{len(neg)}")
    print(f"  stack_buf_unsafe    fires in POS: {feat4_pos}/{len(pos)}  NEG: {feat4_neg}/{len(neg)}")

    # ── 4. Show CWE-121 positive function names that DON'T trigger new features ─
    print("\n" + "="*70)
    print("CWE-121 MISSED POSITIVES (bad fns where new features are ZERO)")
    print("="*70)
    missed = [s for s in pos if s["features"][3] == 0.0 and s["features"][4] == 0.0]
    for s in missed[:20]:
        print(f"  {s['fn']:<40}  file: {s['file']}")
    if len(missed) > 20:
        print(f"  ... and {len(missed)-20} more")

    # ── 5. Specialist 7 (CWE-API) investigation ──────────────────────────────
    print("\n" + "="*70)
    print("SPECIALIST 7 (CWE-API) INVESTIGATION")
    print("="*70)
    print("  Label 7 fires when: is_bad=True AND target_spec==-1 (unknown CWE path)")
    print("  Files that would route to specialist 7:")
    api_files = [f for f in c_files if get_target_specialist(f) == -1]
    print(f"  Files with no CWE match: {len(api_files)}")
    if api_files:
        print("  Sample files:")
        for f in api_files[:10]:
            print(f"    {os.path.basename(f)}")
    else:
        print("  → ALL files in juliet_train_subset match a known CWE path.")
        print("  → Specialist 7 gets ZERO positives because the subset was filtered")
        print("    to only include CWE-121/131/190/476/682/704/839/843.")
        print("  → Recommendation: Either add CWE-API files or disable specialist 7.")

    print("\n[Diagnose] Done.")

if __name__ == "__main__":
    main()
