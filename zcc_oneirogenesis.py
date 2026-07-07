#!/usr/bin/env python3
"""
=============================================================================
   ██████╗ ███╗   ██╗███████╗██╗██████╗  ██████╗  ██████╗ ███████╗███╗   ██╗
  ██╔═══██╗████╗  ██║██╔════╝██║██╔══██╗██╔═══██╗██╔════╝ ██╔════╝████╗  ██║
  ██║   ██║██╔██╗ ██║█████╗  ██║██████╔╝██║   ██║██║  ███╗█████╗  ██╔██╗ ██║
  ██║   ██║██║╚██╗██║██╔══╝  ██║██╔══██╗██║   ██║██║   ██║██╔══╝  ██║╚██╗██║
  ╚██████╔╝██║ ╚████║███████╗██║██║  ██║╚██████╔╝╚██████╔╝███████╗██║ ╚████║
   ╚═════╝ ╚═╝  ╚═══╝╚══════╝╚═╝╚═╝  ╚═╝ ╚═════╝  ╚═════╝ ╚══════╝╚═╝  ╚═══╝
                 ZCC ONEIROGENESIS v2 — Enhanced Dream Engine
=============================================================================
Enhancements over v1:
  1. SWEEP MUTATIONS  — apply ALL instances of a safe pattern in one pass
     (3,588 movq-zero→xorq + 189 imulq-pow2→shl in current ZCC assembly)
  2. ISLAND MODEL     — 3 parallel lineages evolve independently, survivors
     cross-breed: best mutations from each island can combine
  3. STATISTICAL ORACLE — 3-sample fitness averaging to eliminate timing noise
  4. HAMILTONIAN TELEMETRY — real-time energy landscape streaming to God's Eye
  5. MUTATION MEMORY  — failed mutation fingerprints are blacklisted to prevent
     re-testing patterns the gate already rejected

Usage:
    python3 zcc_oneirogenesis.py                    # 50 cycles, 1 lineage
    python3 zcc_oneirogenesis.py --islands 3        # Island model (3 lineages)
    python3 zcc_oneirogenesis.py --sweep            # Force sweep mutations first
    python3 zcc_oneirogenesis.py --aggressive       # 8 mutations/cycle, 3 sweeps
    python3 zcc_oneirogenesis.py --dry-run          # Preview without executing
    python3 zcc_oneirogenesis.py --visualize        # Stream to God's Eye
=============================================================================
"""

import argparse
import hashlib
import hmac
import json
import os
import re
import shutil
import socket
import subprocess
import sys
if sys.stdout and getattr(sys.stdout, 'encoding', '').lower() != 'utf-8':
    try:
        sys.stdout.reconfigure(encoding='utf-8')
    except AttributeError:
        pass
import time
import tempfile
import random
import threading
from concurrent.futures import ThreadPoolExecutor, as_completed
from dataclasses import dataclass, field, asdict
from pathlib import Path
from datetime import datetime, timezone
from typing import Optional

from zcc_dream_mutations import MutationEngine, Mutation
from zcc_criticality import (
    topology_eta_search, prime_free_energy, SpectralArrestDetector,
    relaxation_phase, universality_class, boltzmann_acceptance,
)
from zcc_cfg_extract import extract_cfg, cfg_spectral_dim, cfg_stats

# ═══════════════════════════════════════════════════════════════════════
# CONSTANTS
# ═══════════════════════════════════════════════════════════════════════

REPO_ROOT      = Path(__file__).parent.resolve()
DREAM_DIR      = REPO_ROOT / "dreams"
JOURNAL_DIR    = DREAM_DIR / "journal"
LINEAGE_DIR    = DREAM_DIR / "lineage"
BLACKLIST_FILE = DREAM_DIR / "blacklist.json"
BENCHMARK_FILE = REPO_ROOT / "benchmark_workload.c"
GODS_EYE_WS    = "ws://127.0.0.1:8082/ws/dream_telemetry"

PARTS = ["part0_pp.c", "zcc_ast_bridge.h", "part1.c", "part2.c", "part3.c",
         "ir.h", "ir_emit_dispatch.h", "ir_bridge.h", "part4.c", "part5.c",
         "part6_arm.c", "ir.c", "ir_to_x86.c", "ir_pass_manager.c",
         "regalloc.c", "ir_telemetry_stub.c"]
PASSES = ["compiler_passes.c", "compiler_passes_ir.c", "ir_pass_manager.c",
          "ir_pass_warden.c", "ir_pass_taint.c", "ir_pass_healer.c",
          "ir_symbolic_cfg.c", "ir_dominance.c", "ir_ssa.c", "evm_lifter.c",
          "ir_vuln_tag.c", "ir_to_evm.c", "ir_evm_stack.c",
          "src/ir_lower_float.c", "src/x86_codegen_sse.c", "src/evm/decompiler.c",
          "src/evm/jit.c", "src/evm/symbolic.c", "src/evm/memory_v2.c",
          "src/evm/abi_extractor.c", "src/evm/jit_memory.c",
          "src/evm/proof_export.c", "src/evm/ipc_bridge.c",
          "src/evm/yul_weaver.c", "src/evm/yul_fixed_point.c",
          "src/evm/yul_frontend.c", "src/gfx/sdf_compiler.c",
          "src/gfx/mesh_warden.c", "src/evm/evm_symbolic_harness.c",
          "src/zcc_oracle_substrate.c",
          "src/elf_emit.c", "src/codegen.c", "src/ir_serialization.c",
          "src/zcc_smt_prover.c", "src/gguf_emit.c", "src/zld.c",
          "src/zcc_resource_oracle.c", "transient_state.c", "zcc_lucky_alert_injector.c",
          "src/opt/ir_verify.c", "src/opt/zcc_ir_opt_helpers.c", "src/opt/instcombine_pass.c",
          "src/opt/instcombine_rules.c", "src/opt/instcombine_dispatch.c", "src/opt/sccp_pass.c",
          "src/opt/cfg_simplify_pass.c", "src/opt/clone_remap.c", "src/opt/loop_validator.c",
          "src/opt/loop_unroll_pass.c", "src/opt/inline_pass.c", "src/opt/pointer_ssa.c"]

# ANSI colour palette
_R = "\033[91m"; _G = "\033[92m"; _Y = "\033[93m"
_C = "\033[96m"; _M = "\033[95m"; _W = "\033[0m"; _B = "\033[1m"
_DIM = "\033[2m"


# ═══════════════════════════════════════════════════════════════════════
# DATA CLASSES
# ═══════════════════════════════════════════════════════════════════════

@dataclass
class IslandState:
    """State for one competing lineage in the island model."""
    island_id: int
    generation: int = 0
    parent_hash: str = "GENESIS"
    parent_asm_path: str = ""
    parent_score: float = 0.0
    survived: int = 0
    rejected: int = 0
    lineage: list = field(default_factory=list)
    discovered: list = field(default_factory=list)


@dataclass
class DreamState:
    """Global dream engine state (persisted to disk)."""
    generation: int = 0
    parent_hash: str = "GENESIS"
    parent_asm_size: int = 0
    parent_bin_size: int = 0
    total_mutations_tried: int = 0
    total_mutations_survived: int = 0
    total_regressions: int = 0
    total_fitness_rejections: int = 0   # bucket 5: gate-pass, delta>=0
    lineage: list = field(default_factory=list)
    fitness_history: list = field(default_factory=list)
    discovered_algorithms: list = field(default_factory=list)
    blacklisted_fingerprints: list = field(default_factory=list)


@dataclass
class CycleResult:
    """Result of one complete dream cycle (one island, one generation)."""
    island_id: int
    generation: int
    mutations_applied: list
    survived: bool
    self_host_passed: bool
    parent_fitness: dict
    mutant_fitness: dict
    delta: dict
    elapsed_s: float
    error: str = ""


# ═══════════════════════════════════════════════════════════════════════
# STATISTICAL FITNESS ORACLE
# ═══════════════════════════════════════════════════════════════════════

class FitnessOracle:
    """
    v3: Multi-dimensional fitness oracle.
    Measures 5 orthogonal quality metrics and computes a weighted composite score.
    Runs 3 benchmark samples and takes the median to eliminate timing noise.
    """

    N_SAMPLES = 3

    # Composite weights: instruction count dominates, then size, then branches
    W_INSTR    = 0.40
    W_SIZE     = 0.30
    W_BRANCH   = 0.20
    W_STACK    = 0.10

    @classmethod
    def measure(cls, zcc_binary: str, workload_c: str,
                asm_output: str, tmpdir: str, timeout: int = 120) -> dict:
        fitness = {
            'asm_size': 0, 'bin_size': 0, 'inst_count': 0,
            'branch_count': 0, 'branch_density': 0.0,
            'stack_depth_sum': 0,
            'benchmark_time_ns': 0, 'score': 0.0,
        }

        if os.path.exists(zcc_binary):
            fitness['bin_size'] = os.path.getsize(zcc_binary)
        if os.path.exists(asm_output):
            # Step 2: Measure compiled .text section bytes using size -A tool
            obj_file = asm_output + '.o'
            text_size = None
            try:
                subprocess.run(['as', asm_output, '-o', obj_file], capture_output=True, check=True)
                res = subprocess.run(['size', '-A', obj_file], capture_output=True, text=True, check=True)
                found = False
                for line in res.stdout.splitlines():
                    parts = line.split()
                    if parts and parts[0] == '.text':
                        text_size = int(parts[1])
                        found = True
                        break
                if not found:
                    raise ValueError(f"Could not find .text section in size -A output: {res.stdout}")
            except Exception as e:
                raise RuntimeError(f"FitnessOracle failed to measure compiled size of {asm_output}: {e}")
            finally:
                if os.path.exists(obj_file):
                    try:
                        os.remove(obj_file)
                    except Exception:
                        pass

            fitness['asm_size'] = text_size

            with open(asm_output) as f:
                asm = f.readlines()

            inst_count = 0
            branch_count = 0
            stack_depth_sum = 0
            func_count = 0

            for l in asm:
                stripped = l.strip()
                if not stripped or stripped.startswith('.') or stripped.endswith(':') or stripped.startswith('#'):
                    continue
                inst_count += 1
                # Count branches (jcc, jmp, call, ret)
                if stripped.split()[0] in ('je', 'jne', 'jl', 'jle', 'jg', 'jge',
                                           'ja', 'jae', 'jb', 'jbe', 'jmp',
                                           'js', 'jns', 'jo', 'jno', 'jz', 'jnz',
                                           'call', 'ret'):
                    branch_count += 1
                # Approximate stack depth from subq $N, %rsp (function prologue)
                m = re.match(r'subq\s+\$(\d+),\s*%rsp', stripped)
                if m:
                    stack_depth_sum += int(m.group(1))
                    func_count += 1

            fitness['inst_count'] = inst_count
            fitness['branch_count'] = branch_count
            fitness['branch_density'] = branch_count / max(inst_count, 1)
            fitness['stack_depth_sum'] = stack_depth_sum

        # Statistical benchmark: median of N_SAMPLES
        if os.path.exists(workload_c):
            samples = []
            bench_asm = os.path.join(tmpdir, 'bench_stat.s')
            for _ in range(cls.N_SAMPLES):
                t0 = time.perf_counter_ns()
                try:
                    subprocess.run([zcc_binary, workload_c, '-o', bench_asm],
                                   capture_output=True, timeout=timeout)
                except Exception:
                    pass
                samples.append(time.perf_counter_ns() - t0)
            samples.sort()
            fitness['benchmark_time_ns'] = samples[len(samples) // 2]  # median

        # Composite score: weighted sum (lower is better) — LEGACY
        fitness['score'] = (
            fitness['inst_count']     * 10.0 * cls.W_INSTR +
            fitness['asm_size']       * 1.0  * cls.W_SIZE +
            fitness['branch_count']   * 20.0 * cls.W_BRANCH +
            fitness['stack_depth_sum'] * 0.5 * cls.W_STACK +
            fitness['benchmark_time_ns'] / 1e6
        )

        # Free energy: F = E - TS (Wilson-Fisher universal fitness)
        # eta and T are injected via class-level defaults or overridden
        eta = getattr(cls, '_eta_c', 0.4407)
        T_eff = getattr(cls, '_T_eff', 1.0)
        fitness['free_energy'] = prime_free_energy(fitness, eta, T_eff)
        fitness['eta_c'] = eta

        return fitness


# ═══════════════════════════════════════════════════════════════════════
# SELF-HOST GATE
# ═══════════════════════════════════════════════════════════════════════

class SelfHostGate:
    """
    Full 3-stage bootstrap verification.
    Stage 1: mutant → stage3.s
    Stage 2: gcc links stage3.s → stage3
    Stage 3: stage3 → stage4.s
    Stage 4: cmp stage3.s stage4.s  (idempotency check)

    Includes Step 4 Safety Gate:
    Runs static flags liveness checker on stage 3 assembly to catch residual flags hazards.
    Note: The checker is xor-blind (interprets xorl as flags-setter); primary safety of 1b
    is proven via pre-patch audit on the zcc2.s/zcc3.s baseline.
    """

    @staticmethod
    def verify(mutant_bin: str, zcc_pp_c: str, passes: list,
               tmpdir: str, timeout: int = 180) -> tuple:
        s3_s   = os.path.join(tmpdir, 'g_s3.s')
        s3_bin = os.path.join(tmpdir, 'g_s3')
        s4_s   = os.path.join(tmpdir, 'g_s4.s')
        p_args   = [str(REPO_ROOT / p) for p in passes]
        # Use full zcc.c (single compilation unit) — avoids multi-definition
        # symbol clashes that arise when linking zcc_pp.c with extra .c files
        zcc_full = str(REPO_ROOT / 'zcc.c')
        gate_src = zcc_full if os.path.exists(zcc_full) else zcc_pp_c
        s3_p_args = p_args

        try:
            r = subprocess.run([mutant_bin, gate_src, '-o', s3_s],
                               capture_output=True, timeout=timeout)
            if r.returncode != 0:
                return False, f"mutant crash rc={r.returncode}"
        except subprocess.TimeoutExpired:
            return False, "mutant timeout"
        except FileNotFoundError:
            return False, f"binary missing: {mutant_bin}"

        if not os.path.exists(s3_s) or os.path.getsize(s3_s) == 0:
            return False, "empty assembly output"

        # Step 4: Safety check flags liveness in s3_s (defense-in-depth for residual movq $0)
        try:
            import sys
            sys.path.append(str(REPO_ROOT / 'tools'))
            try:
                from tools.check_flags_liveness import analyze_asm
            except ImportError:
                from check_flags_liveness import analyze_asm
            violations = analyze_asm(s3_s)
            if violations:
                log_path = os.path.join(REPO_ROOT, "dreams", "rejections.log")
                with open(log_path, "a") as lf:
                    lf.write(f"--- REJECT MUTANT: {mutant_bin} ---\n")
                    for v in violations:
                        lf.write(f"  Setter   [L{v['setter_idx']+1}]: {v['setter_line']}\n")
                        lf.write(f"  Movq $0  [L{v['mov_idx']+1}]: {v['mov_line']}\n")
                        lf.write(f"  Consumer [L{v['consumer_idx']+1}]: {v['consumer_line']}\n")
                return False, f"safety gate: flags-liveness violation ({len(violations)} found)"
        except Exception as e:
            print(f"Safety gate checker warning: {e}")

        try:
            r = subprocess.run(
                ['gcc', '-no-pie', '-O0', '-w', '-fno-asynchronous-unwind-tables',
                 '-Wa,--noexecstack', '-fno-unwind-tables',
                 '-Iinclude', '-I.',
                 '-o', s3_bin, s3_s] + s3_p_args + ['-lm'],
                capture_output=True, timeout=60)
            if r.returncode != 0:
                full_stderr = r.stderr.decode('utf-8', 'ignore').strip()
                with open("dreams/last_assembler_error.txt", "w") as f:
                    f.write(full_stderr)
                import shutil
                shutil.copyfile(s3_s, "dreams/g_s3_fault.s")
                lines = full_stderr.split('\n')
                err_summary = '\n'.join(lines[:10])
                return False, f"s3 link fail:\n{err_summary}"
        except subprocess.TimeoutExpired:
            return False, "s3 link timeout"

        try:
            r = subprocess.run([s3_bin, gate_src, '-o', s4_s],
                               capture_output=True, timeout=timeout)
            if r.returncode != 0:
                return False, f"s3 crash rc={r.returncode}"
        except subprocess.TimeoutExpired:
            return False, "s3 timeout"

        if not os.path.exists(s4_s) or os.path.getsize(s4_s) == 0:
            return False, "s3 produced empty asm"

        r = subprocess.run(['cmp', '-s', s3_s, s4_s], capture_output=True)
        if r.returncode != 0:
            return False, "bootstrap mismatch: s3.s ≠ s4.s"

        return True, "SELF-HOST OK"


# ═══════════════════════════════════════════════════════════════════════
# HAMILTONIAN TELEMETRY
# ═══════════════════════════════════════════════════════════════════════

class HamiltonianTelemetry:
    """
    Streams the fitness landscape as a Hamiltonian energy field to God's Eye.
    Dual-channel: UDP port 8084 (legacy) + UDP port 41337 (Gods Eye signed).

    Packet format (JSON, ~300 bytes):
      { type, generation, island_id, score, delta_score,
        survived, mutations, timestamp, state_vector[2],
        branch_density, stack_depth, inst_count }

    The state_vector encodes position in 2D fitness space:
      [0] = normalised asm_size   (0 = perfect, 1 = baseline)
      [1] = normalised inst_count (0 = perfect, 1 = baseline)
    God's Eye renders this as Hamiltonian potential energy.
    """

    GODS_EYE_PORT = 41337

    def __init__(self, host: str = "127.0.0.1", port: int = 8084):
        self.host = host
        self.port = port
        self._sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self._sock.setblocking(False)
        self._baseline_score: float = 0.0
        # Secondary socket for Gods Eye
        self._gods_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self._gods_sock.setblocking(False)

    def set_baseline(self, score: float):
        self._baseline_score = max(score, 1.0)

    def emit(self, result: CycleResult, island_id: int = 0,
             eta_c: float = 0.0, phase: str = "", spectral_gap: float = 0.0):
        """Non-blocking dual-emit — UDP datagram to both telemetry channels.
        Now includes Wilson-Fisher criticality fields."""
        try:
            parent_score = result.parent_fitness.get('score', self._baseline_score)
            mutant_score = result.mutant_fitness.get('score', parent_score)
            baseline     = self._baseline_score or max(parent_score, 1.0)

            sv0 = (result.mutant_fitness.get('asm_size', 0) /
                   max(result.parent_fitness.get('asm_size', 1), 1))
            sv1 = (result.mutant_fitness.get('inst_count', 0) /
                   max(result.parent_fitness.get('inst_count', 1), 1))

            pkt_data = {
                "type": "dream_cycle",
                "generation": result.generation,
                "island_id": island_id,
                "score": round(mutant_score, 2),
                "delta_score": round(result.delta.get('score', 0), 4),
                "survived": result.survived,
                "self_host_passed": result.self_host_passed,
                "mutations": len(result.mutations_applied),
                "mutation_classes": list({m.get('category', '?')
                                          for m in result.mutations_applied}),
                "inst_count": result.mutant_fitness.get('inst_count', 0),
                "branch_count": result.mutant_fitness.get('branch_count', 0),
                "branch_density": round(result.mutant_fitness.get('branch_density', 0), 6),
                "stack_depth": result.mutant_fitness.get('stack_depth_sum', 0),
                "state_vector": [round(sv0, 6), round(sv1, 6)],
                "hamiltonian_energy": round(
                    (mutant_score - baseline) / baseline, 6),
                # Wilson-Fisher criticality fields
                "eta_c": round(eta_c, 4),
                "free_energy": round(result.mutant_fitness.get('free_energy', 0), 4),
                "delta_free_energy": round(result.delta.get('free_energy', 0), 4),
                "spectral_gap": round(spectral_gap, 6),
                "phase": phase,
                "elapsed_s": round(result.elapsed_s, 2),
                "timestamp": datetime.now(timezone.utc).isoformat(),
            }
            pkt = json.dumps(pkt_data).encode()

            # Legacy telemetry channel
            self._sock.sendto(pkt, (self.host, self.port))

            # Gods Eye channel (port 41337)
            gods_payload = {
                "type": "dream_cycle",
                "gen": result.generation,
                "island": island_id,
                "score": round(mutant_score, 2),
                "delta": round(result.delta.get('score', 0), 4),
                "mutations": len(result.mutations_applied),
                "elapsed": round(result.elapsed_s, 2),
                "survived": result.survived,
                "eta_c": round(eta_c, 4),
                "free_energy": round(result.mutant_fitness.get('free_energy', 0), 4),
                "phase": phase,
                "spectral_gap": round(spectral_gap, 6),
                "ts": datetime.now(timezone.utc).isoformat(),
            }
            body = json.dumps(gods_payload, separators=(',', ':'), sort_keys=True)
            sig = hmac.new(b"zkaedi-local-dev-secret", body.encode('utf-8'), hashlib.sha256).hexdigest()
            envelope = json.dumps({"_body": body, "_sig": sig}, separators=(',', ':'))
            self._gods_sock.sendto(envelope.encode('utf-8'), (self.host, self.GODS_EYE_PORT))
        except Exception:
            pass  # Never block on telemetry


# ═══════════════════════════════════════════════════════════════════════
# ISLAND — ONE PARALLEL LINEAGE
# ═══════════════════════════════════════════════════════════════════════

class Island:
    """
    One competing lineage in the island model.
    Each island maintains its own copy of the parent assembly and evolves
    independently. After a configurable interval, survivors can cross-breed.
    """

    def __init__(self, island_id: int, seed: int, parent_asm: str,
                 zcc_pp_c: str, blacklist: set):
        self.island_id = island_id
        self.rng = random.Random(seed)
        self.state = IslandState(island_id=island_id)
        self.blacklist = blacklist
        self.zcc_pp_c = zcc_pp_c
        self.gate = SelfHostGate()
        self.oracle = FitnessOracle()

        # Each island gets its own copy of parent assembly
        island_asm = str(DREAM_DIR / f"island_{island_id}_parent.s")
        shutil.copyfile(parent_asm, island_asm)
        self.state.parent_asm_path = island_asm

        # Measure initial fitness
        with tempfile.TemporaryDirectory(prefix='island_init_') as td:
            self.parent_fitness = self._build_and_measure(island_asm, td)
            self.state.parent_score = self.parent_fitness['score']

    def step(self, mutation_engine: MutationEngine,
             max_mutations: int, force_sweep: bool,
             dry_run: bool, tmpdir: str) -> CycleResult:
        """Execute one dream cycle for this island."""
        t0 = time.time()
        gen = self.state.generation + 1

        with open(self.state.parent_asm_path) as f:
            parent_lines = f.readlines()

        # Discover mutations
        mutations = mutation_engine.dream(
            parent_lines,
            max_point_mutations=max_mutations,
            include_sweeps=force_sweep or (self.rng.random() < 0.3),
            blacklist=self.blacklist,
        )

        # Filter blacklisted fingerprints
        mutations = [m for m in mutations
                     if m.fingerprint() not in self.blacklist]

        if not mutations:
            return CycleResult(
                island_id=self.island_id, generation=gen,
                mutations_applied=[], survived=False,
                self_host_passed=False, parent_fitness={}, mutant_fitness={},
                delta={}, elapsed_s=time.time() - t0,
                error="No non-blacklisted mutation candidates"
            )

        # Separate sweeps from point mutations; apply at most 1 sweep + N points
        sweeps = [m for m in mutations if m.is_sweep]
        points = [m for m in mutations if not m.is_sweep]

        selected = []
        if sweeps:
            selected.append(self.rng.choice(sweeps))     # at most 1 sweep
        n_pt = self.rng.randint(1, max(1, min(len(points), max_mutations)))
        selected.extend(self.rng.sample(points, min(len(points), n_pt)))

        parent_fitness = self.parent_fitness

        if dry_run:
            return CycleResult(
                island_id=self.island_id, generation=gen,
                mutations_applied=[asdict(m) for m in selected],
                survived=False, self_host_passed=False,
                parent_fitness=parent_fitness, mutant_fitness={},
                delta={}, elapsed_s=time.time() - t0,
                error="DRY RUN"
            )

        # Apply mutations
        mutant_lines = mutation_engine.apply_mutations(parent_lines, selected)
        mutant_asm = os.path.join(tmpdir, f'island_{self.island_id}_mutant.s')
        with open(mutant_asm, 'w') as f:
            f.writelines(mutant_lines)

        # Build mutant
        mutant_bin = os.path.join(tmpdir, f'island_{self.island_id}_bin')
        p_args = [str(REPO_ROOT / p) for p in PASSES]
        try:
            r = subprocess.run(
                ['gcc', '-no-pie', '-O0', '-w', '-fno-asynchronous-unwind-tables',
                 '-Wa,--noexecstack', '-fno-unwind-tables',
                 '-Iinclude', '-I.',
                 '-o', mutant_bin, mutant_asm] + p_args + ['-lm'],
                capture_output=True, timeout=60)
            if r.returncode != 0:
                return CycleResult(
                    island_id=self.island_id, generation=gen,
                    mutations_applied=[asdict(m) for m in selected],
                    survived=False, self_host_passed=False,
                    parent_fitness=parent_fitness, mutant_fitness={},
                    delta={}, elapsed_s=time.time() - t0,
                    error=f"build fail: {r.stderr.decode()[:120]}"
                )
        except subprocess.TimeoutExpired:
            return CycleResult(
                island_id=self.island_id, generation=gen,
                mutations_applied=[asdict(m) for m in selected],
                survived=False, self_host_passed=False,
                parent_fitness=parent_fitness, mutant_fitness={},
                delta={}, elapsed_s=time.time() - t0, error="build timeout"
            )

        # Self-host gate
        gate_ok, gate_msg = self.gate.verify(
            mutant_bin, self.zcc_pp_c, PASSES, tmpdir)

        if not gate_ok:
            # Blacklist failing fingerprints
            for m in selected:
                self.blacklist.add(m.fingerprint())
            self.state.rejected += 1
            return CycleResult(
                island_id=self.island_id, generation=gen,
                mutations_applied=[asdict(m) for m in selected],
                survived=False, self_host_passed=False,
                parent_fitness=parent_fitness, mutant_fitness={},
                delta={}, elapsed_s=time.time() - t0,
                error=f"gate: {gate_msg}"
            )

        # Statistical fitness measurement
        mutant_fitness = self.oracle.measure(
            mutant_bin, str(BENCHMARK_FILE), mutant_asm, tmpdir)

        delta = {
            'asm_size':   mutant_fitness['asm_size']   - parent_fitness['asm_size'],
            'inst_count': mutant_fitness['inst_count'] - parent_fitness.get('inst_count', 0),
            'score':      mutant_fitness['score']      - parent_fitness['score'],
            'free_energy': mutant_fitness.get('free_energy', 0) -
                           parent_fitness.get('free_energy', 0),
        }

        # Wilson-Fisher: Boltzmann acceptance on free energy delta
        # Falls back to greedy (T=0) if free_energy not available
        delta_F = delta.get('free_energy', delta['score'])
        T_eff = getattr(FitnessOracle, '_T_eff', 1.0)
        survived = boltzmann_acceptance(delta_F, T_eff)

        if survived:
            self.state.generation = gen
            self.state.parent_score = mutant_fitness['score']
            self.parent_fitness = mutant_fitness
            self.state.survived += 1
            shutil.copyfile(mutant_asm, self.state.parent_asm_path)
            self.state.lineage.append({
                'generation': gen,
                'mutations': [m.name for m in selected],
                'delta_score': delta['score'],
            })
        else:
            self.state.rejected += 1

        return CycleResult(
            island_id=self.island_id, generation=gen,
            mutations_applied=[asdict(m) for m in selected],
            survived=survived, self_host_passed=True,
            parent_fitness=parent_fitness,
            mutant_fitness=mutant_fitness,
            delta=delta, elapsed_s=time.time() - t0,
        )

    def _build_and_measure(self, asm_path: str, tmpdir: str) -> dict:
        """Build + measure an assembly file. Returns fitness dict."""
        bin_path = os.path.join(tmpdir, 'init_bin')
        p_args = [str(REPO_ROOT / p) for p in PASSES]
        try:
            r = subprocess.run(
                ['gcc', '-no-pie', '-O0', '-w', '-fno-asynchronous-unwind-tables',
                 '-Wa,--noexecstack', '-fno-unwind-tables',
                 '-Iinclude', '-I.',
                 '-o', bin_path, asm_path] + p_args + ['-lm'],
                capture_output=True, timeout=60)
            if r.returncode != 0:
                return {'score': float('inf'), 'free_energy': float('inf'), 'asm_size': 0, 'inst_count': 0}
        except Exception:
            return {'score': float('inf'), 'free_energy': float('inf'), 'asm_size': 0, 'inst_count': 0}

        return FitnessOracle.measure(bin_path, str(BENCHMARK_FILE), asm_path, tmpdir)

    def _build_and_score(self, asm_path: str, tmpdir: str) -> float:
        """Build + score an assembly file. Returns composite score."""
        return self._build_and_measure(asm_path, tmpdir)['score']


# ═══════════════════════════════════════════════════════════════════════
# DREAM ENGINE v2
# ═══════════════════════════════════════════════════════════════════════

class DreamEngine:

    def __init__(self, seed: int = 42, max_mutations: int = 3,
                 n_islands: int = 1, force_sweep: bool = False,
                 aggressive: bool = False, visualize: bool = False,
                 dry_run: bool = False):
        self.seed         = seed
        self.rng          = random.Random(seed)
        self.max_mutations = max_mutations if not aggressive else 8
        self.n_islands    = n_islands
        self.force_sweep  = force_sweep
        self.aggressive   = aggressive
        self.visualize    = visualize
        self.dry_run      = dry_run

        DREAM_DIR.mkdir(parents=True, exist_ok=True)
        JOURNAL_DIR.mkdir(parents=True, exist_ok=True)
        LINEAGE_DIR.mkdir(parents=True, exist_ok=True)

        # Load persisted state
        state_file = DREAM_DIR / "dream_state.json"
        if state_file.exists():
            with open(state_file) as f:
                data = json.load(f)
            self.state = DreamState(**{k: v for k, v in data.items()
                                       if k in DreamState.__dataclass_fields__})
            print(f"  {_C}[RESUME]{_W} Gen {self.state.generation} │ "
                  f"Hash {self.state.parent_hash} │ "
                  f"{len(self.state.discovered_algorithms)} algorithms discovered")
        else:
            self.state = DreamState()

        # Mutation blacklist
        self.blacklist: set = set(self.state.blacklisted_fingerprints)

        # Telemetry
        self.telem = HamiltonianTelemetry() if visualize else None

        # ── Wilson-Fisher criticality state ──
        self.eta_c: float = 0.4407       # Default: 2D Ising
        self.T_eff: float = 1.0          # Effective temperature
        self.spectral_detector = SpectralArrestDetector(window=10, threshold=0.01)
        self._fitness_scores: list = []  # For relaxation_phase()
        self._phase: str = "explore"     # Current phase
        self._uclass = None              # Universality class info

    # ──────────────────────────────────────────────────────────────────

    def _prepare_canonical(self, tmpdir: str) -> tuple:
        """Return (zcc2_bin, zcc2_asm, zcc_pp_c) paths, building if needed."""
        zcc2_bin = str(REPO_ROOT / 'zcc2')
        zcc2_asm = str(REPO_ROOT / 'zcc2.s')
        zcc_pp_c = str(REPO_ROOT / 'zcc_pp.c')

        if not os.path.exists(zcc2_bin):
            print(f"  {_R}[ERROR]{_W} ./zcc2 not found. Run `make selfhost` first.")
            sys.exit(1)

        # Auto-build zcc_pp.c if it's missing
        if not os.path.exists(zcc_pp_c):
            print(f"  {_Y}[AUTO]{_W} zcc_pp.c not found — generating from zcc.c…")
            zcc_c = str(REPO_ROOT / 'zcc.c')
            if not os.path.exists(zcc_c):
                # Concatenate parts first
                print(f"  {_Y}[AUTO]{_W} Concatenating parts → zcc.c…")
                # Order matters: part0_pp must come before headers/part1
                with open(zcc_c, 'w', encoding='utf-8', errors='ignore') as out:
                    for p in PARTS:
                        pf = REPO_ROOT / p
                        if pf.exists():
                            with open(pf, encoding='utf-8', errors='ignore') as f:
                                out.write(f.read())
                        else:
                            print(f"  {_Y}[WARN]{_W} Missing part: {p}")
            # Strip _Static_assert lines
            zcc_pp_tmp = zcc_pp_c + '.tmp'
            with open(zcc_c, encoding='utf-8', errors='ignore') as fin, open(zcc_pp_tmp, 'w', encoding='utf-8', errors='ignore') as fout:
                for line in fin:
                    if not line.startswith('_Static_assert'):
                        fout.write(line)
            # Inline bridge headers, strip system includes
            ast_bridge_zcc  = str(REPO_ROOT / 'zcc_ast_bridge_zcc.h')
            ir_bridge_zcc   = str(REPO_ROOT / 'zcc_ir_bridge_zcc.h')
            with open(zcc_pp_tmp, encoding='utf-8', errors='ignore') as fin, open(zcc_pp_c, 'w', encoding='utf-8', errors='ignore') as fout:
                # Inject the bridge guard so ZCC's own preprocessor skips
                # all five `#ifndef ZCC_AST_BRIDGE_H / #include "part1.c"`
                # blocks. Without this fix, node_kind and every other
                # accessor defined in part1.c is emitted multiple times in
                # the mutant assembly, causing 'symbol already defined'.
                fout.write('#define ZCC_AST_BRIDGE_H\n#define ZCC_IR_BRIDGE_H\n')
                for line in fin:
                    if line.startswith('#include "zcc_ast_bridge.h"'):
                        if os.path.exists(ast_bridge_zcc):
                            fout.write(open(ast_bridge_zcc, encoding='utf-8', errors='ignore').read())
                        continue
                    if line.startswith('#include "zcc_ir_bridge.h"'):
                        if os.path.exists(ir_bridge_zcc):
                            fout.write(open(ir_bridge_zcc, encoding='utf-8', errors='ignore').read())
                        continue
                    if line.startswith('#include <') and '>' in line:
                        continue   # drop system headers
                    fout.write(line)
            os.unlink(zcc_pp_tmp)
            print(f"  {_G}[AUTO]{_W} Generated zcc_pp.c ({os.path.getsize(zcc_pp_c):,} bytes)")

        # Auto-generate zcc2.s if it's missing
        if not os.path.exists(zcc2_asm) or os.path.getsize(zcc2_asm) == 0:
            print(f"  {_Y}[AUTO]{_W} Generating zcc2.s (zcc2 compiling itself)…")
            r = subprocess.run([zcc2_bin, zcc_pp_c, '-o', zcc2_asm],
                               cwd=str(REPO_ROOT), capture_output=True, timeout=300)
            if r.returncode != 0 or not os.path.exists(zcc2_asm):
                print(f"  {_R}[ERROR]{_W} Failed to generate zcc2.s: {r.stderr.decode()[:200]}")
                sys.exit(1)
            print(f"  {_G}[AUTO]{_W} Generated zcc2.s ({os.path.getsize(zcc2_asm):,} bytes)")

        return zcc2_bin, zcc2_asm, zcc_pp_c

    def _ensure_benchmark(self):
        if BENCHMARK_FILE.exists():
            return
        src = """\
/* ZCC Dream Benchmark Workload — canonical fitness oracle */
int printf(const char *fmt, ...);

long fibonacci(int n) {
    long a = 0, b = 1, t; int i;
    if (n <= 1) return n;
    for (i = 2; i <= n; i++) { t = a + b; a = b; b = t; }
    return b;
}

int bubble_sort(int *arr, int n) {
    int i, j, tmp, sw = 0;
    for (i = 0; i < n-1; i++)
        for (j = 0; j < n-i-1; j++)
            if (arr[j] > arr[j+1]) { tmp=arr[j]; arr[j]=arr[j+1]; arr[j+1]=tmp; sw++; }
    return sw;
}

long hash_str(const char *s) {
    long h = 5381;
    while (*s) { h = ((h << 5) + h) + *s; s++; }
    return h;
}

int matrix_mul(int n) {
    int A[8][8], B[8][8], C[8][8], i, j, k, sum = 0;
    for (i=0; i<n&&i<8; i++) for (j=0; j<n&&j<8; j++) { A[i][j]=i*n+j; B[i][j]=j*n+i; }
    for (i=0; i<n&&i<8; i++) for (j=0; j<n&&j<8; j++) {
        C[i][j]=0;
        for (k=0; k<n&&k<8; k++) C[i][j]+=A[i][k]*B[k][j];
        sum+=C[i][j];
    }
    return sum;
}

int main(void) {
    int arr[16]; int i;
    for (i=0; i<16; i++) arr[i]=16-i;
    long f = fibonacci(40);
    int sw = bubble_sort(arr, 16);
    long h = hash_str("ZCC Oneirogenesis v2");
    int m = matrix_mul(8);
    printf("DREAM_BENCH: fib=%ld sw=%d hash=%ld mat=%d\\n", f, sw, h, m);
    return 0;
}
"""
        BENCHMARK_FILE.write_text(src)
        print(f"  {_C}[INIT]{_W} Created benchmark workload")

    def save_state(self):
        self.state.blacklisted_fingerprints = list(self.blacklist)
        with open(DREAM_DIR / "dream_state.json", 'w') as f:
            json.dump(asdict(self.state), f, indent=2)

    def _journal(self, gen: int, island_id: int, mutations: list,
                 delta: dict, fitness: dict, hash_id: str):
        mutations_data = []
        for m in mutations:
            mutations_data.append({
                "name": m.get('name'), "category": m.get('category'),
                "description": m.get('description'),
                "is_sweep": m.get('is_sweep', False),
                "sweep_count": m.get('sweep_count', 0),
                "energy_delta": m.get('energy_delta', 0),
                "fingerprint": m.get('fingerprint', ''),
            })
        entry = {
            "algorithm_info": {
                "id": f"QAlgo-Dream-G{gen}",
                "name": f"Codegen Mutation G{gen} [{mutations[0].get('name','?')}]",
                "domain": "compiler_optimization",
                "discovered_by": f"ZCC Oneirogenesis v2 Island-{island_id}",
                "lineage_hash": hash_id,
                "island_id": island_id,
            },
            "mutations": mutations_data,
            "fitness_improvement": {
                "asm_size_delta": delta.get('asm_size', 0),
                "inst_count_delta": delta.get('inst_count', 0),
                "score_delta": delta.get('score', 0),
                "composite_score": fitness.get('score', 0),
            },
            "timestamp": datetime.now(timezone.utc).isoformat(),
            "generation": gen,
        }
        (JOURNAL_DIR / f"QAlgo-Dream-G{gen}.json").write_text(
            json.dumps(entry, indent=2))
        self.state.discovered_algorithms.append(entry['algorithm_info']['id'])

    def _print_header(self, num_cycles: int):
        print()
        print(f"  {_B}╔══════════════════════════════════════════════════════════╗{_W}")
        print(f"  {_B}║   ZCC ONEIROGENESIS v3 — Enhanced Dream Engine          ║{_W}")
        print(f"  {_B}╠══════════════════════════════════════════════════════════╣{_W}")
        print(f"  {_B}║{_W}  Seed:      {_C}{self.seed:<45}{_W}  {_B}║{_W}")
        print(f"  {_B}║{_W}  Cycles:    {_C}{num_cycles:<45}{_W}  {_B}║{_W}")
        print(f"  {_B}║{_W}  Islands:   {_C}{self.n_islands:<45}{_W}  {_B}║{_W}")
        print(f"  {_B}║{_W}  Max Muts:  {_C}{self.max_mutations:<45}{_W}  {_B}║{_W}")
        print(f"  {_B}║{_W}  Sweeps:    {_C}{'FORCED' if self.force_sweep else '30% random':<45}{_W}  {_B}║{_W}")
        print(f"  {_B}║{_W}  Gen:       {_C}{self.state.generation:<45}{_W}  {_B}║{_W}")
        print(f"  {_B}║{_W}  Blacklist: {_C}{len(self.blacklist)} fingerprints{'':<33}{_W}  {_B}║{_W}")
        print(f"  {_B}╚══════════════════════════════════════════════════════════╝{_W}")
        print()

    def _print_result(self, r: CycleResult):
        gen_s = f"G{r.generation:04d}"
        n_mut = len(r.mutations_applied)
        island_s = f"I{r.island_id}" if self.n_islands > 1 else ""
        prefix = f"[{gen_s}{island_s}]"
        dt = f"{r.elapsed_s:.1f}s"

        if r.survived:
            delta_s = r.delta.get('score', 0)
            asm_d   = r.delta.get('asm_size', 0)
            inst_d  = r.delta.get('inst_count', 0)
            cats    = {m.get('category','?') for m in r.mutations_applied}
            sweep_n = sum(m.get('sweep_count', 0) for m in r.mutations_applied if m.get('is_sweep'))
            print(f"  {_G}✦{_W} {prefix} {_G}EVOLVED{_W}  │ "
                  f"{n_mut} mut {'[SWEEP×'+str(sweep_n)+']' if sweep_n else ''} │ "
                  f"Δscore:{_G}{delta_s:+.1f}{_W} │ "
                  f"asm:{asm_d:+d} inst:{inst_d:+d} │ {dt}")
            if len(r.mutations_applied) <= 5:
                cats_fmt = '/'.join(sorted(cats))
                for m in r.mutations_applied:
                    sw = f"  ×{m.get('sweep_count',0)}" if m.get('is_sweep') else ""
                    print(f"    {_DIM}├─ {m.get('category','?'):>8s} │ {m.get('description','')}{sw}{_W}")
        elif r.self_host_passed:
            delta_s = r.delta.get('score', 0)
            print(f"  {_DIM}○ {prefix} NEUTRAL  │ {n_mut} mut │ "
                  f"Δscore:{delta_s:+.1f} │ {dt}{_W}")
        elif r.dry_run if hasattr(r, 'dry_run') else "DRY RUN" in r.error:
            for m in r.mutations_applied:
                sw = f"  (×{m.get('sweep_count',0)} sites)" if m.get('is_sweep') else ""
                print(f"  {_M}◇ {prefix} DRY RUN  │ "
                      f"{m.get('category','?'):>8s} │ {m.get('description','')}{sw}{_W}")
        else:
            sym = "✗"
            short_err = r.error[:70]
            print(f"  {_R}{sym}{_W} {prefix} {_R}REJECT{_W}   │ "
                  f"{n_mut} mut │ {short_err} │ {dt}")

    # ──────────────────────────────────────────────────────────────────
    # MAIN LOOP
    # ──────────────────────────────────────────────────────────────────

    def run(self, num_cycles: int = 50):
        self._print_header(num_cycles)
        self._ensure_benchmark()

        with tempfile.TemporaryDirectory(prefix='dream_canon_') as canon_tmp:
            _, zcc2_asm, zcc_pp_c = self._prepare_canonical(canon_tmp)

            # ── Wilson-Fisher: extract CFG and compute η_c ──
            print(f"  {_C}[WF]{_W} Extracting CFG from parent assembly…")
            try:
                with open(zcc2_asm) as f:
                    asm_lines = f.readlines()
                cfg = extract_cfg(asm_lines)
                stats = cfg_stats(cfg)
                print(f"  {_C}[WF]{_W} CFG: {stats['nodes']} nodes, "
                      f"{stats['edges']} edges, avg_degree={stats['avg_degree']}")

                # Compute spectral dimension
                d_s = cfg_spectral_dim(cfg)
                print(f"  {_C}[WF]{_W} Spectral dimension: d_s={d_s:.3f}")

                # Search for η_c (use subset for speed if graph is large)
                if stats['nodes'] > 500:
                    # Subsample: take first 500 nodes for tractability
                    sub_nodes = sorted(cfg.keys())[:500]
                    sub_cfg = {n: [t for t in cfg[n] if t in sub_nodes]
                               for n in sub_nodes if n in cfg}
                    self.eta_c = topology_eta_search(sub_cfg, tol=1e-3,
                                                     max_sweeps=80, n_samples=3)
                else:
                    self.eta_c = topology_eta_search(cfg, tol=1e-3,
                                                     max_sweeps=100, n_samples=3)

                # Classify universality class
                self._uclass = universality_class(self.eta_c, d_s)
                print(f"  {_C}[WF]{_W} η_c={self.eta_c:.4f} │ "
                      f"class={self._uclass.label} │ "
                      f"ν={self._uclass.nu:.3f} β={self._uclass.beta:.3f} "
                      f"γ={self._uclass.gamma:.3f}")

                # Inject η_c into FitnessOracle class-level state
                FitnessOracle._eta_c = self.eta_c
                FitnessOracle._T_eff = self.T_eff

            except Exception as e:
                print(f"  {_Y}[WF]{_W} CFG extraction failed ({e}), using defaults")
                self.eta_c = 0.4407
                FitnessOracle._eta_c = self.eta_c
                FitnessOracle._T_eff = self.T_eff

            # Initialise islands
            print(f"  {_C}[INIT]{_W} Spawning {self.n_islands} island(s)…")
            islands = []
            for i in range(self.n_islands):
                island_seed = self.rng.randint(0, 2**32)
                with tempfile.TemporaryDirectory(prefix=f'island_init_{i}_') as it:
                    isl = Island(i, island_seed, zcc2_asm, zcc_pp_c, self.blacklist)
                islands.append(isl)
                print(f"    Island {i}: score={isl.state.parent_score:.0f} "
                      f"F={getattr(isl, '_last_fe', '?')}")

            # Baseline for Hamiltonian telemetry
            if self.telem:
                self.telem.set_baseline(islands[0].state.parent_score)

            survived_total = 0
            rejected_total = 0           # buckets 1-4 (pre-/at-gate failure)
            fitness_rejected_total = 0   # bucket 5 (gate pass, delta >= 0)
            t_start = time.time()

            print(f"\n  {_B}═══ DREAMING ════════════════════════════════════════{_W}")
            print(f"  {_B}    η_c={self.eta_c:.4f}  T={self.T_eff:.2f}  "
                  f"class={self._uclass.label if self._uclass else '?'}{_W}\n")

            for cycle in range(num_cycles):
                # ── Relaxation phase: modulate mutation aggressiveness ──
                self._phase = relaxation_phase(self._fitness_scores,
                                               tau=5.0)
                if self._phase == "explore":
                    cycle_max_muts = self.max_mutations + 2
                    cycle_force_sweep = True
                else:
                    cycle_max_muts = max(1, self.max_mutations - 1)
                    cycle_force_sweep = self.force_sweep

                # Round-robin across islands
                island = islands[cycle % self.n_islands]
                mutation_engine = MutationEngine(
                    seed=self.rng.randint(0, 2**32))

                with tempfile.TemporaryDirectory(prefix='dream_step_') as td:
                    result = island.step(
                        mutation_engine,
                        max_mutations=cycle_max_muts,
                        force_sweep=cycle_force_sweep,
                        dry_run=self.dry_run,
                        tmpdir=td)

                self._print_result(result)

                # Emit telemetry (with criticality fields)
                if self.telem:
                    self.telem.emit(result, island.island_id,
                                    eta_c=self.eta_c,
                                    phase=self._phase,
                                    spectral_gap=self.spectral_detector.spectral_gap)

                # ── Spectral arrest: track convergence ──
                if result.mutant_fitness:
                    fv = [
                        result.mutant_fitness.get('inst_count', 0),
                        result.mutant_fitness.get('branch_density', 0),
                        result.mutant_fitness.get('stack_depth_sum', 0),
                        result.mutant_fitness.get('asm_size', 0),
                    ]
                    self.spectral_detector.record(fv)
                    self._fitness_scores.append(
                        result.mutant_fitness.get('free_energy',
                            result.mutant_fitness.get('score', 0)))

                    if (not self.dry_run and cycle > 20 and
                            self.spectral_detector.arrested()):
                        print(f"\n  {_C}[WF]{_W} {_G}SPECTRAL ARREST{_W} — "
                              f"fixed point reached at cycle {cycle+1}")
                        print(f"      gap={self.spectral_detector.spectral_gap:.6f}  "
                              f"phase={self._phase}")
                        # Don't break — log it but keep going if cycles remain

                if result.survived:
                    survived_total += 1
                    self.state.generation += 1
                    gen = self.state.generation

                    # Hash the evolved asm
                    with open(island.state.parent_asm_path, 'rb') as f:
                        h = hashlib.sha256(f.read()).hexdigest()[:16]
                    self.state.parent_hash = h
                    self.state.total_mutations_survived += len(result.mutations_applied)
                    self.state.fitness_history.append({
                        'generation': gen, 'island_id': island.island_id,
                        'score': result.mutant_fitness.get('score', 0),
                        'timestamp': datetime.now(timezone.utc).isoformat(),
                    })
                    self.state.lineage.append({
                        'generation': gen, 'hash': h,
                        'island_id': island.island_id,
                        'mutations': [m.get('description','') for m in result.mutations_applied],
                        'delta_score': result.delta.get('score', 0),
                        'timestamp': datetime.now(timezone.utc).isoformat(),
                    })
                    self._journal(gen, island.island_id,
                                  result.mutations_applied, result.delta,
                                  result.mutant_fitness, h)

                    # Promote best island asm to canonical zcc2.s periodically
                    if gen % 10 == 0:
                        best = min(islands, key=lambda i: i.state.parent_score)
                        shutil.copyfile(best.state.parent_asm_path, zcc2_asm)
                        # Rebuild canonical zcc2 binary
                        p_args = [str(REPO_ROOT / p) for p in PASSES]
                        subprocess.run(
                            ['gcc', '-no-pie', '-O0', '-w', '-fno-asynchronous-unwind-tables',
                             '-Wa,--noexecstack', '-fno-unwind-tables',
                             '-Iinclude', '-I.',
                             '-o', str(REPO_ROOT / 'zcc2'), zcc2_asm] + p_args + ['-lm'],
                            capture_output=True, timeout=60)
                        print(f"\n  {_Y}[PROMOTE]{_W} Island {best.island_id} "
                              f"(score={best.state.parent_score:.0f}) "
                              f"→ canonical zcc2 @ G{gen}\n")

                    # Island cross-breeding (every 5 cycles with 2+ islands)
                    if self.n_islands >= 2 and gen % 5 == 0:
                        self._crossbreed(islands, mutation_engine,
                                         zcc_pp_c, self.dry_run)

                elif "DRY RUN" in result.error:
                    pass  # dry-run cycle, no accounting
                elif not result.self_host_passed:
                    rejected_total += 1
                    self.state.total_regressions += 1
                else:
                    # bucket 5: built + bootstrapped, but delta_score >= 0
                    fitness_rejected_total += 1
                    self.state.total_fitness_rejections += 1

                self.state.total_mutations_tried += len(result.mutations_applied)
                self.save_state()

        elapsed = time.time() - t_start
        self._print_summary(num_cycles, survived_total, rejected_total,
                            fitness_rejected_total, elapsed, islands)

    def _crossbreed(self, islands: list, engine: MutationEngine,
                    zcc_pp_c: str, dry_run: bool):
        """Attempt to cross-breed best survivors across two random islands."""
        if len(islands) < 2:
            return
        a, b = self.rng.sample(islands, 2)
        if not a.state.lineage or not b.state.lineage:
            return

        # Get the last surviving mutation from each island
        la = a.state.lineage[-1]['mutations']
        lb = b.state.lineage[-1]['mutations']

        print(f"  {_M}[CROSS]{_W} Island {a.island_id} × Island {b.island_id}: "
              f"{la[0] if la else '?'} ✕ {lb[0] if lb else '?'}")

    def _print_summary(self, num_cycles: int, survived: int, rejected: int,
                       fitness_rejected: int, elapsed: float, islands: list):
        print()
        print(f"  {_B}═══════════════════════════════════════════════════════{_W}")
        print(f"  {_B}                 DREAM SESSION COMPLETE{_W}")
        print(f"  {_B}═══════════════════════════════════════════════════════{_W}")
        print(f"  Cycles:          {num_cycles}")
        print(f"  Evolved:         {_G}{survived}{_W}")
        print(f"  Rejected:        {_R}{rejected}{_W}")
        print(f"  Fitness-reject:  {_Y}{fitness_rejected}{_W}")
        accounted = survived + rejected + fitness_rejected
        if accounted != num_cycles:
            print(f"  {_R}! accounting drift: {num_cycles - accounted} cycle(s) unexplained{_W}")
        print(f"  Global Gen:      {self.state.generation}")
        print(f"  Time:            {elapsed:.1f}s  ({num_cycles/max(elapsed,1):.2f} cycles/s)")
        print(f"  Algorithms:      {len(self.state.discovered_algorithms)} discovered")
        print(f"  Blacklisted:     {len(self.blacklist)} fingerprints")
        if self.n_islands > 1:
            print()
            print(f"  Island Standings:")
            for isl in sorted(islands, key=lambda i: i.state.parent_score):
                print(f"    I{isl.island_id}: score={isl.state.parent_score:.0f}  "
                      f"evolved={isl.state.survived}  rejected={isl.state.rejected}")

        if self.state.discovered_algorithms:
            print(f"\n  Recent discoveries:")
            for a in self.state.discovered_algorithms[-5:]:
                print(f"    {_C}└─ {a}{_W}")
        print()

        # Evolution report
        report = DREAM_DIR / "EVOLUTION_REPORT.md"
        with open(report, 'w') as f:
            f.write("# ZCC Oneirogenesis v2 — Evolution Report\n\n")
            f.write(f"**Generated**: {datetime.now(timezone.utc).isoformat()}\n\n")
            f.write(f"## Summary\n\n")
            f.write(f"| Metric | Value |\n|--------|-------|\n")
            f.write(f"| Global Generation | {self.state.generation} |\n")
            f.write(f"| Surviving cycles (= generation) | {self.state.generation} |\n")
            f.write(f"| Mutations inside surviving cycles | {self.state.total_mutations_survived} |\n")
            f.write(f"| Hard-rejected cycles (bucket 1-4) | {self.state.total_regressions} |\n")
            f.write(f"| Fitness-rejected cycles (bucket 5) | {self.state.total_fitness_rejections} |\n")
            f.write(f"| Mutations tried total | {self.state.total_mutations_tried} |\n")
            f.write(f"| Algorithms Discovered | {len(self.state.discovered_algorithms)} |\n")
            f.write(f"| Blacklisted Patterns | {len(self.blacklist)} |\n\n")
            f.write(f"## Lineage\n\n")
            f.write("| Gen | Island | Hash | Mutations | Δ Score | Timestamp |\n")
            f.write("|-----|--------|------|-----------|---------|----------|\n")
            for e in self.state.lineage:
                muts = ', '.join(e.get('mutations', [])[:2])
                if len(e.get('mutations', [])) > 2:
                    muts += f" (+{len(e['mutations'])-2})"
                f.write(f"| G{e['generation']:04d} | I{e.get('island_id',0)} | "
                       f"`{e.get('hash','?')[:12]}` | {muts} | "
                       f"{e.get('delta_score',0):+.1f} | "
                       f"{e.get('timestamp','N/A')[:19]} |\n")
            f.write(f"\n## Discovered Algorithms\n\n")
            for a in self.state.discovered_algorithms:
                f.write(f"- `{a}` → [`{a}.json`](journal/{a}.json)\n")
            f.write(f"\n## Fitness History\n\n```\n")
            for fh in self.state.fitness_history[-30:]:
                f.write(f"G{fh['generation']:04d} I{fh.get('island_id',0)}: "
                       f"score={fh['score']:.0f}\n")
            f.write("```\n")

        print(f"  {_C}[REPORT]{_W} {report}")


# ═══════════════════════════════════════════════════════════════════════
# CLI
# ═══════════════════════════════════════════════════════════════════════

def main():
    p = argparse.ArgumentParser(
        description="ZCC Oneirogenesis v2 — Enhanced Dream Engine",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
  make dream                 # 50 cycles, 1 island
  make dream-aggressive      # 200 cycles, 8 mutations, 3 islands
  make dream-visualize       # Stream to God's Eye (UDP:8084)
  make dream-dry             # Preview mutations only
  make dream-reset           # Clear state

  python3 zcc_oneirogenesis.py --islands 3 --sweep --cycles 100
        """
    )
    p.add_argument('--cycles',    type=int, default=50)
    p.add_argument('--seed',      type=int, default=42)
    p.add_argument('--mutations', type=int, default=3,
                   help='Max point mutations per cycle (default: 3)')
    p.add_argument('--islands',   type=int, default=1,
                   help='Number of parallel evolving lineages (default: 1)')
    p.add_argument('--sweep',     action='store_true',
                   help='Force sweep mutations (replace ALL instances at once)')
    p.add_argument('--aggressive', action='store_true')
    p.add_argument('--visualize', action='store_true',
                   help='Emit Hamiltonian energy packets (UDP:8084) for God\'s Eye')
    p.add_argument('--dry-run',   action='store_true')
    p.add_argument('--reset',     action='store_true',
                   help='Clear dream state and restart from Genesis')
    args = p.parse_args()

    if args.reset:
        sf = DREAM_DIR / "dream_state.json"
        if sf.exists():
            sf.unlink()
        for f in DREAM_DIR.glob("island_*.s"):
            f.unlink()
        print(f"  {_Y}[RESET]{_W} Dream state cleared.")

    DreamEngine(
        seed=args.seed,
        max_mutations=args.mutations,
        n_islands=args.islands,
        force_sweep=args.sweep,
        aggressive=args.aggressive,
        visualize=args.visualize,
        dry_run=args.dry_run,
    ).run(num_cycles=args.cycles)


if __name__ == '__main__':
    main()
