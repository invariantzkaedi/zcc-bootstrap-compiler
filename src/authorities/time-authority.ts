// src/authorities/time-authority.ts
import type { UnifiedTelemetryPayload } from "../sovereignty/telemetry-schema";

export class TimeAuthority {
  private logicalClock = 0;
  private lastPhysicalTimestamp = 0;
  private maxAllowedDriftMs = 5000; // Drift ceiling between edge and local nodes

  public sequenceEvent(event: Omit<UnifiedTelemetryPayload, "id" | "ts">): UnifiedTelemetryPayload & { logicalSequence: number } {
    this.logicalClock++;
    const physicalTs = Date.now();
    this.lastPhysicalTimestamp = physicalTs;

    return {
      id: `${this.logicalClock}-${Math.random().toString(36).substring(2, 7)}`,
      ts: physicalTs,
      logicalSequence: this.logicalClock,
      ...event
    };
  }

  public detectClockDrift(nodeId: string, reportedTimestamp: number): { hasDrift: boolean; driftMs: number } {
    const current = Date.now();
    const driftMs = Math.abs(current - reportedTimestamp);
    return {
      hasDrift: driftMs > this.maxAllowedDriftMs,
      driftMs
    };
  }

  public reconstructTimeline(events: UnifiedTelemetryPayload[]): UnifiedTelemetryPayload[] {
    // Sort events contiguously by physical timestamp first, then by alphabetical ID as tie-breaker
    return [...events].sort((a, b) => {
      if (a.ts === b.ts) {
        return a.id.localeCompare(b.id);
      }
      return a.ts - b.ts;
    });
  }
}
