import sys
sys.path.append('/mnt/h/__DOWNLOADS/zcc_github_upload')
from zcc_dream_mutations import MutationEngine
with open('/mnt/h/__DOWNLOADS/zcc_github_upload/dreams/island_0_parent.s') as f:
    lines = f.readlines()
engine = MutationEngine(seed=42)
muts = engine.dream(lines, max_point_mutations=2, include_sweeps=False)
for m in muts:
    print(f"{m.name} at {m.line_range}")
