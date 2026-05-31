// src/sovereignty/action-types.ts

export type ActionDomain = "deployment" | "recovery" | "security" | "compiler" | "optimization";

export interface ProposedAction {
  id: string;
  ts: number;
  domain: ActionDomain;
  subsystem: string;
  action: string;
  proposedBy: string;
  payload: Record<string, any>;
}

export interface Decision {
  actionId: string;
  approved: boolean;
  timestamp: number;
  checks: {
    constitutional: boolean;
    trust: boolean;
    simulation: boolean;
    risk: boolean;
  };
  metrics: {
    trustScore: number;
    riskScore: number;
    simulationDurationMs: number;
  };
  reason: string;
}
