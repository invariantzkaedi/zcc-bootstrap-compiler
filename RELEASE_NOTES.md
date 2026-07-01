# Release Notes — `zcc-sdf-compiler-v2.9-ubomb`

Tag: `v2.9-ubomb`  
Status: Release Candidate  
Benchmark environment: WSL2, RTX 5070 Blackwell target viewer, `fleet_lite.glb` 2.9 GB / 63M vertices.

We are pleased to release the **V2.9 Release Candidate** of the ZCC SDF Compiler:  
`zcc-sdf-compiler-v2.9-ubomb`.

## Headline

> ZCC SDF Compiler V2.9 converts a 2.9 GB / 63M-vertex GLB into an optimized WebGL2 SDF asset pack in ~1.3 seconds under WSL2, using <2 MB peak phase memory and only 100 stratified block reads.

## Highlights

- **Interactive compile latency**: ~1.3s end-to-end on `fleet_lite.glb`.
- **Low-memory execution**: all major phases remain under ~2 MB peak telemetry.
- **Stratified block-local GLB sampling**: reduced random vertex reads to 100 block reads.
- **Hybrid primitive fitting**: PCA-classified spheres, capsules, and oriented boxes.
- **Hierarchical SDF acceleration**: volume-weighted zones plus coarse 3D SDF texture.
- **Fast shader path**: `mapD()` distance-only GLSL pathway for ray steps, normals, shadows, and AO.
- **Debug viewport modes**: beauty, distance bands, coarse SDF, step heatmap, eikonal stress, AO-only, shadows-only.
- **Reusable spatial manifest**: emitted primitive database with normalization, group, primitive, and fit-confidence metadata.

## Toolchain Files

- [`scratch/zcc_sdf_compiler.py`](scratch/zcc_sdf_compiler.py): Optimizing spatial compiler script.
- [`scratch/README.md`](scratch/README.md): Toolchain usage documentation.

## Emitted Asset Pack

- [`scratch/compiled_sdf_max.html`](scratch/compiled_sdf_max.html): Optimized WebGL2 raymarching viewer.
- [`scratch/compiled_sdf_max.primitives.json`](scratch/compiled_sdf_max.primitives.json): Reusable primitive database manifest.
- [`scratch/compiled_sdf_max.telemetry.json`](scratch/compiled_sdf_max.telemetry.json): Telemetry and benchmark log.

## Benchmark Baseline

```json
{
  "source": "fleet_lite.glb",
  "num_spheres": 64,
  "num_samples": 15000,
  "total_elapsed_seconds": 1.2957208156585693,
  "parser": {
    "num_accessors_read": 38,
    "samples_requested": 15000,
    "samples_written": 14941,
    "read_calls": 100,
    "bytes_read": 179292,
    "avg_read_bytes": 1792
  },
  "passes": {
    "stream_parser_seconds": 0.4793407917022705,
    "kmeans_clustering_seconds": 0.7156777381896973,
    "pca_classification_seconds": 0.06052517890930176,
    "hierarchical_grouping_seconds": 0.003543853759765625,
    "coarse_sdf_bake_seconds": 0.0054628849029541016
  }
}
```

## Known Limits

- The current renderer emits static GLSL; very high primitive counts may encounter WebGL2 shader-size or driver compile limits.
- For `K > 256`, a UBO/WebGPU backend is recommended.
- The coarse 3D SDF texture is an acceleration field, not an exact reconstruction of the source mesh.
- Primitive fitting is approximate and optimized for compact real-time rendering, not CAD-accurate geometry recovery.

## Next Milestone: V3.0

Planned V3.0 direction:

```bash
python3 zcc_sdf_compiler.py asset.glb out.html --quality [draft | balanced | high | ultra]
```

Quality presets will control sample count, primitive count, KMeans strategy, adaptive refinement, and coarse SDF resolution.

---

This release establishes the ZCC SDF Compiler as a reusable spatial asset compiler rather than a one-off raymarching exporter.

