// src/directorates/identity-authority.ts
import { createHmac } from "crypto";
import type { ProposedAction } from "../sovereignty/action-types";

export interface KeyRecord {
  secretKey: string;
  status: "active" | "rotated" | "revoked";
  assignedAt: number;
  revokedAt?: number;
}

export interface AgentIdentity {
  agentId: string;
  keys: KeyRecord[];
}

export class IdentityAuthority {
  private identities = new Map<string, AgentIdentity>();

  constructor() {
    // Register canonical core agents with initial active keys
    this.registerAgent("zkaedi-prime-orchestrator", "orchestrator-secret-key-1");
    this.registerAgent("compiler-intelligence-directorate", "compiler-secret-key-1");
    this.registerAgent("recovery-commander", "recovery-secret-key-1");
    this.registerAgent("meta-architect", "meta-secret-key-1");
    this.registerAgent("deployment-commander", "deployment-secret-key-1");
  }

  public registerAgent(agentId: string, secretKey: string): void {
    const keys: KeyRecord[] = [
      {
        secretKey,
        status: "active",
        assignedAt: Date.now()
      }
    ];
    this.identities.set(agentId, { agentId, keys });
  }

  public rotateKey(agentId: string, newSecretKey: string): { success: boolean; reason?: string } {
    const ident = this.identities.get(agentId);
    if (!ident) return { success: false, reason: `Agent '${agentId}' not registered.` };

    // Find and mark prior key as rotated
    let foundActive = false;
    for (const key of ident.keys) {
      if (key.status === "active") {
        key.status = "rotated";
        foundActive = true;
      }
    }

    ident.keys.push({
      secretKey: newSecretKey,
      status: "active",
      assignedAt: Date.now()
    });

    this.identities.set(agentId, ident);
    return { success: true };
  }

  public revokeAgent(agentId: string): { success: boolean; reason?: string } {
    const ident = this.identities.get(agentId);
    if (!ident) return { success: false, reason: `Agent '${agentId}' not registered.` };

    for (const key of ident.keys) {
      if (key.status === "active") {
        key.status = "revoked";
        key.revokedAt = Date.now();
      }
    }

    this.identities.set(agentId, ident);
    return { success: true };
  }

  public verifySignature(agentId: string, action: ProposedAction, signature: string): boolean {
    const ident = this.identities.get(agentId);
    if (!ident) return false;

    // Find active key
    const activeKey = ident.keys.find(k => k.status === "active");
    if (!activeKey) return false;

    try {
      const data = `${action.id}:${action.domain}:${action.subsystem}:${action.proposedBy}`;
      const hash = createHmac("sha256", activeKey.secretKey)
        .update(data)
        .digest("hex");
      return hash === signature;
    } catch (e) {
      return false;
    }
  }

  /**
   * Cryptographic helper to sign payload for valid agents (used to generate valid signatures in tests).
   */
  public generateSignatureForTest(agentId: string, action: ProposedAction): string {
    const ident = this.identities.get(agentId);
    if (!ident) throw new Error(`Agent '${agentId}' not registered.`);
    const activeKey = ident.keys.find(k => k.status === "active");
    if (!activeKey) throw new Error(`Agent '${agentId}' has no active keys.`);

    const data = `${action.id}:${action.domain}:${action.subsystem}:${action.proposedBy}`;
    return createHmac("sha256", activeKey.secretKey)
      .update(data)
      .digest("hex");
  }
}
