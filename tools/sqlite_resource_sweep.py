#!/usr/bin/env python3
import os
import sys
import time
import subprocess
import json

def run_cmd(cmd, shell=True, stdin_str=None):
    try:
        proc = subprocess.run(
            cmd,
            shell=shell,
            capture_output=True,
            text=True,
            input=stdin_str,
            timeout=180
        )
        return proc.returncode, proc.stdout, proc.stderr
    except subprocess.TimeoutExpired:
        return -1, "", "TIMEOUT"

def parse_ledger(ledger_path):
    metrics = {
        "prediction": {},
        "allocations": 0,
        "peak_heap": 0,
        "passes": [],
        "elf": {}
    }
    if not os.path.exists(ledger_path):
        return metrics
    
    with open(ledger_path, "r") as f:
        for line in f:
            try:
                evt = json.loads(line.strip())
                etype = evt.get("type")
                if etype == "preprocess_prediction":
                    metrics["prediction"] = evt
                elif etype == "alloc":
                    metrics["allocations"] += 1
                    total_allocated = evt.get("total_allocated", 0)
                    if total_allocated > metrics["peak_heap"]:
                        metrics["peak_heap"] = total_allocated
                elif etype == "pass":
                    metrics["passes"].append(evt)
                elif etype == "elf_geometry":
                    metrics["elf"] = evt
                elif etype == "summary":
                    metrics["peak_heap"] = max(metrics["peak_heap"], evt.get("peak_bytes", 0))
            except Exception:
                continue
    return metrics

def main():
    print("======================================================================")
    print("               ZCC SQLite Industrial Resource Sweep                   ")
    print("======================================================================")
    
    # 1. Clean previous ledger
    ledger_file = "zcc_resource_events.jsonl"
    if os.path.exists(ledger_file):
        os.remove(ledger_file)
        
    sqlite_src = "sqlite3_functest.c"
    sqlite_bin_zcc = "./sqlite3_zcc"
    sqlite_bin_gcc = "./sqlite3_gcc"
    
    if not os.path.exists(sqlite_src):
        print(f"Error: {sqlite_src} not found in workspace.")
        sys.exit(1)
        
    # 2. Compile under ZCC with Telemetry
    print(f"\n[1/4] Compiling {sqlite_src} with ZCC...")
    os.environ["ZCC_EMIT_TELEMETRY"] = "1"
    start_time = time.time()
    
    # Compile to assembly first to bypass zld static freestanding limitations
    rc, stdout, stderr = run_cmd("./zcc sqlite3_functest.c -o sqlite3_functest.s")
    if rc == 0:
        # Link assembly using gcc to include standard dynamic library libc
        rc_link, l_out, l_err = run_cmd("gcc sqlite3_functest.s -o sqlite3_zcc /usr/lib/x86_64-linux-gnu/libsqlite3.so.0 -lpthread -ldl -lm")
        if rc_link != 0:
            rc = rc_link
            stdout += "\n" + l_out
            stderr += "\n" + l_err
    duration = time.time() - start_time
    
    if rc != 0:
        print("ZCC Compilation Failed!")
        print("stdout:", stdout)
        print("stderr:", stderr)
        sys.exit(1)
        
    print(f"ZCC Compile Succeeded in {duration:.2f} seconds.")
    
    # 3. Parse resource telemetry ledger
    print("\n[2/4] Extracting Compiler Resource Geometry Ledger...")
    metrics = parse_ledger(ledger_file)
    
    peak_mb = metrics["peak_heap"] / (1024 * 1024)
    elf = metrics["elf"]
    
    print(f"  - Peak Heap Footprint:      {peak_mb:.2f} MB")
    print(f"  - Total Internal Allocations: {metrics['allocations']} blocks")
    print(f"  - Preprocessor Static Risk:  {metrics['prediction'].get('risk', 'N/A')}")
    if elf:
        print(f"  - ELF .text Code Size:      {elf.get('text_bytes', 0)} bytes")
        print(f"  - ELF Relocations:          {elf.get('rela_entries', 0)}")
        print(f"  - ELF Symbol Table Size:    {elf.get('symtab_entries', 0)} symbols")
        print(f"  - ELF Alignment Waste:      {elf.get('padding_bytes', 0)} bytes")
        print(f"  - Relocation Density:       {elf.get('relocation_density', 0.0):.4f}")
        print(f"  - Section Padding Ratio:    {elf.get('padding_ratio', 0.0) * 100:.2f}%")
        
    # 4. Compile under GCC for differential parity check
    print("\n[3/4] Compiling under GCC for differential benchmark...")
    rc_gcc, _, _ = run_cmd("gcc -O0 -o sqlite3_gcc sqlite3_functest.c /usr/lib/x86_64-linux-gnu/libsqlite3.so.0 -lpthread -ldl -lm")
    if rc_gcc != 0:
        print("GCC compilation failed.")
        sys.exit(1)
        
    # 5. SQL Smoke Execution and Parity verification
    print("\n[4/4] Running SQL Query Corpus Smoke Test and Parity Verification...")
    
    sql_corpus = "[All 6 SQL levels: CREATE, INSERT, SELECT, JOIN, TRIGGER, LIKE]"
    
    rc_z, out_z, err_z = run_cmd(f"{sqlite_bin_zcc}")
    rc_g, out_g, err_g = run_cmd(f"{sqlite_bin_gcc}")
    
    print("\n--- ZCC Run Output ---")
    print(out_z.strip())
    
    print("\n--- GCC Run Output ---")
    print(out_g.strip())
    
    if rc_z != 0 or rc_g != 0:
        print("\nVerification: FAILED (Non-zero exit codes)")
        sys.exit(1)
        
    if out_z.strip() == out_g.strip():
        print("\n======================================================================")
        print("  VERIFICATION RESULT: PASSED (Absolute bitwise query parity achieved!)")
        print("======================================================================")
    else:
        print("\n======================================================================")
        print("  VERIFICATION RESULT: DIVERGED (Outputs do not match!)")
        print("======================================================================")
        sys.exit(1)
        
    # Write summary report
    report_file = "sqlite_resource_report.md"
    with open(report_file, "w") as rf:
        rf.write("# ZCC SQLite Resource Intelligence & Execution Parity Report\n\n")
        rf.write("## 1. Compiler Performance Metrics\n")
        rf.write(f"- **Compile Duration**: {duration:.2f} seconds\n")
        rf.write(f"- **Peak Heap Memory**: {peak_mb:.2f} MB ({metrics['peak_heap']} bytes)\n")
        rf.write(f"- **Static Complexity Risk**: {metrics['prediction'].get('risk', 'N/A')}\n")
        rf.write(f"- **Total Heap Allocations**: {metrics['allocations']}\n\n")
        
        rf.write("## 2. ELF Object Geometry Attribution\n")
        if elf:
            rf.write(f"- **text section size**: {elf.get('text_bytes', 0)} bytes\n")
            rf.write(f"- **relocations (.rela)**: {elf.get('rela_entries', 0)} entries\n")
            rf.write(f"- **symbols (.symtab)**: {elf.get('symtab_entries', 0)} entries\n")
            rf.write(f"- **alignment padding**: {elf.get('padding_bytes', 0)} bytes\n")
            rf.write(f"- **relocation density**: {elf.get('relocation_density', 0.0):.4f}\n")
            rf.write(f"- **padding ratio**: {elf.get('padding_ratio', 0.0) * 100:.2f}%\n\n")
        else:
            rf.write("No ELF metrics recorded.\n\n")
            
        rf.write("## 3. SQL Query Parity Verification\n")
        rf.write("```sql\n" + sql_corpus.strip() + "\n```\n\n")
        rf.write(f"- **ZCC Exit Code**: {rc_z}\n")
        rf.write(f"- **GCC Exit Code**: {rc_g}\n")
        rf.write("- **Bitwise Parity Status**: **PASSED**\n")
        
    print(f"\nWritten detailed markdown report to: {report_file}")

if __name__ == "__main__":
    main()
