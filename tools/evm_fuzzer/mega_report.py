#!/usr/bin/env python3
import os
import sys
from collections import Counter
from datetime import datetime


def generate_mega_report(corpus_dir, decomp_dir, output_html):
    stats = {
        "total_contracts": 0,
        "abi_functions": 0,
        "memory_ops": 0,
        "successful_decomp": 0,
        "top_selectors": Counter(),
        "interesting_cases": []
    }

    if not os.path.exists(decomp_dir):
        os.makedirs(decomp_dir)

    for fname in os.listdir(decomp_dir):
        if not fname.endswith(".c"):
            continue
        stats["total_contracts"] += 1
        path = os.path.join(decomp_dir, fname)
        with open(path) as f:
            content = f.read()
            stats["successful_decomp"] += 1
            # Count reconstructed functions
            stats["abi_functions"] += content.count("function func_0x")
            # Rough memory op count
            stats["memory_ops"] += content.count("mem[")

            if "function func_0x" in content and len(content) > 800:
                stats["interesting_cases"].append(fname)

    html = f"""<!DOCTYPE html>
<html>
<head><title>ZCC MEGA SWARM — 50k Contracts</title>
<style>body {{font-family:monospace; background:#0b0c0f; color:#0f0;}}</style>
</head>
<body>
<h1>🔱 ZKAEDI MEGA SWARM REPORT — {datetime.now().strftime('%Y-%m-%d %H:%M')}</h1>
<p><strong>{stats["total_contracts"]:,}</strong> contracts processed | 
   <strong>{stats["abi_functions"]}</strong> ABI functions extracted | 
   <strong>{stats["memory_ops"]}</strong> memory ops observed</p>

<h2>Top Interesting Contracts ({len(stats["interesting_cases"])})</h2>
<ul>
"""
    for case in stats["interesting_cases"][:50]:
        html += f"<li><a href='mega_decomp/{case}'>{case}</a></li>\n"

    html += "</ul></body></html>"

    with open(output_html, "w") as f:
        f.write(html)

    print(f"✅ MEGA SWARM COMPLETE — {stats['total_contracts']:,} contracts")
    print(f"   ABI functions: {stats['abi_functions']}")
    print(f"   Interesting cases: {len(stats['interesting_cases'])}")
    print(f"   Report: {output_html}")

if __name__ == "__main__":
    if len(sys.argv) < 4:
        print("Usage: mega_report.py <corpus_dir> <decomp_dir> <output_html>")
        sys.exit(1)
    generate_mega_report(sys.argv[1], sys.argv[2], sys.argv[3])
