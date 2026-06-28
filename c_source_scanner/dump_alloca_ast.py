#!/usr/bin/env python3
"""Dump AST for bad() function in a CWE-121 alloca+memcpy file to diagnose node structure."""
import sys, os, json, subprocess, glob

workspace = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
zcc = os.path.join(workspace, "zcc")
support = os.path.join(workspace, "juliet_train_subset", "C", "testcasesupport")

# Pick first alloca+memcpy file that exists
candidates = [
    "CWE121_Stack_Based_Buffer_Overflow__CWE193_char_alloca_memcpy_04.c",
    "CWE121_Stack_Based_Buffer_Overflow__CWE805_char_alloca_memcpy_53a.c",
]
target = None
for c in candidates:
    found = glob.glob(os.path.join(workspace, "juliet_train_subset", "**", c), recursive=True)
    if found:
        target = found[0]
        break

if not target:
    print("ERROR: no target file found")
    sys.exit(1)

print(f"Parsing: {os.path.basename(target)}")
r = subprocess.run([zcc, "--dump-ast-json", "-I", support, target],
                   capture_output=True, text=True)
if not r.stdout.strip():
    print("ERROR: no stdout from ZCC")
    print("STDERR:", r.stderr[:300])
    sys.exit(1)

try:
    data = json.loads(r.stdout)
except Exception as e:
    print(f"JSON parse error: {e}")
    print("STDOUT (first 200):", r.stdout[:200])
    sys.exit(1)

print(f"Top-level keys:", list(data.keys()))
nodes = data.get("nodes", [])
print(f"Number of nodes: {len(nodes)}")
func_nodes = [n for n in nodes if n.get("kind") == "ND_FUNC_DEF"]
print(f"Number of ND_FUNC_DEF nodes: {len(func_nodes)}")
for fn in func_nodes:
    print(f"  fn: {fn.get('function','?')}")

# Find bad() function
target_fn = None
for fn in func_nodes:
    if "bad" in fn.get("function", "").lower():
        target_fn = fn
        break
if target_fn is None and func_nodes:
    target_fn = func_nodes[0]

if target_fn:
    fname = target_fn.get("function", "?")
    print(f"\n=== Dumping: {fname} ===")
    body = target_fn.get("children", {}).get("body", {})
    stmts = body.get("children", {}).get("stmts", [])
    print(f"Stmts in body: {len(stmts)}")
    for i, s in enumerate(stmts[:8]):
        print(f"\n--- stmt[{i}] ---")
        print(json.dumps(s, indent=2)[:1000])


