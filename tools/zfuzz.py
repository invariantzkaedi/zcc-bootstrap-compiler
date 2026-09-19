#!/usr/bin/env python3
"""zfuzz — differential-testing fuzzer for C compilers (ZCC-ready).

Generates random, UB-free-by-construction C programs, compiles them through
every configured backend, executes them, and diffs (exit_code, stdout).
Any divergence is a miscompilation witness in at least one backend.

Evidence discipline (fabrication-prevention protocol):
  - every compile/run carries exact command, numeric exit code, log path, timestamp
  - verdicts start UNVERIFIED; promoted to PASS only by evidence
  - --fault-inject mode adds a deliberately miscompiling backend to prove
    the diff gate CAN fail (rule 3: a gate never observed failing is UNVERIFIED)

UB-freedom argument (scope of the generator, rule 5):
  - all arithmetic is `unsigned long long` (wraparound is defined, C11 6.2.5p9)
  - shift counts are masked `& 63` before use (always < width)
  - division/modulo denominators are `(d | 1)` — never zero
  - loops have fixed literal trip counts; no recursion; no pointers/arrays
  - only printf("%llu") output — no reliance on impl-defined behavior
  Programs are therefore strictly conforming w.r.t. arithmetic semantics;
  any backend divergence is a compiler bug, not program UB.

Usage:
  zfuzz.py run  --count 50 --seed 1 --out /tmp/zfuzz_run
  zfuzz.py run  --count 10 --seed 1 --out /tmp/zfuzz_fi --fault-inject
  zfuzz.py repro --bundle /tmp/zfuzz_run/case_000007/bundle.json
"""

from __future__ import annotations

import argparse
import hashlib
import json
import logging
import os
import random
import shutil
import subprocess
import sys
import time
from dataclasses import dataclass, field, asdict
from pathlib import Path
from typing import Optional

LOG = logging.getLogger("zfuzz")

RUN_TIMEOUT_S = 10.0
COMPILE_TIMEOUT_S = 60.0
MAX_STDOUT_BYTES = 1 << 20  # 1 MiB cap; generated programs print ~dozens of lines


# --------------------------------------------------------------------------
# Backends
# --------------------------------------------------------------------------

@dataclass(frozen=True)
class Backend:
    name: str
    argv: list[str]          # command; source path and -o out appended
    fault_injected: bool = False  # True => deliberately broken (negative control)


def default_backends() -> list[Backend]:
    """gcc optimization levels as mutually-independent-enough oracles.

    ZCC integration: add Backend("zcc", ["/path/to/zcc"]) — any argv that
    accepts `<src.c> -o <out>` works.
    """
    return [
        Backend("gcc-O0", ["gcc", "-std=c11", "-O0"]),
        Backend("gcc-O2", ["gcc", "-std=c11", "-O2"]),
        Backend("gcc-O3", ["gcc", "-std=c11", "-O3"]),
    ]


FAULT_BACKEND = Backend("evil-gcc", ["__internal_evil__"], fault_injected=True)


# --------------------------------------------------------------------------
# Program generator — UB-free by construction
# --------------------------------------------------------------------------

class CGen:
    """Random C program generator. All values u64; see module docstring for
    the UB-freedom argument. Determinism: fully driven by the seeded RNG."""

    BINOPS = ["+", "-", "*", "&", "|", "^"]

    def __init__(self, rng: random.Random, n_vars: int = 6, n_stmts: int = 14):
        if n_vars < 2:
            raise ValueError(f"n_vars must be >= 2, got {n_vars}")
        if n_stmts < 1:
            raise ValueError(f"n_stmts must be >= 1, got {n_stmts}")
        self.rng = rng
        self.vars = [f"v{i}" for i in range(n_vars)]
        self.n_stmts = n_stmts

    def _var(self) -> str:
        return self.rng.choice(self.vars)

    def _lit(self) -> str:
        return f"{self.rng.getrandbits(self.rng.choice([4, 8, 16, 32, 64]))}ULL"

    def _expr(self, depth: int) -> str:
        if depth <= 0:
            return self._var() if self.rng.random() < 0.7 else self._lit()
        r = self.rng.random()
        a = self._expr(depth - 1)
        b = self._expr(depth - 1)
        if r < 0.55:
            op = self.rng.choice(self.BINOPS)
            return f"({a} {op} {b})"
        if r < 0.70:  # masked shift — count always in [0,63]
            direction = self.rng.choice(["<<", ">>"])
            return f"({a} {direction} ({b} & 63ULL))"
        if r < 0.80:  # guarded div/mod — denominator forced odd, never 0
            op = self.rng.choice(["/", "%"])
            return f"({a} {op} (({b}) | 1ULL))"
        if r < 0.90:  # comparison (yields 0/1, defined)
            cmp_op = self.rng.choice(["<", ">", "<=", ">=", "==", "!="])
            return f"((unsigned long long)({a} {cmp_op} {b}))"
        return f"(~({a}))"

    def _stmt(self, depth: int, indent: str) -> str:
        r = self.rng.random()
        if r < 0.55 or depth <= 0:
            return f"{indent}{self._var()} = {self._expr(2)};"
        if r < 0.80:
            cond = f"({self._expr(1)} & 1ULL)"
            body = self._stmt(depth - 1, indent + "    ")
            alt = self._stmt(depth - 1, indent + "    ")
            return (f"{indent}if {cond} {{\n{body}\n{indent}}} else {{\n"
                    f"{alt}\n{indent}}}")
        trip = self.rng.randint(1, 8)  # fixed literal trip count — terminates
        i = f"i{self.rng.getrandbits(16)}"
        body = self._stmt(depth - 1, indent + "    ")
        return (f"{indent}for (unsigned long long {i} = 0; {i} < {trip}ULL; "
                f"{i}++) {{\n{body}\n{indent}}}")

    def generate(self) -> str:
        decls = "\n".join(
            f"    unsigned long long {v} = {self._lit()};" for v in self.vars
        )
        stmts = "\n".join(self._stmt(2, "    ") for _ in range(self.n_stmts))
        prints = "\n".join(
            f'    printf("{v}=%llu\\n", {v});' for v in self.vars
        )
        checksum = " ^ ".join(self.vars)
        return (
            "#include <stdio.h>\n\n"
            "int main(void) {\n"
            f"{decls}\n{stmts}\n{prints}\n"
            f'    printf("chk=%llu\\n", {checksum});\n'
            "    return 0;\n"
            "}\n"
        )


# --------------------------------------------------------------------------
# Execution with mandatory evidence capture
# --------------------------------------------------------------------------

@dataclass
class Evidence:
    command: list[str]
    exit_code: Optional[int]     # None => did not complete (timeout)
    log_path: str
    timestamp: str
    duration_s: float
    stdout_sha256: Optional[str] = None
    timed_out: bool = False


def _now() -> str:
    return time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())


def run_logged(argv: list[str], log_path: Path, timeout: float,
               cwd: Optional[Path] = None) -> Evidence:
    """Run argv, tee stdout+stderr to log_path, return Evidence.

    Never raises on process failure — nonzero exit and timeout are data,
    not exceptions. Raises only on harness-level errors (unwritable log)."""
    if not argv:
        raise ValueError("empty argv")
    t0 = time.monotonic()
    ts = _now()
    timed_out = False
    try:
        proc = subprocess.run(
            argv, capture_output=True, timeout=timeout,
            cwd=str(cwd) if cwd else None,
        )
        exit_code: Optional[int] = proc.returncode
        out, err = proc.stdout, proc.stderr
    except subprocess.TimeoutExpired as exc:
        exit_code, timed_out = None, True
        out = exc.stdout or b""
        err = (exc.stderr or b"") + f"\nzfuzz: TIMEOUT after {timeout}s\n".encode()
    except FileNotFoundError as exc:
        exit_code = 127
        out, err = b"", f"zfuzz: exec failed: {exc}\n".encode()
    out = out[:MAX_STDOUT_BYTES]
    dur = time.monotonic() - t0
    log_path.parent.mkdir(parents=True, exist_ok=True)
    with open(log_path, "wb") as fh:
        fh.write(b"CMD: " + " ".join(argv).encode() + b"\n")
        fh.write(f"TS: {ts}\nEXIT: {exit_code}\nDUR_S: {dur:.3f}\n"
                 f"--- stdout ---\n".encode())
        fh.write(out)
        fh.write(b"\n--- stderr ---\n")
        fh.write(err)
    return Evidence(
        command=argv, exit_code=exit_code, log_path=str(log_path),
        timestamp=ts, duration_s=round(dur, 3),
        stdout_sha256=hashlib.sha256(out).hexdigest(), timed_out=timed_out,
    )


# --------------------------------------------------------------------------
# Fault injection — the negative control (rule 4: must actually fire)
# --------------------------------------------------------------------------

def evil_mutate(src: Path, dst: Path, rng: random.Random) -> str:
    """Produce a semantically-different program the 'evil' backend compiles
    instead of the real source. Models a miscompilation. Returns a
    description of the injected fault.

    Fault must be GUARANTEED live (rule 4 — a control that can be a dead
    mutation is decoration; observed 6/10 dead with operator swaps).
    We XOR the checksum expression with 1: chk's printed value flips bit 0
    unconditionally, per-variable lines stay identical. Divergence in
    stdout is by construction, independent of program semantics."""
    text = src.read_text()
    marker = '    printf("chk=%llu\\n", '
    if marker not in text:
        raise RuntimeError("checksum printf not found — generator/mutator drift")
    mutated = text.replace(marker, marker + "1ULL ^ ", 1)
    if mutated == text:
        raise RuntimeError("evil_mutate produced identical source — control is decoration")
    dst.write_text(mutated)
    return "checksum XOR 1 (guaranteed-live stdout divergence)"


# --------------------------------------------------------------------------
# Differential harness
# --------------------------------------------------------------------------

@dataclass
class CaseResult:
    case_id: str
    seed: int
    verdict: str = "UNVERIFIED"     # UNVERIFIED | PASS | FAIL | UNVERIFIED-BUILD
    reason: str = ""
    injected_fault: Optional[str] = None
    backends: dict = field(default_factory=dict)  # name -> {compile, run} evidence


def run_case(idx: int, seed: int, out_root: Path, backends: list[Backend],
             fault_inject: bool) -> CaseResult:
    case_id = f"case_{idx:06d}"
    case_dir = out_root / case_id
    case_dir.mkdir(parents=True, exist_ok=True)
    rng = random.Random(seed)

    src = case_dir / "prog.c"
    src.write_text(CGen(rng).generate())

    result = CaseResult(case_id=case_id, seed=seed)
    signatures: dict[str, tuple] = {}
    active = list(backends) + ([FAULT_BACKEND] if fault_inject else [])

    for be in active:
        compile_src = src
        if be.fault_injected:
            compile_src = case_dir / "prog_evil.c"
            result.injected_fault = evil_mutate(src, compile_src, rng)
        binary = case_dir / f"bin_{be.name}"
        cc_argv = (["gcc", "-std=c11", "-O0"] if be.fault_injected else be.argv)
        cc_ev = run_logged(cc_argv + [str(compile_src), "-o", str(binary)],
                           case_dir / f"compile_{be.name}.log", COMPILE_TIMEOUT_S)
        entry: dict = {"compile": asdict(cc_ev)}
        if cc_ev.exit_code != 0:
            entry["run"] = None
            signatures[be.name] = ("COMPILE_FAIL", cc_ev.exit_code)
        else:
            run_ev = run_logged([str(binary)], case_dir / f"run_{be.name}.log",
                                RUN_TIMEOUT_S)
            entry["run"] = asdict(run_ev)
            signatures[be.name] = (
                "TIMEOUT" if run_ev.timed_out else "RAN",
                run_ev.exit_code, run_ev.stdout_sha256,
            )
        result.backends[be.name] = entry

    # Verdict. Reference = non-fault backends; they must (a) all build+run,
    # (b) agree with each other. Fault backend must DISAGREE for the control
    # to count as fired.
    ref_sigs = {b.name: signatures[b.name] for b in backends}
    if any(s[0] == "COMPILE_FAIL" for s in ref_sigs.values()):
        result.verdict = "UNVERIFIED-BUILD"
        result.reason = "reference backend failed to compile; no differential possible"
    elif len(set(ref_sigs.values())) != 1:
        result.verdict = "FAIL"
        result.reason = "reference backends diverge: " + json.dumps(
            {k: list(v) for k, v in ref_sigs.items()})
    elif fault_inject:
        if signatures[FAULT_BACKEND.name] == next(iter(ref_sigs.values())):
            result.verdict = "FAIL"
            result.reason = ("NEGATIVE CONTROL DID NOT FIRE: evil backend "
                            "matched references — diff gate cannot detect faults")
        else:
            result.verdict = "PASS"
            result.reason = "references agree AND injected fault detected as divergence"
    else:
        result.verdict = "PASS"
        result.reason = "all reference backends agree on (exit, stdout)"

    (case_dir / "bundle.json").write_text(json.dumps(asdict(result), indent=2))
    return result


# --------------------------------------------------------------------------
# Campaign driver
# --------------------------------------------------------------------------

def cmd_run(args: argparse.Namespace) -> int:
    out_root = Path(args.out)
    if out_root.exists() and any(out_root.iterdir()):
        LOG.error("output dir %s exists and is non-empty; refusing to mix evidence",
                  out_root)
        return 2
    out_root.mkdir(parents=True, exist_ok=True)
    backends = default_backends()
    for extra in args.backend or []:
        name, _, cmd = extra.partition("=")
        if not name or not cmd:
            LOG.error("bad --backend %r (want name=cmd)", extra)
            return 2
        backends.append(Backend(name, cmd.split()))
    for be in backends:
        if shutil.which(be.argv[0]) is None:
            LOG.error("backend %s: %r not found on PATH", be.name, be.argv[0])
            return 2

    master = random.Random(args.seed)
    tally: dict[str, int] = {}
    results = []
    t0 = time.monotonic()
    for i in range(args.count):
        res = run_case(i, master.getrandbits(63), out_root, backends,
                       args.fault_inject)
        tally[res.verdict] = tally.get(res.verdict, 0) + 1
        results.append({"case": res.case_id, "seed": res.seed,
                        "verdict": res.verdict, "reason": res.reason})
        if res.verdict == "FAIL":
            LOG.warning("%s FAIL: %s", res.case_id, res.reason)
    ledger = {
        "tool": "zfuzz", "version": "0.1.0", "timestamp": _now(),
        "campaign_seed": args.seed, "count": args.count,
        "fault_inject": args.fault_inject,
        "backends": [asdict(b) for b in backends]
                    + ([asdict(FAULT_BACKEND)] if args.fault_inject else []),
        "duration_s": round(time.monotonic() - t0, 2),
        "tally": tally, "results": results,
    }
    (out_root / "ledger.json").write_text(json.dumps(ledger, indent=2))
    print(json.dumps({"tally": tally, "ledger": str(out_root / "ledger.json")}))
    return 0 if tally.get("FAIL", 0) == 0 else 1


def cmd_repro(args: argparse.Namespace) -> int:
    bundle = json.loads(Path(args.bundle).read_text())
    print(f"case={bundle['case_id']} seed={bundle['seed']} "
          f"verdict={bundle['verdict']}\nreason: {bundle['reason']}")
    for name, ev in bundle["backends"].items():
        cc, rn = ev["compile"], ev["run"]
        print(f"  {name}: compile exit={cc['exit_code']} log={cc['log_path']}")
        if rn:
            print(f"           run exit={rn['exit_code']} "
                  f"sha256={rn['stdout_sha256'][:16]}… log={rn['log_path']}")
    return 0


def main(argv: Optional[list[str]] = None) -> int:
    logging.basicConfig(level=logging.INFO,
                        format="%(asctime)s %(levelname)s %(message)s")
    p = argparse.ArgumentParser(prog="zfuzz", description=__doc__)
    sub = p.add_subparsers(dest="cmd", required=True)
    r = sub.add_parser("run", help="run a fuzz campaign")
    r.add_argument("--count", type=int, default=20)
    r.add_argument("--seed", type=int, default=1)
    r.add_argument("--out", required=True)
    r.add_argument("--backend", action="append",
                   help="extra backend, name=cmd (e.g. zcc='/path/zcc')")
    r.add_argument("--fault-inject", action="store_true",
                   help="add deliberately-miscompiling backend; PASS then "
                        "requires the gate to catch it")
    r.set_defaults(fn=cmd_run)
    q = sub.add_parser("repro", help="print evidence for one case bundle")
    q.add_argument("--bundle", required=True)
    q.set_defaults(fn=cmd_repro)
    args = p.parse_args(argv)
    if args.cmd == "run" and args.count < 1:
        p.error("--count must be >= 1")
    return args.fn(args)


if __name__ == "__main__":
    sys.exit(main())
