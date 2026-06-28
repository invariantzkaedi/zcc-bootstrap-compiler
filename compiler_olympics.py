#!/usr/bin/env python3
"""
compiler_olympics.py — ZCC Compiler Olympics Differential Test & Metrics Engine
🎰 High-Signal Stress Generators + 🏆 Real-Time Metrics Dashboard + Creduce hooks.

Phase 2: Sanitizer-oracled references, signed-zero checks, ULP tracking, struct-by-value ABI.
Phase 3: Coverage-guided Olympics — gcov instrumented ZCC build, per-category coverage table,
          new-coverage counter, cold-code-path report, persistent high-coverage corpus.
"""

import argparse
import hashlib
import json
import math
import os
import random
import re
import shutil
import struct
import subprocess
import sys
import tempfile
import time
from concurrent.futures import ProcessPoolExecutor, as_completed
from dataclasses import dataclass, asdict, field
from pathlib import Path
from typing import Optional, List, Dict, Set, Tuple

# ── Configuration ────────────────────────────────────────────────────────────
ZCC_BIN         = os.path.abspath("./zcc")
ZCC_COV_BIN     = os.path.abspath("./zcc_cov")  # Phase 3: gcov-instrumented ZCC
GCC_BIN         = "gcc"
CLANG_BIN       = "clang"
CSMITH_BIN      = "csmith"
CREDUCE_BIN     = "creduce"
CSMITH_INCLUDE  = "/usr/include/csmith"
SYS_INC         = os.path.abspath("zcc_sys_includes")
OLYMPICS_DIR    = Path("olympics_batches")
REGRESSIONS_DIR = Path("tests/regressions")
COVERAGE_DIR    = Path("olympics_coverage")      # Phase 3: gcov data landing zone
CORPUS_DIR      = Path("olympics_corpus")         # Phase 3: high-coverage seed corpus
DIVERGENCE_LOG  = OLYMPICS_DIR / "olympics_divergences.jsonl"
COVERAGE_LOG    = COVERAGE_DIR / "coverage_history.jsonl"  # Phase 3: run history

# Phase 3: the actual compilation units fed to gcc when building zcc_cov.
# gcov only tracks files it directly compiled — NOT the part*.c fragments
# that were concatenated into zcc.c before the build.
COV_SOURCE_FILES: List[str] = [
    "zcc.c",                    # Main monolith (parser + codegen + all parts)
    "compiler_passes.c",
    "compiler_passes_ir.c",
    "ir_pass_manager.c",
    "ir_pass_warden.c",
    "ir_pass_taint.c",
    "ir_pass_healer.c",
    "ir_symbolic_cfg.c",
    "ir_dominance.c",
    "ir_ssa.c",
    "evm_lifter.c",
    "ir_vuln_tag.c",
    "ir_to_evm.c",
    "ir_evm_stack.c",
    "src/ir_lower_float.c",
    "src/x86_codegen_sse.c",
    "ir_telemetry.c",
    "zcc_telemetry.c",
    "src/zcc_oracle_substrate.c",
    "src/elf_emit.c",
    "src/codegen.c",
    "src/ir_serialization.c",
    "src/zcc_smt_prover.c",
    "transient_state.c",
    "zcc_lucky_alert_injector.c",
]
COMPILE_TIMEOUT = 15    # seconds
EXEC_TIMEOUT    = 5     # seconds

CSMITH_FLAGS = [
    "--no-bitfields",
    "--no-comma-operators",
    "--no-volatiles",
    "--no-volatile-pointers",
    "--no-const-pointers",
    "--no-consts",
    "--no-packed-struct",
    "--no-inline-function",
    "--no-longlong",
    "--no-safe-math",
    "--max-funcs", "3",
    "--max-block-depth", "3",
]

# ── Data structures ───────────────────────────────────────────────────────────
@dataclass
class FuzzResult:
    seed:          int
    category:      str          # "csmith" | "const_fold" | "reg_pressure" | "abi_chaos" | "abi_struct_val" | "fp_torture" | "stack_stress" | "huge_init"
    status:        str          # "pass" | "mismatch_stdout" | "crash_zcc_pp" | "crash_zcc_cg" | "crash_gcc" | "ref_disagreement" | "timeout"
    config_matrix: Dict[str, str] # config_name -> result (exit_code, stdout_hash)
    reduced_path:  Optional[str]
    elapsed_ms:    int
    new_cov_lines: int = 0      # Phase 3: lines newly covered by this test case

@dataclass
class CoverageSnapshot:
    """Phase 3: one gcov harvest result from a single test run."""
    seed:          int
    category:      str
    covered_lines: int
    total_lines:   int
    new_lines:     int          # lines not seen in any prior run this session
    pct:           float        # covered_lines / total_lines * 100
    gcov_files:    List[str]    # gcda files that were touched

# ── Helpers ───────────────────────────────────────────────────────────────────
def run(cmd, timeout, input_data=None, cwd=None):
    try:
        r = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=timeout,
            cwd=cwd,
        )
        return r.returncode, r.stdout, r.stderr
    except subprocess.TimeoutExpired:
        return -999, "", "TIMEOUT"
    except Exception as e:
        return -998, "", str(e)

def get_hash(val: str) -> str:
    if not val:
        return "N/A"
    if "crash" in val:
        return val
    return hashlib.sha256(val.encode("utf-8")).hexdigest()[:16]

# ── Float helpers: relative error + true ULP distance ────────────────────────
def parse_floats(text: str) -> List[float]:
    """Extract all float/nan/inf tokens from a text output string."""
    pattern = r"[-+]?\d*\.\d+(?:[eE][-+]?\d+)?|[-+]?\b(?:nan|inf)\b"
    matches = re.findall(pattern, text, re.IGNORECASE)
    result = []
    for m in matches:
        m_lower = m.lower()
        if "nan" in m_lower:
            result.append(float("nan"))
        elif "inf" in m_lower:
            if m.startswith("-"):
                result.append(float("-inf"))
            else:
                result.append(float("inf"))
        else:
            try:
                result.append(float(m))
            except ValueError:
                pass
    return result

def _f64_to_bits(f: float) -> int:
    """Reinterpret a Python float (IEEE-754 double) as its 64-bit integer bit pattern."""
    b = struct.pack("d", f)
    return struct.unpack("Q", b)[0]

def true_ulp_distance(a: float, b: float) -> int:
    """True IEEE-754 ULP distance between two finite doubles.

    Returns the number of representable doubles between |a| and |b|.
    Returns -1 for NaN/Inf inputs (caller should skip).
    The signed-zero pair (+0.0, -0.0) has ULP distance 0 by IEEE convention;
    use the signed-zero sign check separately for correctness.
    """
    if math.isnan(a) or math.isnan(b) or math.isinf(a) or math.isinf(b):
        return -1
    # Map to "biased" representation: flip sign bit for negatives so the
    # integer ordering mirrors the float ordering across zero.
    def _signed_bits(x: float) -> int:
        bits = _f64_to_bits(x)
        if bits >> 63:                    # negative
            return -(bits & 0x7FFFFFFFFFFFFFFF)
        return bits
    return abs(_signed_bits(a) - _signed_bits(b))

def float_outputs_match(out_a: str, out_b: str, tol: float = 1e-5) -> bool:
    """Match float outputs with tolerance, NaN identity, Inf sign, and signed-zero bit checks."""
    floats_a = parse_floats(out_a)
    floats_b = parse_floats(out_b)
    if len(floats_a) != len(floats_b):
        return False
    for f_a, f_b in zip(floats_a, floats_b):
        if math.isnan(f_a):
            if not math.isnan(f_b):
                return False
        elif math.isinf(f_a):
            if not math.isinf(f_b) or (f_a > 0) != (f_b > 0):
                return False
        else:
            if math.isnan(f_b) or math.isinf(f_b):
                return False
            # Signed zero bit identity check
            if f_a == 0.0 and f_b == 0.0:
                if math.copysign(1.0, f_a) != math.copysign(1.0, f_b):
                    return False
            if abs(f_a - f_b) > tol:
                return False
    non_float_a = re.sub(r"[-+]?\d*\.\d+(?:[eE][-+]?\d+)?|[-+]?\b(?:nan|inf)\b", "", out_a, flags=re.IGNORECASE).strip()
    non_float_b = re.sub(r"[-+]?\d*\.\d+(?:[eE][-+]?\d+)?|[-+]?\b(?:nan|inf)\b", "", out_b, flags=re.IGNORECASE).strip()
    return non_float_a == non_float_b

def get_max_float_rel_error(out_a: str, out_b: str) -> float:
    """Maximum relative error between corresponding float pairs in two output strings.

    This is a *relative error proxy*, not a ULP count.  For true ULP distance
    use true_ulp_distance().  Renamed from get_max_float_error (Phase 3).
    """
    floats_a = parse_floats(out_a)
    floats_b = parse_floats(out_b)
    if len(floats_a) != len(floats_b) or not floats_b:
        return 0.0
    max_err = 0.0
    for f_a, f_b in zip(floats_a, floats_b):
        if math.isnan(f_a) or math.isnan(f_b) or math.isinf(f_a) or math.isinf(f_b):
            continue
        diff = abs(f_a - f_b)
        rel_err = diff / abs(f_b) if f_b != 0.0 else diff
        max_err = max(max_err, rel_err)
    return max_err

def get_max_ulp_distance(out_a: str, out_b: str) -> int:
    """Maximum true IEEE-754 ULP distance between corresponding float pairs."""
    floats_a = parse_floats(out_a)
    floats_b = parse_floats(out_b)
    if len(floats_a) != len(floats_b) or not floats_b:
        return 0
    max_ulp = 0
    for f_a, f_b in zip(floats_a, floats_b):
        d = true_ulp_distance(f_a, f_b)
        if d >= 0:
            max_ulp = max(max_ulp, d)
    return max_ulp

# ── Phase 3: Coverage infrastructure ─────────────────────────────────────────

def build_zcc_cov(build_timeout: int = 600) -> bool:
    """Build a gcov-instrumented copy of zcc as ./zcc_cov.

    Uses the pre-assembled zcc.c (same as `make zcc`) plus PASSES but
    with -fprofile-arcs -ftest-coverage.  Returns True on success.
    zcc_cov is *never* stripped so gcov can find the debug info.

    The build is slow on first run (ZCC is a 1MB+ monolith); subsequent
    runs can reuse the binary if the source is unchanged.
    """
    # Re-use the pre-assembled zcc.c — it is always current after `make zcc`
    zcc_cov_src = Path("zcc.c")
    if not zcc_cov_src.exists():
        # Fall back: assemble from parts
        parts = [
            "part1.c", "part0_pp.c", "part2.c", "part3.c",
            "sym_type_ast_ir.c", "part4.c", "zcc_ast_serializer.c",
            "part5.c", "part7_rust.c", "part6_arm.c",
            "ir.c", "ir_to_x86.c", "regalloc.c",
            "ir_telemetry_stub.c", "forgezero_receipt_stub.c",
            "zcc_layout.c", "zcc_layout_dump.c", "zcc_static_assert.c",
        ]
        zcc_cov_src = Path("zcc_cov.c")
        try:
            with open(zcc_cov_src, "w") as out_f:
                for p in parts:
                    pp = Path(p)
                    if pp.exists():
                        out_f.write(pp.read_text(errors="replace"))
        except Exception as e:
            print(f"[cov] Could not assemble zcc_cov.c: {e}")
            return False

    def _read_passes_from_makefile() -> List[str]:
        """Parse the PASSES variable from the Makefile. Returns a list of .c file paths."""
        makefile = Path("Makefile")
        if not makefile.exists():
            return []
        text = makefile.read_text(errors="replace")
        # Find "PASSES = ..." (may span a single long line)
        m = re.search(r"^PASSES\s*=\s*(.+)$", text, re.MULTILINE)
        if not m:
            return []
        return m.group(1).split()

    passes = _read_passes_from_makefile()
    if not passes:
        # Complete fallback matching the Makefile PASSES exactly
        passes = [
            "compiler_passes.c", "compiler_passes_ir.c",
            "ir_pass_manager.c", "ir_pass_warden.c",
            "ir_pass_taint.c", "ir_pass_healer.c",
            "ir_symbolic_cfg.c", "ir_dominance.c", "ir_ssa.c",
            "evm_lifter.c", "ir_vuln_tag.c",
            "ir_to_evm.c", "ir_evm_stack.c",
            "src/ir_lower_float.c", "src/x86_codegen_sse.c",
            "src/evm/decompiler.c", "src/evm/jit.c", "src/evm/symbolic.c",
            "src/evm/memory_v2.c", "src/evm/abi_extractor.c",
            "src/evm/jit_memory.c", "src/evm/proof_export.c",
            "src/evm/ipc_bridge.c", "src/evm/yul_weaver.c",
            "src/evm/yul_fixed_point.c", "src/evm/yul_frontend.c",
            "src/gfx/sdf_compiler.c", "src/gfx/mesh_warden.c",
            "src/evm/evm_symbolic_harness.c",
            "ir_telemetry.c", "zcc_telemetry.c",
            "src/zcc_oracle_substrate.c", "src/elf_emit.c",
            "src/codegen.c", "src/ir_serialization.c",
            "src/zcc_smt_prover.c", "src/gguf_emit.c",
            "src/zld.c", "src/zcc_resource_oracle.c",
            "transient_state.c", "zcc_lucky_alert_injector.c",
        ]
        print("[cov] Makefile PASSES not found; using fallback list.")
    cov_flags = [
        "-O0", "-w", "-g",
        "-fprofile-arcs", "-ftest-coverage",
        "-DZCC_REAL_TELEMETRY", "-Dmain=zcc_main",
    ]

    # Filter passes to only those that exist
    existing_passes = [p for p in passes if Path(p).exists()]

    cmd = ["gcc"] + cov_flags + [str(zcc_cov_src)] + existing_passes + ["-o", ZCC_COV_BIN, "-lm"]
    print(f"[cov] Building zcc_cov (gcov-instrumented, timeout={build_timeout}s)...")
    rc, out, err = run(cmd, timeout=build_timeout)
    if rc != 0:
        print(f"[cov] zcc_cov build FAILED (rc={rc}):\n{err[:400]}")
        return False
    print(f"[cov] zcc_cov built OK → {ZCC_COV_BIN}")
    return True

def reset_gcov_counters() -> None:
    """Zero all .gcda files so each run starts from a clean coverage slate."""
    for gcda in Path(".").rglob("*.gcda"):
        try:
            gcda.unlink()
        except OSError:
            pass

def harvest_gcov() -> Tuple[int, int, List[str]]:
    """Run gcov on all zcc_cov-*.gcno files and return (covered_lines, total_lines, gcov_annotation_files).

    When GCC compiles multiple TUs in a single link command:
      gcc ... zcc.c compiler_passes.c ... -o zcc_cov
    it names the coverage files using the OUTPUT binary as prefix:
      zcc_cov-zcc.gcno, zcc_cov-compiler_passes.gcno, etc.

    gcov called with the .gcno file reads the corresponding .gcda and writes
    a <stem>.c.gcov annotation file in the current directory.
    """
    covered = 0
    total = 0
    gcov_files = []
    gcno_files = sorted(Path(".").glob("zcc_cov-*.gcno"))
    if not gcno_files:
        return 0, 0, []
    for gcno in gcno_files:
        rc, out, err = run(["gcov", "-b", str(gcno)], timeout=30)
        # gcov exits non-zero when .gcda is missing; still parse summary from stderr
        for line in (out + err).splitlines():
            m = re.search(r"Lines executed:(\d+\.\d+)% of (\d+)", line)
            if m:
                pct = float(m.group(1))
                tot = int(m.group(2))
                cov = int(round(pct / 100.0 * tot))
                covered += cov
                total   += tot
        # gcov writes e.g. "codegen.c.gcov" derived from the source path inside the .gcno
        # Scan for any new .gcov files created (gcov prints "Creating 'foo.c.gcov'")
        for line in (out + err).splitlines():
            m2 = re.search(r"Creating '(.+\.gcov)'", line)
            if m2:
                gf = Path(m2.group(1))
                if gf.exists() and str(gf) not in gcov_files:
                    gcov_files.append(str(gf))
    return covered, total, gcov_files

def run_zcc_cov_single(src_path: str) -> bool:
    """Run zcc_cov (gcov build) on a single preprocessed source file.

    We only need to trigger coverage counters; output is discarded.
    Returns True if zcc_cov exited 0 (compiled successfully).
    """
    with tempfile.TemporaryDirectory(prefix="cov_run_") as td:
        out_s = os.path.join(td, "cov_out.s")
        pp_c  = os.path.join(td, "cov_pp.c")
        # Preprocess with regular ZCC
        pp_rc, pp_out, _ = run(
            [ZCC_BIN, f"-I{SYS_INC}", f"-I{CSMITH_INCLUDE}", "--pp-only", src_path],
            timeout=COMPILE_TIMEOUT,
        )
        if pp_rc != 0:
            return False
        with open(pp_c, "w") as f:
            f.write(pp_out)
        cov_rc, _, _ = run(
            [ZCC_COV_BIN, f"-I{SYS_INC}", pp_c, "-o", out_s],
            timeout=COMPILE_TIMEOUT,
        )
        return cov_rc == 0

def compute_coverage_delta(
    seen_globally: Set[Tuple[str, int]],
    gcov_files: List[str],
) -> Tuple[int, Set[Tuple[str, int]]]:
    """Parse gcov annotation files and find lines newly covered vs. seen_globally.

    gcov -b -n produces a .c.gcov file with lines like:
        12345:   42:    some_code();
        #####:   43:    never_reached();

    Returns (new_line_count, updated_seen_set).
    """
    new_lines: Set[Tuple[str, int]] = set()
    for gcov_file in gcov_files:
        p = Path(gcov_file)
        if not p.exists():
            continue
        fname = p.stem  # e.g. "part4.c"
        try:
            for raw_line in p.read_text(errors="replace").splitlines():
                parts = raw_line.split(":", 2)
                if len(parts) < 2:
                    continue
                count_str = parts[0].strip()
                lineno_str = parts[1].strip()
                if count_str in ("-", "#####", ""):
                    continue
                try:
                    if int(count_str) > 0:
                        key = (fname, int(lineno_str))
                        if key not in seen_globally:
                            new_lines.add(key)
                except ValueError:
                    continue
        except Exception:
            continue
    seen_globally.update(new_lines)
    return len(new_lines), seen_globally

COV_SOURCE_FILES = [
    "part1.c", "part0_pp.c", "part2.c", "part3.c",
    "sym_type_ast_ir.c", "part4.c", "part5.c",
    "compiler_passes.c", "ir_pass_manager.c",
    "regalloc.c", "ir_to_x86.c", "ir.c",
]

# ── Custom Code Generators ────────────────────────────────────────────────────
def generate_const_fold(seed: int) -> str:
    random.seed(seed)
    lines = [
        "/* ZCC Olympics: Constant Folding Verifier */",
        "#include <stdio.h>",
        "#include <math.h>",
        "double compute_fold(void) {",
    ]
    
    # Generate nested arithmetic on floats and doubles
    for i in range(15):
        op = random.choice(["+", "-", "*", "/"])
        val1 = random.uniform(1.0, 100.0)
        val2 = random.uniform(1.0, 50.0)
        lines.append(f"    double v{i} = ({val1:.6f} {op} {val2:.6f});")
    
    # Generate calls to math functions with constant inputs (avoiding hypot)
    lines.extend([
        "    double s = sin(0.5) + cos(0.25) - tan(0.125);",
        "    double q = sqrt(4.0) + pow(2.0, 3.0) - atan2(3.0, 4.0);",
    ])
    
    # Mix and combine variables
    lines.append("    double total = s + q;")
    for i in range(15):
        lines.append(f"    total = total + v{i};")
        
    lines.extend([
        "    return total;",
        "}",
        "int main(void) {",
        "    printf(\"%.15g\\n\", compute_fold());",
        "    return 0;",
        "}",
    ])
    return "\n".join(lines)

def generate_reg_pressure(seed: int, mutate: bool = False) -> str:
    """Register pressure generator.

    When mutate=True, uses seed-derived diversity knobs:
      variable_count: 20-200, loop_depth: 1-5,
      arrays, pointer_locals, func_calls, conditionals.
    """
    random.seed(seed)
    if mutate:
        n_d      = random.randint(20, 80)
        n_f      = random.randint(10, 40)
        n_u      = random.randint(10, 40)
        loop_dep = random.randint(1, 5)
        use_arr  = random.random() < 0.5
        use_ptr  = random.random() < 0.4
        use_fn   = random.random() < 0.5
        use_cond = random.random() < 0.5
    else:
        n_d = 40; n_f = 20; n_u = 20
        loop_dep = 1
        use_arr = use_ptr = use_fn = use_cond = False

    lines = [
        "/* ZCC Olympics: Register Pressure Monster */",
        "#include <stdio.h>",
    ]

    if use_fn:
        lines += [
            "static double helper(double x, double y) { return x * 0.99 + y * 0.01; }",
            "static float fhelper(float a, float b) { return a + b * 0.5f; }",
        ]

    lines.append("double register_pressure(double start) {")

    for i in range(n_d):
        lines.append(f"    double d{i} = start + {i * 1.5:.2f};")
    for i in range(n_f):
        lines.append(f"    float f{i} = (float)(start * {i * 0.5:.2f}f);")
    for i in range(n_u):
        lines.append(f"    unsigned int n{i} = {i * 7};")
    if use_arr:
        arr_sz = random.randint(8, 32)
        lines.append(f"    double arr[{arr_sz}];")
        for i in range(arr_sz):
            lines.append(f"    arr[{i}] = start + {i:.1f};")
    if use_ptr:
        lines.append(f"    double *pd = &d0; double ptot = 0.0;")

    # Nested loops
    indent = "    "
    for _depth in range(loop_dep):
        ivar = f"i{_depth}"
        lines.append(f"{indent}for (int {ivar} = 0; {ivar} < 50; {ivar}++) {{")
        indent += "    "

    for i in range(n_d - 1):
        if use_fn:
            lines.append(f"{indent}d{i} = helper(d{i}, d{i+1});")
        else:
            lines.append(f"{indent}d{i} = (d{i} + d{i+1}) * 0.99;")
    for i in range(n_f - 1):
        if use_fn:
            lines.append(f"{indent}f{i} = fhelper(f{i}, f{i+1});")
        else:
            lines.append(f"{indent}f{i} = (f{i} * f{i+1}) + 0.01f;")
    for i in range(n_u - 1):
        lines.append(f"{indent}n{i} = (n{i} + n{i+1}) ^ 0x5f;")
    if use_arr:
        lines.append(f"{indent}arr[{ivar if loop_dep > 0 else '0'} % {arr_sz}] += 1.0;")
    if use_ptr:
        lines.append(f"{indent}ptot += *pd;")
    if use_cond:
        lines.append(f"{indent}if (d0 < 0.0) {{ d0 = -d0; continue; }}")

    # Close nested loops
    for _depth in range(loop_dep):
        indent = indent[:-4]
        lines.append(f"{indent}}}")

    lines += [
        "    double sum_d = 0.0;",
        "    double sum_f = 0.0;",
        "    unsigned int sum_n = 0;",
    ]
    for i in range(n_d):
        lines.append(f"    sum_d += d{i};")
    for i in range(n_f):
        lines.append(f"    sum_f += (double)f{i};")
    for i in range(n_u):
        lines.append(f"    sum_n ^= n{i};")
    if use_arr:
        lines.append(f"    for (int _a = 0; _a < {arr_sz}; _a++) sum_d += arr[_a];")
    if use_ptr:
        lines.append("    sum_d += ptot;")
    lines += [
        "    return sum_d + sum_f + (double)sum_n;",
        "}",
        "int main(void) {",
        "    printf(\"%.15g\\n\", register_pressure(5.0));",
        "    return 0;",
        "}",
    ]
    return "\n".join(lines)

def generate_abi_chaos(seed: int) -> str:
    random.seed(seed)
    lines = [
        "/* ZCC Olympics: ABI Chaos Struct Layout */",
        "#include <stdio.h>",
        "#include <string.h>",
        "typedef struct __attribute__((packed)) {",
        "    char a;",
        "    double b;",
        "    int c;",
        "} StructA;",
        "typedef struct {",
        "    float x;",
        "    StructA y;",
        "    short z;",
        "} StructB;",
        "void update_abi(StructB *val, StructB *ptr) {",
        "    val->x += ptr->x;",
        "    val->y.a ^= ptr->y.a;",
        "    val->y.b += ptr->y.b;",
        "    val->y.c ^= ptr->y.c;",
        "    val->z += ptr->z;",
        "}",
        "int main(void) {",
        "    StructB b1 = { 1.5f, { 'K', 10.25, 42 }, 7 };",
        "    StructB b2 = { 2.5f, { 'M', 20.50, 84 }, 14 };",
        "    /* Verify layout offsets byte-for-byte */",
        "    printf(\"offsets: a=%d b=%d c=%d x=%d y=%d z=%d\\n\", ",
        "           (int)((char*)&b1.y.a - (char*)&b1),",
        "           (int)((char*)&b1.y.b - (char*)&b1),",
        "           (int)((char*)&b1.y.c - (char*)&b1),",
        "           (int)((char*)&b1.x - (char*)&b1),",
        "           (int)((char*)&b1.y - (char*)&b1),",
        "           (int)((char*)&b1.z - (char*)&b1));",
        "    update_abi(&b1, &b2);",
        "    printf(\"res: x=%.2f y.a=%d y.b=%.2f y.c=%d z=%d\\n\", ",
        "           (double)b1.x, (int)b1.y.a, b1.y.b, b1.y.c, (int)b1.z);",
        "    return 0;",
        "}",
    ]
    return "\n".join(lines)

def generate_abi_struct_val(seed: int, mutate: bool = False) -> str:
    """ABI struct-by-value parameter generator.

    When mutate=True: 1-6 fields, mixed int/float/double/char, optional
    nested structs, multiple pass+return-by-value operations.
    """
    random.seed(seed)
    if mutate:
        n_fields  = random.randint(1, 6)
        types     = random.choices(["int", "float", "double", "char", "unsigned"], k=n_fields)
        use_nest  = random.random() < 0.4
        n_ops     = random.randint(2, 5)
    else:
        # Fixed baseline: int2, float2, double2
        types     = None
        use_nest  = False
        n_ops     = 3

    if not mutate:
        # Original fixed version
        lines = [
            "/* ZCC Olympics: ABI Struct-by-Value Parameters */",
            "#include <stdio.h>",
            "typedef struct { int a; int b; } IntStruct;",
            "typedef struct { float x; float y; } FloatStruct;",
            "typedef struct { double u; double v; } DoubleStruct;",
            "IntStruct add_ints(IntStruct s1, IntStruct s2) {",
            "    IntStruct res; res.a = s1.a + s2.a; res.b = s1.b + s2.b; return res;",
            "}",
            "FloatStruct add_floats(FloatStruct s1, FloatStruct s2) {",
            "    FloatStruct res; res.x = s1.x + s2.x; res.y = s1.y + s2.y; return res;",
            "}",
            "DoubleStruct add_doubles(DoubleStruct s1, DoubleStruct s2) {",
            "    DoubleStruct res; res.u = s1.u + s2.u; res.v = s1.v + s2.v; return res;",
            "}",
            "int main(void) {",
            "    IntStruct i1 = { 10, 20 }, i2 = { 5, 15 };",
            "    FloatStruct f1 = { 1.5f, 2.5f }, f2 = { 0.5f, 1.0f };",
            "    DoubleStruct d1 = { 10.5, 20.5 }, d2 = { 5.25, 15.25 };",
            "    IntStruct ir = add_ints(i1, i2);",
            "    FloatStruct fr = add_floats(f1, f2);",
            "    DoubleStruct dr = add_doubles(d1, d2);",
            "    printf(\"ints: %d %d\\n\", ir.a, ir.b);",
            "    printf(\"floats: %.2f %.2f\\n\", (double)fr.x, (double)fr.y);",
            "    printf(\"doubles: %.2f %.2f\\n\", dr.u, dr.v);",
            "    return 0;",
            "}",
        ]
        return "\n".join(lines)

    # Mutated version
    CTYPE_FMT = {
        "int":      ("int",      lambda: str(random.randint(1, 100)),         "%d",    "(int)"),
        "float":    ("float",    lambda: f"{random.uniform(0.5,50.0):.2f}f",  "%.2f", "(double)"),
        "double":   ("double",   lambda: f"{random.uniform(0.5,50.0):.2f}",   "%.2f", ""),
        "char":     ("char",     lambda: str(random.randint(65, 90)),          "%d",    "(int)"),
        "unsigned": ("unsigned", lambda: str(random.randint(1, 200)),          "%u",    "(unsigned)"),
    }
    lines = [
        "/* ZCC Olympics: ABI Struct-by-Value (mutated) */",
        "#include <stdio.h>",
    ]
    if use_nest:
        lines += [
            "typedef struct { int nx; double ny; } Inner;",
        ]
    field_decls = []
    for fi, t in enumerate(types):
        ctype = CTYPE_FMT[t][0]
        if use_nest and fi == 0:
            field_decls.append("    Inner inner_fld;")
        else:
            field_decls.append(f"    {ctype} f{fi};")
    lines.append("typedef struct {")
    lines.extend(field_decls)
    lines.append("} S;")
    # Operator functions
    for op_i in range(n_ops):
        lines.append(f"S op{op_i}(S a, S b) {{")
        lines.append("    S r;")
        for fi, t in enumerate(types):
            if use_nest and fi == 0:
                lines.append(f"    r.inner_fld.nx = a.inner_fld.nx + b.inner_fld.nx;")
                lines.append(f"    r.inner_fld.ny = a.inner_fld.ny - b.inner_fld.ny;")
            else:
                op_sym = random.choice(["+", "-", "|", "^"])
                if t in ("float", "double"):
                    op_sym = random.choice(["+", "-", "*"])
                if t == "char":
                    lines.append(f"    r.f{fi} = ({CTYPE_FMT[t][0]})(a.f{fi} {op_sym} b.f{fi});")
                else:
                    lines.append(f"    r.f{fi} = a.f{fi} {op_sym} b.f{fi};")
        lines.append("    return r;")
        lines.append("}")
    # main
    lines.append("int main(void) {")
    for vi in range(2):
        inits = []
        for fi, t in enumerate(types):
            if use_nest and fi == 0:
                inits.append((str(random.randint(1,10)), f"{random.uniform(1.0,10.0):.2f}"))
            else:
                inits.append(CTYPE_FMT[t][1]())
        if use_nest:
            # ZCC does not support nested brace initializers ({ { {a,b} }, c }).
            # Use field-by-field assignment instead — fully supported.
            lines.append(f"    S v{vi};")
            lines.append(f"    v{vi}.inner_fld.nx = {inits[0][0]};")
            lines.append(f"    v{vi}.inner_fld.ny = {inits[0][1]};")
            for fi, t in enumerate(types):
                if fi == 0:
                    continue  # inner_fld already set above
                lines.append(f"    v{vi}.f{fi} = {inits[fi]};")
        else:
            lines.append(f"    S v{vi} = {{ " + ", ".join(inits) + " };")
    lines.append("    S res = v0;")
    for op_i in range(n_ops):
        lines.append(f"    res = op{op_i}(res, v1);")
    for fi, t in enumerate(types):
        if use_nest and fi == 0:
            lines.append('    printf("nest: %d %.2f\\n", res.inner_fld.nx, res.inner_fld.ny);')
        else:
            fmt = CTYPE_FMT[t][2]
            cast = CTYPE_FMT[t][3]
            lines.append(f'    printf("{fmt}\\n", {cast}res.f{fi});')
    lines += ["    return 0;", "}"]
    return "\n".join(lines)


def generate_fp_torture(seed: int) -> str:
    random.seed(seed)
    lines = [
        "/* ZCC Olympics: Floating-Point Torture */",
        "#include <stdio.h>",
        "#include <math.h>",
        "int main(void) {",
        "    volatile double zero = 0.0;",
        "    volatile double one = 1.0;",
        "    double nan_val = zero / zero;",
        "    double inf_val = one / zero;",
        "    double minf_val = -one / zero;",
        "    double subnormal = 4.9406564584124654e-324;",
        "    /* Signed Zero output values */",
        "    volatile double pzero = 0.0;",
        "    volatile double mzero = -0.0;",
        "    printf(\"is_nan: %d %d\\n\", isnan(nan_val), isnan(one));",
        "    printf(\"is_inf: %d %d\\n\", isinf(inf_val), isinf(nan_val));",
        "    printf(\"sqrt(-1): %.5f\\n\", sqrt(-1.0));",
        "    printf(\"atan2: %.5f\\n\", atan2(1.0, 0.0));",
        "    printf(\"subnormal: %.5g\\n\", subnormal);",
        "    printf(\"signed_zeros: %.1f %.1f\\n\", pzero, mzero);",
        "    return 0;",
        "}",
    ]
    return "\n".join(lines)

def generate_stack_stress(seed: int, mutate: bool = False) -> str:
    """Stack stress recursion generator.

    When mutate=True: depth 50-500, frame_size 16-256,
    optional mutual recursion between two functions.
    """
    random.seed(seed)
    if mutate:
        depth      = random.randint(50, 500)
        frame_sz   = random.randint(16, 256)
        mutual_rec = random.random() < 0.5
        double_arr = frame_sz // 8
        ulong_arr  = frame_sz // 8
    else:
        depth = random.randint(100, 250)
        frame_sz   = 32 + 16       # matches original (double[32] + unsigned long[16])
        double_arr = 32
        ulong_arr  = 16
        mutual_rec = False

    lines = [
        "/* ZCC Olympics: Stack Stress Recursion */",
        "#include <stdio.h>",
    ]
    if mutual_rec:
        # Forward declaration for mutual recursion
        lines.append("unsigned long recurse_b(int depth, unsigned long checksum);")
        lines += [
            "unsigned long recurse_a(int depth, unsigned long checksum) {",
            f"    double temp[{double_arr}];",
            f"    unsigned long buf[{ulong_arr}];",
            f"    for (int i = 0; i < {double_arr}; i++) temp[i] = (double)depth * 1.25;",
            f"    for (int i = 0; i < {ulong_arr}; i++) buf[i] = checksum ^ depth;",
            "    if (depth <= 0) return checksum + buf[0] + (unsigned long)temp[0];",
            "    return recurse_b(depth - 1, checksum + buf[0] + (unsigned long)temp[0]);",
            "}",
            "unsigned long recurse_b(int depth, unsigned long checksum) {",
            f"    double temp[{double_arr}];",
            f"    unsigned long buf[{ulong_arr}];",
            f"    for (int i = 0; i < {double_arr}; i++) temp[i] = (double)depth * 0.75;",
            f"    for (int i = 0; i < {ulong_arr}; i++) buf[i] = checksum ^ (depth * 3);",
            "    if (depth <= 0) return checksum + buf[0] + (unsigned long)temp[0];",
            "    return recurse_a(depth - 1, checksum ^ buf[0] ^ (unsigned long)temp[0]);",
            "}",
        ]
        entry_fn = "recurse_a"
    else:
        lines += [
            "unsigned long recurse(int depth, unsigned long checksum) {",
            f"    double temp[{double_arr}];",
            f"    unsigned long buf[{ulong_arr}];",
            f"    for (int i = 0; i < {double_arr}; i++) temp[i] = (double)depth * 1.25;",
            f"    for (int i = 0; i < {ulong_arr}; i++) buf[i] = checksum ^ depth;",
            "    if (depth <= 0) {",
            "        return checksum + buf[0] + (unsigned long)temp[0];",
            "    }",
            "    return recurse(depth - 1, checksum + buf[0] + (unsigned long)temp[0]);",
            "}",
        ]
        entry_fn = "recurse"

    lines += [
        "int main(void) {",
        f"    printf(\"stack_stress: %lu\\n\", {entry_fn}({depth}, 0xDEADBEEFUL));",
        "    return 0;",
        "}",
    ]
    return "\n".join(lines)

def generate_huge_init(seed: int) -> str:
    random.seed(seed)
    lines = [
        "/* ZCC Olympics: Huge Initializers */",
        "#include <stdio.h>",
        "struct Record {",
        "    int id;",
        "    double value[2];",
        "    char *desc;",
        "};",
        "struct Record records[300] = {",
    ]
    
    # Generate 300 initializer elements
    for i in range(300):
        val1 = random.uniform(1.0, 100.0)
        val2 = random.uniform(1.0, 100.0)
        lines.append(f"    {{ {i}, {{ {val1:.4f}, {val2:.4f} }}, \"record_{i}\" }},")
        
    lines.extend([
        "};",
        "int main(void) {",
        "    double total_val = 0.0;",
        "    long id_xor = 0;",
        "    for (int i = 0; i < 300; i++) {",
        "        total_val += records[i].value[0] - records[i].value[1];",
        "        id_xor ^= records[i].id;",
        "    }",
        "    printf(\"total: %.6f xor=%ld\\n\", total_val, id_xor);",
        "    return 0;",
        "}",
    ])
    return "\n".join(lines)


# ── ABI Matrix Generator ───────────────────────────────────────────────────────
def generate_abi_matrix(seed: int) -> tuple:
    """ABI Matrix exhaustive generator.

    Axes:
      size:    8,16,24,32,40,48 (standard ABI class boundaries)
               15,17,23,25,31,33 (boundary-neighbors, 1B around each threshold)
      shape:   flat, nested, array, union
      mode:    pass, return, pass_return, chain
      packing: normal (standard) | forced __packed__ (neighbor sizes)

    Bucket scheduling is DETERMINISTIC:
      target_size = ALL_BUCKETS[(seed - 1) % 12]
    → --seeds 12  guarantees all 12 buckets covered exactly once.
    → --seeds  6  covers the 6 standard ABI boundaries.

    Returns (src_code: str, target_size: int).
    """
    random.seed(seed)

    # Standard ABI class boundaries (SysV AMD64)
    STD_BUCKETS  = [8, 16, 24, 32, 40, 48]
    # Boundary-neighbor sizes (one byte each side of ABI threshold)
    # These require __attribute__((packed)) to hit the odd byte count.
    NEIGH_BUCKETS = [15, 17, 23, 25, 31, 33]
    ALL_BUCKETS   = STD_BUCKETS + NEIGH_BUCKETS  # 12 total

    # Deterministic scheduling: guarantees full coverage in 12 seeds
    target_size = ALL_BUCKETS[(seed - 1) % len(ALL_BUCKETS)]
    is_neighbor = target_size in NEIGH_BUCKETS

    # Verified field recipes → exact struct sizes (SysV AMD64)
    SIZE_RECIPES: dict = {
        # ── Standard ABI boundaries (no __packed__ needed) ──────────────────
        8:  [
            ["double"],
            ["int", "int"],
            ["float", "float"],
            ["unsigned", "unsigned"],
            ["int", "float"],
        ],
        16: [
            ["double", "double"],
            ["double", "int", "int"],        # 8+4+4
            ["int", "int", "int", "int"],
            ["float", "float", "float", "float"],
            ["double", "float", "float"],    # 8+4+4
        ],
        24: [
            ["double", "double", "double"],
            ["double", "unsigned", "int", "float", "int"],  # 8+4+4+4+4 — the D struct
            ["double", "int", "int", "double"],              # 8+4+4+8
            ["int", "int", "int", "int", "int", "int"],
            ["double", "double", "int", "int"],              # 8+8+4+4
        ],
        32: [
            ["double", "double", "double", "double"],
            ["double", "double", "int", "int", "int", "int"],  # 8+8+4+4+4+4
            ["int", "int", "int", "int", "int", "int", "int", "int"],
        ],
        40: [
            ["double", "double", "double", "double", "double"],
            ["double", "double", "double", "int", "int", "int", "int"],  # 8+8+8+4+4+4+4
        ],
        48: [
            ["double", "double", "double", "double", "double", "double"],
            ["double", "double", "double", "double", "int", "int", "int", "int"],
            ["int", "int", "int", "int", "int", "int", "int", "int",
             "int", "int", "int", "int"],
        ],
        # ── Boundary-neighbor sizes (all use __packed__) ────────────────────
        # 15 = 8+4+1+1+1  (double + float + 3 chars)
        15: [["double", "float", "char", "char", "char"]],
        # 17 = 8+8+1       (double + double-as-long + char)
        17: [["double", "double", "char"]],
        # 23 = 8+8+4+1+1+1 (2 doubles + int + 3 chars)
        23: [["double", "double", "int", "char", "char", "char"]],
        # 25 = 8+8+8+1     (3 doubles + char)
        25: [["double", "double", "double", "char"]],
        # 31 = 8*3+4+1+1+1 (3 doubles + int + 3 chars)
        31: [["double", "double", "double", "int", "char", "char", "char"]],
        # 33 = 8*4+1       (4 doubles + char)
        33: [["double", "double", "double", "double", "char"]],
    }

    # Boundary cluster values for each C type
    BOUNDARY = {
        "int":      ["0", "1", "-1", "7", "16", "2147483647", "-2147483648", "255"],
        "unsigned": ["0", "1", "7", "16", "255", "4294967295", "128"],
        "float":    ["0.0f", "1.0f", "-1.0f", "0.5f", "16.0f", "255.0f", "-0.0f"],
        "double":   ["0.0", "1.0", "-1.0", "0.5", "16.0", "255.0", "-0.0"],
        "char":     ["0", "1", "65", "127", "-128", "32"],
    }

    TYPE_FMT = {
        "int":      ("%d",    "(int)"),
        "unsigned": ("%u",    "(unsigned)"),
        "float":    ("%.2f",  "(double)"),
        "double":   ("%.2f",  ""),
        "char":     ("%d",    "(int)"),
    }

    recipe = random.choice(SIZE_RECIPES[target_size])
    # Neighbor sizes: always packed + flat (packed structs can't safely be nested/unioned)
    if is_neighbor:
        shape  = "flat"
        mode   = random.choice(["pass", "return", "pass_return", "chain"])
        packed = True
    else:
        shape  = random.choice(["flat", "nested", "array", "union"])
        mode   = random.choice(["pass", "return", "pass_return", "chain"])
        packed = (random.random() < 0.25)

    # For array/union shapes, override recipe to compatible flat version
    if shape == "array":
        if target_size % 8 == 0:
            elem_t = "double"; n = target_size // 8
        else:
            elem_t = "int"; n = max(1, target_size // 4)
        recipe = [elem_t] * n
    elif shape == "union":
        if target_size >= 8:
            recipe = ["double"] + ["int"] * ((target_size - 8) // 4)

    packed_attr = " __attribute__((packed))" if packed else ""
    kw = "union" if shape == "union" else "struct"

    lines = [
        f"/* ZCC Olympics: ABI Matrix  size={target_size}B shape={shape} mode={mode}"
        + (" packed" if packed else "")
        + (" [neighbor]" if is_neighbor else "") + " */",
        "#include <stdio.h>",
        "#include <string.h>",
    ]

    # Struct / union definition
    if shape == "array":
        elem_t = recipe[0]
        n = len(recipe)
        lines += [f"typedef struct{packed_attr} {{ {elem_t} arr[{n}]; }} S;"]
    elif shape == "nested":
        inner_fields = recipe[:max(1, len(recipe) // 2)]
        outer_rest   = recipe[len(inner_fields):]
        inner_decls  = "\n".join(f"    {t} i{idx};" for idx, t in enumerate(inner_fields))
        outer_decls  = "\n".join(f"    {t} f{idx};" for idx, t in enumerate(outer_rest))
        lines += [
            f"typedef struct{packed_attr} {{",
            inner_decls,
            "} Inner;",
            f"typedef struct{packed_attr} {{",
            "    Inner inner;",
        ]
        if outer_rest:
            lines.append(outer_decls)
        lines.append("} S;")
    else:  # flat or union
        field_decls = "\n".join(f"    {t} f{idx};" for idx, t in enumerate(recipe))
        lines += [f"typedef {kw}{packed_attr} {{", field_decls, "} S;"]

    def bval(t: str) -> str:
        return random.choice(BOUNDARY.get(t, ["1"]))

    def emit_op(op_name: str) -> list:
        body = [f"S {op_name}(S a, S b) {{", "    S r;", "    memset(&r, 0, sizeof(r));"]
        if shape == "array":
            body += [f"    int k;",
                     f"    for (k = 0; k < {n}; k++) r.arr[k] = a.arr[k] + b.arr[k];"]
        elif shape == "nested":
            for idx, t in enumerate(inner_fields):
                op = "+" if t in ("float", "double") else "^"
                body.append(f"    r.inner.i{idx} = a.inner.i{idx} {op} b.inner.i{idx};")
            for idx, t in enumerate(outer_rest):
                op = "+" if t in ("float", "double") else "|"
                body.append(f"    r.f{idx} = a.f{idx} {op} b.f{idx};")
        elif shape == "union":
            body.append("    r.f0 = a.f0 + b.f0;")
        else:
            for idx, t in enumerate(recipe):
                op_choices = ["+", "-"] if t in ("float", "double") else ["+", "-", "^", "|"]
                op = random.choice(op_choices)
                body.append(f"    r.f{idx} = a.f{idx} {op} b.f{idx};")
        body += ["    return r;", "}"]
        return body

    if mode == "chain":
        lines += emit_op("op")
        lines += emit_op("op2")
    else:
        lines += emit_op("op")

    lines.append("int main(void) {")
    for vi in range(2):
        lines.append(f"    S v{vi};")
        lines.append(f"    memset(&v{vi}, 0, sizeof(v{vi}));")
        if shape == "array":
            for k in range(n):
                lines.append(f"    v{vi}.arr[{k}] = {bval(elem_t)};")
        elif shape == "nested":
            for idx, t in enumerate(inner_fields):
                lines.append(f"    v{vi}.inner.i{idx} = {bval(t)};")
            for idx, t in enumerate(outer_rest):
                lines.append(f"    v{vi}.f{idx} = {bval(t)};")
        elif shape == "union":
            lines.append(f"    v{vi}.f0 = {bval('double')};")
        else:
            for idx, t in enumerate(recipe):
                lines.append(f"    v{vi}.f{idx} = {bval(t)};")

    if mode == "chain":
        lines.append("    S tmp = op(v0, v1);")
        lines.append("    S res = op2(tmp, v1);")
    else:
        lines.append("    S res = op(v0, v1);")

    if shape == "array":
        lines.append('    printf("arr:");')
        lines.append(f'    {{ int k; for (k=0; k<{n}; k++) printf(" %.2f", (double)res.arr[k]); }}')
        lines.append('    printf("\\n");')
    elif shape == "nested":
        fmts  = " ".join(TYPE_FMT[t][0] for t in inner_fields)
        casts = ", ".join(f"{TYPE_FMT[t][1]}res.inner.i{idx}"
                         for idx, t in enumerate(inner_fields))
        lines.append(f'    printf("inner: {fmts}\\n", {casts});')
        if outer_rest:
            fmts2  = " ".join(TYPE_FMT[t][0] for t in outer_rest)
            casts2 = ", ".join(f"{TYPE_FMT[t][1]}res.f{idx}"
                              for idx, t in enumerate(outer_rest))
            lines.append(f'    printf("outer: {fmts2}\\n", {casts2});')
    elif shape == "union":
        lines.append('    printf("union: %.2f\\n", res.f0);')
    else:
        for idx, t in enumerate(recipe):
            fmt, cast = TYPE_FMT[t]
            lines.append(f'    printf("{fmt}\\n", {cast}res.f{idx});')

    lines.append(f'    printf("size=%d\\n", (int)sizeof(S));')
    lines += ["    return 0;", "}"]
    return "\n".join(lines), target_size


# ── Varargs ABI Generator ──────────────────────────────────────────────────────
def generate_varargs_abi(seed: int) -> str:
    """Varargs ABI generator.

    Tests va_list / va_start / va_arg / va_end across three patterns:
      all_double  — n doubles summed
      all_int     — n ints summed
      mixed       — n (int, double) pairs, computes sum(i * d)

    SysV AMD64 varargs pitfalls exercised:
      • floats promoted to double in varargs
      • int args occupy integer register save area (%rdi/%rsi/...)
      • double args occupy SSE save area (%xmm0/...)
      • mixed pattern forces correct interleaving of gp_offset/fp_offset
      • n > 6 ints or n > 8 doubles forces stack-overflow args

    Values use boundary clusters.
    """
    random.seed(seed)

    # How many args per function
    n_d = random.randint(2, 9)   # double args: >8 forces stack
    n_i = random.randint(2, 9)   # int args:    >6 forces stack
    n_m = random.randint(2, 5)   # (int,double) pairs for mixed

    # Boundary cluster values
    dvals = ["1.5", "2.5", "0.5", "16.0", "255.0", "-1.0", "0.0", "7.5", "32.0"]
    ivals = ["1", "7", "16", "255", "-1", "2147483647", "128", "32", "0"]

    # Pick seed-driven values
    d_args = [random.choice(dvals) for _ in range(n_d)]
    i_args = [random.choice(ivals) for _ in range(n_i)]
    m_iargs = [random.choice(ivals[:-2]) for _ in range(n_m)]  # avoid 0/neg for product
    m_dargs = [random.choice(dvals[:-2]) for _ in range(n_m)]

    # Expected outputs (computed in Python for the comment)
    sum_d = sum(float(v) for v in d_args)
    sum_i = sum(int(v) for v in i_args)
    sum_m = sum(abs(int(i)) * float(d) for i, d in zip(m_iargs, m_dargs))

    d_arg_str = ", ".join(d_args)
    i_arg_str = ", ".join(i_args)
    m_arg_str = ", ".join(f"{i}, {d}" for i, d in zip(m_iargs, m_dargs))

    lines = [
        f"/* ZCC Olympics: Varargs ABI  nd={n_d} ni={n_i} nm={n_m} */",
        f"/* Expected: sum_d={sum_d:.2f}  sum_i={sum_i}  sum_m={sum_m:.2f} */",
        "#include <stdio.h>",
        "#include <stdarg.h>",
        "",
        "/* All-double sum */",
        "double sum_doubles(int n, ...) {",
        "    va_list ap;",
        "    va_start(ap, n);",
        "    double acc = 0.0;",
        "    int k;",
        "    for (k = 0; k < n; k++) acc += va_arg(ap, double);",
        "    va_end(ap);",
        "    return acc;",
        "}",
        "",
        "/* All-int sum */",
        "int sum_ints(int n, ...) {",
        "    va_list ap;",
        "    va_start(ap, n);",
        "    int acc = 0;",
        "    int k;",
        "    for (k = 0; k < n; k++) acc += va_arg(ap, int);",
        "    va_end(ap);",
        "    return acc;",
        "}",
        "",
        "/* Mixed: n (int, double) pairs, returns sum(abs(i) * d) */",
        "double sum_mixed(int n, ...) {",
        "    va_list ap;",
        "    va_start(ap, n);",
        "    double acc = 0.0;",
        "    int k;",
        "    for (k = 0; k < n; k++) {",
        "        int    i = va_arg(ap, int);",
        "        double d = va_arg(ap, double);",
        "        if (i < 0) i = -i;",
        "        acc += i * d;",
        "    }",
        "    va_end(ap);",
        "    return acc;",
        "}",
        "",
        "int main(void) {",
        f"    printf(\"sum_d: %.2f\\n\",  sum_doubles({n_d}, {d_arg_str}));",
        f"    printf(\"sum_i: %d\\n\",    sum_ints({n_i}, {i_arg_str}));",
        f"    printf(\"sum_m: %.2f\\n\",  sum_mixed({n_m}, {m_arg_str}));",
        "    return 0;",
        "}",
    ]
    return "\n".join(lines)



def fuzz_one(seed: int, category: str, enable_creduce: bool, creduce_timeout: int) -> FuzzResult:
    t0 = time.time()
    
    # Sanitizer flags for reference compilers (Address & UB Sanitizers)
    san_flags = ["-fsanitize=address,undefined", "-fno-sanitize-recover=all"]
    
    # 6 Config Matrix configurations
    configs = {
        "zcc_o0":     {"bin": ZCC_BIN, "args": [f"-I{SYS_INC}"]},
        "zcc_o2":     {"bin": ZCC_BIN, "args": [f"-I{SYS_INC}"]}, 
        "zcc_nofold": {"bin": ZCC_BIN, "args": [f"-I{SYS_INC}", "--no-fold"]},
        "gcc_o0":     {"bin": GCC_BIN, "args": ["-w", "-O0"] + san_flags},
        "gcc_o2":     {"bin": GCC_BIN, "args": ["-w", "-O2"] + san_flags},
        "clang_o2":   {"bin": CLANG_BIN, "args": ["-w", "-O2"] + san_flags}
    }
    
    # Generate test code according to category
    # mutate_cats is populated in main() from coverage scheduler and passed via a module-level dict
    mutate = getattr(fuzz_one, "_mutate_cats", set())
    if category == "const_fold":
        src_code = generate_const_fold(seed)
    elif category == "reg_pressure":
        src_code = generate_reg_pressure(seed, mutate=(category in mutate))
    elif category == "abi_chaos":
        src_code = generate_abi_chaos(seed)
    elif category == "abi_struct_val":
        src_code = generate_abi_struct_val(seed, mutate=(category in mutate))
    elif category == "abi_matrix":
        src_code, _abi_matrix_size = generate_abi_matrix(seed)
        # Store target_size for gate coverage tracking via result metadata
        fuzz_one._abi_matrix_sizes = getattr(fuzz_one, "_abi_matrix_sizes", {})
        fuzz_one._abi_matrix_sizes[seed] = _abi_matrix_size
    elif category == "varargs_abi":
        src_code = generate_varargs_abi(seed)
    elif category == "fp_torture":
        src_code = generate_fp_torture(seed)
    elif category == "stack_stress":
        src_code = generate_stack_stress(seed, mutate=(category in mutate))
    elif category == "huge_init":
        src_code = generate_huge_init(seed)
    else:
        category = "csmith"
        src_code = ""

    config_results = {}
    
    with tempfile.TemporaryDirectory(prefix=f"olympics_{category}_{seed}_") as tmpdir:
        src = os.path.join(tmpdir, "test.c")
        
        if category == "csmith":
            # Generate Csmith program
            gen_rc, gen_out, gen_err = run(
                ["sh", "-c",
                 f"{CSMITH_BIN} --seed {seed} " + " ".join(CSMITH_FLAGS) + f" > {src}"],
                timeout=10,
                cwd=tmpdir,
            )
            if gen_rc != 0 or not os.path.exists(src) or os.path.getsize(src) == 0:
                return FuzzResult(seed, category, "crash_gcc", {}, None, int((time.time()-t0)*1000))
            # Persist source for Phase 3 coverage pass (Csmith is non-deterministic across seeds)
            src_save = OLYMPICS_DIR / f"olympics_src_{category}_{seed}.c"
            OLYMPICS_DIR.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, src_save)
        else:
            with open(src, "w") as f:
                f.write(src_code)
            # Persist source for Phase 3 coverage pass
            src_save = OLYMPICS_DIR / f"olympics_src_{category}_{seed}.c"
            OLYMPICS_DIR.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, src_save)
                
        # 1. Compile reference targets to establish output correctness
        gcc_ok_b = os.path.join(tmpdir, "gcc_ok")
        gcc_rc, _, _ = run([GCC_BIN, "-w"] + san_flags + [f"-I{CSMITH_INCLUDE}", src, "-o", gcc_ok_b, "-lm"], COMPILE_TIMEOUT, cwd=tmpdir)
        if gcc_rc != 0:
            return FuzzResult(seed, category, "crash_gcc", {}, None, int((time.time()-t0)*1000))
            
        gcc_xrc, gcc_out, gcc_err = run([gcc_ok_b], EXEC_TIMEOUT, cwd=tmpdir)
        if gcc_xrc != 0 or "ASan:" in gcc_err or "UndefinedBehaviorSanitizer:" in gcc_err:
            # Skip programs triggering ASan/UBSan issues (reject UB)
            return FuzzResult(seed, category, "crash_gcc", {}, None, int((time.time()-t0)*1000))
            
        golden_out = gcc_out.strip()
        
        # Verify Clang to catch reference oracle disagreements (also instrumented with ASan/UBSan)
        clang_ok_b = os.path.join(tmpdir, "clang_ok")
        clang_rc, _, _ = run([CLANG_BIN, "-w"] + san_flags + [f"-I{CSMITH_INCLUDE}", src, "-o", clang_ok_b, "-lm"], COMPILE_TIMEOUT, cwd=tmpdir)
        if clang_rc == 0:
            clang_xrc, clang_out, clang_err = run([clang_ok_b], EXEC_TIMEOUT, cwd=tmpdir)
            if clang_xrc == 0 and "ASan:" not in clang_err and "UndefinedBehaviorSanitizer:" not in clang_err:
                clang_norm = clang_out.strip()
                if category in ("fp_torture", "const_fold"):
                    ref_match = float_outputs_match(golden_out, clang_norm, tol=1e-5)
                else:
                    ref_match = (golden_out == clang_norm)
                if not ref_match:
                    return FuzzResult(seed, category, "ref_disagreement", {}, None, int((time.time()-t0)*1000))
            else:
                return FuzzResult(seed, category, "crash_gcc", {}, None, int((time.time()-t0)*1000))
        
        # 2. Compile and run each configuration in the matrix
        status = "pass"
        
        for name, config in configs.items():
            out_bin = os.path.join(tmpdir, f"bin_{name}")
            
            if "zcc" in name:
                # 2-step ZCC compilation (ZCC frontend -> GCC link)
                # First run preprocessor to ensure includes are fully expanded
                pp_src = os.path.join(tmpdir, f"pp_{name}.c")
                pp_rc, pp_out, pp_err = run([ZCC_BIN, f"-I{SYS_INC}", f"-I{CSMITH_INCLUDE}", "--pp-only", src], COMPILE_TIMEOUT, cwd=tmpdir)
                if pp_rc != 0:
                    status = "crash_zcc_pp"
                    break
                with open(pp_src, "w") as f:
                    f.write(pp_out)
                    
                zcc_s = os.path.join(tmpdir, f"s_{name}.s")
                # Compile to assembly
                compile_args = [ZCC_BIN] + config["args"] + [pp_src, "-o", zcc_s]
                rc, _, err = run(compile_args, COMPILE_TIMEOUT, cwd=tmpdir)
                if rc == 0:
                    # Link assembly using GCC
                    link_rc, _, err = run([GCC_BIN, "-w", zcc_s, "-o", out_bin, "-lm"], COMPILE_TIMEOUT, cwd=tmpdir)
                    if link_rc != 0:
                        rc = link_rc
                if rc != 0:
                    status = "crash_zcc_cg"
                    config_results[name] = f"crash_cg ({err[:64].strip()})"
                    break
            else:
                # GCC / Clang reference targets
                rc, _, err = run([config["bin"]] + config["args"] + [f"-I{CSMITH_INCLUDE}", src, "-o", out_bin, "-lm"], COMPILE_TIMEOUT, cwd=tmpdir)
                if rc != 0:
                    status = "crash_gcc"
                    break
                    
            # Run program
            xrc, xout, _ = run([out_bin], EXEC_TIMEOUT, cwd=tmpdir)
            norm_out = xout.strip()
            
            if xrc != 0:
                config_results[name] = f"crash_exit_{xrc}"
                if "zcc" in name:
                    status = "mismatch_stdout"
            else:
                config_results[name] = norm_out
                matched = False
                if category in ("fp_torture", "const_fold"):
                    matched = float_outputs_match(norm_out, golden_out, tol=1e-5)
                else:
                    matched = (norm_out == golden_out)
                if not matched:
                    status = "mismatch_stdout"
                    
        # Save every executed case metadata
        meta = {
            "seed": seed,
            "category": category,
            "expected_hash": get_hash(golden_out),
            "zcc_o0_hash": get_hash(config_results.get("zcc_o0", "")),
            "zcc_o2_hash": get_hash(config_results.get("zcc_o2", "")),
            "gcc_o2_hash": get_hash(config_results.get("gcc_o2", ""))
        }
        meta_path = OLYMPICS_DIR / f"metadata_{category}_{seed}.json"
        with open(meta_path, "w") as f:
            json.dump(meta, f, indent=2)
                    
        if status != "pass" and status != "crash_gcc":
            # Save divergence test case
            reduced_path = _save_divergence(seed, category, src, status, config_results, golden_out, enable_creduce, creduce_timeout)
            # CG-ABI-STRUCT-002 rule: abi_matrix failures auto-land in tests/regressions/
            if category == "abi_matrix":
                _save_abi_matrix_regression(seed, src, status, config_results, golden_out)
            return FuzzResult(seed, category, status, config_results, reduced_path, int((time.time()-t0)*1000))
            
        return FuzzResult(seed, category, status, config_results, None, int((time.time()-t0)*1000))

def _save_abi_matrix_regression(seed: int, src_path: str, status: str,
                                 config_results: dict, golden: str) -> None:
    """Auto-save abi_matrix failures to tests/regressions/ as CG_ABI_<id>.{c,md}."""
    import hashlib, shutil
    reg_dir = Path(__file__).parent / "tests" / "regressions"
    reg_dir.mkdir(parents=True, exist_ok=True)
    bug_id = f"ABI_{seed:04d}"
    c_dest  = reg_dir / f"CG_{bug_id}.c"
    md_dest = reg_dir / f"CG_{bug_id}.md"
    shutil.copy2(src_path, c_dest)
    zcc_out = config_results.get("zcc_o0", "?")
    gcc_out = golden or config_results.get("gcc_o0", "?")
    md = f"""# CG-{bug_id}
## ABI Matrix Regression — Auto-Generated

| Field | Value |
|-------|-------|
| **Bug ID** | CG-{bug_id} |
| **Seed** | {seed} |
| **Status** | {status} |
| **Category** | abi_matrix |
| **Source** | `tests/regressions/CG_{bug_id}.c` |

## Output Divergence

```
GCC:  {gcc_out.strip()[:200]}
ZCC:  {str(zcc_out).strip()[:200]}
```

## Reproduction

```bash
./zcc -I./zcc_sys_includes tests/regressions/CG_{bug_id}.c -o /tmp/t.s
gcc /tmp/t.s -o /tmp/t && /tmp/t
gcc -O0 tests/regressions/CG_{bug_id}.c -o /tmp/ref && /tmp/ref
```

## Status

- [ ] Root cause identified
- [ ] Fix applied in `part4.c`
- [ ] Selfhost verified
- [ ] Gate passing
"""
    md_dest.write_text(md)
    print(f"[regression] Saved: {c_dest.name}  {md_dest.name}")


def _save_divergence(seed: int, category: str, src_path: str, status: str, config_results: Dict[str, str], golden: str, enable_creduce: bool, creduce_timeout: int) -> str:
    OLYMPICS_DIR.mkdir(parents=True, exist_ok=True)
    dest = OLYMPICS_DIR / f"olympics_divergence_{category}_{seed}.c"
    import shutil
    shutil.copy2(src_path, dest)
    
    # Save a diagnostic metadata log
    log_dest = OLYMPICS_DIR / f"olympics_divergence_{category}_{seed}.log"
    with open(log_dest, "w") as f:
        f.write(f"Category: {category}\nSeed: {seed}\nStatus: {status}\n")
        f.write(f"Golden standard: {repr(golden)}\n")
        f.write("Configuration Results:\n")
        for k, v in config_results.items():
            f.write(f"  {k}: {repr(v)}\n")
            
    if enable_creduce:
        # Spawn creduce interest script
        interest = OLYMPICS_DIR / f"olympics_interest_{category}_{seed}.sh"
        _write_interest_script(interest, seed, category, status, golden)
        # Non-blocking creduce execution
        _run_creduce_nonblocking(dest, interest, seed, category, creduce_timeout)
        
    return str(dest)

def _write_interest_script(path: Path, seed: int, category: str, status: str, golden: str):
    if status == "crash_zcc_pp":
        script = f"""#!/bin/bash
set -e
FILE="$1"
{GCC_BIN} -w "$FILE" -o /tmp/gcc_int_{seed} -lm 2>/dev/null || exit 1
{ZCC_BIN} -I{SYS_INC} -I{CSMITH_INCLUDE} --pp-only "$FILE" >/dev/null 2>/dev/null
RET=$?
[ $RET -ne 0 ] && exit 0
exit 1
"""
    elif status == "crash_zcc_cg":
        script = f"""#!/bin/bash
set -e
FILE="$1"
{GCC_BIN} -w "$FILE" -o /tmp/gcc_int_{seed} -lm 2>/dev/null || exit 1
{ZCC_BIN} -I{SYS_INC} -I{CSMITH_INCLUDE} --pp-only "$FILE" > /tmp/pp_int_{seed}.c 2>/dev/null || exit 1
{ZCC_BIN} -I{SYS_INC} /tmp/pp_int_{seed}.c -o /tmp/zcc_int_{seed}.s 2>/dev/null
RET=$?
[ $RET -ne 0 ] && exit 0
exit 1
"""
    else:
        # mismatch
        script = f"""#!/bin/bash
set -e
FILE="$1"
{GCC_BIN} -w "$FILE" -o /tmp/gcc_int_{seed} -lm 2>/dev/null || exit 1
/tmp/gcc_int_{seed} > /tmp/gcc_out_{seed} 2>/dev/null || exit 1
{ZCC_BIN} -I{SYS_INC} -I{CSMITH_INCLUDE} --pp-only "$FILE" > /tmp/pp_int_{seed}.c 2>/dev/null || exit 1
{ZCC_BIN} -I{SYS_INC} /tmp/pp_int_{seed}.c -o /tmp/zcc_int_{seed}.s 2>/dev/null || exit 1
{GCC_BIN} -w /tmp/zcc_int_{seed}.s -o /tmp/zcc_int_{seed} -lm 2>/dev/null || exit 1
/tmp/zcc_int_{seed} > /tmp/zcc_out_{seed} 2>/dev/null || exit 1
if ! diff -q /tmp/gcc_out_{seed} /tmp/zcc_out_{seed} >/dev/null; then
    exit 0
fi
exit 1
"""
    path.write_text(script)
    path.chmod(0o755)

def _run_creduce_nonblocking(src: Path, interest: Path, seed: int, category: str, creduce_timeout: int):
    REGRESSIONS_DIR.mkdir(parents=True, exist_ok=True)
    reduced = REGRESSIONS_DIR / f"reduced_{category}_{seed}.c"
    def do_reduce():
        with tempfile.TemporaryDirectory(prefix=f"creduce_olympics_{seed}_") as tmpdir:
            import shutil
            work = os.path.join(tmpdir, "test.c")
            shutil.copy2(src, work)
            rc, _, _ = run(
                ["nice", "-n", "19", CREDUCE_BIN, "--n", "1", str(interest), work],
                timeout=creduce_timeout,
                cwd=tmpdir,
            )
            if rc == 0 and os.path.exists(work):
                shutil.copy2(work, reduced)
    
    import threading
    t = threading.Thread(target=do_reduce, daemon=True)
    t.start()

# ── Main Olympics Dashboard ───────────────────────────────────────────────────
def main():
    parser = argparse.ArgumentParser(description="ZCC Compiler Olympics Suite")
    parser.add_argument("--seeds", type=int, default=15,
                        help="Number of seeds to run per custom category (default: 15)")
    parser.add_argument("--csmith-seeds", type=int, default=30,
                        help="Number of seeds to run for Csmith category (default: 30)")
    parser.add_argument("--workers", type=int, default=4,
                        help="Parallel workers (default: 4)")
    parser.add_argument("--no-creduce", action="store_true",
                        help="Disable creduce failure minimization")
    parser.add_argument("--creduce-timeout", type=int, default=120,
                        help="Creduce timeout in seconds")
    # Phase 3: coverage flags
    parser.add_argument("--coverage", action="store_true",
                        help="Phase 3: build zcc_cov (gcov-instrumented ZCC) and collect per-test coverage")
    parser.add_argument("--corpus", action="store_true",
                        help="Phase 3: persist test sources that find new coverage into olympics_corpus/")
    parser.add_argument("--cov-build-timeout", type=int, default=600,
                        help="Phase 3: timeout in seconds for the gcov-instrumented zcc_cov build (default: 600)")
    parser.add_argument("--coverage-limit", type=int, default=0,
                        help="Phase 3: max number of passing tests to run through the coverage pass (0 = unlimited)")
    parser.add_argument("--mutate-cold-generators", action="store_true",
                        help="Phase 3: enable diversity knobs for categories scoring below median lines/seed")
    args = parser.parse_args()

    OLYMPICS_DIR.mkdir(parents=True, exist_ok=True)
    REGRESSIONS_DIR.mkdir(parents=True, exist_ok=True)
    if args.coverage:
        COVERAGE_DIR.mkdir(parents=True, exist_ok=True)
    if args.corpus:
        CORPUS_DIR.mkdir(parents=True, exist_ok=True)
    
    categories = ["csmith", "const_fold", "reg_pressure", "abi_chaos", "abi_struct_val",
                  "abi_matrix", "varargs_abi", "fp_torture", "stack_stress", "huge_init"]

    # ABI matrix size buckets — 6 standard + 6 boundary-neighbors
    # --seeds 6  covers standard boundaries; --seeds 12 covers all 12
    ABI_MATRIX_BUCKETS = [8, 16, 24, 32, 40, 48, 15, 17, 23, 25, 31, 33]

    # CG-ABI-STRUCT-002: abi_struct_val is permanently mutated.
    # Coverage-guided fuzzing proved that 24B mixed-field struct diversity
    # is required to surface ABI correctness bugs. Never let this go cold.
    ALWAYS_MUTATE: set = {"abi_struct_val"}

    # Bootstrap --mutate-cold-generators from last coverage run history
    fuzz_one._mutate_cats = set()  # type: ignore[attr-defined]
    if args.mutate_cold_generators and COVERAGE_LOG.exists():
        try:
            lines_hist = COVERAGE_LOG.read_text().strip().splitlines()
            if lines_hist:
                last_run = json.loads(lines_hist[-1])
                by_cat = last_run.get("by_category", {})
                hist_scores = {
                    cat: by_cat[cat]["new_lines"] / max(1, by_cat[cat]["seeds"])
                    for cat in categories if cat in by_cat
                }
                if hist_scores:
                    hist_median = sorted(hist_scores.values())[len(hist_scores) // 2]
                    cold_thresh = hist_median * 0.25
                    cold_from_history = {c for c, s in hist_scores.items() if s < cold_thresh}
                    fuzz_one._mutate_cats = cold_from_history  # type: ignore[attr-defined]
                    if cold_from_history:
                        print(f"[mutate] Cold categories from last run (score < {cold_thresh:.0f}): {sorted(cold_from_history)}")
                        print(f"[mutate] These generators will use diversity knobs this run.")
        except (json.JSONDecodeError, KeyError, IndexError) as e:
            print(f"[mutate] WARNING: could not parse coverage history: {e}")

     # Unconditionally merge ALWAYS_MUTATE (CG-ABI-STRUCT-002 permanent fixture)
    fuzz_one._mutate_cats = fuzz_one._mutate_cats | ALWAYS_MUTATE  # type: ignore[attr-defined]
    print(f"[mutate] Permanent mutate categories (nightly fixture): {sorted(ALWAYS_MUTATE)}")

    # Print deterministic ABI bucket schedule so failures are immediately explainable
    if "abi_matrix" in categories and args.seeds >= 1:
        _sched = [(s, ABI_MATRIX_BUCKETS[(s - 1) % len(ABI_MATRIX_BUCKETS)])
                  for s in range(1, args.seeds + 1)]
        _std_count = min(args.seeds, 6)
        _full_count = min(args.seeds, 12)
        print(f"[abi_matrix] Bucket schedule ({args.seeds} seeds):")
        for seed_num, bucket in _sched:
            tag = "[neighbor]" if bucket in (15, 17, 23, 25, 31, 33) else ""
            print(f"    seed {seed_num:2d} → {bucket:2d}B {tag}")
        if args.seeds >= 12:
            print(f"[abi_matrix] Gate: all 12 buckets REQUIRED (hard-fail if missing)")
        elif args.seeds >= 6:
            print(f"[abi_matrix] Gate: 6 standard buckets REQUIRED; neighbors optional")
        else:
            print(f"[abi_matrix] Gate: warn-only (seeds < 6)")

    # Construct task list

    tasks = []
    category_counts = {cat: 0 for cat in categories}
    for cat in categories:
        count = args.csmith_seeds if cat == "csmith" else args.seeds
        for i in range(1, count + 1):
            tasks.append((1000 + i, cat))
            
    total_tasks = len(tasks)
    
    metrics = {
        "generated": total_tasks,
        "rejected": 0,
        "compiled": 0,
        "executed": 0,
        "crash_pp": 0,
        "crash_cg": 0,
        "mismatch": 0,
        "crash_gcc": 0,
        "ref_disagreements": 0,
        "timeouts": 0,
        "avg_compile_ms": 0.0,
        "max_float_rel_error": 0.0,   # Phase 3: renamed from max_ulp_error (was rel-error proxy)
        "max_ulp_distance": 0           # Phase 3: true IEEE-754 ULP distance
    }
    
    passed_tasks = []
    failures = []
    
    print(f"🔱 ZCC Compiler Olympics — Parallel differential test suite running ({total_tasks} tasks)...", flush=True)
    
    t_start = time.time()
    compile_times = []
    
    enable_creduce = not args.no_creduce
    
    with ProcessPoolExecutor(max_workers=args.workers) as pool:
        futures = {pool.submit(fuzz_one, seed, cat, enable_creduce, args.creduce_timeout): (seed, cat) for seed, cat in tasks}
        
        done = 0
        for fut in as_completed(futures):
            done += 1
            seed, cat = futures[fut]
            result = fut.result()
            
            if result.status == "crash_gcc":
                metrics["crash_gcc"] += 1
                metrics["rejected"] += 1
            elif result.status == "ref_disagreement":
                metrics["ref_disagreements"] += 1
                metrics["rejected"] += 1
            else:
                metrics["compiled"] += 1
                metrics["avg_compile_ms"] += result.elapsed_ms
                compile_times.append(result.elapsed_ms)
                category_counts[cat] += 1
                
                # Check maximum relative error and true ULP distance
                zcc_out = result.config_matrix.get("zcc_o2", "")
                gcc_out = result.config_matrix.get("gcc_o2", "")
                if "crash" not in zcc_out and "crash" not in gcc_out:
                    rel_err = get_max_float_rel_error(zcc_out, gcc_out)
                    ulp_d   = get_max_ulp_distance(zcc_out, gcc_out)
                    metrics["max_float_rel_error"] = max(metrics["max_float_rel_error"], rel_err)
                    metrics["max_ulp_distance"]    = max(metrics["max_ulp_distance"], ulp_d)
                
                if result.status == "pass":
                    metrics["executed"] += 1
                    passed_tasks.append(result)
                elif result.status == "crash_zcc_pp":
                    metrics["crash_pp"] += 1
                    failures.append(result)
                elif result.status == "crash_zcc_cg":
                    metrics["crash_cg"] += 1
                    failures.append(result)
                elif result.status == "mismatch_stdout":
                    metrics["mismatch"] += 1
                    failures.append(result)
                elif result.status == "timeout":
                    metrics["timeouts"] += 1
                    failures.append(result)
                    
            if done % 10 == 0 or done == total_tasks:
                elapsed = time.time() - t_start
                rate = done / elapsed
                print(f"  [{done}/{total_tasks}] {rate:.1f} tests/s | pass={metrics['executed']} reject={metrics['rejected']} fail={metrics['crash_pp'] + metrics['crash_cg'] + metrics['mismatch']}", flush=True)

    elapsed_time = time.time() - t_start
    avg_compile = sum(compile_times) / len(compile_times) if compile_times else 0.0
    
    # Write aggregated metrics log
    if DIVERGENCE_LOG.exists():
        DIVERGENCE_LOG.unlink()
    for fail in failures:
        line = json.dumps(asdict(fail)) + "\n"
        with open(DIVERGENCE_LOG, "a") as f:
            f.write(line)
            
    # Calculate Score
    success_rate = (metrics["executed"] / metrics["compiled"]) * 100.0 if metrics["compiled"] > 0 else 0.0
    if success_rate >= 100.0:
        award = "🏆 GOLD"
    elif success_rate >= 98.0:
        award = "🥈 SILVER"
    elif success_rate >= 95.0:
        award = "🥉 BRONZE"
    else:
        award = "❌ FAILED"
        
    print("\n" + "="*50)
    print("              ZCC COMPILER OLYMPICS")
    print("="*50)
    print(f"Programs Generated......... {metrics['generated']}")
    print(f"Programs Rejected.......... {metrics['rejected']} (UB / SIGFPE skipped)")
    print(f"Programs Compiled.......... {metrics['compiled']}")
    print(f"Programs Executed.......... {metrics['executed']}")
    print(f"Preprocessor Crashes....... {metrics['crash_pp']}")
    print(f"Codegen Compiler Crashes... {metrics['crash_cg']}")
    print(f"Wrong-code Mismatches...... {metrics['mismatch']}")
    print(f"Reference Disagreements.... {metrics['ref_disagreements']}")
    print(f"Timeouts................... {metrics['timeouts']}")
    print(f"Avg Compile Profile........ {avg_compile:.1f} ms")
    print(f"Max Float Rel Error........ {metrics['max_float_rel_error']:.5e}   (relative)")
    print(f"Max Float ULP Distance..... {metrics['max_ulp_distance']}            (true IEEE-754 ULP)")
    print(f"Total Olympics Time........ {elapsed_time:.1f} s")
    print("-"*50)
    print(f"Pass / Success Rate........ {success_rate:.1f}%")
    print(f"Overall Olympics Score..... {award}")
    print("="*50)
    
    # Gates check
    zcc_crashes = metrics["crash_pp"] + metrics["crash_cg"]
    wrong_code = metrics["mismatch"]
    ref_disagreements = metrics["ref_disagreements"]
    timeout_rate = (metrics["timeouts"] / total_tasks) * 100.0 if total_tasks > 0 else 0.0
    
    gate_failed = False
    print("\n🔱 Checking Olympics Gates:")
    if wrong_code == 0:
        print("  [PASS] wrong_code == 0")
    else:
        print(f"  [FAIL] wrong_code == {wrong_code} (expected 0)")
        gate_failed = True
        
    if zcc_crashes == 0:
        print("  [PASS] zcc_crashes == 0")
    else:
        print(f"  [FAIL] zcc_crashes == {zcc_crashes} (expected 0)")
        gate_failed = True
        
    if ref_disagreements == 0:
        print("  [PASS] reference_oracle_disagreements == 0")
    else:
        print(f"  [FAIL] reference_oracle_disagreements == {ref_disagreements} (expected 0)")
        gate_failed = True
        
    if timeout_rate < 5.0:
        print(f"  [PASS] timeout_rate < 5% ({timeout_rate:.1f}%)")
    else:
        print(f"  [FAIL] timeout_rate >= 5% ({timeout_rate:.1f}%)")
        gate_failed = True
        
    all_categories_ok = True
    for cat in categories:
        if cat != "csmith" and category_counts[cat] < 1:
            all_categories_ok = False
            print(f"  [FAIL] custom category {cat} executed < 1 times ({category_counts[cat]} times)")
    if all_categories_ok:
        print("  [PASS] all custom categories executed >= 1")
    else:
        gate_failed = True

    # ABI matrix size-bucket gate.
    # Severity scales with seed count because the deterministic schedule guarantees:
    #   seeds >= 6  → all 6 standard buckets reachable  (fail if missing)
    #   seeds >= 12 → all 12 buckets reachable           (fail if missing)
    #   seeds  < 6  → warn only
    # Recompute in the main process (worker-side state doesn't cross process boundary).
    abi_sizes_hit: set = set()
    for r in passed_tasks + failures:
        if r.category == "abi_matrix":
            try:
                _, sz = generate_abi_matrix(r.seed)
                abi_sizes_hit.add(sz)
            except Exception:
                pass

    STD_BUCKETS   = [8, 16, 24, 32, 40, 48]
    ALL_BUCKETS_12 = ABI_MATRIX_BUCKETS  # [8,16,24,32,40,48,15,17,23,25,31,33]

    if args.seeds >= 12:
        required_buckets = ALL_BUCKETS_12
        bucket_label     = "all 12 (std + neighbors)"
    elif args.seeds >= 6:
        required_buckets = STD_BUCKETS
        bucket_label     = "6 standard"
    else:
        required_buckets = []
        bucket_label     = "none (seeds < 6)"

    abi_buckets_missing = [b for b in required_buckets if b not in abi_sizes_hit]

    if not required_buckets:
        print(f"  [WARN] abi_matrix bucket gate skipped (seeds={args.seeds} < 6); "
              f"covered so far: {sorted(abi_sizes_hit)}")
    elif not abi_buckets_missing:
        print(f"  [PASS] abi_matrix {bucket_label} buckets covered: {sorted(abi_sizes_hit)}")
    else:
        print(f"  [FAIL] abi_matrix missing {bucket_label} buckets: {abi_buckets_missing} "
              f"(covered: {sorted(abi_sizes_hit)})")
        gate_failed = True



    if gate_failed:
        print("\n❌ GATES CHECK FAILED. Exiting with code 1.")
        sys.exit(1)

    # ── Phase 3: Coverage-Guided Pass ────────────────────────────────────────
    if args.coverage:
        print("\n" + "="*60)
        print("       PHASE 3 — COVERAGE-GUIDED OLYMPICS")
        print("="*60)

        # 1. Build the gcov-instrumented compiler
        cov_build_ok = build_zcc_cov(build_timeout=args.cov_build_timeout)
        if not cov_build_ok:
            print("[cov] WARNING: zcc_cov build failed. Coverage pass skipped.")
        else:
            # 2. Sequential coverage pass (parallel gcov counters are not safe)
            seen_globally: Set[Tuple[str, int]] = set()
            category_new_cov: Dict[str, int] = {cat: 0 for cat in categories}
            category_total_seeds: Dict[str, int] = {cat: 0 for cat in categories}
            total_new_cov  = 0
            corpus_saved   = 0
            cov_run_total  = 0
            cov_run_ok     = 0

            # Keep track of per-category covered/total for the table
            cat_covered: Dict[str, int] = {cat: 0 for cat in categories}
            cat_total:   Dict[str, int] = {cat: 0 for cat in categories}

            cov_limit = args.coverage_limit if args.coverage_limit > 0 else len(passed_tasks)
            cov_tasks = passed_tasks[:cov_limit]
            print(f"\n[cov] Running coverage pass on {len(cov_tasks)}/{len(passed_tasks)} passing test cases (sequential)...")
            if cov_limit < len(passed_tasks):
                print(f"[cov] (limited to first {cov_limit} by --coverage-limit)")

            for result in cov_tasks:
                # Use the source file persisted during fuzz_one()
                # (all categories including csmith are saved there now)
                src_file = OLYMPICS_DIR / f"olympics_src_{result.category}_{result.seed}.c"

                if not src_file.exists():
                    # Last-resort: regenerate from seed for non-csmith categories
                    cat = result.category
                    if cat == "const_fold":
                        code = generate_const_fold(result.seed)
                    elif cat == "reg_pressure":
                        code = generate_reg_pressure(result.seed)
                    elif cat == "abi_chaos":
                        code = generate_abi_chaos(result.seed)
                    elif cat == "abi_struct_val":
                        code = generate_abi_struct_val(result.seed)
                    elif cat == "fp_torture":
                        code = generate_fp_torture(result.seed)
                    elif cat == "stack_stress":
                        code = generate_stack_stress(result.seed)
                    elif cat == "huge_init":
                        code = generate_huge_init(result.seed)
                    else:
                        # csmith without on-disk copy — skip (can't regenerate)
                        print(f"[cov] WARN: no saved source for csmith seed {result.seed}, skipping")
                        continue
                    src_file.write_text(code)

                if not src_file.exists():
                    continue

                # Reset gcda counters, run zcc_cov on this single source
                reset_gcov_counters()
                ok = run_zcc_cov_single(str(src_file))
                cov_run_total += 1
                if not ok:
                    continue
                cov_run_ok += 1

                # Harvest gcov and compute delta
                covered, total, gcov_files = harvest_gcov()
                new_lines, seen_globally = compute_coverage_delta(seen_globally, gcov_files)

                category_new_cov[result.category]   += new_lines
                category_total_seeds[result.category] += 1
                cat_covered[result.category]          += covered
                cat_total[result.category]            += total
                total_new_cov                         += new_lines

                if new_lines > 0 and args.corpus:
                    dest = CORPUS_DIR / f"{result.category}_{result.seed}.c"
                    shutil.copy2(str(src_file), dest)
                    corpus_saved += 1

            # 3. Category coverage table
            print("\n╔" + "═"*70 + "╗")
            print("║  COVERAGE BY CATEGORY                                                 ║")
            print("╠" + "═"*20 + "╦" + "═"*10 + "╦" + "═"*12 + "╦" + "═"*12 + "╦" + "═"*12 + "╣")
            print(f"║ {'Category':<18} ║ {'Seeds':>8} ║ {'New Lines':>10} ║ {'Avg Cov%':>10} ║ {'GMV':>10} ║")
            print("╠" + "═"*20 + "╬" + "═"*10 + "╬" + "═"*12 + "╬" + "═"*12 + "╬" + "═"*12 + "╣")
            for cat in categories:
                seeds_n = category_total_seeds[cat]
                new_n   = category_new_cov[cat]
                gmv     = new_n / max(1, seeds_n)
                avg_pct = 0.0
                if cat_total[cat] > 0:
                    avg_pct = cat_covered[cat] / cat_total[cat] * 100.0
                print(f"║ {cat:<18} ║ {seeds_n:>8} ║ {new_n:>10} ║ {avg_pct:>9.1f}% ║ {gmv:>9.1f}  ║")
            print("╚" + "═"*20 + "╩" + "═"*10 + "╩" + "═"*12 + "╩" + "═"*12 + "╩" + "═"*12 + "╝")

            # 4. Cold-code-path report: top-10 categories by LOWEST new-cov contribution
            print("\n🧊 Cold-Code Paths (categories with fewest new lines found):")
            sorted_cats = sorted(categories, key=lambda c: category_new_cov[c])
            for i, cat in enumerate(sorted_cats[:10]):
                _gmv = category_new_cov[cat] / max(1, category_total_seeds[cat])
                print(f"  {i+1:2d}. {cat:<20} {category_new_cov[cat]:>5} new lines / {category_total_seeds[cat]} seeds  (GMV={_gmv:.1f})")

            # 5. Summary
            print(f"\n[cov] Coverage runs:   {cov_run_ok}/{cov_run_total} ok")
            print(f"[cov] Total new lines: {total_new_cov} (session delta)")
            print(f"[cov] Seen globally:   {len(seen_globally)} unique (file, line) pairs")
            if args.corpus:
                print(f"[cov] Corpus saved:    {corpus_saved} high-coverage seeds → {CORPUS_DIR}/")

            # 6. Persist run summary to coverage history
            run_record = {
                "timestamp":   time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
                "total_tasks": total_tasks,
                "cov_seeds":   cov_run_ok,
                "new_lines":   total_new_cov,
                "seen_total":  len(seen_globally),
                "corpus_saved": corpus_saved if args.corpus else 0,
                "by_category": {
                    cat: {
                        "new_lines": category_new_cov[cat],
                        "seeds":     category_total_seeds[cat],
                        "gmv":       round(category_new_cov[cat] / max(1, category_total_seeds[cat]), 2),
                    }
                    for cat in categories
                },
            }
            COVERAGE_LOG.parent.mkdir(parents=True, exist_ok=True)
            with open(COVERAGE_LOG, "a") as f:
                f.write(json.dumps(run_record) + "\n")
            print(f"[cov] Run record appended → {COVERAGE_LOG}")

            # 7. Coverage Scheduler — next-run seed budget recommendation
            print("\n" + "═"*60)
            print("  📊 COVERAGE SCHEDULER — Next Run Recommendation")
            print("═"*60)
            scores = {
                cat: category_new_cov[cat] / max(1, category_total_seeds[cat])
                for cat in categories
            }
            sorted_scores = sorted(scores.items(), key=lambda x: -x[1])
            score_vals = list(scores.values())
            if score_vals:
                median_score = sorted(score_vals)[len(score_vals) // 2]
            else:
                median_score = 1.0

            # Tier thresholds
            high_thresh   = median_score * 2.0
            cold_thresh   = median_score * 0.25

            # Compute recommended seed budget
            total_budget = args.seeds * len(categories) + args.csmith_seeds
            high_cats  = [c for c, s in sorted_scores if s >= high_thresh]
            med_cats   = [c for c, s in sorted_scores if cold_thresh <= s < high_thresh]
            cold_cats  = [c for c, s in sorted_scores if s < cold_thresh]

            print(f"{'Category':<22} {'Score':>8}  {'Action':<40}")
            print("-"*72)
            for cat, score in sorted_scores:
                if cat == "csmith":
                    base = args.csmith_seeds
                else:
                    base = args.seeds
                if score >= high_thresh:
                    extra = max(5, min(50, int(score / max(1, median_score)) * base // 2))
                    action = f"+{extra} seeds  🔥 HIGH YIELD"
                elif score >= cold_thresh:
                    extra = base
                    action = f"+{extra} seeds  ✅ NORMAL"
                else:
                    action = f"mutate generator  🧊 COLD (score={score:.0f})"
                print(f"  {cat:<20} {score:>8.0f}  {action}")

            # Print the concrete next run command
            print()
            cold_flag = " --mutate-cold-generators" if cold_cats else ""
            high_seed  = max(args.seeds, min(50, int(sorted_scores[0][1] / max(1, median_score)) * args.seeds)) if high_cats else args.seeds
            next_cov_limit = min(total_budget, max(20, cov_run_ok * 2))
            print("  Suggested next run:")
            print(f"    python3 compiler_olympics.py \\")
            print(f"      --seeds {high_seed} --csmith-seeds {max(args.csmith_seeds, 10)} \\")
            print(f"      --coverage --corpus --coverage-limit {next_cov_limit}{cold_flag} \\")
            print(f"      --cov-build-timeout {args.cov_build_timeout}")
            print("═"*60)

            # Wire mutate cats into fuzz_one for next run (if called again in same process)
            if cold_cats:
                fuzz_one._mutate_cats = set(cold_cats)  # type: ignore[attr-defined]

    print("\n✅ ALL GATES PASSED SUCCESSFULLY.")
    sys.exit(0)

if __name__ == "__main__":
    main()
