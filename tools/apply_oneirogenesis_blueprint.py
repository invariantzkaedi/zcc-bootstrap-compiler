#!/usr/bin/env python3
"""
ZCC Oneirogenesis — Blueprint Auto-Apply & Verification Tool
Safely applies and verifies discovered algorithm blueprints (QAlgo-Dream-G*.json)
against the target assembly or compiler pipeline with full 3-stage self-host verification.
"""

import os
import sys
import json
import argparse
import tempfile
from pathlib import Path

# Add repo root to sys.path
REPO_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO_ROOT))

from zcc_oneirogenesis import SelfHostGate, FitnessOracle, PASSES, REPO_ROOT


def apply_sweep_branch_straightening(asm_lines: list) -> tuple:
    """Removes all jmp instructions whose target label immediately follows."""
    new_lines = []
    i = 0
    removed_count = 0
    n = len(asm_lines)

    while i < n:
        line = asm_lines[i]
        stripped = line.strip()
        if stripped.startswith("jmp\t.L") or stripped.startswith("jmp .L"):
            target_label = stripped.split()[-1]
            # Look ahead for next non-empty/non-comment line
            j = i + 1
            next_label = None
            while j < n:
                nxt = asm_lines[j].strip()
                if not nxt or nxt.startswith("#") or nxt.startswith("//"):
                    j += 1
                    continue
                if nxt.endswith(":") and nxt[:-1] == target_label:
                    next_label = target_label
                break

            if next_label == target_label:
                # Target label immediately follows -> skip redundant jmp!
                removed_count += 1
                i += 1
                continue

        new_lines.append(line)
        i += 1

    return new_lines, removed_count


def apply_sweep_cmpq_zero_to_testq(asm_lines: list) -> tuple:
    """Replaces SAFE cmpq $0, %rX instructions with testq %rX, %rX."""
    new_lines = []
    replaced_count = 0

    for line in asm_lines:
        stripped = line.strip()
        if stripped.startswith("cmpq\t$0, %") or stripped.startswith("cmpq $0, %"):
            parts = stripped.split(",")
            if len(parts) == 2:
                reg = parts[1].strip()
                # Check for standard 64-bit registers (%rax..%r15)
                if reg in ("%rax", "%rbx", "%rcx", "%rdx", "%rsi", "%rdi", "%rbp", "%rsp",
                           "%r8", "%r9", "%r10", "%r11", "%r12", "%r13", "%r14", "%r15"):
                    # Preserve original indent
                    indent = line[:line.find("c")]
                    new_line = f"{indent}testq\t{reg}, {reg}\n"
                    new_lines.append(new_line)
                    replaced_count += 1
                    continue

        new_lines.append(line)

    return new_lines, replaced_count


def apply_sweep_testb_bit_test(asm_lines: list) -> tuple:
    """Replaces andq $1, %rX followed by testq %rX, %rX with testb $1, %rXb."""
    import re
    reg_map = {"%rax": "%al", "%rbx": "%bl", "%rcx": "%cl", "%rdx": "%dl",
               "%rsi": "%sil", "%rdi": "%dil", "%rbp": "%bpl", "%rsp": "%spl",
               "%r8": "%r8b", "%r9": "%r9b", "%r10": "%r10b", "%r11": "%r11b",
               "%r12": "%r12b", "%r13": "%r13b", "%r14": "%r14b", "%r15": "%r15b"}
    new_lines = []
    idx = 0
    n = len(asm_lines)
    count = 0
    while idx < n:
        if idx + 1 < n:
            m1 = re.match(r'(\s*)andq\s+\$1,\s*(%\w+)\s*$', asm_lines[idx])
            m2 = re.match(r'\s*testq\s+(%\w+),\s*(%\w+)\s*$', asm_lines[idx + 1])
            if m1 and m2 and m1.group(2) == m2.group(1) and m2.group(1) == m2.group(2):
                r = m1.group(2)
                if r in reg_map:
                    indent = m1.group(1)
                    new_lines.append(f"{indent}testb\t$1, {reg_map[r]}\n")
                    idx += 2
                    count += 1
                    continue
        new_lines.append(asm_lines[idx])
        idx += 1
    return new_lines, count


def apply_blueprint(blueprint_path: str, input_asm_path: str, output_asm_path: str) -> dict:
    """Applies a blueprint JSON file to an input assembly file."""
    with open(blueprint_path) as f:
        blueprint = json.load(f)

    with open(input_asm_path, errors="ignore") as f:
        lines = f.readlines()

    applied_summary = []
    total_modifications = 0

    for mut in blueprint.get("mutations", []):
        cat = mut.get("category")
        name = mut.get("name")

        if name == "sweep_branch_straighten" or cat == "SWEEP" and "branch" in mut.get("description", ""):
            lines, count = apply_sweep_branch_straightening(lines)
            applied_summary.append(f"sweep_branch_straighten: removed {count} redundant jmps")
            total_modifications += count
        elif name == "sweep_cmpq_zero_to_testq" or cat == "SWEEP" and "cmpq" in mut.get("description", ""):
            lines, count = apply_sweep_cmpq_zero_to_testq(lines)
            applied_summary.append(f"sweep_cmpq_zero_to_testq: replaced {count} cmpq $0 with testq")
            total_modifications += count
        elif name == "sweep_testb_bit_test" or cat == "SWEEP" and "testb" in mut.get("description", ""):
            lines, count = apply_sweep_testb_bit_test(lines)
            applied_summary.append(f"sweep_testb_bit_test: replaced {count} andq $1 + testq pairs with testb")
            total_modifications += count
        elif mut.get("original_asm") and mut.get("mutated_asm"):
            orig = mut.get("original_asm")
            mutated = mut.get("mutated_asm")
            full_text = "".join(lines)
            if orig in full_text:
                full_text = full_text.replace(orig, mutated, 1)
                lines = full_text.splitlines(keepends=True)
                applied_summary.append(f"{name}: {mut.get('description', '')}")
                total_modifications += 1



    with open(output_asm_path, "w", encoding="utf-8") as f:
        f.writelines(lines)

    return {
        "blueprint_id": blueprint.get("algorithm_info", {}).get("id"),
        "modifications": total_modifications,
        "summary": applied_summary,
    }


def main():
    parser = argparse.ArgumentParser(description="Auto-apply & verify Oneirogenesis blueprints")
    parser.add_argument("blueprint", help="Blueprint ID or path (e.g. QAlgo-Dream-G1 or dreams/journal/QAlgo-Dream-G1.json)")
    parser.add_argument("--input", default="zcc2.s", help="Target input assembly file (default: zcc2.s)")
    parser.add_argument("--output", default="zcc_optimized.s", help="Target output assembly file")
    parser.add_argument("--verify", action="store_true", default=True, help="Run self-host verification gate")
    parser.add_argument("--target", default="x86_64", choices=["x86_64", "wasm32", "arm64", "riscv64", "win64_pe"], help="Target backend architecture")
    args = parser.parse_args()

    bp_path = args.blueprint
    if not os.path.exists(bp_path):
        candidate = REPO_ROOT / "dreams" / "journal" / f"{bp_path}.json"
        if candidate.exists():
            bp_path = str(candidate)
        else:
            print(f"[ERROR] Blueprint file not found: {args.blueprint}")
            sys.exit(1)

    inp_path = str(REPO_ROOT / args.input) if not os.path.isabs(args.input) else args.input
    out_path = str(REPO_ROOT / args.output) if not os.path.isabs(args.output) else args.output

    print(f"=== ZCC Oneirogenesis Blueprint Application ===")
    print(f"Blueprint: {bp_path}")
    print(f"Input:     {inp_path}")
    print(f"Output:    {out_path}")

    res = apply_blueprint(bp_path, inp_path, out_path)
    print(f"\n[APPLIED] {res['modifications']} total transformation(s):")
    for s in res['summary']:
        print(f"  - {s}")

    if args.verify:
        print(f"\n[GATE] Running 3-stage self-host verification gate on optimized assembly...")
        with tempfile.TemporaryDirectory(prefix="bp_gate_") as td:
            # Build mutant binary from output assembly
            mutant_bin = os.path.join(td, "mutant_zcc")
            p_args = [str(REPO_ROOT / p) for p in PASSES]
            cmd = ["gcc", "-no-pie", "-O0", "-w", "-fno-asynchronous-unwind-tables",
                   "-Wa,--noexecstack", "-fno-unwind-tables",
                   "-Iinclude", "-I.", "-o", mutant_bin, out_path] + p_args + ["-lm"]
            r = os.system(" ".join(cmd))
            if r != 0:
                print(f"[FAIL] Failed to build binary from optimized assembly output")
                sys.exit(1)

            zcc_pp_c = str(REPO_ROOT / "zcc_pp.c")
            passed, msg = SelfHostGate.verify(mutant_bin, zcc_pp_c, PASSES, td)
            if passed:
                print(f"[SUCCESS] Gate 1 Self-Host Verified: {msg}")

                # Measure structural score improvements
                m_orig = FitnessOracle.measure(mutant_bin, "benchmark_workload.c", inp_path, td, deterministic=True)
                m_opt = FitnessOracle.measure(mutant_bin, "benchmark_workload.c", out_path, td, deterministic=True)

                d_score = m_opt['structural_score'] - m_orig['structural_score']
                d_asm = m_opt['asm_size'] - m_orig['asm_size']
                d_inst = m_opt['inst_count'] - m_orig['inst_count']

                print(f"\n[VERIFIED METRICS]")
                print(f"  - Structural Score Delta: {d_score:+.1f} (from {m_orig['structural_score']:.1f} -> {m_opt['structural_score']:.1f})")
                print(f"  - Assembly Size Delta:    {d_asm:+d} bytes")
                print(f"  - Instruction Count Delta:{d_inst:+d} insts")
                print(f"\n★ BLUEPRINT {res['blueprint_id']} SUCCESSFULLY APPLIED & VERIFIED ★")
            else:
                print(f"[FAIL] Self-host verification failed: {msg}")
                sys.exit(1)


if __name__ == "__main__":
    main()
