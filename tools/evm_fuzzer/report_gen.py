#!/usr/bin/env python3
import os
import sys
from datetime import datetime


def generate_report(swarm_dir, decomp_dir, output):
    cids = set()
    if os.path.exists(swarm_dir):
        for f in os.listdir(swarm_dir):
            if f.startswith("contract_") and f.endswith(".bin"):
                cid = f.replace("contract_", "").replace(".bin", "")
                cids.add(cid)
    
    contracts = []
    success_count = 0
    partial_count = 0
    fail_count = 0

    for cid in sorted(list(cids), key=lambda x: int(x) if x.isdigit() else x):
        bin_file = f"contract_{cid}.bin"
        c_file = f"contract_{cid}.c"
        bin_path = os.path.join(swarm_dir, bin_file)
        c_path = os.path.join(decomp_dir, c_file)
        
        size = os.path.getsize(bin_path) if os.path.exists(bin_path) else 0
        
        if not os.path.exists(c_path):
            status = "FAIL"
            decomp_lines = 0
            fail_count += 1
        else:
            try:
                with open(c_path, errors="ignore") as fc:
                    decomp = fc.read()
                decomp_lines = len(decomp.splitlines())
                if "uint256_t" in decomp:
                    status = "SUCCESS"
                    success_count += 1
                else:
                    status = "PARTIAL"
                    partial_count += 1
            except Exception:
                status = "FAIL"
                decomp_lines = 0
                fail_count += 1

            contracts.append({
                "id": cid,
                "size": size,
                "decomp_lines": decomp_lines,
                "status": status
            })

    total = len(contracts)
    print(f"SwarmDecompile Summary: TOTAL={total} SUCCESS={success_count} PARTIAL={partial_count} FAIL={fail_count}")

    html = f"""<!DOCTYPE html>
<html>
<head><title>ZKAEDI SwarmDecompile Report — {datetime.now().strftime('%Y-%m-%d %H:%M')}</title>
<style>body {{ font-family: monospace; background:#0b0c0f; color:#0f0; }} table {{ border-collapse: collapse; }} th,td {{ padding:8px; border:1px solid #222; }}</style>
</head>
<body>
<h1>🔱 ZKAEDI SWARMDECOMPILE REPORT</h1>
<p>Contracts processed: {total} | SUCCESS: {success_count} | PARTIAL: {partial_count} | FAIL: {fail_count}</p>
<table>
<tr><th>ID</th><th>Bytecode Size</th><th>Decomp Lines</th><th>Status</th></tr>
"""
    for c in sorted(contracts, key=lambda x: int(x["id"]) if x["id"].isdigit() else x["id"])[:100]:
        html += f"<tr><td>{c['id']}</td><td>{c['size']}</td><td>{c['decomp_lines']}</td><td>{c['status']}</td></tr>\n"
    
    html += "</table></body></html>"
    with open(output, "w") as f:
        f.write(html)

if __name__ == "__main__":
    generate_report(sys.argv[1], sys.argv[2], sys.argv[3])
