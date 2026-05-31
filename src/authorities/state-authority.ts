// src/authorities/state-authority.ts

export class StateAuthority {
  private activeState = new Map<string, { health: string; timestamp: number; details?: string }>();

  public reconcileState(nodeId: string, newState: string, reporterId: string, details?: string): { reconciled: boolean; targetState: string } {
    const current = this.activeState.get(nodeId);
    
    // Default reconciliation logic (failures override degradation, degradation overrides healthy)
    if (!current) {
      this.activeState.set(nodeId, { health: newState, timestamp: Date.now(), details });
      return { reconciled: true, targetState: newState };
    }

    let target = newState;
    if (current.health === "failed") {
      target = "failed"; // Keep failure until recovered
    } else if (current.health === "degraded" && newState === "healthy") {
      target = "degraded"; // Require explicit recovery trigger
    }

    this.activeState.set(nodeId, { health: target, timestamp: Date.now(), details });
    return { reconciled: target === newState, targetState: target };
  }

  public getSnapshot(): Record<string, any> {
    const snapshot: Record<string, any> = {};
    for (const [key, val] of this.activeState.entries()) {
      snapshot[key] = { ...val };
    }
    return snapshot;
  }
}
