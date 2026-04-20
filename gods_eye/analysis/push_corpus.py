import os
from huggingface_hub import HfApi, CommitOperationAdd

api = HfApi(token = os.environ.get("HF_TOKEN", ""))
files = [
    "part1.c", "part2.c", "part3.c", "part4.c", "part5.c",
    "zcc.c", "ir.c", "ir.h", "ir_bridge.h", "ir_to_x86.c",
    "compiler_passes.c", "compiler_passes_ir.c", "Makefile",
    "raytracer.c", "mesh_renderer.c", "test_compound.c"
]

ops = []
for f in files:
    try:
        ops.append(CommitOperationAdd(path_in_repo=f"compiler/{f}", path_or_fileobj=f))
    except Exception as e:
        print(f"Skip {f}: {e}")

try:
    result = api.create_commit(
        repo_id="zkaedi/zcc-ir-prime-v1",
        repo_type="dataset",
        operations=ops,
        commit_message="🔱 ZCC Graduation — C99 Compound Literals, Teapot Mesh, Doom Parse Audit"
    )
    print(result)
except Exception as e:
    api = HfApi(token = os.environ.get("HF_TOKEN", ""))
    result = api.create_commit(
        repo_id="zkaedi/zcc-ir-prime-v1",
        repo_type="dataset",
        operations=ops,
        commit_message="🔱 ZCC Graduation — C99 Compound Literals, Teapot Mesh, Doom Parse Audit"
    )
    print(result)
