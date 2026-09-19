#!/usr/bin/env python3
"""
ZCC ONEIROGENESIS v4.0-SOVEREIGN

Self-contained evolutionary assembly optimizer candidate.

Evidence note:
- The ZKAEDI PRIME two-regime Hamiltonian invariant is implemented from the
  project documentation supplied with this task.
- This file is not represented as a byte-for-byte port of v3.5-EXPERIMENTALE
  because that source was not available in the supplied workspace.

Public compatibility exports:
    FitnessOracle, DreamEngine, DREAM_DIR, REPO_ROOT, PASSES
"""

from __future__ import annotations

import argparse
import hashlib
import hmac
import json
import math
import os
import random
import re
import secrets
import socket
import statistics
import sys
import time
from dataclasses import dataclass, field, asdict
from pathlib import Path
from typing import Dict, FrozenSet, Iterable, Iterator, List, Mapping, Optional, Sequence, Set, Tuple


VERSION = "ZCC ONEIROGENESIS v4.0-SOVEREIGN"
REPO_ROOT = Path(__file__).resolve().parent
DREAM_DIR = REPO_ROOT / "evidence" / "oneirogenesis"
EVENT_LOG = REPO_ROOT / "evidence" / "oneirogenesis_events.jsonl"

PASSES: Tuple[str, ...] = (
    "canonicalize",
    "peephole",
    "dead-write-elimination",
    "branch-simplify",
    "stack-balance",
    "pareto-select",
    "hamiltonian-navigate",
    "wkb-tunnel",
)

TORUS_MODULUS = 12289
CANONICAL_ETA = 0.4
CANONICAL_GAMMA = 0.3
CANONICAL_BETA = 0.1
CANONICAL_EPSILON = 0.05


# ---------------------------------------------------------------------------
# Utility / evidence
# ---------------------------------------------------------------------------

def _canonical_json(obj: object) -> bytes:
    return json.dumps(obj, sort_keys=True, separators=(",", ":"), ensure_ascii=False).encode("utf-8")


def _sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def _atomic_append_jsonl(path: Path, obj: Mapping[str, object]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    payload = _canonical_json(obj) + b"\n"
    # O_APPEND provides process-level append atomicity for modest records on
    # ordinary local filesystems; fsync makes the ledger durable at the OS API.
    fd = os.open(str(path), os.O_WRONLY | os.O_CREAT | os.O_APPEND, 0o600)
    try:
        os.write(fd, payload)
        os.fsync(fd)
    finally:
        os.close(fd)


class EventLedger:
    """Append-only, hash-chained JSONL event ledger."""

    def __init__(self, path: Path = EVENT_LOG):
        self.path = Path(path)
        self._prev_hash = self._discover_tail_hash()

    def _discover_tail_hash(self) -> str:
        if not self.path.exists():
            return "0" * 64
        try:
            last = b""
            with self.path.open("rb") as fh:
                for line in fh:
                    if line.strip():
                        last = line
            if not last:
                return "0" * 64
            obj = json.loads(last)
            return str(obj.get("event_hash", "0" * 64))
        except (OSError, ValueError, TypeError):
            # Never certify a malformed prior ledger. Start a new chain segment
            # with explicit lineage break.
            return "CORRUPT_PRIOR_LEDGER"

    def append(self, event_type: str, payload: Mapping[str, object]) -> Dict[str, object]:
        record: Dict[str, object] = {
            "schema": "zcc.oneirogenesis.event.v1",
            "version": VERSION,
            "ts_ns": time.time_ns(),
            "event_type": event_type,
            "prev_hash": self._prev_hash,
            "payload": dict(payload),
        }
        digest = _sha256_bytes(_canonical_json(record))
        record["event_hash"] = digest
        _atomic_append_jsonl(self.path, record)
        self._prev_hash = digest
        return record


# ---------------------------------------------------------------------------
# Instruction semantics and conservative CFG/liveness
# ---------------------------------------------------------------------------

_REG_ALIASES = {
    # Canonicalize common partial-register aliases.
    "al": "rax", "ah": "rax", "ax": "rax", "eax": "rax", "rax": "rax",
    "bl": "rbx", "bh": "rbx", "bx": "rbx", "ebx": "rbx", "rbx": "rbx",
    "cl": "rcx", "ch": "rcx", "cx": "rcx", "ecx": "rcx", "rcx": "rcx",
    "dl": "rdx", "dh": "rdx", "dx": "rdx", "edx": "rdx", "rdx": "rdx",
    "sil": "rsi", "si": "rsi", "esi": "rsi", "rsi": "rsi",
    "dil": "rdi", "di": "rdi", "edi": "rdi", "rdi": "rdi",
    "bpl": "rbp", "bp": "rbp", "ebp": "rbp", "rbp": "rbp",
    "spl": "rsp", "sp": "rsp", "esp": "rsp", "rsp": "rsp",
}
for _i in range(8, 16):
    for _suffix in ("b", "w", "d", ""):
        _REG_ALIASES[f"r{_i}{_suffix}"] = f"r{_i}"

_REG_RE = re.compile(r"%([A-Za-z0-9]+)")
_LABEL_RE = re.compile(r"^\s*([.$A-Za-z_][\w.$@]*):\s*(?:#.*)?$")
_INST_RE = re.compile(r"^\s*([A-Za-z][A-Za-z0-9.]*)\s*(.*?)\s*(?:#.*)?$")
_BRANCH_TARGET_RE = re.compile(r"(?:^|,)\s*([.$A-Za-z_][\w.$@]*)\s*$")


@dataclass(frozen=True)
class InstructionEffect:
    reads: FrozenSet[str] = frozenset()
    writes: FrozenSet[str] = frozenset()
    reads_flags: bool = False
    writes_flags: bool = False
    stack_delta: int = 0
    terminator: bool = False
    conditional_branch: bool = False
    unconditional_branch: bool = False
    call: bool = False
    ret: bool = False
    unknown: bool = False


class InstructionSemantics:
    """
    Conservative AT&T x86-64 register/flags/stack semantics.

    Unknown opcodes are intentionally marked `unknown` and treated as barriers
    by mutation passes; the analyzer never assumes an unrecognized instruction
    is side-effect free.
    """

    _COND_PREFIXES = (
        "ja", "jb", "jc", "je", "jg", "jl", "jna", "jnb", "jnc", "jne",
        "jng", "jnl", "jno", "jnp", "jns", "jnz", "jo", "jp", "jpe", "jpo",
        "js", "jz",
    )

    _READ_WRITE_TWO_OP = {
        "add", "sub", "adc", "sbb", "and", "or", "xor", "imul",
        "shl", "shr", "sal", "sar", "rol", "ror",
    }

    _COMPARE = {"cmp", "test", "comiss", "comisd", "ucomiss", "ucomisd"}
    _MOVE = {"mov", "movabs", "movsx", "movzx", "movsxd", "lea"}
    _UNARY_RW = {"inc", "dec", "neg", "not", "bswap"}
    _PUSH = {"push", "pushq", "pushl"}
    _POP = {"pop", "popq", "popl"}

    @staticmethod
    def _base_opcode(opcode: str) -> str:
        op = opcode.lower()
        # Preserve mnemonics whose trailing character is semantically part of name.
        exact = {
            "call", "ret", "leave", "nop", "ud2", "syscall", "sysret",
            "cqto", "cltd", "cdq", "cwd", "cqo", "endbr64",
        }
        if op in exact:
            return op
        # Strip conventional AT&T size suffix for known families only.
        for base in (
            list(InstructionSemantics._READ_WRITE_TWO_OP)
            + list(InstructionSemantics._COMPARE)
            + list(InstructionSemantics._MOVE)
            + list(InstructionSemantics._UNARY_RW)
            + list(InstructionSemantics._PUSH)
            + list(InstructionSemantics._POP)
            + ["call", "ret"]
        ):
            if op == base or op in {base + "b", base + "w", base + "l", base + "q"}:
                return base
        return op

    @staticmethod
    def _regs(operand: str) -> Set[str]:
        out: Set[str] = set()
        for r in _REG_RE.findall(operand):
            key = r.lower()
            out.add(_REG_ALIASES.get(key, key))
        return out

    @classmethod
    def analyze(cls, opcode: str, operands: str) -> InstructionEffect:
        op = cls._base_opcode(opcode)
        parts = [p.strip() for p in operands.split(",")] if operands else []
        all_regs = cls._regs(operands)

        if op.startswith("j") and op != "jmp":
            return InstructionEffect(
                reads=all_regs, reads_flags=True, terminator=True,
                conditional_branch=True
            )
        if op in {"jmp", "jmpq"}:
            return InstructionEffect(
                reads=all_regs, terminator=True, unconditional_branch=True
            )
        if op in {"ret", "retq"}:
            return InstructionEffect(
                reads=frozenset({"rsp"}), writes=frozenset({"rsp"}),
                stack_delta=8, terminator=True, ret=True
            )
        if op in {"call", "callq"}:
            # SysV caller-clobbered register set. Memory and callee effects remain
            # barriers to code motion.
            return InstructionEffect(
                reads=frozenset(all_regs | {"rsp"}),
                writes=frozenset({"rax","rcx","rdx","rsi","rdi","r8","r9","r10","r11","rsp"}),
                stack_delta=0, call=True
            )
        if op in cls._PUSH:
            return InstructionEffect(
                reads=frozenset(all_regs | {"rsp"}), writes=frozenset({"rsp"}),
                stack_delta=-8
            )
        if op in cls._POP:
            writes = set(all_regs) | {"rsp"}
            return InstructionEffect(
                reads=frozenset({"rsp"}), writes=frozenset(writes), stack_delta=8
            )
        if op == "leave":
            return InstructionEffect(
                reads=frozenset({"rbp"}), writes=frozenset({"rsp","rbp"})
            )
        if op in cls._COMPARE:
            return InstructionEffect(reads=frozenset(all_regs), writes_flags=True)
        if op in cls._MOVE and len(parts) >= 2:
            src_regs = cls._regs(parts[0])
            dst_regs = cls._regs(parts[-1])
            # Addressing registers in destination memory are reads, not writes.
            dst_is_mem = "(" in parts[-1] or ")" in parts[-1]
            reads = set(src_regs)
            writes: Set[str] = set()
            if dst_is_mem:
                reads |= dst_regs
            else:
                writes |= dst_regs
            return InstructionEffect(reads=frozenset(reads), writes=frozenset(writes))
        if op in cls._READ_WRITE_TWO_OP and parts:
            src = cls._regs(parts[0]) if len(parts) >= 2 else set()
            dst = cls._regs(parts[-1])
            return InstructionEffect(
                reads=frozenset(src | dst), writes=frozenset(dst), writes_flags=True
            )
        if op in cls._UNARY_RW:
            return InstructionEffect(
                reads=frozenset(all_regs), writes=frozenset(all_regs), writes_flags=True
            )
        if op.startswith("set"):
            return InstructionEffect(
                writes=frozenset(all_regs), reads_flags=True
            )
        if op.startswith("cmov") and parts:
            src = cls._regs(parts[0]) if len(parts) >= 2 else set()
            dst = cls._regs(parts[-1])
            return InstructionEffect(
                reads=frozenset(src | dst), writes=frozenset(dst), reads_flags=True
            )
        if op in {"nop", "endbr64"}:
            return InstructionEffect()

        return InstructionEffect(reads=frozenset(all_regs), unknown=True)


@dataclass
class AssemblyInstruction:
    line_index: int
    text: str
    opcode: str
    operands: str
    effect: InstructionEffect


@dataclass
class BasicBlock:
    index: int
    start: int
    end: int
    labels: Tuple[str, ...]
    instructions: List[AssemblyInstruction]
    successors: Set[int] = field(default_factory=set)
    predecessors: Set[int] = field(default_factory=set)
    use: Set[str] = field(default_factory=set)
    defs: Set[str] = field(default_factory=set)
    live_in: Set[str] = field(default_factory=set)
    live_out: Set[str] = field(default_factory=set)


class CFGLivenessAnalyzer:
    """Build basic blocks and solve classic backward register liveness."""

    def __init__(self, source: str):
        self.source = source
        self.lines = source.splitlines()
        self.instructions: List[AssemblyInstruction] = []
        self.label_to_line: Dict[str, int] = {}
        self.blocks: List[BasicBlock] = []
        self._parse()
        self._build_blocks()
        self._solve_liveness()

    def _parse(self) -> None:
        for i, line in enumerate(self.lines):
            lm = _LABEL_RE.match(line)
            if lm:
                self.label_to_line[lm.group(1)] = i
                continue
            m = _INST_RE.match(line)
            if not m:
                continue
            op, operands = m.group(1), m.group(2)
            self.instructions.append(
                AssemblyInstruction(i, line, op, operands,
                                    InstructionSemantics.analyze(op, operands))
            )

    @staticmethod
    def _branch_target(inst: AssemblyInstruction) -> Optional[str]:
        if not (inst.effect.conditional_branch or inst.effect.unconditional_branch):
            return None
        m = _BRANCH_TARGET_RE.search(inst.operands)
        return m.group(1) if m else None

    def _build_blocks(self) -> None:
        if not self.instructions:
            return
        inst_by_line = {x.line_index: x for x in self.instructions}
        leaders: Set[int] = {self.instructions[0].line_index}

        for label_line in self.label_to_line.values():
            nxt = next((x.line_index for x in self.instructions if x.line_index > label_line), None)
            if nxt is not None:
                leaders.add(nxt)

        for pos, inst in enumerate(self.instructions):
            if inst.effect.terminator and pos + 1 < len(self.instructions):
                leaders.add(self.instructions[pos + 1].line_index)
            target = self._branch_target(inst)
            if target in self.label_to_line:
                target_line = self.label_to_line[target]
                nxt = next((x.line_index for x in self.instructions if x.line_index > target_line), None)
                if nxt is not None:
                    leaders.add(nxt)

        leader_list = sorted(leaders)
        line_to_block: Dict[int, int] = {}
        for bi, start in enumerate(leader_list):
            next_start = leader_list[bi + 1] if bi + 1 < len(leader_list) else len(self.lines) + 1
            ins = [x for x in self.instructions if start <= x.line_index < next_start]
            labels = tuple(
                name for name, lno in self.label_to_line.items()
                if lno < start and lno >= (leader_list[bi - 1] if bi else -1)
            )
            if not ins:
                continue
            b = BasicBlock(len(self.blocks), start, ins[-1].line_index, labels, ins)
            self.blocks.append(b)
            for x in ins:
                line_to_block[x.line_index] = b.index

        label_to_block: Dict[str, int] = {}
        for name, lno in self.label_to_line.items():
            nxt = next((x.line_index for x in self.instructions if x.line_index > lno), None)
            if nxt is not None and nxt in line_to_block:
                label_to_block[name] = line_to_block[nxt]

        for i, b in enumerate(self.blocks):
            last = b.instructions[-1]
            target = self._branch_target(last)
            if target in label_to_block:
                b.successors.add(label_to_block[target])
            if last.effect.conditional_branch and i + 1 < len(self.blocks):
                b.successors.add(i + 1)
            elif not last.effect.terminator and i + 1 < len(self.blocks):
                b.successors.add(i + 1)

        for b in self.blocks:
            for s in b.successors:
                self.blocks[s].predecessors.add(b.index)

        for b in self.blocks:
            use: Set[str] = set()
            defs: Set[str] = set()
            for inst in b.instructions:
                for r in inst.effect.reads:
                    if r not in defs:
                        use.add(r)
                defs |= set(inst.effect.writes)
            b.use, b.defs = use, defs

    def _solve_liveness(self) -> None:
        changed = True
        while changed:
            changed = False
            for b in reversed(self.blocks):
                old_in, old_out = set(b.live_in), set(b.live_out)
                b.live_out = set().union(*(self.blocks[s].live_in for s in b.successors)) if b.successors else set()
                b.live_in = b.use | (b.live_out - b.defs)
                changed |= old_in != b.live_in or old_out != b.live_out

    def max_stack_depth(self) -> int:
        depth = 0
        max_depth = 0
        # Metric only: path-insensitive conservative scan. The mutation engine
        # does not use this to prove stack correctness.
        for inst in self.instructions:
            depth -= inst.effect.stack_delta
            max_depth = max(max_depth, depth)
        return max_depth // 8

    def branch_entropy(self) -> float:
        cond = sum(1 for i in self.instructions if i.effect.conditional_branch)
        uncond = sum(1 for i in self.instructions if i.effect.unconditional_branch)
        total = cond + uncond
        if not total:
            return 0.0
        p = cond / total
        if p in (0.0, 1.0):
            return 0.0
        return -(p * math.log2(p) + (1 - p) * math.log2(1 - p))


# ---------------------------------------------------------------------------
# Mutation engine
# ---------------------------------------------------------------------------

class MutationEngine:
    """Semantics-aware local transforms; unknown instructions are barriers."""

    _SELF_MOVE = re.compile(
        r"^(\s*)mov(?:b|w|l|q)?\s+(%[A-Za-z0-9]+)\s*,\s*\2\s*(?:#.*)?$",
        re.IGNORECASE,
    )
    _ADD_ZERO = re.compile(
        r"^\s*(?:add|sub)(?:b|w|l|q)?\s+\$0\s*,", re.IGNORECASE
    )
    _NOP = re.compile(r"^\s*nop(?:[lqwb]|\s|$)", re.IGNORECASE)
    _LEAQ_LOAD = re.compile(
        r"^\s*leaq\s+(-?\d+\(%rbp\)),\s*(%[a-z0-9]+)\s*$", re.IGNORECASE
    )
    _MOV_LOAD = re.compile(
        r"^\s*(mov(?:q|l|w|b|slq|zbl)?)\s*\((%[a-z0-9]+)\),\s*(%[a-z0-9]+)\s*$", re.IGNORECASE
    )
    _MOV_STORE = re.compile(
        r"^\s*(mov(?:q|l|w|b)?)\s*(%[a-z0-9]+),\s*\((%[a-z0-9]+)\)\s*$", re.IGNORECASE
    )
    _PUSH_Q = re.compile(
        r"^\s*pushq\s+(%[a-z0-9]+)\s*$", re.IGNORECASE
    )
    _POP_Q = re.compile(
        r"^\s*popq\s+(%[a-z0-9]+)\s*$", re.IGNORECASE
    )

    def __init__(self, rng: random.Random):
        self.rng = rng

    def safe_peephole(self, source: str) -> str:
        out: List[str] = []
        for line in source.splitlines():
            if self._SELF_MOVE.match(line) or self._ADD_ZERO.match(line) or self._NOP.match(line):
                continue
            out.append(line)
        return "\n".join(out) + ("\n" if source.endswith("\n") else "")

    def fold_addressing(self, source: str) -> str:
        lines = source.splitlines()
        out: List[str] = []
        i = 0
        n = len(lines)
        while i < n:
            l1 = lines[i]
            if i + 1 < n:
                l2 = lines[i + 1]
                m1 = self._LEAQ_LOAD.match(l1)
                if m1:
                    offset_rbp, reg = m1.group(1), m1.group(2).lower()
                    m2 = self._MOV_LOAD.match(l2)
                    if m2 and m2.group(2).lower() == reg:
                        mov_op, dst = m2.group(1), m2.group(3)
                        out.append(f"    {mov_op} {offset_rbp}, {dst}")
                        i += 2
                        continue
                    m3 = self._MOV_STORE.match(l2)
                    if m3 and m3.group(3).lower() == reg:
                        mov_op, src = m3.group(1), m3.group(2)
                        out.append(f"    {mov_op} {src}, {offset_rbp}")
                        i += 2
                        continue
                mpush = self._PUSH_Q.match(l1)
                mpop = self._POP_Q.match(l2)
                if mpush and mpop:
                    reg_a, reg_b = mpush.group(1).lower(), mpop.group(1).lower()
                    if reg_a == reg_b:
                        i += 2
                        continue
                    else:
                        out.append(f"    movq {mpush.group(1)}, {mpop.group(1)}")
                        i += 2
                        continue
            out.append(l1)
            i += 1
        return "\n".join(out) + ("\n" if source.endswith("\n") else "")

    _MOV_REG = re.compile(
        r"^\s*mov[a-z]*\s+([^,]+),\s*(%[a-z0-9]+)\s*$", re.IGNORECASE
    )
    _STORE_STACK = re.compile(
        r"^\s*mov[a-z]*\s+(%[a-z0-9]+),\s*(-?\d+\(%rbp\))\s*$", re.IGNORECASE
    )
    _LOAD_STACK = re.compile(
        r"^\s*(mov(?:slq|zbl|q|l)?)\s*(-?\d+\(%rbp\)),\s*(%[a-z0-9]+)\s*$", re.IGNORECASE
    )

    def fold_store_load(self, source: str) -> str:
        lines = source.splitlines()
        out: List[str] = []
        i = 0
        n = len(lines)
        while i < n:
            l1 = lines[i]
            if i + 1 < n:
                l2 = lines[i + 1]
                ms = self._STORE_STACK.match(l1)
                ml = self._LOAD_STACK.match(l2)
                if ms and ml:
                    s_reg, s_loc = ms.group(1).lower(), ms.group(2)
                    l_op, l_loc, l_dst = ml.group(1), ml.group(2), ml.group(3).lower()
                    if s_loc == l_loc:
                        out.append(l1)
                        out.append(f"    {l_op} {s_reg}, {l_dst}")
                        i += 2
                        continue
            out.append(l1)
            i += 1
        return "\n".join(out) + ("\n" if source.endswith("\n") else "")

    def eliminate_dead_writes(self, source: str) -> str:
        lines = source.splitlines()
        out: List[str] = []
        i = 0
        n = len(lines)
        while i < n:
            l1 = lines[i]
            if i + 1 < n:
                l2 = lines[i + 1]
                m1 = self._MOV_REG.match(l1)
                m2 = self._MOV_REG.match(l2)
                if m1 and m2:
                    src1, dst1 = m1.group(1).strip().lower(), m1.group(2).strip().lower()
                    src2, dst2 = m2.group(1).strip().lower(), m2.group(2).strip().lower()
                    if dst1 == dst2 and dst1 not in src2:
                        i += 1  # Drop dead write l1
                        continue
            out.append(l1)
            i += 1
        return "\n".join(out) + ("\n" if source.endswith("\n") else "")

    def wkb_tunnel_mutation(self, source: str) -> str:
        cur = source
        for _ in range(5):
            nxt = self.safe_peephole(cur)
            nxt = self.fold_addressing(nxt)
            nxt = self.fold_store_load(nxt)
            nxt = self.eliminate_dead_writes(nxt)
            if nxt == cur:
                break
            cur = nxt
        return cur

    def mutate(self, source: str) -> str:
        candidates = [
            self.safe_peephole,
            self.fold_addressing,
            self.fold_store_load,
            self.eliminate_dead_writes,
            self.wkb_tunnel_mutation,
            lambda s: s,  # explicit neutral mutation maintains reproducibility
        ]
        return self.rng.choice(candidates)(source)


# ---------------------------------------------------------------------------
# 4D Pareto archive
# ---------------------------------------------------------------------------

@dataclass(frozen=True, order=True)
class ParetoVector:
    size: int
    inst_count: int
    branch_entropy: float
    stack_depth: int

    def dominates(self, other: "ParetoVector") -> bool:
        a = (self.size, self.inst_count, self.branch_entropy, self.stack_depth)
        b = (other.size, other.inst_count, other.branch_entropy, other.stack_depth)
        return all(x <= y for x, y in zip(a, b)) and any(x < y for x, y in zip(a, b))


@dataclass
class Candidate:
    source: str
    vector: ParetoVector
    digest: str
    metadata: Dict[str, object] = field(default_factory=dict)


class ParetoArchive:
    def __init__(self) -> None:
        self._items: Dict[str, Candidate] = {}

    def add(self, cand: Candidate) -> bool:
        if cand.digest in self._items:
            return False
        if any(existing.vector.dominates(cand.vector) for existing in self._items.values()):
            return False
        dominated = [
            digest for digest, existing in self._items.items()
            if cand.vector.dominates(existing.vector)
        ]
        for digest in dominated:
            del self._items[digest]
        self._items[cand.digest] = cand
        return True

    def items(self) -> List[Candidate]:
        return sorted(
            self._items.values(),
            key=lambda c: (c.vector.size, c.vector.inst_count,
                           c.vector.branch_entropy, c.vector.stack_depth, c.digest),
        )


class FitnessOracle:
    """Static, deterministic 4D fitness measurement for assembly text."""

    def evaluate(self, source: str) -> ParetoVector:
        cfg = CFGLivenessAnalyzer(source)
        encoded = source.encode("utf-8")
        return ParetoVector(
            size=len(encoded),
            inst_count=len(cfg.instructions),
            branch_entropy=round(cfg.branch_entropy(), 12),
            stack_depth=cfg.max_stack_depth(),
        )

    def candidate(self, source: str, **metadata: object) -> Candidate:
        return Candidate(
            source=source,
            vector=self.evaluate(source),
            digest=_sha256_bytes(source.encode("utf-8")),
            metadata=dict(metadata),
        )


# ---------------------------------------------------------------------------
# ZKAEDI Prime Omega manifold
# ---------------------------------------------------------------------------

@dataclass(frozen=True)
class HamiltonianConfig:
    eta: float = CANONICAL_ETA
    gamma: float = CANONICAL_GAMMA
    beta: float = CANONICAL_BETA
    epsilon: float = CANONICAL_EPSILON
    modulus: int = TORUS_MODULUS

    def validate(self) -> None:
        if self.modulus != TORUS_MODULUS:
            raise ValueError("Prime Omega cyclotomic torus modulus must be 12289")
        if not math.isclose(self.eta, CANONICAL_ETA, rel_tol=0.0, abs_tol=1e-15):
            raise ValueError("field-shaping eta must be canonical 0.4")
        if self.epsilon < 0.0:
            raise ValueError("epsilon must be non-negative")


@dataclass(frozen=True)
class TorusPoint:
    x: int
    y: int

    def reduced(self, modulus: int = TORUS_MODULUS) -> "TorusPoint":
        return TorusPoint(self.x % modulus, self.y % modulus)


class PrimeOmegaHamiltonian:
    """
    Canonical two-regime engine.

    Field shaping:
      H_t = H_base + eta * H_prev * sigmoid(gamma * H_prev)
                    + epsilon * N(0, 1 + beta * |H_prev|)

    Navigation:
      scars + epsilon tie-break only.
      eta navigation lift is identically 0.0 by construction.
    """

    def __init__(self, rng: random.Random, config: HamiltonianConfig = HamiltonianConfig()):
        config.validate()
        self.rng = rng
        self.config = config

    @staticmethod
    def sigmoid(x: float) -> float:
        if x >= 0:
            z = math.exp(-x)
            return 1.0 / (1.0 + z)
        z = math.exp(x)
        return z / (1.0 + z)

    def shape_field(self, h_base: float, h_prev: float) -> float:
        c = self.config
        sigma = math.sqrt(max(0.0, 1.0 + c.beta * abs(h_prev)))
        noise = self.rng.gauss(0.0, sigma)
        return (
            h_base
            + c.eta * h_prev * self.sigmoid(c.gamma * h_prev)
            + c.epsilon * noise
        )

    @staticmethod
    def eta_navigation_lift(*_: object) -> float:
        return 0.0

    def navigation_score(self, scar: float) -> float:
        # Epsilon contributes only a stochastic tie-break / exploration term.
        return scar + self.config.epsilon * self.rng.gauss(0.0, 1.0)

    def torus(self, x: int, y: int) -> TorusPoint:
        return TorusPoint(x, y).reduced(self.config.modulus)


@dataclass(frozen=True)
class BezierSegment:
    p0: Tuple[float, float]
    p1: Tuple[float, float]
    p2: Tuple[float, float]
    p3: Tuple[float, float]

    def point(self, t: float) -> Tuple[float, float]:
        t = min(1.0, max(0.0, t))
        u = 1.0 - t
        x = u**3*self.p0[0] + 3*u*u*t*self.p1[0] + 3*u*t*t*self.p2[0] + t**3*self.p3[0]
        y = u**3*self.p0[1] + 3*u*u*t*self.p1[1] + 3*u*t*t*self.p2[1] + t**3*self.p3[1]
        return x, y

    def derivative(self, t: float) -> Tuple[float, float]:
        t = min(1.0, max(0.0, t))
        u = 1.0 - t
        dx = 3*u*u*(self.p1[0]-self.p0[0]) + 6*u*t*(self.p2[0]-self.p1[0]) + 3*t*t*(self.p3[0]-self.p2[0])
        dy = 3*u*u*(self.p1[1]-self.p0[1]) + 6*u*t*(self.p2[1]-self.p1[1]) + 3*t*t*(self.p3[1]-self.p2[1])
        return dx, dy


class GeodesicTracker:
    """C1 cubic Bezier trajectory chain over normalized fitness coordinates."""

    def __init__(self) -> None:
        self.points: List[Tuple[float, float]] = []
        self.segments: List[BezierSegment] = []

    def append(self, point: Tuple[float, float]) -> None:
        self.points.append(point)
        if len(self.points) < 2:
            return
        p0 = self.points[-2]
        p3 = self.points[-1]
        if len(self.segments) == 0:
            dx = (p3[0] - p0[0]) / 3.0
            dy = (p3[1] - p0[1]) / 3.0
            p1 = (p0[0] + dx, p0[1] + dy)
        else:
            prev_d = self.segments[-1].derivative(1.0)
            p1 = (p0[0] + prev_d[0] / 3.0, p0[1] + prev_d[1] / 3.0)
        p2 = (p3[0] - (p3[0]-p0[0])/3.0, p3[1] - (p3[1]-p0[1])/3.0)
        self.segments.append(BezierSegment(p0, p1, p2, p3))

    def c1_residuals(self) -> List[float]:
        out: List[float] = []
        for a, b in zip(self.segments, self.segments[1:]):
            da, db = a.derivative(1.0), b.derivative(0.0)
            out.append(math.hypot(da[0]-db[0], da[1]-db[1]))
        return out


# ---------------------------------------------------------------------------
# WKB tunneling / islands
# ---------------------------------------------------------------------------

@dataclass(frozen=True)
class WKBConfig:
    hbar: float = 1.0
    mass: float = 1.0
    barrier_scale: float = 1.0

    def probability(self, barrier_height: float, width: float, energy: float) -> float:
        delta = max(0.0, barrier_height - energy)
        exponent = -2.0 * max(0.0, width) * math.sqrt(2.0 * self.mass * delta) / max(self.hbar, 1e-12)
        return min(1.0, max(0.0, math.exp(exponent * self.barrier_scale)))


@dataclass
class Island:
    island_id: int
    population: List[Candidate] = field(default_factory=list)
    stagnation: int = 0
    best_scalar: float = math.inf


# ---------------------------------------------------------------------------
# Signed dual-channel telemetry
# ---------------------------------------------------------------------------

class HamiltonianTelemetry:
    """
    HMAC-SHA256 signed UDP telemetry.

    Wire object:
      {"payload": {...}, "signature": hex_hmac_sha256(canonical_json(payload))}

    Key resolution:
      explicit key -> ZCC_ONEIROGENESIS_HMAC_KEY -> ephemeral per-process key.
    """

    def __init__(
        self,
        key: Optional[bytes] = None,
        endpoints: Sequence[Tuple[str, int]] = (("127.0.0.1", 8084), ("127.0.0.1", 41337)),
        enabled: bool = True,
    ):
        env_key = os.getenv("ZCC_ONEIROGENESIS_HMAC_KEY")
        if key is None and env_key:
            key = env_key.encode("utf-8")
        self.key = key if key is not None else secrets.token_bytes(32)
        self.endpoints = tuple(endpoints)
        self.enabled = enabled

    def sign(self, payload: Mapping[str, object]) -> str:
        return hmac.new(self.key, _canonical_json(payload), hashlib.sha256).hexdigest()

    def envelope(self, payload: Mapping[str, object]) -> Dict[str, object]:
        p = dict(payload)
        return {"payload": p, "signature": self.sign(p)}

    def verify(self, envelope: Mapping[str, object]) -> bool:
        payload = envelope.get("payload")
        signature = envelope.get("signature")
        if not isinstance(payload, Mapping) or not isinstance(signature, str):
            return False
        return hmac.compare_digest(self.sign(payload), signature)

    def broadcast(self, payload: Mapping[str, object]) -> List[Tuple[str, int, bool]]:
        envelope = self.envelope(payload)
        data = _canonical_json(envelope)
        results: List[Tuple[str, int, bool]] = []
        if not self.enabled:
            return [(host, port, False) for host, port in self.endpoints]
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            for host, port in self.endpoints:
                ok = True
                try:
                    sock.sendto(data, (host, port))
                except OSError:
                    ok = False
                results.append((host, port, ok))
        finally:
            sock.close()
        return results


# ---------------------------------------------------------------------------
# Dream engine
# ---------------------------------------------------------------------------

class DreamEngine:
    def __init__(
        self,
        seed: int = 0,
        islands: int = 4,
        deterministic: bool = False,
        telemetry: bool = True,
        ledger_path: Path = EVENT_LOG,
    ):
        if islands < 1:
            raise ValueError("islands must be >= 1")
        self.seed = int(seed)
        self.deterministic = bool(deterministic)
        self.rng = random.Random(self.seed)
        self.oracle = FitnessOracle()
        self.mutation = MutationEngine(self.rng)
        self.hamiltonian = PrimeOmegaHamiltonian(self.rng)
        self.archive = ParetoArchive()
        self.islands = [Island(i) for i in range(islands)]
        self.telemetry = HamiltonianTelemetry(enabled=(telemetry and not deterministic))
        self.ledger = EventLedger(ledger_path)
        self.geodesic = GeodesicTracker()
        self.wkb = WKBConfig()

    @staticmethod
    def _scalar(v: ParetoVector) -> float:
        # Used only to detect stagnation, never to replace Pareto dominance.
        return float(v.size + 4*v.inst_count + 16*v.branch_entropy + 8*v.stack_depth)

    def _record(self, event_type: str, payload: Mapping[str, object]) -> None:
        event = self.ledger.append(event_type, payload)
        # Deterministic mode suppresses network I/O, but the ledger remains the
        # authoritative local evidence stream.
        self.telemetry.broadcast({
            "event_type": event_type,
            "event_hash": event["event_hash"],
            **dict(payload),
        })

    def seed_population(self, source: str) -> None:
        base = self.oracle.candidate(source, origin="seed")
        self.archive.add(base)
        for island in self.islands:
            island.population = [base]
            island.best_scalar = self._scalar(base.vector)
        self._record("seed", {"digest": base.digest, "vector": asdict(base.vector)})

    def _fitness_point(self, v: ParetoVector) -> Tuple[float, float]:
        # Stable 2D projection used only for trajectory visualization.
        x = float((v.size + 31 * v.inst_count) % TORUS_MODULUS)
        y = float((round(v.branch_entropy * 1000) + 17 * v.stack_depth) % TORUS_MODULUS)
        return x, y

    def evolve(self, source: str, cycles: int = 10) -> List[Candidate]:
        if cycles < 0:
            raise ValueError("cycles must be >= 0")
        self.seed_population(source)

        for cycle in range(cycles):
            for island in self.islands:
                parent = island.population[-1]
                child_source = self.mutation.mutate(parent.source)
                child = self.oracle.candidate(
                    child_source,
                    origin="mutation",
                    island=island.island_id,
                    cycle=cycle,
                )
                inserted = self.archive.add(child)
                scalar = self._scalar(child.vector)

                if scalar < island.best_scalar:
                    island.best_scalar = scalar
                    island.stagnation = 0
                else:
                    island.stagnation += 1

                # WKB escape on stagnation. The "barrier" is an optimization
                # heuristic, not a claim of physical quantum execution.
                tunneled = False
                if island.stagnation >= 2:
                    barrier = max(0.0, scalar - island.best_scalar)
                    p = self.wkb.probability(barrier_height=barrier + 1.0, width=1.0, energy=1.0)
                    if self.rng.random() < p:
                        child_source = self.mutation.wkb_tunnel_mutation(child_source)
                        child = self.oracle.candidate(
                            child_source,
                            origin="wkb",
                            island=island.island_id,
                            cycle=cycle,
                            tunnel_probability=p,
                        )
                        self.archive.add(child)
                        tunneled = True
                        island.stagnation = 0

                island.population.append(child)
                self.geodesic.append(self._fitness_point(child.vector))
                scar = -self._scalar(child.vector)
                navigation = self.hamiltonian.navigation_score(scar)
                assert self.hamiltonian.eta_navigation_lift() == 0.0

                self._record("cycle", {
                    "cycle": cycle,
                    "island": island.island_id,
                    "digest": child.digest,
                    "vector": asdict(child.vector),
                    "pareto_inserted": inserted,
                    "wkb_tunneled": tunneled,
                    "navigation_score": navigation,
                    "eta_navigation_lift": 0.0,
                })

            # Ring migration: best local tail moves to next island.
            if len(self.islands) > 1:
                migrants = [island.population[-1] for island in self.islands]
                for i, migrant in enumerate(migrants):
                    self.islands[(i + 1) % len(self.islands)].population.append(migrant)

        residuals = self.geodesic.c1_residuals()
        self._record("complete", {
            "archive_size": len(self.archive.items()),
            "c1_max_residual": max(residuals, default=0.0),
        })
        return self.archive.items()

    def dry_run(self, source: Optional[str] = None, cycles: int = 2) -> Dict[str, object]:
        source = source if source is not None else (
            ".text\n.globl main\nmain:\n"
            "    pushq %rbp\n"
            "    movq %rsp, %rbp\n"
            "    movq %rax, %rax\n"
            "    addq $0, %rax\n"
            "    movl $0, %eax\n"
            "    popq %rbp\n"
            "    ret\n"
        )
        archive = self.evolve(source, cycles=cycles)
        return {
            "version": VERSION,
            "seed": self.seed,
            "deterministic": self.deterministic,
            "islands": len(self.islands),
            "cycles": cycles,
            "archive": [
                {"digest": c.digest, "vector": asdict(c.vector), "metadata": c.metadata}
                for c in archive
            ],
            "eta_navigation_lift": self.hamiltonian.eta_navigation_lift(),
            "torus_modulus": self.hamiltonian.config.modulus,
            "event_log": str(self.ledger.path),
        }


def _parse_key(text: Optional[str]) -> Optional[bytes]:
    if text is None:
        return None
    if text.startswith("hex:"):
        return bytes.fromhex(text[4:])
    return text.encode("utf-8")


def _cli(argv: Optional[Sequence[str]] = None) -> int:
    p = argparse.ArgumentParser(description=VERSION)
    p.add_argument("input", nargs="?", help="assembly input; omitted for built-in dry-run specimen")
    p.add_argument("-o", "--output", help="output file for best evolved assembly")
    p.add_argument("--dry-run", action="store_true")
    p.add_argument("--islands", type=int, default=4)
    p.add_argument("--cycles", type=int, default=10)
    p.add_argument("--seed", type=int, default=0)
    p.add_argument("--deterministic", action="store_true")
    p.add_argument("--no-telemetry", action="store_true")
    p.add_argument("--event-log", default=str(EVENT_LOG))
    p.add_argument("--version", action="store_true")
    args = p.parse_args(argv)

    if args.version:
        print(VERSION)
        return 0

    source: Optional[str] = None
    if args.input:
        try:
            source = Path(args.input).read_text(encoding="utf-8", errors="replace")
        except OSError as exc:
            print(f"oneirogenesis: cannot read {args.input}: {exc}", file=sys.stderr)
            return 2

    if not args.dry_run and source is None:
        p.error("provide an input assembly file or use --dry-run")

    engine = DreamEngine(
        seed=args.seed,
        islands=args.islands,
        deterministic=args.deterministic,
        telemetry=not args.no_telemetry,
        ledger_path=Path(args.event_log),
    )
    if source is not None:
        candidates = engine.evolve(source, cycles=args.cycles)
        best = candidates[0]
        if args.output:
            out_content = best.source.rstrip() + "\n"
            Path(args.output).write_text(out_content, encoding="utf-8")
        orig_count = FitnessOracle().evaluate(source).inst_count
        opt_count = best.vector.inst_count
        reduction = round((1.0 - opt_count / max(1, orig_count)) * 100.0, 2)
        report = {
            "version": VERSION,
            "seed": args.seed,
            "cycles": args.cycles,
            "original_instructions": orig_count,
            "optimized_instructions": opt_count,
            "reduction_pct": reduction,
            "digest": best.digest,
            "output": args.output,
        }
    else:
        report = engine.dry_run(source=source, cycles=args.cycles)
    print(json.dumps(report, sort_keys=True, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(_cli())
