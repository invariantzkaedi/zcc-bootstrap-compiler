// src/directorates/runtime-coordinator.ts
import { createHash } from "crypto";
import type { ExecutionPermit } from "./execution-authority";

export class RuntimeCoordinator {
  private permitQueue: ExecutionPermit[] = [];
  private subsystemLocks = new Map<string, string>(); // subsystem -> permitId

  public enqueuePermit(permit: ExecutionPermit): void {
    if (permit.executed) {
      throw new Error(`Scheduling rejected: Permit '${permit.permitId}' has already been executed.`);
    }
    this.permitQueue.push(permit);
  }

  public acquireLock(subsystem: string, permitId: string): boolean {
    const activeLock = this.subsystemLocks.get(subsystem);
    if (activeLock && activeLock !== permitId) {
      return false; // Subsystem is locked by another running permit
    }
    this.subsystemLocks.set(subsystem, permitId);
    return true;
  }

  public releaseLock(subsystem: string, permitId: string): void {
    const activeLock = this.subsystemLocks.get(subsystem);
    if (activeLock === permitId) {
      this.subsystemLocks.delete(subsystem);
    }
  }

  public getQueueLength(): number {
    return this.permitQueue.length;
  }

  public getActiveLock(subsystem: string): string | undefined {
    return this.subsystemLocks.get(subsystem);
  }
}
