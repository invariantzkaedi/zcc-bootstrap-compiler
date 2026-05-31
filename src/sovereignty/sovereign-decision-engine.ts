// src/sovereignty/sovereign-decision-engine.ts
import { ConstitutionalAuthority } from "./constitutional-authority";
import { TrustAuthority } from "./trust-authority";
import { RiskModel } from "./risk-model";
import { IdentityAuthority } from "../directorates/identity-authority";
import { AuditAuthority } from "../directorates/audit-authority";
import type { ProposedAction, Decision } from "./action-types";

export class SovereignDecisionEngine {
  private constitutionalAuthority = new ConstitutionalAuthority();
  private trustAuthority = new TrustAuthority();
  private riskModel = new RiskModel();
  private identityAuthority = new IdentityAuthority();
  private auditAuthority = new AuditAuthority();
  
  private trustThreshold = 0.70; // 70% threshold required for critical actions

  constructor() {}

  public getIdentityAuthority(): IdentityAuthority {
    return this.identityAuthority;
  }

  public getAuditAuthority(): AuditAuthority {
    return this.auditAuthority;
  }

  public authorize(action: ProposedAction, signature?: string): Decision {
    const started = Date.now();

    // 1. Constitutional Authority Check
    const constitutionalCheck = this.constitutionalAuthority.evaluateAction(action);
    const hasConstitutionalPass = constitutionalCheck.pass;

    // 2. Trust / Cryptographic Identity Check
    let hasSignaturePass = true;
    if (signature) {
      hasSignaturePass = this.identityAuthority.verifySignature(action.proposedBy, action, signature);
    }
    const trustScore = this.trustAuthority.getTrustScore(action.proposedBy);
    const hasTrustPass = hasSignaturePass && (trustScore >= this.trustThreshold);

    // 3. Digital Twin Simulation Check
    const hasSimulationPass = this.runRealityMirrorSimulation(action);

    // 4. Risk Model Check
    const riskScore = this.riskModel.calculateRisk(action);
    const hasRiskPass = riskScore < 0.25;

    // 🔱 Core Sovereign Authorization Gate Enforcement
    const approved = hasConstitutionalPass && hasTrustPass && hasSimulationPass && hasRiskPass;

    // Build explanatory logs
    let reason = "Action approved and authorized by the Sovereign Decision Engine.";
    if (!approved) {
      const failures: string[] = [];
      if (!hasConstitutionalPass) {
        failures.push(`Constitutional failure: Rule '${constitutionalCheck.failedRuleId}' failed: ${constitutionalCheck.details}`);
      }
      if (!hasTrustPass) {
        failures.push(`Trust failure: Agent trust score ${trustScore.toFixed(2)} is beneath ${this.trustThreshold} or signature is invalid.`);
      }
      if (!hasSimulationPass) {
        failures.push(`Simulation failure: Projected future state crashed or drifted from baseline stability metrics.`);
      }
      if (!hasRiskPass) {
        failures.push(`Risk failure: Proposed action risk score ${riskScore.toFixed(2)} exceeds safety limit of 0.25.`);
      }
      reason = `Action rejected due to: ${failures.join(" | ")}`;
    }

    // Record outcome to agent reputation
    if (approved) {
      this.trustAuthority.adjustReputation(action.proposedBy, 0.02); // Increment trust slightly on success
    } else {
      this.trustAuthority.adjustReputation(action.proposedBy, -0.05); // Decrement trust on failure
    }

    const decision: Decision = {
      actionId: action.id,
      approved,
      timestamp: Date.now(),
      checks: {
        constitutional: hasConstitutionalPass,
        trust: hasTrustPass,
        simulation: hasSimulationPass,
        risk: hasRiskPass
      },
      metrics: {
        trustScore,
        riskScore,
        simulationDurationMs: Date.now() - started
      },
      reason
    };

    // Immutably append block to chained audit ledger
    this.auditAuthority.recordDecision(decision);

    return decision;
  }

  private runRealityMirrorSimulation(action: ProposedAction): boolean {
    // Simulates Temporal Twin scenarios (Past, Present, Future)
    // Critical checks: verify state boundaries
    if (action.payload?.projectedStateDrift !== undefined) {
      return action.payload.projectedStateDrift <= 0.05; // Reject if drift exceeds 5%
    }
    if (action.payload?.simulatedCpuSaturation === true) {
      return false; // Reject if digital twin predicts deadlock or loop saturation
    }
    return true;
  }
}

