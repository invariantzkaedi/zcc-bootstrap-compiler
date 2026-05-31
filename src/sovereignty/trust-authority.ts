// src/sovereignty/trust-authority.ts
import { createHmac } from "crypto";
import type { ProposedAction } from "./action-types";

export class TrustAuthority {
  private agentReputations = new Map<string, number>(); // ID -> score [0..1.0]
  private sharedSecret = "zkaedi-sovereign-trust-key"; // Secret for HMAC check

  constructor() {
    // Register canonical core agents with perfect initial trust
    this.agentReputations.set("zkaedi-prime-orchestrator", 1.0);
    this.agentReputations.set("compiler-intelligence-directorate", 1.0);
    this.agentReputations.set("recovery-commander", 1.0);
    this.agentReputations.set("meta-architect", 1.0);
  }

  public checkSignature(action: ProposedAction, signature: string): boolean {
    if (!signature) return false;
    try {
      const data = `${action.id}:${action.domain}:${action.subsystem}:${action.proposedBy}`;
      const hash = createHmac("sha256", this.sharedSecret)
        .update(data)
        .digest("hex");
      return hash === signature;
    } catch (e) {
      return false;
    }
  }

  public getTrustScore(agentId: string): number {
    if (!this.agentReputations.has(agentId)) {
      // Unrecognized agents start at a baseline score
      return 0.50;
    }
    return this.agentReputations.get(agentId) ?? 0.50;
  }

  public adjustReputation(agentId: string, delta: number): void {
    const current = this.getTrustScore(agentId);
    const updated = Math.max(0.0, Math.min(1.0, current + delta));
    this.agentReputations.set(agentId, updated);
  }

  public verifyTelemetryAuthenticity(subsystem: string, payload: Record<string, any>): boolean {
    // Simulates validating hardware-sealed integrity or cryptographic logs
    if (payload?.hardwareAttestationSignature) {
      return payload.hardwareAttestationSignature === "VERIFIED_HW_SEAL";
    }
    return true;
  }
}
