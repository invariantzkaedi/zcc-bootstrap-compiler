// src/directorates/observability-authority.ts
import type { Decision } from "../sovereignty/action-types";
import type { ExecutionPermit } from "./execution-authority";
import type { StateSnapshot } from "./rollback-authority";

export interface LineageTrace {
  decisionId: string;
  decisionHash: string;
  permitId: string;
  snapshotId?: string;
  subsystem: string;
  executedStatus: "pending" | "success" | "rolled_back";
  timestamp: number;
}

export class ObservabilityAuthority {
  private traces = new Map<string, LineageTrace>(); // permitId -> Trace

  public registerDecisionAndPermit(decision: Decision, permit: ExecutionPermit, subsystem: string): LineageTrace {
    const trace: LineageTrace = {
      decisionId: decision.actionId,
      decisionHash: permit.decisionHash,
      permitId: permit.permitId,
      subsystem,
      executedStatus: "pending",
      timestamp: Date.now()
    };
    this.traces.set(permit.permitId, trace);
    return trace;
  }

  public recordSnapshotBinding(permitId: string, snapshot: StateSnapshot): void {
    const trace = this.traces.get(permitId);
    if (trace) {
      trace.snapshotId = snapshot.snapshotId;
      this.traces.set(permitId, trace);
    }
  }

  public updateExecutionStatus(permitId: string, status: "success" | "rolled_back"): void {
    const trace = this.traces.get(permitId);
    if (trace) {
      trace.executedStatus = status;
      this.traces.set(permitId, trace);
    }
  }

  public getTrace(permitId: string): LineageTrace | undefined {
    return this.traces.get(permitId);
  }
}
