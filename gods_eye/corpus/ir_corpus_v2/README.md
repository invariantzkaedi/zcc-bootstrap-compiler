# ZCC IR Telemetry Corpus - Graphics Experiments

**Version**: v2.0 (April 2026)  
**Experiments**: 5 graphics workloads  
**Events**: 25 IR optimization events  
**Size**: 8.8KB JSONL  

## Experiments Included

1. **exp1_raytracer.jsonl** - SIMD ray tracer (13 functions, 28.3% IR reduction)
2. **exp2_voxel.jsonl** - Voxel rendering engine
3. **exp3_audio.jsonl** - Audio visualizer
4. **exp4_vr.jsonl** - VR stereo renderer
5. **exp5_physics.jsonl** - Physics simulation engine

## Event Schema

Each line is a JSON object with fields:
- `event_type`: "ir_pass_complete" or "summary"
- `pass_name`: Pass identifier (dce, const_fold, strength_reduce, dce2)
- `ir_nodes_before/after`: Node counts before/after optimization
- `ir_nodes_deleted/modified`: Transformation metrics
- `ir_funcs`: Number of functions processed
- `h_t_state`: ZKAEDI PRIME Hamiltonian state
- `timestamp`: Unix timestamp
- ZKAEDI PRIME fields: gpu_temp_c, gpu_util_pct, swarm_cycles

## IR Pipeline

Standard 4-pass pipeline:
1. **DCE** (Dead Code Elimination)
2. **Const Fold** (Constant folding)
3. **Strength Reduce** (Strength reduction)
4. **DCE2** (Second DCE pass)

## Compilation Command

```bash
./zcc <source.c> --ir --telemetry -o <output.s> 2>&1 > <output.jsonl>
```

## Validation

All files validated with:
```bash
python3 -c "import json; [json.loads(line) for line in open('file.jsonl')]"
```

Status: ✅ All 10 files valid JSON Lines format
