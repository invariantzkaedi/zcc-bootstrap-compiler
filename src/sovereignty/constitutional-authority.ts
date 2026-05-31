// src/sovereignty/constitutional-authority.ts
import type { ProposedAction } from "./action-types";

export interface ConstitutionalRule {
  id: string;
  domain: ProposedAction["domain"];
  priority: number;
  description: string;
  evaluate(action: ProposedAction): boolean;
}

export class ConstitutionalAuthority {
  private rules: ConstitutionalRule[] = [];

  constructor() {
    this.initializeDefaultRules();
  }

  public registerRule(rule: ConstitutionalRule): void {
    this.rules.push(rule);
  }

  public evaluateAction(action: ProposedAction): { pass: boolean; failedRuleId?: string; details?: string } {
    // Sort rules by priority (highest first)
    const activeRules = this.rules
      .filter(r => r.domain === action.domain)
      .sort((a, b) => b.priority - a.priority);

    for (const rule of activeRules) {
      try {
        const pass = rule.evaluate(action);
        if (!pass) {
          return { pass: false, failedRuleId: rule.id, details: rule.description };
        }
      } catch (e) {
        return { pass: false, failedRuleId: rule.id, details: `Rule evaluation crash: ${String(e)}` };
      }
    }

    return { pass: true };
  }

  private initializeDefaultRules(): void {
    // 1. ZCC Compiler ABI Guard
    this.registerRule({
      id: "ZCC_ABI_ALIGNMENT_GUARD",
      domain: "compiler",
      priority: 100,
      description: "Compilations must preserve 100% SystemV alignment constraints for structure limits.",
      evaluate: (action) => {
        if (action.payload?.alignmentPadBytes !== undefined) {
          return action.payload.alignmentPadBytes % 8 === 0;
        }
        return true;
      }
    });

    // 2. Self-Host Parity Guard
    this.registerRule({
      id: "ZCC_BOOTSTRAP_PARITY_GUARD",
      domain: "compiler",
      priority: 95,
      description: "Stage 2 and Stage 3 self-host assembly binaries must converge to bitwise parity.",
      evaluate: (action) => {
        if (action.payload?.stageParityVerified !== undefined) {
          return action.payload.stageParityVerified === true;
        }
        return true;
      }
    });

    // 3. Recovery Failure Cascade Prevention
    this.registerRule({
      id: "RECOVERY_LOOP_PREVENTION",
      domain: "recovery",
      priority: 90,
      description: "A single subsystem cannot trigger recovery actions more than 3 times in a 5-minute window.",
      evaluate: (action) => {
        if (action.payload?.recentRecoveryAttempts !== undefined) {
          return action.payload.recentRecoveryAttempts <= 3;
        }
        return true;
      }
    });

    // 4. Deployment Verification Standard
    this.registerRule({
      id: "DEPLOYMENT_SMOKE_TEST_REQUIRED",
      domain: "deployment",
      priority: 85,
      description: "Canary and production deployments must run automated verification smoke tests before promotion.",
      evaluate: (action) => {
        return action.payload?.hasSmokeTest === true;
      }
    });

    // 5. Optimization Performance Boundary
    this.registerRule({
      id: "OPTIMIZATION_LATENCY_CEILING",
      domain: "optimization",
      priority: 80,
      description: "Optimization passes must not introduce latency regressions greater than 10%.",
      evaluate: (action) => {
        if (action.payload?.predictedLatencyDrift !== undefined) {
          return action.payload.predictedLatencyDrift <= 0.10;
        }
        return true;
      }
    });
  }
}
