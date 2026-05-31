// src/directorates/rollback-authority.ts

export interface StateSnapshot {
  snapshotId: string;
  subsystem: string;
  timestamp: number;
  data: Record<string, any>;
}

export class RollbackAuthority {
  private snapshots = new Map<string, StateSnapshot[]>();
  private activeCheckpoints = new Map<string, string>(); // subsystem -> active snapshotId

  public createCheckpoint(subsystem: string, stateData: Record<string, any>): StateSnapshot {
    const snapshotId = `snap-${Math.random().toString(36).substring(2, 8)}`;
    const snapshot: StateSnapshot = {
      snapshotId,
      subsystem,
      timestamp: Date.now(),
      data: { ...stateData }
    };

    const history = this.snapshots.get(subsystem) ?? [];
    history.push(snapshot);
    this.snapshots.set(subsystem, history);

    this.activeCheckpoints.set(subsystem, snapshotId);
    return snapshot;
  }

  public revertToLastCheckpoint(subsystem: string): { success: boolean; revertedState?: Record<string, any>; reason?: string } {
    const history = this.snapshots.get(subsystem) ?? [];
    if (history.length === 0) {
      return { success: false, reason: `No state checkpoints found for subsystem '${subsystem}'.` };
    }

    // Retrieve the most recent snapshot (reversion target)
    const targetSnapshot = history[history.length - 1];
    
    // Simulates reverting state by returning the restored data
    this.activeCheckpoints.set(subsystem, targetSnapshot.snapshotId);

    return {
      success: true,
      revertedState: targetSnapshot.data,
      reason: `Successfully reverted subsystem '${subsystem}' to checkpoint '${targetSnapshot.snapshotId}' from ${new Date(targetSnapshot.timestamp).toISOString()}.`
    };
  }

  public getActiveCheckpoint(subsystem: string): string | undefined {
    return this.activeCheckpoints.get(subsystem);
  }

  public getSnapshotHistoryCount(subsystem: string): number {
    return (this.snapshots.get(subsystem) ?? []).length;
  }
}
