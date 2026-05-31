// src/authorities/protocol-authority.ts
import type { UnifiedTelemetryPayload } from "../sovereignty/telemetry-schema";

export class ProtocolAuthority {
  private activeVersion = "1.0.0";
  private supportedDomains = new Set(["zcc", "hamiltonian", "agent", "deployment", "recovery", "cloudflare", "dashboard"]);
  private requiredFields = ["id", "ts", "domain", "subsystem", "event", "severity"];

  public validatePayload(payload: any): { valid: boolean; error?: string } {
    if (!payload || typeof payload !== "object") {
      return { valid: false, error: "Payload must be a non-null object." };
    }

    // Check required fields
    for (const field of this.requiredFields) {
      if (payload[field] === undefined || payload[field] === null) {
        return { valid: false, error: `Protocol mismatch: Required field '${field}' is missing.` };
      }
    }

    // Validate domain
    if (!this.supportedDomains.has(payload.domain)) {
      return { valid: false, error: `Protocol mismatch: Domain '${payload.domain}' is not supported in version ${this.activeVersion}.` };
    }

    // Validate severity
    const validSeverities = ["debug", "info", "warn", "error", "critical"];
    if (!validSeverities.includes(payload.severity)) {
      return { valid: false, error: `Protocol mismatch: Severity level '${payload.severity}' is invalid.` };
    }

    // Validate version if supplied
    if (payload.payload?.protocolVersion && payload.payload.protocolVersion !== this.activeVersion) {
      return { valid: false, error: `Protocol drift: Version '${payload.payload.protocolVersion}' does not match '${this.activeVersion}'.` };
    }

    return { valid: true };
  }
}
