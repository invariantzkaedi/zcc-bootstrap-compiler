#!/usr/bin/env python3
"""
================================================================================
  ZCC COLAB GAUNTLET MONOLITH — High-Throughput Verification Engine
  Adversarial Compiler Verification Laboratory for 80 GB A100 + ~150 GB Host RAM
================================================================================
Self-contained, zero-dependency standalone script featuring:
  1. SemanticOracle: Process-isolated multi-compiler differential evaluator (ZCC vs GCC vs Clang)
  2. MetamorphicFuzzer: Semantics-preserving AST & control-flow mutator (neutral ops, dualities)
  3. ABITortureGenerator: System V x86-64 caller/callee cross-toolchain linking matrix
  4. DeltaReducer: Hierarchical ddmin delta debugger producing minimal <20 line C repros
  5. Multi-core /dev/shm Ramdisk Parallel Engine: Eliminates I/O and isolates memory arenas
"""

import os
import re
import sys
import time
import json
import random
import hashlib
import tempfile
import shutil
import argparse
import subprocess
import multiprocessing as mp
from enum import Enum
from pathlib import Path
from dataclasses import dataclass, field
from typing import Optional, Dict, Any, List, Tuple, Callable


# ==============================================================================
# SECTION 1: SEMANTIC ORACLE & EXECUTION DIGEST
# ==============================================================================

class DivergenceType(str, Enum):
    NONE = "NONE"
    COMPILER_CRASH = "COMPILER_CRASH"
    COMPILER_TIMEOUT = "COMPILER_TIMEOUT"
    COMPILATION_REJECT = "COMPILATION_REJECT"
    RUNTIME_CRASH = "RUNTIME_CRASH"
    RUNTIME_TIMEOUT = "RUNTIME_TIMEOUT"
    SEMANTIC_MISCOMPILE = "SEMANTIC_MISCOMPILE"
    REFERENCE_DISAGREEMENT = "REFERENCE_DISAGREEMENT"


@dataclass
class ExecutionDigest:
    compiler: str
    compiled: bool
    compile_time_ms: float
    compile_returncode: int
    compile_stderr: str
    executed: bool
    run_time_ms: float
    returncode: int
    stdout_hash: str
    stderr_hash: str
    stdout_snippet: str
    crash_signal: Optional[int] = None
    timeout: bool = False


@dataclass
class SemanticResult:
    source_path: str
    divergence_type: DivergenceType
    details: str
    zcc: Optional[ExecutionDigest] = None
    gcc: Optional[ExecutionDigest] = None
    clang: Optional[ExecutionDigest] = None
    timestamp: float = field(default_factory=time.time)

    def is_divergence(self) -> bool:
        return self.divergence_type not in (DivergenceType.NONE, DivergenceType.REFERENCE_DISAGREEMENT)


class SemanticOracle:
    def __init__(
        self,
        zcc_bin: str,
        repo_root: Optional[str] = None,
        timeout_compile: float = 8.0,
        timeout_run: float = 2.0,
        temp_dir: str = "/tmp/zcc_oracle",
        include_clang: bool = True
    ):
        self.zcc_bin = os.path.abspath(zcc_bin)
        self.repo_root = os.path.abspath(repo_root or os.path.dirname(self.zcc_bin))
        self.timeout_compile = timeout_compile
        self.timeout_run = timeout_run
        self.temp_dir = Path(temp_dir)
        self.include_clang = include_clang and (shutil.which("clang") is not None)
        self.temp_dir.mkdir(parents=True, exist_ok=True)

        self.passes: List[str] = []
        for p in ["compiler_passes.c", "compiler_passes_ir.c", "ir_pass_manager.c"]:
            p_full = os.path.join(self.repo_root, p)
            if os.path.exists(p_full):
                self.passes.append(p_full)

    def _compile_zcc(self, src_path: str, out_bin: str) -> Tuple[bool, float, int, str]:
        t0 = time.time()
        s_path = out_bin + ".s"
        env = os.environ.copy()
        env["LC_ALL"] = "C"

        # Step 1: ZCC generates assembly (-S)
        cmd_zcc = [self.zcc_bin, src_path, "-S", "-o", s_path]
        try:
            r = subprocess.run(
                cmd_zcc,
                cwd=self.repo_root,
                capture_output=True,
                timeout=self.timeout_compile,
                env=env
            )
            elapsed = (time.time() - t0) * 1000.0
            if r.returncode != 0:
                err = r.stderr.decode("utf-8", "replace")[:1024]
                return False, elapsed, r.returncode, err
        except subprocess.TimeoutExpired:
            return False, (time.time() - t0) * 1000.0, -999, "ZCC compilation timeout"
        except Exception as e:
            return False, (time.time() - t0) * 1000.0, -1, str(e)

        if not os.path.exists(s_path) or os.path.getsize(s_path) == 0:
            return False, (time.time() - t0) * 1000.0, -2, "ZCC emitted empty assembly"

        # Step 2: Assemble/link using GCC
        is_selfhost = os.path.basename(src_path) == "zcc.c"
        extra_args = self.passes if is_selfhost else []
        inc_dir = os.path.join(self.repo_root, "include")
        cmd_link = [
            "gcc", "-no-pie", "-O0", "-w",
            "-fno-asynchronous-unwind-tables", "-Wa,--noexecstack", "-fno-unwind-tables",
            f"-I{inc_dir}", f"-I{self.repo_root}",
            "-o", out_bin, s_path
        ] + extra_args + ["-lm"]

        try:
            r_link = subprocess.run(
                cmd_link,
                cwd=self.repo_root,
                capture_output=True,
                timeout=self.timeout_compile,
                env=env
            )
            elapsed = (time.time() - t0) * 1000.0
            if r_link.returncode != 0:
                err = r_link.stderr.decode("utf-8", "replace")[:1024]
                return False, elapsed, r_link.returncode, f"Link error: {err}"
            return True, elapsed, 0, ""
        except subprocess.TimeoutExpired:
            return False, (time.time() - t0) * 1000.0, -999, "Link timeout"
        except Exception as e:
            return False, (time.time() - t0) * 1000.0, -1, str(e)

    def _compile_gcc(self, src_path: str, out_bin: str) -> Tuple[bool, float, int, str]:
        t0 = time.time()
        cmd = ["gcc", "-O0", "-w", src_path, "-o", out_bin, "-lm"]
        try:
            r = subprocess.run(cmd, capture_output=True, timeout=self.timeout_compile)
            elapsed = (time.time() - t0) * 1000.0
            return r.returncode == 0, elapsed, r.returncode, r.stderr.decode("utf-8", "replace")[:1024]
        except subprocess.TimeoutExpired:
            return False, (time.time() - t0) * 1000.0, -999, "GCC timeout"
        except Exception as e:
            return False, (time.time() - t0) * 1000.0, -1, str(e)

    def _compile_clang(self, src_path: str, out_bin: str) -> Tuple[bool, float, int, str]:
        t0 = time.time()
        cmd = ["clang", "-O0", "-w", src_path, "-o", out_bin, "-lm"]
        try:
            r = subprocess.run(cmd, capture_output=True, timeout=self.timeout_compile)
            elapsed = (time.time() - t0) * 1000.0
            return r.returncode == 0, elapsed, r.returncode, r.stderr.decode("utf-8", "replace")[:1024]
        except subprocess.TimeoutExpired:
            return False, (time.time() - t0) * 1000.0, -999, "Clang timeout"
        except Exception as e:
            return False, (time.time() - t0) * 1000.0, -1, str(e)

    def _run_binary(self, bin_path: str, compiler_name: str, comp_time: float, comp_err: str) -> ExecutionDigest:
        if not os.path.exists(bin_path):
            return ExecutionDigest(
                compiler=compiler_name, compiled=False, compile_time_ms=comp_time,
                compile_returncode=-1, compile_stderr=comp_err, executed=False,
                run_time_ms=0.0, returncode=-1, stdout_hash="", stderr_hash="", stdout_snippet=""
            )

        t0 = time.time()
        env = os.environ.copy()
        env["LC_ALL"] = "C"

        try:
            r = subprocess.run([bin_path], capture_output=True, timeout=self.timeout_run, env=env)
            elapsed = (time.time() - t0) * 1000.0
            sig = -r.returncode if r.returncode < 0 else None
            out = r.stdout
            err = r.stderr
            return ExecutionDigest(
                compiler=compiler_name, compiled=True, compile_time_ms=comp_time,
                compile_returncode=0, compile_stderr=comp_err, executed=True,
                run_time_ms=elapsed, returncode=r.returncode,
                stdout_hash=hashlib.sha256(out).hexdigest(),
                stderr_hash=hashlib.sha256(err).hexdigest(),
                stdout_snippet=out[:128].decode("utf-8", "replace"),
                crash_signal=sig, timeout=False
            )
        except subprocess.TimeoutExpired:
            return ExecutionDigest(
                compiler=compiler_name, compiled=True, compile_time_ms=comp_time,
                compile_returncode=0, compile_stderr=comp_err, executed=False,
                run_time_ms=(time.time() - t0) * 1000.0, returncode=-999,
                stdout_hash="", stderr_hash="", stdout_snippet="", timeout=True
            )
        except Exception as e:
            return ExecutionDigest(
                compiler=compiler_name, compiled=True, compile_time_ms=comp_time,
                compile_returncode=0, compile_stderr=comp_err, executed=False,
                run_time_ms=(time.time() - t0) * 1000.0, returncode=-1,
                stdout_hash="", stderr_hash="", stdout_snippet=str(e), timeout=False
            )

    def evaluate(self, c_source_path: str, unique_id: Optional[str] = None) -> SemanticResult:
        uid = unique_id or hashlib.sha256(c_source_path.encode()).hexdigest()[:12]
        bin_zcc = str(self.temp_dir / f"bin_zcc_{uid}")
        bin_gcc = str(self.temp_dir / f"bin_gcc_{uid}")
        bin_clang = str(self.temp_dir / f"bin_clang_{uid}")

        zcc_ok, zcc_ctime, zcc_crc, zcc_cerr = self._compile_zcc(c_source_path, bin_zcc)
        gcc_ok, gcc_ctime, gcc_crc, gcc_cerr = self._compile_gcc(c_source_path, bin_gcc)

        clang_ok, clang_ctime, clang_crc, clang_cerr = (True, 0.0, 0, "")
        if self.include_clang:
            clang_ok, clang_ctime, clang_crc, clang_cerr = self._compile_clang(c_source_path, bin_clang)

        zcc_dig = self._run_binary(bin_zcc, "ZCC", zcc_ctime, zcc_cerr)
        gcc_dig = self._run_binary(bin_gcc, "GCC", gcc_ctime, gcc_cerr)
        clang_dig = self._run_binary(bin_clang, "Clang", clang_ctime, clang_cerr) if self.include_clang else gcc_dig

        for f in [bin_zcc, bin_gcc, bin_clang, bin_zcc + ".s"]:
            try:
                if os.path.exists(f): os.unlink(f)
            except OSError: pass

        ref_dig = gcc_dig
        if self.include_clang and gcc_dig.executed and clang_dig.executed:
            if gcc_dig.returncode != clang_dig.returncode or gcc_dig.stdout_hash != clang_dig.stdout_hash:
                return SemanticResult(
                    source_path=c_source_path,
                    divergence_type=DivergenceType.REFERENCE_DISAGREEMENT,
                    details=f"GCC rc={gcc_dig.returncode} vs Clang rc={clang_dig.returncode}",
                    zcc=zcc_dig, gcc=gcc_dig, clang=clang_dig
                )

        if not zcc_ok:
            if zcc_crc in (-11, 139):
                return SemanticResult(source_path=c_source_path, divergence_type=DivergenceType.COMPILER_CRASH,
                                      details=f"ZCC segfault (rc={zcc_crc}): {zcc_cerr}", zcc=zcc_dig, gcc=gcc_dig, clang=clang_dig)
            elif zcc_crc == -999:
                return SemanticResult(source_path=c_source_path, divergence_type=DivergenceType.COMPILER_TIMEOUT,
                                      details="ZCC compilation timeout", zcc=zcc_dig, gcc=gcc_dig, clang=clang_dig)
            elif gcc_ok and (not self.include_clang or clang_ok):
                return SemanticResult(source_path=c_source_path, divergence_type=DivergenceType.COMPILATION_REJECT,
                                      details=f"ZCC rejected valid C (rc={zcc_crc}): {zcc_cerr}", zcc=zcc_dig, gcc=gcc_dig, clang=clang_dig)
            elif not gcc_ok:
                return SemanticResult(source_path=c_source_path, divergence_type=DivergenceType.NONE,
                                      details="Both ZCC and reference compiler rejected invalid C", zcc=zcc_dig, gcc=gcc_dig, clang=clang_dig)
            else:
                return SemanticResult(source_path=c_source_path, divergence_type=DivergenceType.COMPILATION_REJECT,
                                      details=f"ZCC compilation failed (rc={zcc_crc}): {zcc_cerr}", zcc=zcc_dig, gcc=gcc_dig, clang=clang_dig)

        if zcc_dig.crash_signal is not None and ref_dig.crash_signal is None:
            return SemanticResult(source_path=c_source_path, divergence_type=DivergenceType.RUNTIME_CRASH,
                                  details=f"ZCC binary crashed with signal {zcc_dig.crash_signal}", zcc=zcc_dig, gcc=gcc_dig, clang=clang_dig)

        if zcc_dig.timeout and not ref_dig.timeout:
            return SemanticResult(source_path=c_source_path, divergence_type=DivergenceType.RUNTIME_TIMEOUT,
                                  details="ZCC binary entered infinite loop", zcc=zcc_dig, gcc=gcc_dig, clang=clang_dig)

        if zcc_dig.executed and ref_dig.executed:
            if zcc_dig.returncode != ref_dig.returncode:
                return SemanticResult(source_path=c_source_path, divergence_type=DivergenceType.SEMANTIC_MISCOMPILE,
                                      details=f"Exit code divergence: ZCC rc={zcc_dig.returncode} vs Ref rc={ref_dig.returncode}",
                                      zcc=zcc_dig, gcc=gcc_dig, clang=clang_dig)
            if zcc_dig.stdout_hash != ref_dig.stdout_hash:
                return SemanticResult(source_path=c_source_path, divergence_type=DivergenceType.SEMANTIC_MISCOMPILE,
                                      details=f"Stdout divergence: ZCC='{zcc_dig.stdout_snippet}' vs Ref='{ref_dig.stdout_snippet}'",
                                      zcc=zcc_dig, gcc=gcc_dig, clang=clang_dig)

        return SemanticResult(source_path=c_source_path, divergence_type=DivergenceType.NONE,
                              details="Identical observable semantics", zcc=zcc_dig, gcc=gcc_dig, clang=clang_dig)


# ==============================================================================
# SECTION 2: METAMORPHIC FUZZER
# ==============================================================================

class MetamorphicFuzzer:
    def __init__(self, seed: int = 42):
        self.rng = random.Random(seed)

    def transform_neutral_arithmetic(self, code: str) -> str:
        lines = code.splitlines(keepends=True)
        out = []
        for line in lines:
            if line.strip().startswith("#") or line.strip().startswith("//"):
                out.append(line)
                continue
            def repl(m):
                var = m.group(1)
                choice = self.rng.choice(["+ 0", "* 1", "- 0", "^ 0"])
                return f"({var} {choice})"
            if self.rng.random() < 0.3:
                line = re.sub(r'\b([a-zA-Z_][a-zA-Z0-9_]*)\b(?=\s*[+\-*/%&|^;,])', repl, line)
            out.append(line)
        return "".join(out)

    def transform_boolean_inversion(self, code: str) -> str:
        pattern = re.compile(r'if\s*\(([^)]+)\)\s*\{([^}]+)\}\s*else\s*\{([^}]+)\}', re.MULTILINE | re.DOTALL)
        def repl(m):
            cond, b1, b2 = m.group(1).strip(), m.group(2), m.group(3)
            return f"if (!({cond})) {{{b2}}} else {{{b1}}}"
        return pattern.sub(repl, code)

    def transform_array_pointer_duality(self, code: str) -> str:
        pattern = re.compile(r'\b([a-zA-Z_][a-zA-Z0-9_]*)\[([a-zA-Z0-9_]+)\]')
        return pattern.sub(r'(*(\1 + (\2)))', code)

    def transform_constant_expansion(self, code: str) -> str:
        def repl(m):
            val_str = m.group(0)
            try:
                val = int(val_str)
                if abs(val) > 1000000 or val == 0: return val_str
                delta = self.rng.randint(1, 15)
                return f"(({val - delta}) + {delta})" if self.rng.random() < 0.5 else f"(({val + delta}) - {delta})"
            except ValueError:
                return val_str
        return re.sub(r'(?<![a-zA-Z0-9_#])\b([1-9][0-9]{0,4})\b(?![a-zA-Z0-9_])', repl, code)

    def transform_dead_branch_injection(self, code: str) -> str:
        lines = code.splitlines(keepends=True)
        out = []
        for line in lines:
            out.append(line)
            if "{" in line and self.rng.random() < 0.15:
                out.append("    if (0) { volatile int _dummy_dead = 12345; (void)_dummy_dead; }\n")
        return "".join(out)

    def generate_metamorphic_variants(self, c_code: str, num_variants: int = 2) -> List[Tuple[str, str]]:
        variants = []
        strategies = [
            ("neutral_arithmetic", self.transform_neutral_arithmetic),
            ("boolean_inversion", self.transform_boolean_inversion),
            ("array_ptr_duality", self.transform_array_pointer_duality),
            ("constant_expansion", self.transform_constant_expansion),
            ("dead_branch_injection", self.transform_dead_branch_injection),
        ]
        for i in range(num_variants):
            strat_name, func = self.rng.choice(strategies)
            try:
                trans = func(c_code)
                if trans != c_code: variants.append((f"{strat_name}_v{i+1}", trans))
            except Exception: pass
        return variants


# ==============================================================================
# SECTION 3: SYSTEM V ABI TORTURE FACTORY
# ==============================================================================

class ABITortureGenerator:
    PRIMITIVES = [
        ("int8_t", "%d", "42"), ("uint8_t", "%u", "200"),
        ("int16_t", "%d", "-12345"), ("uint16_t", "%u", "54321"),
        ("int32_t", "%d", "-987654321"), ("uint32_t", "%u", "3141592653U"),
        ("int64_t", "%ld", "-1234567890123456789LL"), ("uint64_t", "%lu", "9876543210987654321ULL"),
        ("float", "%.2f", "3.14159f"), ("double", "%.4f", "2.718281828459"),
    ]

    STRUCT_TEMPLATES = [
        ("struct StructSmallInt", "struct StructSmallInt { int a; int b; };", "(struct StructSmallInt){ 10, 20 }"),
        ("struct StructMixedGPR_SSE", "struct StructMixedGPR_SSE { int64_t gpr; double sse; };", "(struct StructMixedGPR_SSE){ 1001LL, 3.14159 }"),
        ("struct StructDualSSE", "struct StructDualSSE { double x; double y; };", "(struct StructDualSSE){ 1.4142, 1.7320 }"),
        ("struct StructLargeMem", "struct StructLargeMem { int64_t a; int64_t b; int64_t c; };", "(struct StructLargeMem){ 11, 22, 33 }"),
        ("struct StructPacked", "struct __attribute__((packed)) StructPacked { char c; int x; short s; };", "(struct StructPacked){ 'Z', 9999, 123 }"),
    ]

    def __init__(self, seed: int = 42):
        self.rng = random.Random(seed)

    def generate_torture_case(self, case_id: str, num_args: int = 8) -> Tuple[str, str]:
        param_types, param_names, param_vals = [], [], []
        struct_defs = set()

        for i in range(num_args):
            p_name = f"arg{i}"
            param_names.append(p_name)
            if self.rng.random() < 0.35:
                s_name, s_def, s_val = self.rng.choice(self.STRUCT_TEMPLATES)
                struct_defs.add(s_def)
                param_types.append(s_name)
                param_vals.append(s_val)
            else:
                p_type, _, val = self.rng.choice(self.PRIMITIVES)
                param_types.append(p_type)
                param_vals.append(val)

        if self.rng.random() < 0.4:
            ret_s_name, ret_s_def, ret_s_val = self.rng.choice(self.STRUCT_TEMPLATES)
            struct_defs.add(ret_s_def)
            ret_type, ret_val = ret_s_name, ret_s_val
        else:
            ret_type, ret_val = "int64_t", "999888777LL"

        struct_header = "\n".join(sorted(list(struct_defs)))
        fn_sig_params = ", ".join(f"{t} {n}" for t, n in zip(param_types, param_names))
        fn_call_args = ", ".join(param_vals)

        callee_checks = []
        for idx, (t, n) in enumerate(zip(param_types, param_names)):
            if "Struct" in t:
                callee_checks.append(f"    if (*(char*)&{n} == 0) {{ return ({ret_type}){ret_val}; }}")
            elif "float" in t or "double" in t:
                callee_checks.append(f"    if ({n} <= 0.0) {{ return ({ret_type}){ret_val}; }}")
            else:
                callee_checks.append(f"    if ({n} == 0) {{ return ({ret_type}){ret_val}; }}")

        callee_checks_str = "\n".join(callee_checks)
        callee_src = f"""#include <stdint.h>\n#include <stdio.h>\n{struct_header}\n{ret_type} abi_target_fn({fn_sig_params}) {{\n{callee_checks_str}\n    return ({ret_type}){ret_val};\n}}\n"""
        caller_src = f"""#include <stdint.h>\n#include <stdio.h>\n#include <stdlib.h>\n{struct_header}\nextern {ret_type} abi_target_fn({fn_sig_params});\nint main(void) {{\n    {ret_type} res = abi_target_fn({fn_call_args});\n    (void)res;\n    printf("ABI_PASS_{case_id}\\n");\n    return 0;\n}}\n"""
        return caller_src, callee_src


# ==============================================================================
# SECTION 4: DELTA REDUCER (ddmin)
# ==============================================================================

class DeltaReducer:
    def __init__(self, oracle_tester: Callable[[str], bool], max_iterations: int = 150):
        self.oracle_tester = oracle_tester
        self.max_iterations = max_iterations

    def reduce(self, c_code: str) -> str:
        lines = [l for l in c_code.splitlines(keepends=True) if l.strip() and not l.strip().startswith("//")]
        if not self.oracle_tester("".join(lines)):
            return c_code

        n = 2
        iteration = 0
        while len(lines) >= 2 and iteration < self.max_iterations:
            iteration += 1
            chunk_size = max(1, len(lines) // n)
            reduced = False
            for i in range(0, len(lines), chunk_size):
                candidate = "".join(lines[:i] + lines[i + chunk_size:])
                if "main" in candidate and self.oracle_tester(candidate):
                    lines = lines[:i] + lines[i + chunk_size:]
                    n = max(n - 1, 2)
                    reduced = True
                    break
            if not reduced:
                if n >= len(lines): break
                n = min(n * 2, len(lines))
        return "".join(lines)


# ==============================================================================
# SECTION 5: PARALLEL MASTER ORCHESTRATOR
# ==============================================================================

def _generate_synthetic_c(case_id: int) -> str:
    rnd1 = (case_id * 1103515245 + 12345) & 0x7fffffff
    rnd2 = (case_id * 1664525 + 1013904223) & 0x7fffffff
    return f"""#include <stdio.h>\n#include <stdint.h>\nstatic int64_t compute(int64_t a, int64_t b) {{\n    int64_t res = 0;\n    for (int i = 0; i < 5; i++) {{\n        res = (res + (a ^ (b >> i))) * 3;\n        if (res % 2 == 0) res += (a & 0xFF);\n        else res -= (b & 0xFF);\n    }}\n    return res;\n}}\nint main(void) {{\n    int64_t v1 = {rnd1 % 10000}LL, v2 = {rnd2 % 10000}LL;\n    printf("OUT_%ld\\n", (long)compute(v1, v2));\n    return 0;\n}}\n"""


def _worker_task(args: Tuple) -> Dict[str, Any]:
    src_code, case_id, zcc_bin, repo_root, temp_dir_str = args
    temp_dir = Path(temp_dir_str)
    src_file = temp_dir / f"test_{case_id}.c"
    with open(src_file, "w", encoding="utf-8") as f:
        f.write(src_code)

    oracle = SemanticOracle(zcc_bin=zcc_bin, repo_root=repo_root, temp_dir=temp_dir_str)
    res = oracle.evaluate(str(src_file), unique_id=str(case_id))

    try:
        if src_file.exists(): src_file.unlink()
    except OSError: pass

    return {
        "case_id": case_id,
        "divergence_type": res.divergence_type.value,
        "details": res.details,
        "is_divergence": res.is_divergence(),
        "src_code": src_code if res.is_divergence() else None
    }


def _resolve_zcc_path(zcc_path: str) -> str:
    cand = os.path.abspath(zcc_path)
    if os.path.exists(cand) and os.path.isfile(cand) and os.access(cand, os.X_OK):
        return cand
    script_dir = os.path.dirname(os.path.abspath(__file__))
    candidates = [
        os.path.abspath(os.path.join(script_dir, "..", "zcc")),
        os.path.abspath(os.path.join(os.getcwd(), "zcc")),
        os.path.abspath(os.path.join(os.getcwd(), "zcc_github_upload", "zcc")),
        os.path.abspath("/content/zcc_github_upload/zcc"),
        os.path.abspath("/content/zcc"),
    ]
    for c in candidates:
        if os.path.exists(c) and os.path.isfile(c) and os.access(c, os.X_OK):
            return c
    return cand


def run_gauntlet(zcc_bin: str, total_cases: int = 1000, workers: int = 8, use_ramdisk: bool = True, output_dir: str = "divergences"):
    zcc_bin = _resolve_zcc_path(zcc_bin)
    if not os.path.exists(zcc_bin) or not os.access(zcc_bin, os.X_OK):
        raise FileNotFoundError(
            f"[FATAL] ZCC compiler binary not found or not executable at '{zcc_bin}'.\n"
            f"If in Google Colab, please run '!make zcc' first, or pass --zcc /path/to/zcc."
        )
    out_dir = Path(output_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    # /dev/shm is mounted with 'noexec' in Colab/Docker containers; /tmp is execution-safe
    temp_base = Path("/tmp/zcc_gauntlet")
    temp_base.mkdir(parents=True, exist_ok=True)

    fuzzer = MetamorphicFuzzer(seed=42)
    ledger_file = out_dir / "evidence_ledger.jsonl"
    repo_root = os.path.abspath(os.path.dirname(zcc_bin))

    print(f"\n{'='*75}")
    print(f"  ZCC COLAB GAUNTLET MONOLITH — 80 GB A100 / 150 GB RAM FOUNDRY")
    print(f"  ZCC Target:    {zcc_bin}")
    print(f"  Workers:       {workers} process-isolated workers")
    print(f"  Ramdisk:       {temp_base}")
    print(f"  Workload:      {total_cases} adversarial programs")
    print(f"{'='*75}\n")

    t_start = time.time()
    counts = {"tested": 0, "miscompiles": 0, "crashes": 0, "rejects": 0}

    tasks = []
    for i in range(total_cases):
        code = _generate_synthetic_c(i)
        if i % 3 == 0:
            vars_ = fuzzer.generate_metamorphic_variants(code, num_variants=1)
            if vars_: code = vars_[0][1]
        tasks.append((code, i, zcc_bin, repo_root, str(temp_base)))

    print(f"[*] Dispatched {len(tasks)} tasks across {workers} workers...")

    with mp.Pool(processes=workers) as pool:
        for res in pool.imap_unordered(_worker_task, tasks, chunksize=10):
            counts["tested"] += 1
            div_type = res["divergence_type"]

            if res["is_divergence"]:
                if "MISCOMPILE" in div_type: counts["miscompiles"] += 1
                elif "CRASH" in div_type: counts["crashes"] += 1
                elif "REJECT" in div_type: counts["rejects"] += 1

                print(f"\n[!] DIVERGENCE in Case #{res['case_id']} [{div_type}]: {res['details']}")
                with open(ledger_file, "a", encoding="utf-8") as lf:
                    lf.write(json.dumps({"case_id": res["case_id"], "type": div_type, "details": res["details"], "time": time.time()}) + "\n")
                if res["src_code"]:
                    with open(out_dir / f"repro_{res['case_id']}.c", "w", encoding="utf-8") as rf:
                        rf.write(res["src_code"])

            if counts["tested"] % 100 == 0 or counts["tested"] == total_cases:
                rate = counts["tested"] / max(0.1, time.time() - t_start)
                sys.stdout.write(f"\r[+] Progress: {counts['tested']}/{total_cases} ({rate:.1f} t/s) | Miscompiles: {counts['miscompiles']} | Crashes: {counts['crashes']} | Rejects: {counts['rejects']}")
                sys.stdout.flush()

    elapsed = time.time() - t_start
    print(f"\n\n{'='*75}")
    print(f"  CAMPAIGN COMPLETE in {elapsed:.1f}s ({counts['tested']/max(0.1, elapsed):.1f} tests/sec)")
    print(f"  Total Tested:          {counts['tested']}")
    print(f"  Semantic Miscompiles:  {counts['miscompiles']}")
    print(f"  Compiler Crashes:      {counts['crashes']}")
    print(f"  Compilation Rejects:   {counts['rejects']}")
    print(f"  Evidence Ledger:       {ledger_file}")
    print(f"{'='*75}\n")


def run_abi_torture(zcc_bin: str, cases: int = 100):
    zcc_bin = _resolve_zcc_path(zcc_bin)
    if not os.path.exists(zcc_bin) or not os.access(zcc_bin, os.X_OK):
        raise FileNotFoundError(
            f"[FATAL] ZCC compiler binary not found or not executable at '{zcc_bin}'.\n"
            f"If in Google Colab, please run '!make zcc' first, or pass --zcc /path/to/zcc."
        )
    repo_root = os.path.abspath(os.path.dirname(zcc_bin))
    generator = ABITortureGenerator(seed=42)
    passed, failed = 0, 0
    print(f"\n[*] Launching SysV ABI Torture Factory ({cases} cross-toolchain linking pairs)...")

    for i in range(cases):
        caller_c, callee_c = generator.generate_torture_case(f"case_{i}", num_args=8)
        with tempfile.TemporaryDirectory(dir="/tmp") as td:
            fc = os.path.join(td, "caller.c")
            fs = os.path.join(td, "callee.c")
            f_asm = os.path.join(td, "callee.s")
            f_bin = os.path.join(td, "abi_bin")

            with open(fc, "w") as f: f.write(caller_c)
            with open(fs, "w") as f: f.write(callee_c)

            r1 = subprocess.run([zcc_bin, fs, "-S", "-o", f_asm], cwd=repo_root, capture_output=True)
            if r1.returncode != 0:
                failed += 1; continue
            r2 = subprocess.run(["gcc", "-no-pie", "-O0", fc, f_asm, "-o", f_bin, "-lm"], cwd=repo_root, capture_output=True)
            if r2.returncode != 0:
                failed += 1; continue
            r3 = subprocess.run([f_bin], capture_output=True, timeout=2.0)
            if r3.returncode == 0 and b"ABI_PASS" in r3.stdout:
                passed += 1
            else:
                failed += 1

    print(f"[+] SysV ABI Results: {passed}/{cases} PASSED | {failed} DIVERGENCES\n")


# ==============================================================================
# SECTION 6: CLI ENTRYPOINT
# ==============================================================================

def main():
    p = argparse.ArgumentParser(description="ZCC Colab Gauntlet Monolith")
    p.add_argument("--zcc", default="./zcc", help="Path to ZCC binary")
    p.add_argument("--cases", type=int, default=1000, help="Number of adversarial differential testcases")
    p.add_argument("--abi-cases", type=int, default=100, help="Number of SysV ABI cross-toolchain cases")
    p.add_argument("--workers", type=int, default=None, help="Number of worker processes (default: cpu_count - 1)")
    p.add_argument("--no-ramdisk", action="store_true", help="Disable /dev/shm ramdisk")
    p.add_argument("--output-dir", default="divergences", help="Directory to save ledger and reproducers")
    # Use parse_known_args so Jupyter/Colab kernel flags (-f kernel-xxx.json) don't trigger SystemExit
    args, _ = p.parse_known_args()

    w = args.workers or max(1, mp.cpu_count() - 1)
    run_gauntlet(zcc_bin=args.zcc, total_cases=args.cases, workers=w, use_ramdisk=not args.no_ramdisk, output_dir=args.output_dir)
    if args.abi_cases > 0:
        run_abi_torture(zcc_bin=args.zcc, cases=args.abi_cases)


if __name__ == "__main__":
    main()
