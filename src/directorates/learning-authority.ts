// src/directorates/learning-authority.ts
import type { Decision } from "../sovereignty/action-types";

export interface ExecutionResult {
  actionId: string;
  success: boolean;
  timestamp: number;
}

export class LearningAuthority {
  private executionHistory: ExecutionResult[] = [];
  private agentSuccessRate = new Map<string, { total: number; success: number }>();

  public logOutcome(decision: Decision, success: boolean): void {
    const agent = decision.metrics?.trustScore !== undefined ? "authorized-agent" : "unknown-agent"; // Placeholder fallback
    this.executionHistory.push({
      actionId: decision.actionId,
      success,
      timestamp: Date.now()
    });

    const stats = this.agentSuccessRate.get(agent) ?? { total: 0, success: 0 };
    stats.total++;
    if (success) stats.success++;
    this.agentSuccessRate.set(agent, stats);
  }

  public getHistoricalSuccessRate(agent: string): number {
    const stats = this.agentSuccessRate.get(agent);
    if (!stats || stats.total === 0) return 1.0; // Assume perfect baseline
    return stats.success / stats.total;
  }
}
