// src/authorities/policy-authority.ts
import type { ProposedAction } from "../sovereignty/action-types";

export interface PolicyRule {
  id: string;
  domain: string;
  evaluate(action: ProposedAction): boolean;
}

export class PolicyAuthority {
  private activePolicies = new Map<string, PolicyRule>();

  constructor() {
    this.initializeCorePolicies();
  }

  public evaluateAction(action: ProposedAction): { compliant: boolean; violatedPolicyId?: string } {
    for (const [id, rule] of this.activePolicies.entries()) {
      if (rule.domain === action.domain) {
        if (!rule.evaluate(action)) {
          return { compliant: false, violatedPolicyId: id };
        }
      }
    }
    return { compliant: true };
  }

  private initializeCorePolicies(): void {
    // 1. Hardened Compiler Rule: Code generation optimizations must not skip verification sweeps
    this.activePolicies.set("POL_COMPILER_STABILITY", {
      id: "POL_COMPILER_STABILITY",
      domain: "compiler",
      evaluate: (action) => {
        return action.payload?.skipVerifications !== true;
      }
    });

    // 2. Hardened Security Rule: Credentials must not be logged or leaked
    this.activePolicies.set("POL_SECURITY_SECRET_PROTECTION", {
      id: "POL_SECURITY_SECRET_PROTECTION",
      domain: "security",
      evaluate: (action) => {
        if (action.payload?.containsRawCredentials !== undefined) {
          return action.payload.containsRawCredentials === false;
        }
        return true;
      }
    });
  }
}
