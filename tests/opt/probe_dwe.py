import os
import sys
from pathlib import Path

# Add repo root to sys.path so zcc_oneirogenesis can be imported
repo_root = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(repo_root))

from zcc_oneirogenesis import DreamEngine, FitnessOracle

candidate_paths = [
    repo_root / 'tests' / 'opt' / 'specimen.s',
    repo_root / 'tests' / 'opt' / 'specimen_opt.s',
    Path('/tmp/opt_oneiro.s'),
]

source = None
for p in candidate_paths:
    if p.exists():
        source = p.read_text(encoding='utf-8', errors='replace')
        break

if source is None:
    # Use fallback specimen if file not present
    source = (repo_root / 'tests' / 'opt' / 'specimen.s').read_text(encoding='utf-8', errors='replace')

engine = DreamEngine(seed=0, islands=4, deterministic=True, telemetry=False)
oracle = FitnessOracle()

orig_insts = oracle.evaluate(source).inst_count
candidates = engine.evolve(source, cycles=10)
best = candidates[0]
opt_insts = best.vector.inst_count

eliminated = orig_insts - opt_insts
reduction_pct = (1.0 - opt_insts / max(1, orig_insts)) * 100.0

print(f'Total eliminated / optimized: {eliminated}')
print(f'Remaining instructions: {opt_insts}')
print(f'Total Reduction from {orig_insts}: {reduction_pct:.2f}%')
