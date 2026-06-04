import os
import sys

_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.append(_dir)
from zcc_dream_mutations import MutationEngine

with open(os.path.join(_dir, 'dreams/island_0_parent.s')) as f:
    lines = f.readlines()
engine = MutationEngine(seed=42)
muts = engine.dream(lines, max_point_mutations=2, include_sweeps=False)
for m in muts:
    print(f"{m.name} at {m.line_range}")
