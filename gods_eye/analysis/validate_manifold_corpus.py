import json, sys

path = "manifold_corpus_test.jsonl"
records = []
with open(path) as f:
    for i, line in enumerate(f):
        line = line.strip()
        if not line:
            continue
        try:
            obj = json.loads(line)
            records.append(obj)
        except Exception as e:
            print(f"FAIL line {i}: {e}")
            sys.exit(1)

print(f"JSONL valid: {len(records)} records")
for rec in records:
    print(f"  func={rec.get('func','?')} nodes={rec.get('node_count','?')} step={rec.get('step','?')} converged={rec.get('converged','?')}")
print("CORPUS EXPORT: OK")
