#!/usr/bin/env python3
"""
Applies all discovered algorithm blueprints (QAlgo-Dream-G*.json) to zcc2.s,
verifies via the 3-stage SelfHostGate, and permanently updates zcc2.s if successful.
"""

import os
import sys
import tempfile
import subprocess
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO_ROOT))

from tools.apply_oneirogenesis_blueprint import apply_blueprint
from zcc_oneirogenesis import SelfHostGate, FitnessOracle, PASSES

def main():
    journal = REPO_ROOT / "dreams" / "journal"
    bps = sorted(list(journal.glob("QAlgo-Dream-G*.json")), key=lambda p: int(p.stem.split("-G")[1]))

    if not bps:
        print("[ERROR] No discovered algorithm blueprints found in dreams/journal")
        sys.exit(1)

    src_asm = str(REPO_ROOT / "zcc2.s")
    tmp_out = str(REPO_ROOT / "zcc2_optimized.s")

    print(f"=== Compounding All {len(bps)} Discovered Blueprints ===")
    curr_inp = src_asm
    total_mods = 0

    for bp in bps:
        res = apply_blueprint(str(bp), curr_inp, tmp_out)
        total_mods += res["modifications"]
        print(f"  [+] {bp.name}: {res['modifications']} transformations applied")
        curr_inp = tmp_out

    print(f"\n[GATE] Running 3-stage self-host verification on compounded assembly ({total_mods} total transformations)...")
    with tempfile.TemporaryDirectory(prefix="all_bp_gate_") as td:
        mutant_bin = os.path.join(td, "mutant_zcc")
        p_args = [str(REPO_ROOT / p) for p in PASSES]
        cmd = ["gcc", "-no-pie", "-O0", "-w", "-fno-asynchronous-unwind-tables",
               "-Wa,--noexecstack", "-fno-unwind-tables",
               "-Iinclude", "-I.", "-o", mutant_bin, tmp_out] + p_args + ["-lm"]
        subprocess.run(cmd, check=True)

        passed, msg = SelfHostGate.verify(mutant_bin, str(REPO_ROOT / "zcc_pp.c"), PASSES, td)
        if passed:
            m_orig = FitnessOracle.measure(mutant_bin, "benchmark_workload.c", src_asm, td, deterministic=True)
            m_opt = FitnessOracle.measure(mutant_bin, "benchmark_workload.c", tmp_out, td, deterministic=True)

            print(f"\n[VERIFICATION SUCCESSFUL — SELF-HOST GATE PASS]")
            print(f"  - Original Structural Score:   {m_orig['structural_score']:.1f}")
            print(f"  - Optimized Structural Score:  {m_opt['structural_score']:.1f} (Δ = {m_opt['structural_score'] - m_orig['structural_score']:+.1f})")
            print(f"  - Total Bytes Saved:           {m_orig['asm_size'] - m_opt['asm_size']:,} bytes")
            print(f"  - Total Insts Removed:         {m_orig['inst_count'] - m_opt['inst_count']:,} insts")

            # Update zcc2.s permanently
            os.replace(tmp_out, src_asm)
            print(f"\n★ PERMANENTLY APPLIED ALL DISCOVERED ALGORITHMS TO zcc2.s ★")
        else:
            print(f"\n[FAIL] Self-host verification failed: {msg}")
            if os.path.exists(tmp_out):
                os.remove(tmp_out)
            sys.exit(1)

if __name__ == "__main__":
    main()
