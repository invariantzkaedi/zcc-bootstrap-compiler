import torch
from typing import Dict, Tuple, Optional

def normalize_barycentric_weights(
    uvw: torch.Tensor,
    tri_idx: torch.Tensor,
    clamp_negative: bool = True,
    eps: float = 1e-8,
    return_report: bool = False
) -> Tuple[torch.Tensor, torch.Tensor, Optional[Dict]]:
    """
    Post-process barycentric coordinates with optional quality reporting.
    Leaves invalid assignments (tri_idx == -1) untouched.
    """
    valid_mask = tri_idx >= 0
    assigned_count = valid_mask.sum().item()
    total_count = tri_idx.shape[0]

    if assigned_count == 0:
        if return_report:
            return uvw, tri_idx, {
                "assignment_rate": 0.0,
                "clamped_vertices": 0,
                "clamped_ratio": 0.0,
                "mean_weight_sum_before": 0.0,
                "mean_weight_sum_after": 0.0,
                "negative_weight_ratio": 0.0
            }
        return uvw, tri_idx, None

    valid_uvw = uvw[valid_mask].clone()
    weight_sum_before = valid_uvw.sum(dim=1)

    # Count how many vertices had negative weights
    negative_mask = (valid_uvw < 0).any(dim=1)
    clamped_count = negative_mask.sum().item() if clamp_negative else 0

    if clamp_negative:
        valid_uvw = torch.clamp(valid_uvw, min=0.0)

    # Renormalize
    weight_sum = valid_uvw.sum(dim=1, keepdim=True)
    normalized = valid_uvw / (weight_sum + eps)

    # Write results back
    uvw_out = uvw.clone()
    uvw_out[valid_mask] = normalized

    weight_sum_after = normalized.sum(dim=1)

    if return_report:
        report = {
            "total_vertices": total_count,
            "assigned_vertices": assigned_count,
            "assignment_rate": round(assigned_count / total_count, 4),
            "clamped_vertices": clamped_count,
            "clamped_ratio": round(clamped_count / assigned_count, 4) if assigned_count > 0 else 0.0,
            "mean_weight_sum_before": round(weight_sum_before.mean().item(), 4),
            "mean_weight_sum_after": round(weight_sum_after.mean().item(), 4),
            "negative_weight_ratio": round(negative_mask.float().mean().item(), 4),
        }
        return uvw_out, tri_idx, report

    return uvw_out, tri_idx, None
