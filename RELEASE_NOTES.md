# Release Notes — `zcc-sdf-compiler-v3.1-ubomb`

Tag: `v3.1-ubomb`  
Status: Release Candidate  
Benchmark environment: WSL2, RTX 5070 Blackwell target viewer, `fleet_lite.glb` 2.9 GB / 63M vertices.

We are pleased to release the **V3.1 Release Candidate** of the ZCC SDF Compiler:  
`zcc-sdf-compiler-v3.1-ubomb`.

## Headline

> ZCC SDF Compiler V3.1 introduces Error-Bounded Residual Hybrid SDF Compilation, reducing mean fitting errors by 42x on held-out samples (from 10.08 cm down to 0.23 cm) with a 0.0% empirical bound violation rate, compiling in ~2.0 seconds with <2.5 MB peak phase memory.

## Highlights

- **Held-out validation stats**: Automatically partitions points into an 80/20 split to evaluate empirical bound coverage and out-of-sample visual errors.
- **Vectorized CPU evaluator**: Accelerated Python-side primitive distance calculation, yielding a 19.7x speedup on the residual baking pass.
- **Warp-aligned coordinate space**: Aligns the 3D texture lookup domain with dynamic primitive animation coordinates to prevent texture sliding.
- **Conservative sphere tracing**: Steps safely on `mapDSafe()` (visual distance minus local error bound) while performing hit tests and normals shading on `mapDVisual()`.
- **Integrated MP3 player**: Adds local audio file uploads and play/pause controls routed through the Web Audio FFT analyser.
- **Debug viewport modes**: Added new visual overlays for residual maps, error bounds, safe-step ratios, and splitscreen comparison (Mode 10).

## Toolchain Files

- [`tools/zcc_sdf_compiler.py`](tools/zcc_sdf_compiler.py): Optimizing spatial compiler script.
- [`scratch/README.md`](scratch/README.md): Toolchain usage documentation.

## Emitted Asset Pack

- [`scratch/compiled_sdf_max.html`](scratch/compiled_sdf_max.html): Optimized WebGL2 raymarching viewer.
- [`scratch/compiled_sdf_max.primitives.json`](scratch/compiled_sdf_max.primitives.json): Reusable primitive database manifest.
- [`scratch/compiled_sdf_max.telemetry.json`](scratch/compiled_sdf_max.telemetry.json): Telemetry and benchmark log.

## Benchmark & Validation Baseline

```json
{
  "source": "fleet_lite.glb",
  "num_spheres": 64,
  "num_samples": 15000,
  "total_elapsed_seconds": 2.018022298812866,
  "parser": {
    "num_accessors_read": 38,
    "samples_requested": 15000,
    "samples_written": 14941,
    "read_calls": 100,
    "bytes_read": 179292,
    "avg_read_bytes": 1792
  },
  "passes": {
    "stream_parser_seconds": 0.4555530548095703,
    "kmeans_clustering_seconds": 0.6875457763671875,
    "pca_classification_seconds": 0.058724164962768555,
    "hierarchical_grouping_seconds": 0.0033288002014160156,
    "coarse_sdf_bake_seconds": 0.0050694942474365234,
    "residual_field_bake_seconds": 0.7527165412902832
  },
  "residual_validation": {
    "heldout_samples": 2989,
    "analytic_mean_abs": 0.10085051506757736,
    "analytic_p95_abs": 0.13532553613185883,
    "visual_mean_abs": 0.0023909190203994513,
    "visual_p95_abs": 0.014045601710677147,
    "visual_max_abs": 0.028964512050151825,
    "bound_violation_rate": 0.0,
    "max_bound_violation": 0.0,
    "p95_bound_margin": 0.030802201479673386
  }
}
```

## Known Limits

- The current renderer emits static GLSL; very high primitive counts may encounter WebGL2 shader-size or driver compile limits.
- The local error bounds provide empirical coverage certificates for the sampled point domain rather than formal analytical proofs.
- Textures are bound as discrete units; for `K > 256`, a WebGPU storage buffer format is recommended.
