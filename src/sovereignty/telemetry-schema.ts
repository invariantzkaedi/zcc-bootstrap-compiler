// src/sovereignty/telemetry-schema.ts

export type DomainType = 
  | "zcc" 
  | "hamiltonian" 
  | "agent" 
  | "deployment" 
  | "recovery" 
  | "cloudflare" 
  | "dashboard";

export type SeverityType = "debug" | "info" | "warn" | "error" | "critical";

export interface UnifiedTelemetryPayload {
  // Common Structural Fields
  id: string;
  ts: number;
  domain: DomainType;
  subsystem: string;
  event: string;
  severity: SeverityType;
  traceId?: string;

  // Numerical Telemetry Metrics
  metrics?: {
    // Compiler specific metrics
    peakHeapMb?: number;
    allocationsCount?: number;
    textBytes?: number;
    relocationCount?: number;
    paddingBytes?: number;
    preprocessorRisk?: number;
    
    // System & Network metrics
    cpuPercent?: number;
    memoryMb?: number;
    latencyMs?: number;
    throughputDps?: number;
    
    // Stability specific metrics
    energy?: number;
    entropy?: number;
    lyapunovExponent?: number;
  };

  // Structured Metadata Labels
  labels?: Record<string, string>;

  // Deep Arbitrary Payloads
  payload?: Record<string, any>;
}
