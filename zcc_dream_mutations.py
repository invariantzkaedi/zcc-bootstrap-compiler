#!/usr/bin/env python3
"""
=============================================================================
ZCC Dream Mutations v3 — Expanded Genome
=============================================================================
Assembly mutation engine with 10 scanner types across 4 categories:

  SWEEP (whole-assembly, safe rewrites):
    1. movq $0 → xorq         ~3,588 sites (4 bytes saved each)
    2. imulq $2^n → shlq        ~189 sites (3→1 cycle latency)
    3. cmpq $0 → testq        ~4,072 sites (shorter encoding)
    4. jmp .Lx / .Lx: removal     ~N sites (branch straightening)

  POINT (site-specific):
    5. Instruction scheduling   ~4,256 sites (WAR hazard reorder)
    6. Dead move elimination       ~N sites (overwritten before read)
    7. Redundant load after store   ~N sites (store/reload same reg)
    8. LEA optimization             ~N sites (mov+add → leaq)
    9. Push/pop elimination         ~N sites (cancel pairs)

  IDIOM (multi-instruction → single):
   10. load/inc/store → incq, decq, negq, notq, double→shl1

  PEEPHOLE (context-sensitive):
   11. cmpq $0 point, mov chain, leaq identity

Population crossover support: mutations can be combined via fingerprint
matching to share discovered patterns between island lineages.
=============================================================================
"""

import re
import random
import hashlib
from dataclasses import dataclass, field
from typing import Optional


@dataclass
class Mutation:
    """A single atomic mutation applied to assembly."""
    name: str
    category: str      # PEEPHOLE | SCHEDULE | STRENGTH | IDIOM | SWEEP
    description: str
    line_range: tuple  # (start_line, end_line) in original
    original_asm: str
    mutated_asm: str
    energy_delta: float = 0.0
    is_sweep: bool = False       # True = applies ALL occurrences, not just one
    sweep_count: int = 0         # How many occurrences will be replaced

    def fingerprint(self) -> str:
        data = f"{self.name}:{self.original_asm}:{self.mutated_asm}"
        return hashlib.sha256(data.encode()).hexdigest()[:16]


class MutationEngine:
    """
    v2: Profiling-driven, sweep-capable, crossover-ready mutation engine.

    Three mutation modes:
      SWEEP:  Scan entire assembly, apply every matching pattern at once.
              Produces large gains. Used for well-understood, safe rewrites.
      POINT:  Apply one specific instance at a precise line range.
              Used for scheduling and complex idioms where each site matters.
      CROSS:  Combine fingerprints from two parent mutations into a new one.
              Used in island-model crossover between competing lineages.
    """

    def __init__(self, seed: int = 42):
        self.rng = random.Random(seed)
        self.mutations_found: list[Mutation] = []

    # ──────────────────────────────────────────────────────────────────
    # PUBLIC API
    # ──────────────────────────────────────────────────────────────────

    def dream(self, asm_lines: list[str], max_point_mutations: int = 5,
              include_sweeps: bool = True) -> list[Mutation]:
        """
        Discover all applicable mutations.
        Returns sweep mutations (full-assembly passes) + point mutations
        (individual sites) up to max_point_mutations.
        """
        self.mutations_found = []
        results = []

        if include_sweeps:
            results.extend(self._sweep_zero_mov_to_xor(asm_lines))
            results.extend(self._sweep_strength_reduction(asm_lines))
            results.extend(self._sweep_cmpq_zero_to_testq(asm_lines))
            results.extend(self._sweep_branch_straighten(asm_lines))

        # --- POINT mutations ---
        point_candidates = []
        point_candidates.extend(self._scan_schedule(asm_lines))
        point_candidates.extend(self._scan_idiom(asm_lines))
        point_candidates.extend(self._scan_peephole_point(asm_lines))
        point_candidates.extend(self._scan_dead_move(asm_lines))
        point_candidates.extend(self._scan_redundant_load_after_store(asm_lines))
        point_candidates.extend(self._scan_lea_optimization(asm_lines))
        point_candidates.extend(self._scan_push_pop_elimination(asm_lines))

        # Sample from point candidates
        self.rng.shuffle(point_candidates)
        results.extend(point_candidates[:max_point_mutations])

        self.mutations_found = results
        return results

    def apply_mutation(self, asm_lines: list[str], mutation: Mutation) -> list[str]:
        """Apply a single mutation (SWEEP or POINT) to the assembly."""
        if mutation.is_sweep:
            return self._apply_sweep(asm_lines, mutation)
        else:
            return self._apply_point(asm_lines, mutation)

    def apply_mutations(self, asm_lines: list[str],
                        mutations: list[Mutation]) -> list[str]:
        """Apply all mutations. Sweeps first, then point mutations in reverse order."""
        result = list(asm_lines)

        # Apply sweeps first (they replace the whole text)
        for m in mutations:
            if m.is_sweep:
                result = self._apply_sweep(result, m)

        # Apply point mutations in reverse line order (preserves offsets)
        point_muts = sorted(
            [m for m in mutations if not m.is_sweep],
            key=lambda m: m.line_range[0], reverse=True
        )
        for m in point_muts:
            result = self._apply_point(result, m)

        return result

    def crossover(self, mut_a: Mutation, mut_b: Mutation) -> Optional[Mutation]:
        """
        Island model crossover: combine two compatible mutations.
        Only valid if they target different line ranges (non-overlapping).
        Returns a new compound mutation or None if incompatible.
        """
        if mut_a.is_sweep or mut_b.is_sweep:
            return None  # Sweeps don't crossover
        a_start, a_end = mut_a.line_range
        b_start, b_end = mut_b.line_range
        if a_end > b_start and b_end > a_start:
            return None  # Overlapping ranges — incompatible

        # Combine into a compound mutation
        return Mutation(
            name=f"cross_{mut_a.name}_{mut_b.name}",
            category="CROSS",
            description=f"Crossover: [{mut_a.description}] ✕ [{mut_b.description}]",
            line_range=(min(a_start, b_start), max(a_end, b_end)),
            original_asm=f"{mut_a.original_asm}\n---\n{mut_b.original_asm}",
            mutated_asm=f"{mut_a.mutated_asm}\n---\n{mut_b.mutated_asm}",
            energy_delta=mut_a.energy_delta + mut_b.energy_delta,
            is_sweep=False,
        )

    # ──────────────────────────────────────────────────────────────────
    # SWEEP MUTATIONS (whole-assembly pass)
    # ──────────────────────────────────────────────────────────────────

    def _is_zero_mov_safe(self, lines: list[str], idx: int, reg: str) -> bool:
        """
        Verify if EFLAGS is consumed by a conditional branch, setcc, cmovcc, or adc/sbb
        before being killed or exiting the basic block / label boundary, up to 20 instructions.
        """
        clobber_patterns = [
            r'\bcmp[qwl]?', r'\btest[qwl]?', r'\badd[qwl]?', r'\bsub[qwl]?',
            r'\band[qwl]?', r'\bor[qwl]?', r'\bxor[qwl]?', r'\bshl[qwl]?',
            r'\bshr[qwl]?', r'\bsar[qwl]?', r'\bimul[qwl]?', r'\bneg[qwl]?'
        ]
        cond_jumps = [
            r'\bje\b', r'\bjne\b', r'\bjg\b', r'\bjge\b', r'\bjl\b', r'\bjle\b',
            r'\bja\b', r'\bjae\b', r'\bjb\b', r'\bjbe\b', r'\bjs\b', r'\bjns\b',
            r'\bjc\b', r'\bjnc\b', r'\bjo\b', r'\bjno\b', r'\bjp\b', r'\bjnp\b',
            r'\bjz\b', r'\bjnz\b', r'\bjrcxz\b', r'\bloop',
            r'\bset[a-z]{1,3}\b', r'\bcmov[a-z]{1,3}\b',
            r'\badc[qwl]?\b', r'\bsbb[qwl]?\b'
        ]
        terminators = [
            r'\bret\b', r'\bleave\b', r'\bjmp\b'
        ]
        
        count = 0
        for k in range(idx + 1, len(lines)):
            line = lines[k].strip()
            if not line or line.startswith('#'):
                continue
            if line.endswith(':'):
                return False  # Label is unsafe target
            count += 1
            if count > 20:
                return False  # Reached scan window cap
            if any(re.search(pat, line) for pat in cond_jumps):
                return False
            if any(re.search(pat, line) for pat in clobber_patterns):
                return True
            if any(re.search(pat, line) for pat in terminators):
                return True
        return True

    def _sweep_zero_mov_to_xor(self, lines: list[str]) -> list[Mutation]:
        """
        SWEEP: Replace ALL movq $0, %rX with xorq %rX, %rX.

        ZCC emits 3,588 of these. Each:
          movq $0, %rax   (7 bytes: REX.W + MOV + imm64)
          →  xorq %rax, %rax  (3 bytes: REX.W + XOR r/m,r)

        Saves 4 bytes × 3,588 occurrences = 14,352 bytes from the binary.
        Also clears FLAGS (a bonus for subsequent comparisons).
        x86 processors have a zeroing idiom recognizer for xor reg,reg.
        """
        count = 0
        for idx, l in enumerate(lines):
            m = re.match(r'\s*movq\s+\$0,\s*(%[re]?[a-z]{2,3})', l)
            if m:
                reg = m.group(1)
                if self._is_zero_mov_safe(lines, idx, reg):
                    count += 1

        if count == 0:
            return []

        return [Mutation(
            name="sweep_zero_mov_to_xor",
            category="SWEEP",
            description=f"Sweep: replace ALL {count:,} movq $0,%rX with xorq %rX,%rX",
            line_range=(0, 0),  # unused for sweeps
            original_asm="movq $0, %r*",
            mutated_asm="xorq %r*, %r*",
            energy_delta=-(count * 4.0),  # 4-byte savings per instance
            is_sweep=True,
            sweep_count=count,
        )]

    def _sweep_strength_reduction(self, lines: list[str]) -> list[Mutation]:
        """
        SWEEP: Replace ALL imulq $N (where N is power-of-2) with shlq $log2(N).

        ZCC emits 189 imulq instructions (struct member offset computations,
        array indexing). Multiply latency on modern x86: 3 cycles.
        Shift latency: 1 cycle. Throughput: 1/cycle vs 1/3 cycles.
        """
        count = 0
        for l in lines:
            m = re.match(r'\s*imulq\s+\$(\d+),', l)
            if m:
                v = int(m.group(1))
                if v > 1 and (v & (v - 1)) == 0:
                    count += 1

        if count == 0:
            return []

        return [Mutation(
            name="sweep_imulq_pow2_to_shl",
            category="SWEEP",
            description=f"Sweep: replace ALL {count} imulq $2^n with shlq $n (3→1 cycle)",
            line_range=(0, 0),
            original_asm="imulq $2^n, %rX",
            mutated_asm="shlq $n, %rX",
            energy_delta=-(count * 2.0),  # latency improvement approximation
            is_sweep=True,
            sweep_count=count,
        )]

    def _apply_sweep(self, lines: list[str], mutation: Mutation) -> list[str]:
        """Execute a sweep mutation against the full assembly."""
        result = []
        name = mutation.name

        if name == "sweep_zero_mov_to_xor":
            for idx, line in enumerate(lines):
                m = re.match(r'(\s*)movq\s+\$0,\s*(%[re]?[a-z]{2,3})\s*$', line)
                if m:
                    indent = m.group(1)
                    reg = m.group(2)
                    if self._is_zero_mov_safe(lines, idx, reg):
                        result.append(f"{indent}xorq {reg}, {reg}\n")
                    else:
                        result.append(line)
                else:
                    result.append(line)

        elif name == "sweep_imulq_pow2_to_shl":
            for line in lines:
                m = re.match(r'(\s*)imulq\s+\$(\d+),\s*(%\w+)', line)
                if m:
                    indent = m.group(1)
                    val = int(m.group(2))
                    reg = m.group(3)
                    if val > 1 and (val & (val - 1)) == 0:
                        shift = val.bit_length() - 1
                        result.append(f"{indent}shlq ${shift}, {reg}\n")
                        continue
                result.append(line)
        elif name == "sweep_cmpq_zero_to_testq":
            for line in lines:
                m = re.match(r'(\s*)cmpq\s+\$0,\s*(%\w+)\s*$', line)
                if m:
                    indent = m.group(1)
                    reg = m.group(2)
                    result.append(f"{indent}testq {reg}, {reg}\n")
                else:
                    result.append(line)

        elif name == "sweep_branch_straighten":
            skip_next = False
            for idx in range(len(lines)):
                if skip_next:
                    skip_next = False
                    continue
                line = lines[idx]
                m = re.match(r'\s*jmp\s+(\.\w+)\s*$', line)
                if m and idx + 1 < len(lines):
                    next_line = lines[idx + 1].strip()
                    if next_line == m.group(1) + ':':
                        # jmp to next line — remove the jmp
                        continue
                result.append(line)

        else:
            result = lines  # unknown sweep — pass through

        return result

    def _apply_point(self, lines: list[str], mutation: Mutation) -> list[str]:
        """Apply a point mutation at its specific line range."""
        start, end = mutation.line_range
        end = min(end, len(lines))
        result = lines[:start]
        if mutation.mutated_asm.strip():
            for ml in mutation.mutated_asm.split('\n'):
                stripped = ml.strip()
                if stripped:
                    # Preserve indentation style
                    result.append(f"    {stripped}\n")
        result.extend(lines[end:])
        return result

    # ──────────────────────────────────────────────────────────────────
    # POINT MUTATIONS — SCHEDULING
    # ──────────────────────────────────────────────────────────────────

    def _scan_schedule(self, lines: list[str]) -> list[Mutation]:
        """
        Find WAR-hazard pairs where reordering reduces pipeline stalls.

        ZCC's codegen frequently emits:
          movq %rax, -8(%rbp)     ← writes rax to memory
          addq $1, %rbp           ← independent (doesn't read rax or mem)
          movq -8(%rbp), %rcx     ← reads the memory written above

        Swapping instructions 1 and 2 gives the CPU two more cycles to
        complete the store before the load is issued. On micro-benchmarks
        with the Skylake memory subsystem, this eliminates ~4-cycle stalls.

        Profile data: 4,256 candidates in current ZCC assembly.
        """
        mutations = []
        n = len(lines)

        sampled_indices = list(range(n - 2))
        self.rng.shuffle(sampled_indices)
        # Only scan up to 2000 random positions to keep discovery fast
        for i in sampled_indices[:2000]:
            s1 = lines[i].strip()
            s2 = lines[i + 1].strip()

            if not s1 or s1.endswith(':') or s1.startswith('.'):
                continue
            if not s2 or s2.endswith(':') or s2.startswith('.'):
                continue
            # Don't reorder across control flow
            if any(s1.startswith(j) for j in ('j', 'call', 'ret', 'leave')):
                continue
            if any(s2.startswith(j) for j in ('j', 'call', 'ret', 'leave')):
                continue

            rw1 = self._write_regs(s1)
            rr1 = self._read_regs(s1)
            rw2 = self._write_regs(s2)
            rr2 = self._read_regs(s2)

            # Independence check
            if rw1 & rr2 or rw2 & rr1 or rw1 & rw2:
                continue

            # WAR hazard with instruction 3?
            if i + 2 < n:
                s3 = lines[i + 2].strip()
                rr3 = self._read_regs(s3)
                if rw1 & rr3:
                    mutations.append(Mutation(
                        name="schedule_war_pair",
                        category="SCHEDULE",
                        description="Swap independent pair to reduce WAR pipeline stall",
                        line_range=(i, i + 2),
                        original_asm=f"{s1}\n{s2}",
                        mutated_asm=f"    {s2}\n    {s1}",
                        energy_delta=-0.5,
                    ))

        return mutations

    # ──────────────────────────────────────────────────────────────────
    # POINT MUTATIONS — IDIOM RECOGNITION
    # ──────────────────────────────────────────────────────────────────

    def _scan_idiom(self, lines: list[str]) -> list[Mutation]:
        """
        Multi-instruction patterns → single instruction.

        ZCC frequently emits load/compute/store sequences for simple
        operations like increment/decrement (local variable updates).
        The x86 ISA has direct memory-operand versions of these.
        """
        mutations = []
        n = len(lines)

        for i in range(n - 3):
            s1 = lines[i].strip()
            s2 = lines[i + 1].strip()
            s3 = lines[i + 2].strip()

            # Pattern: load → increment → store → incq mem
            m1 = re.match(r'movq\s+(-?\d+\(%rbp\)),\s*%rax', s1)
            if m1:
                mem = m1.group(1)

                if s2 == 'addq $1, %rax':
                    m3 = re.match(r'movq\s+%rax,\s*(-?\d+\(%rbp\))', s3)
                    if m3 and m3.group(1) == mem:
                        mutations.append(Mutation(
                            name="load_inc_store_to_incq",
                            category="IDIOM",
                            description=f"Collapse load/add1/store → incq {mem}",
                            line_range=(i, i + 3),
                            original_asm=f"{s1}\n{s2}\n{s3}",
                            mutated_asm=f"    incq {mem}",
                            energy_delta=-5.0,  # 3 insns → 1
                        ))

                if s2 == 'subq $1, %rax':
                    m3 = re.match(r'movq\s+%rax,\s*(-?\d+\(%rbp\))', s3)
                    if m3 and m3.group(1) == mem:
                        mutations.append(Mutation(
                            name="load_dec_store_to_decq",
                            category="IDIOM",
                            description=f"Collapse load/sub1/store → decq {mem}",
                            line_range=(i, i + 3),
                            original_asm=f"{s1}\n{s2}\n{s3}",
                            mutated_asm=f"    decq {mem}",
                            energy_delta=-5.0,
                        ))

                # Pattern: load → negq → store → negq mem
                if s2 == 'negq %rax':
                    m3 = re.match(r'movq\s+%rax,\s*(-?\d+\(%rbp\))', s3)
                    if m3 and m3.group(1) == mem:
                        mutations.append(Mutation(
                            name="load_neg_store_to_negq",
                            category="IDIOM",
                            description=f"Collapse load/negq/store → negq {mem}",
                            line_range=(i, i + 3),
                            original_asm=f"{s1}\n{s2}\n{s3}",
                            mutated_asm=f"    negq {mem}",
                            energy_delta=-5.0,
                        ))

                # Pattern: load → notq → store → notq mem
                if s2 == 'notq %rax':
                    m3 = re.match(r'movq\s+%rax,\s*(-?\d+\(%rbp\))', s3)
                    if m3 and m3.group(1) == mem:
                        mutations.append(Mutation(
                            name="load_not_store_to_notq",
                            category="IDIOM",
                            description=f"Collapse load/notq/store → notq {mem}",
                            line_range=(i, i + 3),
                            original_asm=f"{s1}\n{s2}\n{s3}",
                            mutated_asm=f"    notq {mem}",
                            energy_delta=-5.0,
                        ))

            # Pattern: movq → shlq $1 → (same as multiply-by-2)
            # movq %rax, %r11; addq %r11, %rax → shlq $1, %rax
            m4 = re.match(r'movq\s+%rax,\s*%r11', s1)
            if m4 and s2 == 'addq %r11, %rax':
                mutations.append(Mutation(
                    name="double_to_shl1",
                    category="IDIOM",
                    description="Collapse copy+add (×2) → shlq $1, %rax",
                    line_range=(i, i + 2),
                    original_asm=f"{s1}\n{s2}",
                    mutated_asm="    shlq $1, %rax",
                    energy_delta=-3.0,
                ))

        return mutations

    # ──────────────────────────────────────────────────────────────────
    # POINT MUTATIONS — PEEPHOLE (non-sweep specific sites)
    # ──────────────────────────────────────────────────────────────────

    def _scan_peephole_point(self, lines: list[str]) -> list[Mutation]:
        """
        Single-instruction point rewrites not covered by sweeps.
        Focuses on patterns that need context-sensitivity.
        """
        mutations = []
        n = len(lines)

        for i in range(n - 1):
            s1 = lines[i].strip()
            s2 = lines[i + 1].strip()

            # test %rax, %rax; sete %cl → setne not after negate...
            # Actually: cmpq $0, %rX → testq %rX, %rX (shorter encoding)
            mc = re.match(r'cmpq\s+\$0,\s*(%\w+)', s1)
            if mc:
                reg = mc.group(1)
                mutations.append(Mutation(
                    name="cmpq_zero_to_testq",
                    category="PEEPHOLE",
                    description=f"Replace cmpq $0,{reg} with testq {reg},{reg}",
                    line_range=(i, i + 1),
                    original_asm=s1,
                    mutated_asm=f"    testq {reg}, {reg}",
                    energy_delta=-0.5,  # 1-byte shorter encoding
                ))

            # movq %rax, %rcx; movq %rcx, %rdx (chain) → movq %rax, %rdx
            mc2 = re.match(r'movq\s+%rax,\s*(%r\w+)', s1)
            mc3 = re.match(r'movq\s+(%r\w+),\s*(%r\w+)', s2)
            if mc2 and mc3 and mc2.group(1) == mc3.group(1) and mc3.group(2) != '%rax':
                mutations.append(Mutation(
                    name="mov_chain_collapse",
                    category="PEEPHOLE",
                    description=f"Collapse mov chain: %rax→{mc2.group(1)}→{mc3.group(2)}",
                    line_range=(i, i + 2),
                    original_asm=f"{s1}\n{s2}",
                    mutated_asm=f"    {s1}\n    movq %rax, {mc3.group(2)}",
                    energy_delta=-1.0,
                ))

            # leaq 0(%rax), %rax → (dead: lea with 0 offset is identity)
            ml = re.match(r'leaq\s+0\((%\w+)\),\s*(%\w+)', s1)
            if ml and ml.group(1) == ml.group(2):
                mutations.append(Mutation(
                    name="leaq_zero_elim",
                    category="PEEPHOLE",
                    description=f"Eliminate leaq 0({ml.group(1)}),{ml.group(2)} (identity)",
                    line_range=(i, i + 1),
                    original_asm=s1,
                    mutated_asm="",
                    energy_delta=-1.5,
                ))

        return mutations

    # ──────────────────────────────────────────────────────────────────
    # SWEEP MUTATIONS — NEW (v3)
    # ──────────────────────────────────────────────────────────────────

    def _sweep_cmpq_zero_to_testq(self, lines: list[str]) -> list[Mutation]:
        """
        SWEEP: Replace ALL cmpq $0, %rX with testq %rX, %rX.
        testq is shorter (no immediate operand) and faster.
        """
        count = 0
        for l in lines:
            if re.match(r'\s*cmpq\s+\$0,\s*%\w+', l):
                count += 1
        if count == 0:
            return []
        return [Mutation(
            name="sweep_cmpq_zero_to_testq",
            category="SWEEP",
            description=f"Sweep: replace ALL {count:,} cmpq $0,%rX with testq %rX,%rX",
            line_range=(0, 0),
            original_asm="cmpq $0, %r*",
            mutated_asm="testq %r*, %r*",
            energy_delta=-(count * 1.0),
            is_sweep=True,
            sweep_count=count,
        )]

    def _sweep_branch_straighten(self, lines: list[str]) -> list[Mutation]:
        """
        SWEEP: Remove jmp instructions that jump to the immediately next label.
        Pattern: jmp .Lxx / .Lxx:  — the jmp is a no-op.
        """
        count = 0
        for i in range(len(lines) - 1):
            m = re.match(r'\s*jmp\s+(\.\w+)\s*$', lines[i])
            if m and lines[i + 1].strip() == m.group(1) + ':':
                count += 1
        if count == 0:
            return []
        return [Mutation(
            name="sweep_branch_straighten",
            category="SWEEP",
            description=f"Sweep: remove ALL {count:,} jmp-to-next-label (branch straightening)",
            line_range=(0, 0),
            original_asm="jmp .Lx / .Lx:",
            mutated_asm="(removed) / .Lx:",
            energy_delta=-(count * 3.0),
            is_sweep=True,
            sweep_count=count,
        )]

    # ──────────────────────────────────────────────────────────────────
    # POINT MUTATIONS — NEW SCANNERS (v3)
    # ──────────────────────────────────────────────────────────────────

    def _scan_dead_move(self, lines: list[str]) -> list[Mutation]:
        """
        DEAD MOVE ELIMINATION
        Pattern: movq %rA, %rB ... movq %rC, %rB (with no read of %rB between)
        The first mov is dead — remove it.
        """
        mutations = []
        n = len(lines)
        sampled = list(range(n - 1))
        self.rng.shuffle(sampled)

        for i in sampled[:1500]:
            s1 = lines[i].strip()
            m1 = re.match(r'movq\s+(%\w+),\s*(%\w+)\s*$', s1)
            if not m1:
                continue
            dst_reg = m1.group(2)
            # dst_reg must be a GP register, not a memory operand
            if '(' in dst_reg:
                continue
            # Scan forward for overwrite before read
            dead = False
            for j in range(i + 1, min(i + 20, n)):
                sj = lines[j].strip()
                if not sj or sj.endswith(':') or sj.startswith('.'):
                    break
                if any(sj.startswith(x) for x in ('j', 'call', 'ret', 'leave')):
                    break  # control flow — stop
                read_regs = self._read_regs(sj)
                if dst_reg in read_regs:
                    break  # register is read — not dead
                write_regs = self._write_regs(sj)
                if dst_reg in write_regs:
                    dead = True  # register overwritten before any read
                    break
            if dead:
                mutations.append(Mutation(
                    name="dead_move_elim",
                    category="PEEPHOLE",
                    description=f"Remove dead movq to {dst_reg} (overwritten at +{j-i})",
                    line_range=(i, i + 1),
                    original_asm=s1,
                    mutated_asm="",
                    energy_delta=-2.0,
                ))
        return mutations

    def _scan_redundant_load_after_store(self, lines: list[str]) -> list[Mutation]:
        """
        REDUNDANT LOAD AFTER STORE
        Pattern: movq %rax, -8(%rbp) / movq -8(%rbp), %rax
        The value is already in %rax — remove the load.
        """
        mutations = []
        n = len(lines)
        for i in range(n - 1):
            s1 = lines[i].strip()
            s2 = lines[i + 1].strip()
            # Store: movq %reg, MEM
            m1 = re.match(r'movq\s+(%\w+),\s*(-?\d+\(%rbp\))\s*$', s1)
            if not m1:
                continue
            reg = m1.group(1)
            mem = m1.group(2)
            # Load: movq MEM, %reg (same reg, same mem)
            m2 = re.match(r'movq\s+(-?\d+\(%rbp\)),\s*(%\w+)\s*$', s2)
            if m2 and m2.group(1) == mem and m2.group(2) == reg:
                mutations.append(Mutation(
                    name="redundant_load_after_store",
                    category="PEEPHOLE",
                    description=f"Remove redundant load: {reg} already has {mem}",
                    line_range=(i + 1, i + 2),
                    original_asm=s2,
                    mutated_asm="",
                    energy_delta=-2.5,
                ))
        return mutations

    def _scan_lea_optimization(self, lines: list[str]) -> list[Mutation]:
        """
        LEA OPTIMIZATION
        Pattern: movq %rA, %rB / addq $N, %rB → leaq N(%rA), %rB
        Single instruction replaces two.
        """
        mutations = []
        n = len(lines)
        for i in range(n - 1):
            s1 = lines[i].strip()
            s2 = lines[i + 1].strip()
            m1 = re.match(r'movq\s+(%\w+),\s*(%\w+)\s*$', s1)
            if not m1:
                continue
            src_reg = m1.group(1)
            dst_reg = m1.group(2)
            if '(' in src_reg or '(' in dst_reg:
                continue
            m2 = re.match(r'addq\s+\$(\d+),\s*(%\w+)\s*$', s2)
            if m2 and m2.group(2) == dst_reg:
                imm = m2.group(1)
                mutations.append(Mutation(
                    name="lea_optimization",
                    category="IDIOM",
                    description=f"Fuse mov+add → leaq ${imm}({src_reg}), {dst_reg}",
                    line_range=(i, i + 2),
                    original_asm=f"{s1}\n{s2}",
                    mutated_asm=f"    leaq {imm}({src_reg}), {dst_reg}",
                    energy_delta=-3.0,
                ))
        return mutations

    def _scan_push_pop_elimination(self, lines: list[str]) -> list[Mutation]:
        """
        PUSH/POP ELIMINATION
        Pattern: pushq %rax / popq %rax (with nothing between that reads the stack)
        Remove both — they cancel out.
        """
        mutations = []
        n = len(lines)
        for i in range(n - 1):
            s1 = lines[i].strip()
            s2 = lines[i + 1].strip()
            m1 = re.match(r'pushq\s+(%\w+)\s*$', s1)
            m2 = re.match(r'popq\s+(%\w+)\s*$', s2)
            if m1 and m2 and m1.group(1) == m2.group(1):
                reg = m1.group(1)
                mutations.append(Mutation(
                    name="push_pop_elim",
                    category="PEEPHOLE",
                    description=f"Remove cancel pair pushq/popq {reg}",
                    line_range=(i, i + 2),
                    original_asm=f"{s1}\n{s2}",
                    mutated_asm="",
                    energy_delta=-4.0,
                ))
        return mutations

    # ──────────────────────────────────────────────────────────────────
    # HELPERS
    # ──────────────────────────────────────────────────────────────────

    @staticmethod
    def _write_regs(inst: str) -> set:
        regs = set()
        parts = inst.split()
        if not parts:
            return regs
        op = parts[0]
        if op in ('movq', 'movl', 'movb', 'leaq', 'xorq', 'xorl',
                  'addq', 'subq', 'imulq', 'shlq', 'shrq', 'sarq',
                  'andq', 'orq', 'incq', 'decq', 'negq', 'notq',
                  'cltq', 'cqto', 'testq', 'movsbq', 'movzbl'):
            operands = inst[len(op):].strip().split(',')
            if operands:
                last = operands[-1].strip()
                m = re.findall(r'(%\w+)', last)
                if m and '(' not in last:
                    regs.update(m)
        if op == 'popq':
            regs.update(re.findall(r'(%\w+)', inst))
        if op == 'call':
            regs.update({'%rax', '%rcx', '%rdx', '%rsi', '%rdi',
                         '%r8', '%r9', '%r10', '%r11'})
        return regs

    @staticmethod
    def _read_regs(inst: str) -> set:
        regs = set()
        parts = inst.split()
        if not parts:
            return regs
        op = parts[0]
        if op in ('movq', 'movl', 'leaq', 'movsbq', 'movzbl'):
            operands = inst[len(op):].strip().split(',')
            if operands:
                regs.update(re.findall(r'(%\w+)', operands[0]))
        elif op == 'pushq':
            regs.update(re.findall(r'(%\w+)', inst))
        elif op in ('addq', 'subq', 'xorq', 'andq', 'orq', 'cmpq', 'testq',
                    'imulq', 'shlq', 'shrq', 'sarq', 'incq', 'decq', 'negq',
                    'notq', 'cltq'):
            regs.update(re.findall(r'(%\w+)', inst))
        elif op == 'call':
            regs.update({'%rdi', '%rsi', '%rdx', '%rcx', '%r8', '%r9'})
        return regs
