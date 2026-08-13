#!/usr/bin/env python3
import os
import sys
import tempfile
import random
import hashlib

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from zcc_oneirogenesis import FitnessOracle, SelfHostGate, DREAM_DIR, REPO_ROOT, PASSES
from zcc_dream_mutations import MutationEngine
from zcc_criticality import boltzmann_acceptance

def main():
    zcc2_asm = "zcc2.s"
    if not os.path.exists(zcc2_asm):
        print("zcc2.s not found")
        return

    with open(zcc2_asm) as f:
        parent_lines = f.readlines()

    rng = random.Random(42)
    engine = MutationEngine(seed=42)
    gate = SelfHostGate()
    zcc_pp_c = "zcc_pp.c"

    with tempfile.TemporaryDirectory() as tmpdir:
        parent_bin = os.path.join(tmpdir, "parent_bin")
        p_args = [str(REPO_ROOT / p) for p in PASSES]
        import subprocess
        subprocess.run(['gcc', '-no-pie', '-O0', '-w', '-fno-asynchronous-unwind-tables',
                        '-Wa,--noexecstack', '-fno-unwind-tables',
                        '-Iinclude', '-I.', '-o', parent_bin, zcc2_asm] + p_args + ['-lm'],
                       capture_output=True, check=True)

        parent_fitness = FitnessOracle.measure(parent_bin, "benchmark_workload.c", zcc2_asm, tmpdir, deterministic=True)
        print("=== PARENT FITNESS ===")
        print(f"asm_size:         {parent_fitness['asm_size']}")
        print(f"inst_count:       {parent_fitness['inst_count']}")
        print(f"branch_count:     {parent_fitness['branch_count']}")
        print(f"stack_depth_sum:  {parent_fitness['stack_depth_sum']}")
        print(f"structural_score: {parent_fitness['structural_score']:.4f}")
        print(f"selection_score:  {parent_fitness['selection_score']:.4f}")
        print(f"free_energy:      {parent_fitness['free_energy']:.4f}")

        # Discover mutations
        mutations = engine.dream(parent_lines, max_point_mutations=3, include_sweeps=True, blacklist=set())
        print(f"\nDiscovered {len(mutations)} mutation candidates.")

        for i in range(min(5, len(mutations))):
            mut = mutations[i]
            print(f"\n--- Testing Candidate {i+1}: {mut.category} | {mut.description} ---")
            mutant_lines = engine.apply_mutations(parent_lines, [mut])
            mutant_asm = os.path.join(tmpdir, f"mutant_{i}.s")
            with open(mutant_asm, "w") as f:
                f.writelines(mutant_lines)

            mutant_hash = hashlib.sha256("".join(mutant_lines).encode("utf-8")).hexdigest()[:16]

            # Build mutant
            mutant_bin = os.path.join(tmpdir, f"mutant_{i}_bin")
            r = subprocess.run(['gcc', '-no-pie', '-O0', '-w', '-fno-asynchronous-unwind-tables',
                                '-Wa,--noexecstack', '-fno-unwind-tables',
                                '-Iinclude', '-I.', '-o', mutant_bin, mutant_asm] + p_args + ['-lm'],
                               capture_output=True)
            if r.returncode != 0:
                print(f"Build failed: {r.stderr.decode()[:100]}")
                continue

            gate_ok, gate_msg = gate.verify(mutant_bin, zcc_pp_c, PASSES, tmpdir)
            print(f"Self-host gate: {gate_ok} ({gate_msg})")

            if not gate_ok:
                continue

            mutant_fitness = FitnessOracle.measure(mutant_bin, "benchmark_workload.c", mutant_asm, tmpdir, deterministic=True)
            print(f"Mutant asm_size:         {mutant_fitness['asm_size']} (delta: {mutant_fitness['asm_size'] - parent_fitness['asm_size']})")
            print(f"Mutant inst_count:       {mutant_fitness['inst_count']} (delta: {mutant_fitness['inst_count'] - parent_fitness['inst_count']})")
            print(f"Mutant branch_count:     {mutant_fitness['branch_count']} (delta: {mutant_fitness['branch_count'] - parent_fitness['branch_count']})")
            print(f"Mutant stack_depth_sum:  {mutant_fitness['stack_depth_sum']} (delta: {mutant_fitness['stack_depth_sum'] - parent_fitness['stack_depth_sum']})")
            print(f"Mutant structural_score: {mutant_fitness['structural_score']:.4f} (delta: {mutant_fitness['structural_score'] - parent_fitness['structural_score']:.4f})")
            print(f"Mutant selection_score:  {mutant_fitness['selection_score']:.4f} (delta: {mutant_fitness['selection_score'] - parent_fitness['selection_score']:.4f})")
            print(f"Mutant free_energy:      {mutant_fitness['free_energy']:.4f} (delta_F: {mutant_fitness['free_energy'] - parent_fitness['free_energy']:.4f})")

            delta_F = mutant_fitness['free_energy'] - parent_fitness['free_energy']
            accepted = boltzmann_acceptance(delta_F, 1.0, rng=rng)
            print(f"Boltzmann acceptance decision: {accepted}")

if __name__ == "__main__":
    main()
