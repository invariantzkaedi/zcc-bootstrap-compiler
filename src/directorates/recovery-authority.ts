// src/directorates/recovery-authority.ts
import type { ProposedAction } from "../sovereignty/action-types";

export class RecoveryAuthority {
  private recoveryExecutionHistory = new Map<string, { timestamp: number; action: string }[]>();

  public evaluateRecovery(action: ProposedAction): { allowed: boolean; reason: string } {
    const nodeId = action.subsystem;
    const history = this.recoveryExecutionHistory.get(nodeId) ?? [];

    // Filter events in the last 5 minutes (300,000ms)
    const fiveMinutesAgo = Date.now() - 300000;
    const recentAttempts = history.filter(h => h.timestamp > fiveMinutesAgo);

    // Limit recovery to maximum 3 attempts per 5-minute window
    if (recentAttempts.length >= 3) {
      return {
        allowed: false,
        reason: `Recovery blocked: Subsystem '${nodeId}' has reached the limit of 3 recovery attempts in 5 minutes.`
      };
    }

    // Record this attempt
    history.push({ timestamp: Date.now(), action: action.action });
    this.recoveryExecutionHistory.set(nodeId, history);

    return {
      allowed: true,
      reason: `Recovery sequence approved for execution (Attempt ${history.length} overall).`
    };
  }
}
