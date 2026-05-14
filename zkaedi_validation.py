import torch
from typing import Dict

def validate_zkaedi_pipeline(
    uvw: torch.Tensor,
    tri_idx: torch.Tensor,
    ghost_faces: torch.Tensor,
    ghost_verts: torch.Tensor = None,
    verbose: bool = True
) -> Dict[str, bool]:
    """
    Runs core validation invariants for the ZKAEDI pipeline.
    Returns a dict of results and prints a clean report if verbose=True.
    """
    results = {}
    N = uvw.shape[0]

    if verbose:
        print("\n[ZKAEDI Validation] Running pipeline invariants...")

    # === 1. Barycentric Partition of Unity ===
    weight_sum = uvw.sum(dim=1)
    sum_ok = torch.allclose(weight_sum, torch.ones_like(weight_sum), atol=1e-5)
    results["barycentric_sum_to_one"] = sum_ok
    if verbose:
        status = "✅ PASS" if sum_ok else "❌ FAIL"
        print(f"  Barycentric weights sum to 1.0     : {status}  (mean={weight_sum.mean():.6f})")

    # === 2. Non-negative Weights ===
    non_negative = (uvw >= 0).all().item()
    results["barycentric_non_negative"] = non_negative
    if verbose:
        status = "✅ PASS" if non_negative else "❌ FAIL"
        print(f"  Barycentric weights are non-negative: {status}")

    # === 3. Valid Triangle Indices ===
    valid_triangles = ((tri_idx >= 0) & (tri_idx < ghost_faces.shape[0])).all().item()
    results["valid_triangle_indices"] = valid_triangles
    if verbose:
        status = "✅ PASS" if valid_triangles else "❌ FAIL"
        print(f"  All triangle indices are valid     : {status}")

    # === 4. No NaN Values ===
    no_nans = not torch.isnan(uvw).any().item()
    results["no_nan_values"] = no_nans
    if verbose:
        status = "✅ PASS" if no_nans else "❌ FAIL"
        print(f"  No NaN values in barycentrics      : {status}")

    # === 5. Basic Ghost Mesh Sanity (optional but useful) ===
    if ghost_verts is not None:
        avg_neighbors = 6.0  # placeholder — replace with real adjacency check if available
        ghost_ok = ghost_verts.shape[0] > 100 and ghost_faces.shape[0] > 50
        results["ghost_mesh_sanity"] = ghost_ok
        if verbose:
            status = "✅ PASS" if ghost_ok else "⚠️  CHECK"
            print(f"  Ghost Mesh basic sanity            : {status}  "
                  f"(V={ghost_verts.shape[0]}, F={ghost_faces.shape[0]})")

    # === Final Summary ===
    all_passed = all(results.values())
    results["all_invariants_passed"] = all_passed

    if verbose:
        print("-" * 50)
        if all_passed:
            print("[ZKAEDI Validation] ✅ All core invariants passed. Safe to scale.")
        else:
            print("[ZKAEDI Validation] ❌ One or more invariants failed. Investigate before scaling.")
        print("-" * 50)

    return results


# ====================== USAGE EXAMPLE ======================
if __name__ == "__main__":
    pass
