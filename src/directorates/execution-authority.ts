// src/directorates/execution-authority.ts
import { createHash } from "crypto";
import type { Decision } from "../sovereignty/action-types";

export interface ExecutionPermit {
  permitId: string;
  decisionHash: string;
  issuedAt: number;
  expiresAt: number;
  executed: boolean;
}

export class ExecutionAuthority {
  private permits = new Map<string, ExecutionPermit>();
  private permitDurationMs = 600000; // 10 minutes SLA expiration window

  public issuePermit(decision: Decision): ExecutionPermit {
    if (!decision.approved) {
      throw new Error(`Permit issuance rejected: Decision ${decision.actionId} was not approved.`);
    }

    const decisionHash = createHash("sha256")
      .update(`${decision.actionId}:${decision.timestamp}:${decision.approved}`)
      .digest("hex");

    const permitId = `perm-${createHash("sha256")
      .update(`${decisionHash}:${Date.now()}:${Math.random()}`)
      .digest("hex")
      .substring(0, 12)}`;

    const issuedAt = Date.now();
    const expiresAt = issuedAt + this.permitDurationMs;

    const permit: ExecutionPermit = {
      permitId,
      decisionHash,
      issuedAt,
      expiresAt,
      executed: false
    };

    this.permits.set(permitId, permit);
    return permit;
  }

  public validateAndConsumePermit(permitId: string): { valid: boolean; reason?: string } {
    const permit = this.permits.get(permitId);
    if (!permit) {
      return { valid: false, reason: `Invalid permit: Permit ID '${permitId}' not found.` };
    }

    if (permit.executed) {
      return { valid: false, reason: `Replay attack detected: Permit '${permitId}' has already been executed.` };
    }

    if (Date.now() > permit.expiresAt) {
      return { valid: false, reason: `Permit expired: Current timestamp exceeds window limit by ${Date.now() - permit.expiresAt}ms.` };
    }

    // Mark as consumed to prevent replay attacks
    permit.executed = true;
    this.permits.set(permitId, permit);

    return { valid: true };
  }

  public getPermit(permitId: string): ExecutionPermit | undefined {
    return this.permits.get(permitId);
  }
}
