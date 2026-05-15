from dataclasses import dataclass
from typing import Dict, List
from enum import Enum

class Severity(Enum):
    INFO = "INFO"
    WARNING = "WARNING"
    ERROR = "ERROR"
    CRITICAL = "CRITICAL"

@dataclass
class DiagnosticTip:
    severity: Severity
    message: str
    suggested_action: str
    priority: int  # Lower = higher priority

class ZkaediDiagnostics:
    """
    Legendary-grade diagnostic engine for the ZKAEDI pipeline.
    Analyzes quality reports and validation results, then returns
    prioritized, actionable intelligence.
    """

    def analyze(self, quality_report: Dict, validation_results: Dict) -> List[DiagnosticTip]:
        tips: List[DiagnosticTip] = []

        assignment_rate = quality_report.get("assignment_rate", 0.0)
        clamped_ratio = quality_report.get("clamped_ratio", 0.0)
        mean_sum_after = quality_report.get("mean_weight_sum_after", 1.0)
        negative_ratio = quality_report.get("negative_weight_ratio", 0.0)

        # === Critical Issues ===
        if assignment_rate < 0.4:
            tips.append(DiagnosticTip(
                severity=Severity.CRITICAL,
                message=f"Extremely low assignment rate ({assignment_rate*100:.1f}%)",
                suggested_action="Increase voxel search radius significantly or regenerate a denser Ghost Mesh.",
                priority=1
            ))

        if mean_sum_after < 0.6:
            tips.append(DiagnosticTip(
                severity=Severity.CRITICAL,
                message=f"Very poor barycentric quality (mean sum = {mean_sum_after:.3f})",
                suggested_action="The kernel is producing unreliable mappings. Review distance calculation and acceptance logic.",
                priority=2
            ))

        # === Major Issues ===
        if assignment_rate < 0.7:
            tips.append(DiagnosticTip(
                severity=Severity.ERROR,
                message=f"Low triangle assignment rate ({assignment_rate*100:.1f}%)",
                suggested_action="Expand search radius in query kernel or increase MAX_TRIS_PER_VOXEL.",
                priority=3
            ))

        if clamped_ratio > 0.25:
            tips.append(DiagnosticTip(
                severity=Severity.ERROR,
                message=f"High negative weight rate ({clamped_ratio*100:.1f}% needed clamping)",
                suggested_action="Tighten barycentric acceptance threshold in the Triton query kernel.",
                priority=4
            ))

        if negative_ratio > 0.2:
            tips.append(DiagnosticTip(
                severity=Severity.WARNING,
                message=f"Elevated negative barycentric ratio ({negative_ratio*100:.1f}%)",
                suggested_action="Consider improving the distance-to-center prefilter or using a more accurate point-to-triangle test.",
                priority=5
            ))

        # === Quality Signals ===
        if 0.7 <= assignment_rate < 0.9 and clamped_ratio < 0.15:
            tips.append(DiagnosticTip(
                severity=Severity.INFO,
                message="Mapping quality is acceptable but has room for improvement.",
                suggested_action="Fine-tune grid resolution or increase search radius slightly for better coverage.",
                priority=10
            ))

        if not tips:
            tips.append(DiagnosticTip(
                severity=Severity.INFO,
                message="Pipeline health looks strong.",
                suggested_action="No immediate action required.",
                priority=99
            ))

        # Sort by priority
        tips.sort(key=lambda x: x.priority)
        return tips

    def log_tips(self, tips: List[DiagnosticTip], logger):
        """Pretty print the diagnostic tips."""
        logger.info("=== ZKAEDI DIAGNOSTIC INTELLIGENCE ===")
        for tip in tips:
            prefix = f"[{tip.severity.value}]"
            logger.info(f"{prefix} {tip.message}")
            logger.info(f"    -> {tip.suggested_action}")
        logger.info("=====================================")
