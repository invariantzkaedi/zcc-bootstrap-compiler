// src/sovereignty/risk-model.ts
import type { ProposedAction } from "./action-types";

export class RiskModel {
  public calculateRisk(action: ProposedAction): number {
    let risk = 0.05; // Baseline risk

    // 1. Evaluate by action domain complexity
    switch (action.domain) {
      case "security":
        risk += 0.05; // Security policy alterations are low-medium baseline risk
        break;
      case "compiler":
        risk += 0.15; // Direct codegen/compilation impacts have high baseline risk
        break;
      case "recovery":
        risk += 0.10; // Service remediation has medium-high baseline risk
        break;
      case "deployment":
        risk += 0.08; // Production release impacts have medium baseline risk
        break;
      case "optimization":
        risk += 0.03; // Performance tuning has low baseline risk
        break;
    }

    // 2. Evaluate specific hazardous factors in payloads
    if (action.payload?.affectsCoreRegistry === true) {
      risk += 0.20; // Structural modifications to agent registry
    }
    if (action.payload?.disablesSecurityPolicies === true) {
      risk += 0.50; // High risk safety bypasses
    }
    if (action.payload?.modifiesStackFrames === true) {
      risk += 0.30; // High risk ABI adjustments
    }
    if (action.payload?.skipVerifications === true) {
      risk += 0.40; // Skipping gates is extremely risky
    }
    if (action.payload?.estimatedBlastRadius !== undefined) {
      risk += action.payload.estimatedBlastRadius * 0.15; // Dynamic radius multiplier
    }

    return Math.min(1.0, risk);
  }
}
