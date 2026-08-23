#!/usr/bin/env python3
"""
🔱 ZCC CSMITH DIFFERENTIAL FUZZING WARFARE HARNESS
Emits random C programs via Csmith (or YARPGen if available), compiles under ZCC,
GCC, and Clang, and runs them to verify behavior parity. Automatically reduces
any caught divergences or crashes using C-Reduce.
"""

import os
import sys
import argparse
import subprocess
import shutil
import time
import random
import json
from pathlib import Path

# Fix console encoding on Windows hosts
if sys.platform == "win32":
    try:
        sys.stdout.reconfigure(encoding="utf-8")
        sys.stderr.reconfigure(encoding="utf-8")
    except (AttributeError, ValueError):
        os.environ["PYTHONIOENCODING"] = "utf-8"

DEFAULT_CSMITH_ARGS = (
    "--max-funcs 3 --max-expr-complexity 3 --max-block-depth 2 "
    "--no-arrays --no-structs --no-pointers --no-bitfields --no-unions "
    "--no-volatiles --no-inline-function --no-math64 --safe-math"
)

# Prefix all shell commands with nice -n 19 to run at lowest background priority.
# Prevents csmith/gcc/creduce floods from saturating the host CPU.
NICE_PREFIX = "nice -n 19 ionice -c 3 "

def run_cmd(cmd, timeout=10.0, cwd=None, no_nice=False):
    """Run shell command capturing stdout, stderr, and exit code."""
    if not no_nice:
        cmd = NICE_PREFIX + cmd
    try:
        res = subprocess.run(
            cmd,
            shell=True,
            capture_output=True,
            text=True,
            timeout=timeout,
            cwd=cwd
        )
        return res.returncode, res.stdout, res.stderr, False
    except subprocess.TimeoutExpired as e:
        return -1, e.stdout.decode("utf-8", errors="ignore") if e.stdout else "", \
               e.stderr.decode("utf-8", errors="ignore") if e.stderr else "TIMEOUT", True
    except Exception as e:
        return -99, "", str(e), False

class FuzzWarfare:
    def __init__(self, args):
        self.iterations = args.iterations
        self.timeout = args.timeout
        self.csmith_args = args.csmith_args
        self.tmp_dir = Path(args.tmp_dir).resolve()
        self.out_dir = Path(args.out_dir).resolve()
        
        # Ensure workspace directories
        self.tmp_dir.mkdir(parents=True, exist_ok=True)
        self.out_dir.mkdir(parents=True, exist_ok=True)
        
        # Find compiler & tool binary paths
        self.csmith_bin = shutil.which("csmith")
        self.creduce_bin = shutil.which("creduce")
        self.gcc_bin = shutil.which("gcc")
        self.clang_bin = shutil.which("clang")
        self.zcc_bin = "./zcc"

        self.enable_reduce = args.enable_reduce
        self.reduce_timeout = args.reduce_timeout
        self.cooldown = getattr(args, "cooldown", 0.5)

        # Check critical pre-requisites
        if not self.csmith_bin:
            print("[CRITICAL] csmith binary not found in WSL path.")
            sys.exit(1)
        if not Path(self.zcc_bin).exists():
            print(f"[CRITICAL] ZCC compiler binary '{self.zcc_bin}' not found. Run make selfhost first.")
            sys.exit(1)
            
        print("🔱 ZCC Csmith Differential Fuzzing Warfare Initialized.")
        print(f"   CSmith:   {self.csmith_bin}")
        print(f"   CReduce:  {self.creduce_bin if self.creduce_bin else 'NOT INSTALLED (reduction disabled)'}")
        print(f"   GCC:      {self.gcc_bin}")
        print(f"   Clang:    {self.clang_bin}")
        print(f"   ZCC:      {self.zcc_bin}")

    def generate_csmith_program(self, seed, out_file):
        """Generate a random C program using Csmith."""
        cmd = f"{self.csmith_bin} --seed {seed} {self.csmith_args} > {out_file}"
        code, stdout, stderr, timed_out = run_cmd(cmd)
        if code != 0:
            print(f"   [ERROR] Csmith generation failed for seed {seed}: {stderr}")
            return False
        return True

    def test_compilation_and_execution(self, c_file):
        """Compile and execute under ZCC, GCC (-O0), GCC (-O3), and Clang (-O3)."""
        results = {}
        c_path = Path(c_file).resolve()
        
        # 1. Compile with GCC (-O3) - Reference
        gcc_o3_bin = self.tmp_dir / "bin_gcc_o3"
        cmd_gcc_o3 = f"gcc -O3 -I/usr/include/csmith {c_path} -o {gcc_o3_bin} -lm"
        code, stdout, stderr, timed_out = run_cmd(cmd_gcc_o3)
        results["gcc_o3_compile"] = {"code": code, "stderr": stderr, "timeout": timed_out}
        if code == 0:
            code_run, stdout_run, stderr_run, timed_out_run = run_cmd(str(gcc_o3_bin), timeout=self.timeout)
            results["gcc_o3_run"] = {"code": code_run, "stdout": stdout_run, "stderr": stderr_run, "timeout": timed_out_run}

        # 2. Compile with GCC (-O0)
        gcc_o0_bin = self.tmp_dir / "bin_gcc_o0"
        cmd_gcc_o0 = f"gcc -O0 -I/usr/include/csmith {c_path} -o {gcc_o0_bin} -lm"
        code, stdout, stderr, timed_out = run_cmd(cmd_gcc_o0)
        results["gcc_o0_compile"] = {"code": code, "stderr": stderr, "timeout": timed_out}
        if code == 0:
            code_run, stdout_run, stderr_run, timed_out_run = run_cmd(str(gcc_o0_bin), timeout=self.timeout)
            results["gcc_o0_run"] = {"code": code_run, "stdout": stdout_run, "stderr": stderr_run, "timeout": timed_out_run}

        # 3. Compile with Clang (-O3)
        if self.clang_bin:
            clang_bin = self.tmp_dir / "bin_clang"
            cmd_clang = f"clang -O3 -I/usr/include/csmith {c_path} -o {clang_bin} -lm"
            code, stdout, stderr, timed_out = run_cmd(cmd_clang)
            results["clang_compile"] = {"code": code, "stderr": stderr, "timeout": timed_out}
            if code == 0:
                code_run, stdout_run, stderr_run, timed_out_run = run_cmd(str(clang_bin), timeout=self.timeout)
                results["clang_run"] = {"code": code_run, "stdout": stdout_run, "stderr": stderr_run, "timeout": timed_out_run}

        # 4. Compile with ZCC
        zcc_s = self.tmp_dir / "bin_zcc.s"
        zcc_bin = self.tmp_dir / "bin_zcc"
        cmd_zcc = f"{self.zcc_bin} -Izcc_sys_includes -I/usr/include/csmith {c_path} -o {zcc_s}"
        code, stdout, stderr, timed_out = run_cmd(cmd_zcc)
        results["zcc_compile"] = {"code": code, "stderr": stderr, "timeout": timed_out}
        
        if code == 0:
            cmd_link = f"gcc {zcc_s} -o {zcc_bin} -lm"
            code_link, stdout_link, stderr_link, timed_out_link = run_cmd(cmd_link)
            results["zcc_link"] = {"code": code_link, "stderr": stderr_link, "timeout": timed_out_link}
            
            if code_link == 0:
                code_run, stdout_run, stderr_run, timed_out_run = run_cmd(str(zcc_bin), timeout=self.timeout)
                results["zcc_run"] = {"code": code_run, "stdout": stdout_run, "stderr": stderr_run, "timeout": timed_out_run}

        return results

    def verify_parity(self, results):
        """Analyze results to detect compiler or execution mismatches."""
        # Check ZCC compilation crash
        if results.get("zcc_compile", {}).get("code", 0) != 0:
            # Check if GCC compiled it successfully
            if results.get("gcc_o3_compile", {}).get("code", 0) == 0:
                return "ZCC_COMPILE_CRASH", results["zcc_compile"]["stderr"]

        # Check ZCC linker crash
        if "zcc_link" in results and results["zcc_link"]["code"] != 0:
            return "ZCC_LINK_CRASH", results["zcc_link"]["stderr"]

        # Check ZCC runtime timeout/crash
        zcc_run = results.get("zcc_run")
        if zcc_run:
            if zcc_run["timeout"]:
                gcc_run = results.get("gcc_o3_run")
                if gcc_run and gcc_run.get("timeout"):
                    # Both ZCC and GCC reference timed out natively. Ignore.
                    pass
                else:
                    return "ZCC_RUNTIME_TIMEOUT", "ZCC execution timed out."
            if zcc_run["code"] != 0:
                # Check if GCC reference runs successfully
                gcc_run = results.get("gcc_o3_run")
                if gcc_run and gcc_run["code"] == 0:
                    return "ZCC_RUNTIME_CRASH", f"Exit Code: {zcc_run['code']}\nStderr: {zcc_run['stderr']}"

        # Check output mismatch
        if zcc_run and zcc_run["code"] == 0:
            gcc_run = results.get("gcc_o3_run")
            if gcc_run and gcc_run["code"] == 0:
                zcc_out = zcc_run["stdout"].strip()
                gcc_out = gcc_run["stdout"].strip()
                if zcc_out != gcc_out:
                    return "ZCC_CODEGEN_DIVERGENCE", f"ZCC Output:\n{zcc_out}\n\nGCC -O3 Output:\n{gcc_out}"

        return "OK", ""

    def run_reduction(self, seed, c_file, failure_type):
        """Run C-Reduce to minimize the failing test case."""
        if not self.enable_reduce:
            print("   [INFO] C-Reduce disabled (use --enable-reduce to enable). Skipping.")
            return
        if not self.creduce_bin:
            print("   [INFO] C-Reduce not installed. Skipping reduction.")
            return
            
        c_path = Path(c_file).resolve()
        sh_path = self.tmp_dir / "interesting.sh"
        zcc_abs = Path(self.zcc_bin).resolve()
        sys_inc_abs = Path("zcc_sys_includes").resolve()

        # Write specific interestingness test script
        script_content = "#!/bin/bash\n"
        script_content += f"ZCC_BIN=\"{zcc_abs}\"\n"
        script_content += f"SYS_INC=\"-I{sys_inc_abs}\"\n"
        script_content += "c_file=\"test_fuzz_tmp.c\"\n\n"
        
        if failure_type == "ZCC_COMPILE_CRASH":
            script_content += "# Check ZCC compilation fails\n"
            script_content += "$ZCC_BIN $SYS_INC -I/usr/include/csmith $c_file -o bin_zcc.s 2>zcc_err.log\n"
            script_content += "if [ $? -eq 0 ]; then exit 1; fi\n"
            script_content += "# Ensure GCC compiles and runs consistently (so no UB)\n"
            script_content += "gcc -O0 -I/usr/include/csmith $c_file -o bin_gcc_o0 -lm &>/dev/null || exit 1\n"
            script_content += "gcc -O3 -I/usr/include/csmith $c_file -o bin_gcc_o3 -lm &>/dev/null || exit 1\n"
            script_content += "out_o0=$(./bin_gcc_o0)\n"
            script_content += "out_o3=$(./bin_gcc_o3)\n"
            script_content += "if [ \"$out_o0\" != \"$out_o3\" ]; then exit 1; fi\n"
            script_content += "exit 0\n"
            
        elif failure_type in ["ZCC_LINK_CRASH", "ZCC_RUNTIME_CRASH", "ZCC_RUNTIME_TIMEOUT"]:
            script_content += "# Compile and link with ZCC\n"
            script_content += "$ZCC_BIN $SYS_INC -I/usr/include/csmith $c_file -o bin_zcc.s &>/dev/null || exit 1\n"
            if failure_type == "ZCC_LINK_CRASH":
                script_content += "gcc bin_zcc.s -o bin_zcc -lm &>/dev/null\n"
                script_content += "if [ $? -eq 0 ]; then exit 1; fi\n"
            elif failure_type == "ZCC_RUNTIME_TIMEOUT":
                script_content += "gcc bin_zcc.s -o bin_zcc -lm &>/dev/null || exit 1\n"
                script_content += "timeout 2 ./bin_zcc &>/dev/null\n"
                script_content += "if [ $? -ne 124 ]; then exit 1; fi\n"
            else:
                script_content += "gcc bin_zcc.s -o bin_zcc -lm &>/dev/null || exit 1\n"
                script_content += "./bin_zcc &>/dev/null\n"
                script_content += "if [ $? -eq 0 ]; then exit 1; fi\n"
            script_content += "# Ensure GCC compiles and runs consistently (so no UB)\n"
            script_content += "gcc -O0 -I/usr/include/csmith $c_file -o bin_gcc_o0 -lm &>/dev/null || exit 1\n"
            script_content += "gcc -O3 -I/usr/include/csmith $c_file -o bin_gcc_o3 -lm &>/dev/null || exit 1\n"
            if failure_type == "ZCC_RUNTIME_TIMEOUT":
                script_content += "timeout 2 ./bin_gcc_o0 &>/dev/null || exit 1\n"
                script_content += "timeout 2 ./bin_gcc_o3 &>/dev/null || exit 1\n"
            else:
                script_content += "out_o0=$(./bin_gcc_o0)\n"
                script_content += "out_o3=$(./bin_gcc_o3)\n"
                script_content += "if [ \"$out_o0\" != \"$out_o3\" ]; then exit 1; fi\n"
            script_content += "exit 0\n"
            
        elif failure_type == "ZCC_CODEGEN_DIVERGENCE":
            script_content += "# Compile and link ZCC\n"
            script_content += "$ZCC_BIN $SYS_INC -I/usr/include/csmith $c_file -o bin_zcc.s &>/dev/null || exit 1\n"
            script_content += "gcc bin_zcc.s -o bin_zcc -lm &>/dev/null || exit 1\n"
            script_content += "# Ensure GCC compiles and runs consistently (so no UB)\n"
            script_content += "gcc -O0 -I/usr/include/csmith $c_file -o bin_gcc_o0 -lm &>/dev/null || exit 1\n"
            script_content += "gcc -O3 -I/usr/include/csmith $c_file -o bin_gcc_o3 -lm &>/dev/null || exit 1\n"
            script_content += "out_o0=$(./bin_gcc_o0)\n"
            script_content += "out_o3=$(./bin_gcc_o3)\n"
            script_content += "if [ \"$out_o0\" != \"$out_o3\" ]; then exit 1; fi\n"
            script_content += "# Capture and compare ZCC output\n"
            script_content += "out_zcc=$(./bin_zcc)\n"
            script_content += "if [ \"$out_zcc\" != \"$out_o3\" ]; then exit 0; else exit 1; fi\n"
            
        else:
            # Catch-all fallback
            script_content += "exit 1\n"
            
        sh_path.write_text(script_content, encoding="utf-8")
        try:
            sh_path.chmod(0o755)
        except PermissionError:
            pass
        
        print(f"   [INFO] Starting C-Reduce on seed {seed} ({failure_type})...")
        
        # Copy file to a dedicated reduction file
        reduction_c = self.tmp_dir / "test_fuzz_tmp.c"
        if reduction_c != c_path:
            shutil.copyfile(c_path, reduction_c)
            
        # Run creduce — capped at reduce_timeout (default 60s, not 600s)
        cmd_creduce = f"creduce {sh_path} {reduction_c} --timeout 5"
        code, stdout, stderr, timed_out = run_cmd(cmd_creduce, timeout=float(self.reduce_timeout), cwd=str(self.tmp_dir))
        
        # Save reduced result
        reduced_dest = self.out_dir / f"seed_{seed}_reduced.c"
        if reduction_c.exists():
            shutil.copyfile(reduction_c, reduced_dest)
            print(f"   [SUCCESS] Reduced code saved to: {reduced_dest}")
        else:
            print("   [WARNING] Reduction failed or file was deleted.")

    def run_warfare(self):
        """Execute the differential fuzzing campaign."""
        print(f"🚀 Launching fuzzing campaign for {self.iterations} iterations...")
        
        failures_found = 0
        for i in range(1, self.iterations + 1):
            seed = random.randint(1, 10000000)
            c_file = self.tmp_dir / f"fuzz_{seed}.c"
            
            print(f"[{i}/{self.iterations}] Seed: {seed} ... Generating", end="", flush=True)
            if not self.generate_csmith_program(seed, c_file):
                continue
                
            print(" -> Compiling", end="", flush=True)
            results = self.test_compilation_and_execution(c_file)
            
            print(" -> Verifying Parity", end="", flush=True)
            failure, details = self.verify_parity(results)
            
            if failure == "OK":
                print(" -> [PASS]")
                # Clean up file to preserve disk space
                if c_file.exists():
                    c_file.unlink()
            else:
                print(f" -> [FAIL: {failure}]")
                failures_found += 1
                # Save original reproducer
                dest_orig = self.out_dir / f"seed_{seed}_original.c"
                shutil.copyfile(c_file, dest_orig)
                
                # Write log file
                log_file = self.out_dir / f"seed_{seed}_log.txt"
                log_file.write_text(f"Failure Type: {failure}\nDetails:\n{details}\n", encoding="utf-8")
                
                # Run C-Reduce
                self.run_reduction(seed, c_file, failure)
                
            if hasattr(self, 'cooldown') and self.cooldown > 0:
                time.sleep(self.cooldown)
                
        print(f"\n🔱 Fuzzing campaign finished. Total iterations: {self.iterations}. Failures found: {failures_found}.")

def main():
    p = argparse.ArgumentParser(description="🔱 ZCC Csmith Differential Fuzzing Warfare Harness")
    p.add_argument("--iterations", type=int, default=100, help="Number of fuzzing seeds to test")
    p.add_argument("--timeout", type=float, default=5.0, help="Execution timeout for compiled programs")
    p.add_argument("--cooldown", type=float, default=0.5, help="Thermal cooldown pause between iterations in seconds (default: 0.5s)")
    p.add_argument("--csmith-args", type=str, default=DEFAULT_CSMITH_ARGS, help="Arguments passed to Csmith")
    p.add_argument("--tmp-dir", type=str, default="tmp_fuzz", help="Temp working directory")
    p.add_argument("--out-dir", type=str, default="fuzz_warfare", help="Output directory for failures & reduced test cases")
    p.add_argument("--enable-reduce", action="store_true", default=False,
                   help="Enable C-Reduce on failures (CPU-intensive, off by default)")
    p.add_argument("--reduce-timeout", type=int, default=60,
                   help="C-Reduce max wall time per failure in seconds (default: 60, was 600)")
    args = p.parse_args()
    
    harness = FuzzWarfare(args)
    harness.run_warfare()

if __name__ == "__main__":
    main()
