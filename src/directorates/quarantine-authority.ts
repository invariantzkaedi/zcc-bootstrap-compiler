// src/directorates/quarantine-authority.ts

export interface QuarantineRecord {
  subsystem: string;
  quarantinedAt: number;
  reason: string;
}

export class QuarantineAuthority {
  private quarantinedNodes = new Map<string, QuarantineRecord>();

  public quarantineSubsystem(subsystem: string, reason: string): QuarantineRecord {
    const record: QuarantineRecord = {
      subsystem,
      quarantinedAt: Date.now(),
      reason
    };
    this.quarantinedNodes.set(subsystem, record);
    console.warn(`[QUARANTINE-ALERT] Subsystem '${subsystem}' quarantined. Reason: ${reason}`);
    return record;
  }

  public liftQuarantine(subsystem: string): boolean {
    return this.quarantinedNodes.delete(subsystem);
  }

  public isQuarantined(subsystem: string): boolean {
    return this.quarantinedNodes.has(subsystem);
  }

  public authorizeStateMutation(subsystem: string, mutationDescription: string): { allowed: boolean; reason?: string } {
    if (this.isQuarantined(subsystem)) {
      return {
        allowed: false,
        reason: `State mutation rejected: Subsystem '${subsystem}' is currently quarantined and isolated from the core registry.`
      };
    }
    return { allowed: true };
  }

  public getQuarantinedSubsystems(): string[] {
    return Array.from(this.quarantinedNodes.keys());
  }
}
