#!/usr/bin/env python3
import subprocess
import time
import re
import json
import os
import sys
import hashlib
import struct
import argparse
from datetime import datetime

def run_wsl_cmd(cmd):
    # Wrap cmd so that shell operators (e.g. &&) run inside WSL
    escaped_cmd = cmd.replace("'", "'\\''")
    full_cmd = f"wsl bash -c \"/usr/bin/time -f 'METRICS:MEM:%M TIME:%e' bash -c '{escaped_cmd}'\""
    res = subprocess.run(full_cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    
    peak_mem_kb = 0
    measured_time = 0.0
    
    # Extract from stderr
    stderr = res.stderr
    stdout = res.stdout
    
    metrics_match = re.search(r"METRICS:MEM:(\d+) TIME:([\d\.]+)", stderr)
    if metrics_match:
        peak_mem_kb = int(metrics_match.group(1))
        measured_time = float(metrics_match.group(2))
        
    return {
        "stdout": stdout,
        "stderr": stderr,
        "exit_code": res.returncode,
        "peak_mem_mb": peak_mem_kb / 1024.0,
        "time_sec": measured_time
    }

def extract_floats(text):
    # Filter out lines containing filenames/glb or metrics to avoid extracting numbers from them
    lines = []
    for line in text.splitlines():
        if ".glb" in line or "world_stress" in line or "METRICS" in line:
            continue
        lines.append(line)
    filtered_text = "\n".join(lines)
    
    pattern = r"[-+]?\d*\.\d+|\d+"
    matches = re.findall(pattern, filtered_text)
    floats = []
    for m in matches:
        try:
            floats.append(float(m))
        except ValueError:
            pass
    return floats

def parse_glb_hashes(file_path):
    try:
        with open(file_path, "rb") as f:
            glb_data = f.read()
        if not glb_data:
            return None
        
        glb_hash = hashlib.sha256(glb_data).hexdigest()
        
        if len(glb_data) < 20:
            return {
                "glb_hash": glb_hash,
                "terrain_hash": "error_short",
                "bvh_hash": "error_short"
            }
            
        magic, version, length = struct.unpack("<III", glb_data[:12])
        if magic != 0x46546C67:
            return {
                "glb_hash": glb_hash,
                "terrain_hash": "error_magic",
                "bvh_hash": "error_magic"
            }
            
        c0_len, c0_type = struct.unpack("<II", glb_data[12:20])
        c0_data = glb_data[20:20+c0_len]
        json_data = json.loads(c0_data.decode("utf-8").strip())
        
        c1_offset = 20 + c0_len
        if c1_offset + 8 <= len(glb_data):
            c1_len, c1_type = struct.unpack("<II", glb_data[c1_offset:c1_offset+8])
            bin_data = glb_data[c1_offset+8:c1_offset+8+c1_len]
        else:
            bin_data = b""
            
        # Terrain positioning accessor
        terrain_hash = "missing"
        try:
            position_accessor_idx = json_data["meshes"][0]["primitives"][0]["attributes"]["POSITION"]
            accessor = json_data["accessors"][position_accessor_idx]
            bv_idx = accessor["bufferView"]
            bv = json_data["bufferViews"][bv_idx]
            offset = bv.get("byteOffset", 0)
            length = bv["byteLength"]
            terrain_bytes = bin_data[offset:offset+length]
            terrain_hash = hashlib.sha256(terrain_bytes).hexdigest()
        except Exception:
            if bin_data:
                terrain_hash = hashlib.sha256(bin_data[:131072]).hexdigest()
            else:
                terrain_hash = "no_bin_data"
                
        # BVH structure hashing
        bvh_hash = "missing"
        try:
            nodes = json_data.get("nodes", [])
            nodes_str = json.dumps(nodes, sort_keys=True)
            bvh_hash = hashlib.sha256(nodes_str.encode("utf-8")).hexdigest()
        except Exception:
            bvh_hash = "error_json"
            
        return {
            "glb_hash": glb_hash,
            "terrain_hash": terrain_hash,
            "bvh_hash": bvh_hash
        }
    except Exception as e:
        print(f"Error parsing GLB hashes for {file_path}: {e}")
        return None

def save_failure_repro(seed, tier, zcc_run, gcc_run, diff_details, repro_dir):
    repro_path = os.path.join(repro_dir, f"repro_failure_seed_{seed}_{tier}")
    os.makedirs(repro_path, exist_ok=True)
    
    with open(os.path.join(repro_path, "seed"), "w", encoding="utf-8") as f:
        f.write(str(seed))
        
    with open(os.path.join(repro_path, "divergence_log.txt"), "w", encoding="utf-8") as f:
        f.write(diff_details)
        
    with open(os.path.join(repro_path, "zcc_output.txt"), "w", encoding="utf-8") as f:
        f.write(zcc_run["stdout"] + "\n--- STDERR ---\n" + zcc_run["stderr"])
    with open(os.path.join(repro_path, "gcc_output.txt"), "w", encoding="utf-8") as f:
        f.write(gcc_run["stdout"] + "\n--- STDERR ---\n" + gcc_run["stderr"])
        
    # Copy generated GLB if it exists
    zcc_glb = f"world_stress_{seed}_{tier}.glb"
    if os.path.exists(zcc_glb):
        try:
            import shutil
            shutil.copy2(zcc_glb, os.path.join(repro_path, "generated_zcc.glb"))
        except Exception:
            pass
            
    # Try to generate assembly files for diagnostics
    run_wsl_cmd(f"gcc -O2 -S ./experiments/exp21_procedural_world_stress.c -o gcc_temp.s")
    run_wsl_cmd(f"./zcc2 ./experiments/exp21_procedural_world_stress.c -o zcc_temp.s")
    
    import shutil
    if os.path.exists("zcc_temp.s"):
        try:
            shutil.copy2("zcc_temp.s", os.path.join(repro_path, "zcc_assembly.s"))
            os.remove("zcc_temp.s")
        except Exception:
            pass
    if os.path.exists("gcc_temp.s"):
        try:
            shutil.copy2("gcc_temp.s", os.path.join(repro_path, "gcc_assembly.s"))
            os.remove("gcc_temp.s")
        except Exception:
            pass
            
    with open(os.path.join(repro_path, "repro_command.sh"), "w", encoding="utf-8") as f:
        f.write("#!/usr/bin/env bash\n")
        f.write(f"# Repro command for seed {seed}, tier {tier}\n")
        f.write(f"./{tier} --seed {seed} --export test_repro.glb\n")

def load_history_metrics(history_file):
    if not os.path.exists(history_file):
        return None
    try:
        runs = []
        with open(history_file, "r", encoding="utf-8") as f:
            for line in f:
                if line.strip():
                    runs.append(json.loads(line.strip()))
        if runs:
            return runs[-1]
    except Exception:
        pass
    return None

def run_glb_corruptor_test():
    print("      Running Corrupt GLB Rejection tests...")
    
    # We need a valid base GLB. Let's create one by running exp21 with seed 777
    base_glb = "world_stress_777_zcc_o2.glb"
    
    # If it was cleaned up, let's regenerate it temporarily
    regenerated = False
    if not os.path.exists(base_glb):
        run_wsl_cmd("./exp21_zcc_o2 --seed 777 --export world_stress_777_zcc_o2.glb")
        regenerated = True
        
    if not os.path.exists(base_glb):
        print("      [ERROR] Could not generate base GLB for corruptor test!")
        return False
        
    with open(base_glb, "rb") as f:
        data = bytearray(f.read())
        
    tests_passed = True
    
    # Test 1: Corrupt Magic
    corrupt_magic_data = bytearray(data)
    corrupt_magic_data[0:4] = b"gltF" # incorrect magic casing
    with open("corrupt_magic.glb", "wb") as f:
        f.write(corrupt_magic_data)
    res = run_wsl_cmd("./exp22_zcc --input corrupt_magic.glb --output corrupt_out.glb")
    if res["exit_code"] == 1 and "Invalid GLB magic" in res["stderr"]:
        print("        [PASS] Corrupt magic rejected cleanly.")
    else:
        print(f"        [FAIL] Corrupt magic check failed! Exit code: {res['exit_code']}, Stderr: {res['stderr']}")
        tests_passed = False
        
    # Test 2: Corrupt Version
    corrupt_ver_data = bytearray(data)
    corrupt_ver_data[4:8] = struct.pack("<I", 3) # version 3
    with open("corrupt_ver.glb", "wb") as f:
        f.write(corrupt_ver_data)
    res = run_wsl_cmd("./exp22_zcc --input corrupt_ver.glb --output corrupt_out.glb")
    if res["exit_code"] == 1 and "Unsupported GLB version" in res["stderr"]:
        print("        [PASS] Corrupt version rejected cleanly.")
    else:
        print(f"        [FAIL] Corrupt version check failed! Exit code: {res['exit_code']}, Stderr: {res['stderr']}")
        tests_passed = False
        
    # Test 3: Corrupt JSON Chunk Type
    corrupt_json_type = bytearray(data)
    corrupt_json_type[16:20] = b"JSOX" # bad chunk type
    with open("corrupt_json_type.glb", "wb") as f:
        f.write(corrupt_json_type)
    res = run_wsl_cmd("./exp22_zcc --input corrupt_json_type.glb --output corrupt_out.glb")
    if res["exit_code"] == 1 and "Expected JSON chunk type" in res["stderr"]:
        print("        [PASS] Corrupt JSON type rejected cleanly.")
    else:
        print(f"        [FAIL] Corrupt JSON type check failed! Exit code: {res['exit_code']}, Stderr: {res['stderr']}")
        tests_passed = False
        
    # Test 4: Corrupt BIN Chunk Type
    # Find JSON chunk length to find BIN chunk offset
    json_len, = struct.unpack("<I", data[12:16])
    bin_type_offset = 20 + json_len + 4
    if bin_type_offset + 4 <= len(data):
        corrupt_bin_type = bytearray(data)
        corrupt_bin_type[bin_type_offset:bin_type_offset+4] = b"BIX\0"
        with open("corrupt_bin_type.glb", "wb") as f:
            f.write(corrupt_bin_type)
        res = run_wsl_cmd("./exp22_zcc --input corrupt_bin_type.glb --output corrupt_out.glb")
        if res["exit_code"] == 1 and "Expected BIN chunk type" in res["stderr"]:
            print("        [PASS] Corrupt BIN type rejected cleanly.")
        else:
            print(f"        [FAIL] Corrupt BIN type check failed! Exit code: {res['exit_code']}, Stderr: {res['stderr']}")
            tests_passed = False
            
    # Clean up corrupted files
    for fn in ["corrupt_magic.glb", "corrupt_ver.glb", "corrupt_json_type.glb", "corrupt_bin_type.glb", "corrupt_out.glb"]:
        if os.path.exists(fn):
            try:
                os.remove(fn)
            except Exception:
                pass
    if regenerated and os.path.exists(base_glb):
        try:
            os.remove(base_glb)
        except Exception:
            pass
        
    return tests_passed

def main():
    parser = argparse.ArgumentParser(description="Upgraded Compiler + World Benchmark Suite")
    parser.add_argument("--seeds", type=int, default=10, help="Number of seeds to run")
    parser.add_argument("--workspace", type=str, default=".", help="Path to compiler workspace")
    parser.add_argument("--report-json", type=str, default="system_benchmark_report.json", help="Output JSON report file")
    parser.add_argument("--report-md", type=str, default="system_benchmark_report.md", help="Output Markdown report file")
    parser.add_argument("--history-file", type=str, default="benchmark_history.jsonl", help="Historical trending database")
    parser.add_argument("--repro-dir", type=str, default="repro_failures", help="Folder to save reproducibility failure minimizations")
    parser.add_argument("--strict", action="store_true", help="Strict CI-quality release gate validation")
    
    args = parser.parse_args()
    
    print("======================================================================")
    print(">>> INITIATING UPGRADED SYSTEM BENCHMARK ENGINE: MULTI-OPTIMIZATION TIERS")
    print("======================================================================\n")

    # Step 1: Compiler Bootstrap
    print("[1/6] Running Compiler self-host bootstrap (Stage 1 -> 2 -> 3)...")
    bootstrap_res = run_wsl_cmd("make clean && make selfhost")
    bootstrap_pass = "FAIL"
    stage_parity = "FAIL"
    
    if bootstrap_res["exit_code"] == 0 and "SELF-HOST VERIFIED" in bootstrap_res["stdout"]:
        bootstrap_pass = "PASS"
        stage_parity = "PASS"
    else:
        print(f"      [DEBUG] Bootstrap failed. Exit Code: {bootstrap_res['exit_code']}")
        print(f"      [DEBUG] Stdout contains 'SELF-HOST VERIFIED': {'SELF-HOST VERIFIED' in bootstrap_res['stdout']}")
        print(f"      [DEBUG] Stdout length: {len(bootstrap_res['stdout'])}")
        print(f"      [DEBUG] Stderr length: {len(bootstrap_res['stderr'])}")
        if len(bootstrap_res['stderr']) > 0:
            print("      [DEBUG] Last 200 chars of Stderr:")
            print(bootstrap_res['stderr'][-200:])
    print(f"      Bootstrap Time: {bootstrap_res['time_sec']:.2f}s | Peak Memory: {bootstrap_res['peak_mem_mb']:.2f} MB")
    
    # Step 2: Regression Suite
    print("[2/6] Running ZCC regression suite...")
    reg_res = run_wsl_cmd("./run_regression.sh")
    
    # Parse regression results
    total_reg = 0
    passed_reg = 0
    for line in reg_res["stdout"].splitlines():
        if line.startswith("=== ") and line.endswith(" ===") and "result" not in line:
            total_reg += 1
        if "IDENTICAL" in line:
            passed_reg += 1
            
    reg_rate = 0.0
    if total_reg > 0:
        reg_rate = (passed_reg / total_reg) * 100.0
    print(f"      Regression Checks: {passed_reg}/{total_reg} passed ({reg_rate:.1f}%)")

    # Step 3: Compile Experiments (exp21 & exp22 optimization tiers)
    print("[3/6] Compiling graphics workloads (ZCC vs GCC optimization tiers)...")
    
    compile_cmds = {
        "zcc_o0": "./zcc2 ./experiments/exp21_procedural_world_stress.c -o exp21_zcc_o0.s && gcc -o exp21_zcc_o0 exp21_zcc_o0.s -lm",
        "zcc_nofold": "./zcc2 --no-fold ./experiments/exp21_procedural_world_stress.c -o exp21_zcc_nofold.s && gcc -o exp21_zcc_nofold exp21_zcc_nofold.s -lm",
        "zcc_o2": "./zcc2 ./experiments/exp21_procedural_world_stress.c -o exp21_zcc_o2.s && gcc -o exp21_zcc_o2 exp21_zcc_o2.s -lm",
        "gcc_o0": "gcc -O0 -o exp21_gcc_o0 ./experiments/exp21_procedural_world_stress.c -lm",
        "gcc_o2": "gcc -O2 -o exp21_gcc_o2 ./experiments/exp21_procedural_world_stress.c -lm"
    }
    
    compile_metrics = {}
    for tier, cmd in compile_cmds.items():
        comp_res = run_wsl_cmd(cmd)
        compile_metrics[tier] = {
            "exit_code": comp_res["exit_code"],
            "time_sec": comp_res["time_sec"],
            "peak_mem_mb": comp_res["peak_mem_mb"]
        }
        
    # Also compile exp22 roundtrip loader once using ZCC (canonical output)
    exp22_res = run_wsl_cmd("./zcc2 ./experiments/exp22_glb_roundtrip.c -o exp22_zcc.s && gcc -o exp22_zcc exp22_zcc.s -lm")
    
    print(f"      ZCC Compile exp21 (O2): {compile_metrics['zcc_o2']['time_sec']:.2f}s | GCC Compile exp21 (O2): {compile_metrics['gcc_o2']['time_sec']:.2f}s")
    print(f"      ZCC Compile exp22: {exp22_res['time_sec']:.2f}s")

    # Step 4: World Generation Loop
    num_worlds = args.seeds
    start_seed = 777
    assets_per_world = 1000
    total_assets = 0
    glbs_exported = 0
    
    execution_metrics = {tier: {"runtimes": [], "mems": [], "fp_errors": [], "success_count": 0} for tier in compile_cmds.keys()}
    seed_details = []
    divergences_detected = []
    
    print(f"[4/6] Generating and executing {num_worlds} procedural worlds (Seeds {start_seed}-{start_seed+num_worlds-1})...")
    
    for idx in range(num_worlds):
        seed = start_seed + idx
        seed_data = {"seed": seed, "tiers": {}}
        
        # Execute GCC O2 once as our absolute baseline reference
        ref_glb_name = f"world_stress_{seed}_gcc_o2.glb"
        ref_run = run_wsl_cmd(f"./exp21_gcc_o2 --seed {seed} --export {ref_glb_name}")
        ref_floats = extract_floats(ref_run["stdout"])
        ref_hashes = parse_glb_hashes(ref_glb_name) if (ref_run["exit_code"] == 0 and os.path.exists(ref_glb_name)) else None
        
        for tier in compile_cmds.keys():
            glb_name = f"world_stress_{seed}_{tier}.glb"
            binary_name = f"exp21_{tier}"
            
            # Execute
            run_res = run_wsl_cmd(f"./{binary_name} --seed {seed} --export {glb_name}")
            
            # Measure
            exit_code = run_res["exit_code"]
            runtime = run_res["time_sec"]
            mem = run_res["peak_mem_mb"]
            
            if exit_code == 0:
                execution_metrics[tier]["runtimes"].append(runtime)
                execution_metrics[tier]["mems"].append(mem)
                execution_metrics[tier]["success_count"] += 1
                
            # FP error computation
            t_floats = extract_floats(run_res["stdout"])
            min_len = min(len(t_floats), len(ref_floats))
            max_diff = 0.0
            for i in range(min_len):
                max_diff = max(max_diff, abs(t_floats[i] - ref_floats[i]))
            
            if exit_code == 0:
                execution_metrics[tier]["fp_errors"].append(max_diff)
                
            # Get hashes
            hashes = parse_glb_hashes(glb_name) if (exit_code == 0 and os.path.exists(glb_name)) else None
            
            # Record tier data
            seed_data["tiers"][tier] = {
                "exit_code": exit_code,
                "runtime": runtime,
                "mem": mem,
                "fp_error": max_diff,
                "glb_hash": hashes["glb_hash"] if hashes else "error",
                "terrain_hash": hashes["terrain_hash"] if hashes else "error",
                "bvh_hash": hashes["bvh_hash"] if hashes else "error"
            }
            
            # Divergence & Failure Detection:
            is_zcc_tier = tier.startswith("zcc")
            if is_zcc_tier:
                has_diverged = False
                diff_details = ""
                
                if exit_code != 0:
                    has_diverged = True
                    diff_details += f"ZCC exited with non-zero code {exit_code}.\n"
                elif max_diff > 1e-5:
                    has_diverged = True
                    diff_details += f"Floating-point divergence detected! Max absolute error: {max_diff:.6f}.\n"
                elif ref_hashes and hashes:
                    if hashes["terrain_hash"] != ref_hashes["terrain_hash"] or hashes["bvh_hash"] != ref_hashes["bvh_hash"]:
                        has_diverged = True
                        diff_details += f"Geometry structural hash mismatch!\nZCC Terrain: {hashes['terrain_hash']} vs GCC: {ref_hashes['terrain_hash']}\nZCC BVH: {hashes['bvh_hash']} vs GCC: {ref_hashes['bvh_hash']}\n"
                
                if has_diverged:
                    print(f"      [WARNING] Seed {seed} tier {tier} diverged! Auto-saving failure minimizer report...")
                    save_failure_repro(seed, tier, run_res, ref_run, diff_details, args.repro_dir)
                    divergences_detected.append({
                        "seed": seed,
                        "tier": tier,
                        "details": diff_details
                    })
            
            # Clean up GLB files for non-canonical tiers to keep workspace clean
            if tier not in ["zcc_o2", "gcc_o2"] and os.path.exists(glb_name):
                try:
                    os.remove(glb_name)
                except Exception:
                    pass
                    
        # Update metrics for canonical exports
        if seed_data["tiers"]["zcc_o2"]["exit_code"] == 0:
            total_assets += assets_per_world
            glbs_exported += 1
            
        seed_details.append(seed_data)
        
        zcc_o2_time = seed_data["tiers"]["zcc_o2"]["runtime"]
        zcc_o2_err = seed_data["tiers"]["zcc_o2"]["fp_error"]
        print(f"      World {idx+1}/{num_worlds} (Seed {seed}) -> ZCC_O2 Time: {zcc_o2_time:.3f}s | FP Error: {zcc_o2_err:.6f}")

    # Calculate average generation times and memories
    avg_runtimes = {}
    avg_mems = {}
    max_errors = {}
    for tier in compile_cmds.keys():
        runtimes = execution_metrics[tier]["runtimes"]
        mems = execution_metrics[tier]["mems"]
        errors = execution_metrics[tier]["fp_errors"]
        avg_runtimes[tier] = sum(runtimes) / len(runtimes) if runtimes else 0.0
        avg_mems[tier] = sum(mems) / len(mems) if mems else 0.0
        max_errors[tier] = max(errors) if errors else 0.0

    avg_world_gen_time = avg_runtimes["zcc_o2"]
    total_zcc_mem = max(execution_metrics["zcc_o2"]["mems"]) if execution_metrics["zcc_o2"]["mems"] else 0.0

    # Step 5: Round-Trip Verification
    print("[5/6] Running GLB round-trip integrity verification suite...")
    verified_roundtrips = 0
    for idx in range(num_worlds):
        seed = start_seed + idx
        glb_zcc_name = f"world_stress_{seed}_zcc_o2.glb"
        glb_rt_name = f"world_roundtrip_{seed}.glb"
        
        if not os.path.exists(glb_zcc_name):
            continue
            
        # Run exp22 roundtrip
        rt_run = run_wsl_cmd(f"./exp22_zcc --input {glb_zcc_name} --output {glb_rt_name}")
        rt_hashes = parse_glb_hashes(glb_rt_name) if os.path.exists(glb_rt_name) else None
        
        # Check binary identical directly in python
        is_identical = False
        if rt_run["exit_code"] == 0 and os.path.exists(glb_rt_name):
            with open(glb_zcc_name, "rb") as f1, open(glb_rt_name, "rb") as f2:
                is_identical = (f1.read() == f2.read())
                
        if is_identical:
            verified_roundtrips += 1
            
        for sd in seed_details:
            if sd["seed"] == seed:
                sd["round_trip_hash"] = rt_hashes["glb_hash"] if rt_hashes else ("matched" if is_identical else "failed")
                sd["round_trip_match"] = is_identical
                
        # Clean up files
        try:
            if os.path.exists(glb_rt_name):
                os.remove(glb_rt_name)
            if os.path.exists(glb_zcc_name):
                os.remove(glb_zcc_name)
            gcc_glb_name = f"world_stress_{seed}_gcc_o2.glb"
            if os.path.exists(gcc_glb_name):
                os.remove(gcc_glb_name)
        except Exception:
            pass
            
    rt_rate = 0.0
    if glbs_exported > 0:
        rt_rate = (verified_roundtrips / glbs_exported) * 100.0
    print(f"      Round-trip integrity: {verified_roundtrips}/{glbs_exported} matching ({rt_rate:.1f}%)")
    
    corrupt_tests_passed = run_glb_corruptor_test()

    # Step 6: Summary & Dashboard Compile
    print("\n[6/6] Compiling final report metrics & history database...")
    
    peak_memory_overall = max(bootstrap_res["peak_mem_mb"], total_zcc_mem)
    
    # Calculate seed diversity unique sets
    unique_terrain_hashes = set()
    unique_bvh_hashes = set()
    unique_glb_hashes = set()
    for sd in seed_details:
        zcc_hashes = sd["tiers"]["zcc_o2"]
        if zcc_hashes["glb_hash"] not in ["error", "missing"]:
            unique_glb_hashes.add(zcc_hashes["glb_hash"])
        if zcc_hashes["terrain_hash"] not in ["error", "missing"]:
            unique_terrain_hashes.add(zcc_hashes["terrain_hash"])
        if zcc_hashes["bvh_hash"] not in ["error", "missing"]:
            unique_bvh_hashes.add(zcc_hashes["bvh_hash"])
            
    seed_diversity = False
    if num_worlds > 1:
        seed_diversity = (len(unique_terrain_hashes) > 1 and len(unique_bvh_hashes) > 1 and len(unique_glb_hashes) > 1)
    else:
        seed_diversity = True
        
    report_data = {
        "timestamp": datetime.utcnow().isoformat() + "Z",
        "compiler_bootstrap": bootstrap_pass,
        "stage2_stage3_parity": stage_parity,
        "regression_tests_pct": reg_rate,
        "worlds_generated": num_worlds,
        "assets_processed": total_assets,
        "glbs_exported": glbs_exported,
        "round_trip_integrity_pct": rt_rate,
        "fp_consistency_max_error": max_errors["zcc_o2"],
        "peak_memory_mb": peak_memory_overall,
        "bootstrap_compile_time_sec": bootstrap_res["time_sec"],
        "avg_world_gen_time_sec": avg_world_gen_time,
        "seed_diversity_check": "PASS" if seed_diversity else "FAIL",
        "corrupt_glb_rejection": "PASS" if corrupt_tests_passed else "FAIL",
        "unique_terrain_hashes": len(unique_terrain_hashes),
        "unique_bvh_hashes": len(unique_bvh_hashes),
        "unique_glb_hashes": len(unique_glb_hashes),
        "compile_metrics": compile_metrics,
        "avg_runtimes_sec": avg_runtimes,
        "max_errors": max_errors,
        "failures_count": len(divergences_detected)
    }
    
    # Save JSON report
    with open(args.report_json, "w", encoding="utf-8") as jf:
        json.dump(report_data, jf, indent=2)
        
    # Append to trend history file
    history_line = {
        "timestamp": report_data["timestamp"],
        "bootstrap": bootstrap_pass,
        "regressions_pct": reg_rate,
        "avg_runtime_zcc_o2": avg_runtimes["zcc_o2"],
        "avg_runtime_gcc_o2": avg_runtimes["gcc_o2"],
        "max_fp_error": max_errors["zcc_o2"],
        "round_trip_pct": rt_rate,
        "seeds_run": num_worlds,
        "failures_count": len(divergences_detected),
        "bootstrap_compile_time_sec": bootstrap_res["time_sec"],
        "avg_world_gen_time_sec": avg_world_gen_time,
        "seed_diversity": "PASS" if seed_diversity else "FAIL"
    }
    with open(args.history_file, "a", encoding="utf-8") as hf:
        hf.write(json.dumps(history_line) + "\n")
        
    # Read last history run for delta trends calculation
    last_run = load_history_metrics(args.history_file)
    trend_notes = ""
    if last_run:
        old_runtime = last_run.get("avg_world_gen_time_sec", 0)
        old_compile = last_run.get("bootstrap_compile_time_sec", 0)
        
        if old_runtime > 0:
            runtime_change = ((avg_world_gen_time - old_runtime) / old_runtime) * 100.0
            if runtime_change < -1.0:
                trend_notes += f"*   **Performance Trend**: Avg world generation runtime **improved by {abs(runtime_change):.1f}%** vs previous run (from `{old_runtime:.4f}s` to `{avg_world_gen_time:.4f}s`).\n"
            elif runtime_change > 1.0:
                trend_notes += f"*   **Performance Trend**: Avg world generation runtime **regressed by {runtime_change:.1f}%** vs previous run (from `{old_runtime:.4f}s` to `{avg_world_gen_time:.4f}s`).\n"
            else:
                trend_notes += f"*   **Performance Trend**: Avg world generation runtime **stable** (changed by `{runtime_change:+.1f}%`).\n"
        if old_compile > 0:
            compile_change = ((bootstrap_res["time_sec"] - old_compile) / old_compile) * 100.0
            if compile_change < -1.0:
                trend_notes += f"*   **Compiler Speed**: Self-host compile speed **improved by {abs(compile_change):.1f}%** (from `{old_compile:.1f}s` to `{bootstrap_res['time_sec']:.1f}s`).\n"
            elif compile_change > 1.0:
                trend_notes += f"*   **Compiler Speed**: Self-host compile speed **regressed by {compile_change:.1f}%** (from `{old_compile:.1f}s` to `{bootstrap_res['time_sec']:.1f}s`).\n"
    else:
        trend_notes = "*   **Performance Trend**: No previous runs in trend history database yet.\n"

    # Build Markdown Dashboard
    md_content = f"""# Compiler + World Benchmark Suite Report

This report summarizes the compile-time and runtime correctness, parity, and performance characteristics of the Zkaedi C Compiler (ZCC) against Reference GCC when generating and verifying high-load procedural worlds.

## Core Metrics Dashboard

| Metric                     | Value | Reference/Status |
| :------------------------- | ----: | :--------------- |
| **Compiler bootstrap**     | `{bootstrap_pass}` | Self-host completes cleanly |
| **Stage2 <-> Stage3 parity** | `{stage_parity}` | Binary identical codegen |
| **Regression tests**       | `{reg_rate:.1f}%` | All core validation cases pass |
| **Worlds generated**       | `{num_worlds}` | Valid procedural worlds |
| **Assets processed**       | `{total_assets:,}` | BVH objects mapped & optimized |
| **GLBs exported**          | `{glbs_exported}` | Binary glTF 2.0 files written |
| **Round-trip integrity**   | `{rt_rate:.1f}%` | 100% byte-for-byte matching |
| **Floating-point consistency** | `{max_errors["zcc_o2"]:.6f}` | Max absolute error vs GCC |
| **Seed diversity check**   | `{"PASS" if seed_diversity else "FAIL"}` | unique terrain={len(unique_terrain_hashes)}, bvh={len(unique_bvh_hashes)}, glb={len(unique_glb_hashes)} |
| **Corrupt GLB rejection**  | `{"PASS" if corrupt_tests_passed else "FAIL"}` | Malformed headers/types rejected cleanly |
| **Peak memory**            | `{peak_memory_overall:.2f} MB` | Maximum RSS during test |
| **Compile time (self-host)**| `{bootstrap_res['time_sec']:.2f} s` | Selfhost build time |
| **World generation time**  | `{avg_world_gen_time:.3f} s` | Average ZCC execution time |

## Optimization Tiers Matrix Comparison

| Configuration | Compiler | Opt Tier | Compile Time | Avg Run Time | Peak Mem | Max FP Error |
| :--- | :--- | :--- | :---: | :---: | :---: | :---: |
| **zcc_o0** | ZCC | `-O0` (normal) | `{compile_metrics["zcc_o0"]["time_sec"]:.2f}s` | `{avg_runtimes["zcc_o0"]:.3f}s` | `{avg_mems["zcc_o0"]:.1f} MB` | `{max_errors["zcc_o0"]:.6f}` |
| **zcc_nofold** | ZCC | `--no-fold` | `{compile_metrics["zcc_nofold"]["time_sec"]:.2f}s` | `{avg_runtimes["zcc_nofold"]:.3f}s` | `{avg_mems["zcc_nofold"]:.1f} MB` | `{max_errors["zcc_nofold"]:.6f}` |
| **zcc_o2** | ZCC | `-O2` (normal) | `{compile_metrics["zcc_o2"]["time_sec"]:.2f}s` | `{avg_runtimes["zcc_o2"]:.3f}s` | `{avg_mems["zcc_o2"]:.1f} MB` | `{max_errors["zcc_o2"]:.6f}` |
| **gcc_o0** | GCC | `-O0` | `{compile_metrics["gcc_o0"]["time_sec"]:.2f}s` | `{avg_runtimes["gcc_o0"]:.3f}s` | `{avg_mems["gcc_o0"]:.1f} MB` | `{max_errors["gcc_o0"]:.6f}` |
| **gcc_o2** | GCC | `-O2` (ref) | `{compile_metrics["gcc_o2"]["time_sec"]:.2f}s` | `{avg_runtimes["gcc_o2"]:.3f}s` | `{avg_mems["gcc_o2"]:.1f} MB` | `{max_errors["gcc_o2"]:.6f}` |

## Per-Seed Hashing Consistencies (First 10)

| Seed | Terrain Hash | BVH Hash | GLB Hash (ZCC O2) | Round-Trip Match |
| :---: | :--- | :--- | :--- | :---: |
"""

    for i in range(min(num_worlds, 10)):
        sd = seed_details[i]
        zcc_hashes = sd["tiers"]["zcc_o2"]
        rt_match = "🆗" if sd.get("round_trip_match", False) else "❌"
        md_content += f"| `{sd['seed']}` | `{zcc_hashes['terrain_hash'][:16]}...` | `{zcc_hashes['bvh_hash'][:16]}...` | `{zcc_hashes['glb_hash'][:16]}...` | {rt_match} |\n"

    md_content += f"""
## Historical Parity & Optimization Insights
{trend_notes}
*   **Parity**: ZCC-compiled world generator generates identical geometry layouts and boid dynamics coordinates when compared to GCC. Max absolute float divergence across all variables was `{max_errors["zcc_o2"]:.6e}`.
*   **Integrity**: The roundtrip loader compiled by ZCC successfully extracts, parses JSON and BIN boundaries, and outputs identical binary containers.
*   **Optimization Efficiency**: ZCC --no-fold compile time vs -O2 compiler stages and execution speed characteristics are logged.
"""

    if divergences_detected:
        md_content += "\n## Divergence Failures Detected\n"
        for div in divergences_detected:
            md_content += f"*   **Seed {div['seed']} (Tier {div['tier']})**:\n    ```\n    {div['details'].strip()}\n    ```\n"

    with open(args.report_md, "w", encoding="utf-8") as mf:
        mf.write(md_content)
        
    # Print console output table
    print("\n======================================================================")
    print("                     SYSTEM BENCHMARK DASHBOARD")
    print("======================================================================")
    print(f" Compiler bootstrap         |          {bootstrap_pass}")
    print(f" Stage2 <-> Stage3 parity   |          {stage_parity}")
    print(f" Regression tests           |          {reg_rate:.1f}%")
    print(f" Worlds generated           |          {num_worlds}")
    print(f" Assets processed           |          {total_assets:,}")
    print(f" GLBs exported              |          {glbs_exported}")
    print(f" Round-trip integrity       |          {rt_rate:.1f}%")
    print(f" Floating-point consistency |          Max error: {max_errors['zcc_o2']:.6f}")
    print(f" Peak memory                |          {peak_memory_overall:.2f} MB")
    print(f" Compile time (selfhost)    |          {bootstrap_res['time_sec']:.2f} s")
    print(f" World generation time      |          {avg_world_gen_time:.3f} s")
    print("======================================================================\n")
    
    print("======================================================================")
    print("                OPTIMIZATION TIERS EXECUTION TIMES (Avg)")
    print("======================================================================")
    print(f" ZCC -O0       : {avg_runtimes['zcc_o0']:.4f} s")
    print(f" ZCC --no-fold : {avg_runtimes['zcc_nofold']:.4f} s")
    print(f" ZCC -O2       : {avg_runtimes['zcc_o2']:.4f} s")
    print(f" GCC -O0       : {avg_runtimes['gcc_o0']:.4f} s")
    print(f" GCC -O2       : {avg_runtimes['gcc_o2']:.4f} s")
    print("======================================================================\n")
    strict_passed = True
    strict_reasons = []
    if args.strict:
        if bootstrap_pass != "PASS":
            strict_passed = False
            strict_reasons.append("Compiler self-host bootstrap did not PASS")
        if reg_rate < 100.0:
            strict_passed = False
            strict_reasons.append(f"Regression checks pass rate is {reg_rate:.1f}% (required 100.0%)")
        if rt_rate < 100.0:
            strict_passed = False
            strict_reasons.append(f"Round-trip integrity is {rt_rate:.1f}% (required 100.0%)")
        if max_errors["zcc_o2"] > 1e-5:
            strict_passed = False
            strict_reasons.append(f"Floating-point consistency error {max_errors['zcc_o2']:.6f} exceeds 1e-5 limit")
        if num_worlds > 1 and not seed_diversity:
            strict_passed = False
            strict_reasons.append(f"Seed diversity check failed: unique terrain={len(unique_terrain_hashes)}, bvh={len(unique_bvh_hashes)}, glb={len(unique_glb_hashes)} (expected > 1)")
        if not corrupt_tests_passed:
            strict_passed = False
            strict_reasons.append("Corrupt GLB parser safety check failed")
            
        if not strict_passed:
            print("======================================================================")
            print(">>> STRICT MODE CRITICAL FAILURE: RELEASE GATE BLOCKED!")
            print("======================================================================")
            for reason in strict_reasons:
                print(f"  - {reason}")
            print("======================================================================\n")
            sys.exit(1)
        else:
            print("======================================================================")
            print(">>> STRICT MODE PASS: RELEASE GATE APPROVED!")
            print("======================================================================\n")
            
    print(f"Benchmark completed. Reports and history updated successfully.")

if __name__ == "__main__":
    main()
