import json
from huggingface_hub import HfApi, hf_hub_download

api = HfApi()
repo_id = "zkaedi/zcc-compiler-bug-corpus"

files = api.list_repo_files(repo_id=repo_id, repo_type="dataset")
jsonl_files = [f for f in files if f.endswith('.jsonl')]

print(f"Found files: {jsonl_files}")

all_bugs = []
for f in jsonl_files:
    try:
        path = hf_hub_download(repo_id=repo_id, filename=f, repo_type="dataset")
        with open(path, 'r') as file:
            for line in file:
                if line.strip():
                    all_bugs.append(json.loads(line))
    except Exception as e:
        print(f"Error downloading {f}: {e}")

print("\n--- Existing Bugs ---")
existing_ids = [bug.get('id', 'UNKNOWN') for bug in all_bugs]
print(sorted(existing_ids))
