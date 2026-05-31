// src/directorates/anomaly-authority.ts
import type { SystemMetrics } from "./health-authority";

export interface AnomalyRecord {
  anomalyId: string;
  subsystem: string;
  anomalyType: "cpu_spike" | "memory_drift" | "unknown_behavior";
  severity: "low" | "medium" | "high";
  message: string;
}

export class AnomalyAuthority {
  private normalCpuMean = 45;
  private normalCpuStdDev = 15;

  public detectAnomalies(subsystem: string, metrics: SystemMetrics): AnomalyRecord[] {
    const anomalies: AnomalyRecord[] = [];

    // Simple statistical z-score modeling for CPU saturation
    const zScore = Math.abs(metrics.cpuPercent - this.normalCpuMean) / this.normalCpuStdDev;
    if (zScore > 3.0) {
      anomalies.push({
        anomalyId: `anom-${Math.random().toString(36).substring(2, 8)}`,
        subsystem,
        anomalyType: "cpu_spike",
        severity: metrics.cpuPercent > 90 ? "high" : "medium",
        message: `Statistical anomaly: CPU saturation ${metrics.cpuPercent}% is ${zScore.toFixed(1)} standard deviations from expected behavior.`
      });
    }

    return anomalies;
  }
}
