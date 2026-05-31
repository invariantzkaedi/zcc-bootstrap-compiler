// src/directorates/policy-evolution-authority.ts
import type { Decision } from "../sovereignty/action-types";

export interface PolicyRefinement {
  policyId: string;
  recommendedAdjustment: string;
  confidence: number;
}

export class PolicyEvolutionAuthority {
  private violationCounts = new Map<string, number>();

  public auditViolations(decision: Decision): PolicyRefinement[] {
    if (decision.approved) return [];

    const reason = decision.reason;
    let failedPolicy = "UNKNOWN_POLICY";

    if (reason.includes("ZCC_ABI_ALIGNMENT_GUARD")) failedPolicy = "ZCC_ABI_ALIGNMENT_GUARD";
    else if (reason.includes("ZCC_BOOTSTRAP_PARITY_GUARD")) failedPolicy = "ZCC_BOOTSTRAP_PARITY_GUARD";
    else if (reason.includes("RECOVERY_LOOP_PREVENTION")) failedPolicy = "RECOVERY_LOOP_PREVENTION";
    else if (reason.includes("DEPLOYMENT_SMOKE_TEST_REQUIRED")) failedPolicy = "DEPLOYMENT_SMOKE_TEST_REQUIRED";
    else if (reason.includes("OPTIMIZATION_LATENCY_CEILING")) failedPolicy = "OPTIMIZATION_LATENCY_CEILING";

    const count = (this.violationCounts.get(failedPolicy) ?? 0) + 1;
    this.violationCounts.set(failedPolicy, count);

    // If a policy is violated repeatedly, propose structural refinements
    if (count >= 3) {
      return [
        {
          policyId: failedPolicy,
          recommendedAdjustment: `Refine limits for '${failedPolicy}' to minimize false-positive compiler cascades.`,
          confidence: 0.85
        }
      ];
    }

    return [];
  }
}
