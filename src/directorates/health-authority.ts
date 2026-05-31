// src/directorates/health-authority.ts

export type HealthStatus = "healthy" | "degraded" | "failed" | "quarantined";

export interface SystemMetrics {
  cpuPercent: number;
  errorRate: number;
  heapFootprintMb: number;
}

export class HealthAuthority {
  private statusRegistry = new Map<string, HealthStatus>();
  private metricsHistory = new Map<string, SystemMetrics[]>();

  public evaluateSubsystemHealth(subsystem: string, metrics: SystemMetrics): HealthStatus {
    const history = this.metricsHistory.get(subsystem) ?? [];
    history.push(metrics);
    this.metricsHistory.set(subsystem, history);

    // Current status if forced to quarantine
    const currentStatus = this.statusRegistry.get(subsystem);
    if (currentStatus === "quarantined") {
      return "quarantined";
    }

    let status: HealthStatus = "healthy";

    if (metrics.errorRate > 0.05 || metrics.cpuPercent > 90 || metrics.heapFootprintMb > 12000) {
      status = "failed";
    } else if (metrics.errorRate > 0.02 || metrics.cpuPercent > 80) {
      status = "degraded";
    }

    this.statusRegistry.set(subsystem, status);
    return status;
  }

  public getSubsystemStatus(subsystem: string): HealthStatus {
    return this.statusRegistry.get(subsystem) ?? "healthy";
  }

  public setQuarantined(subsystem: string): void {
    this.statusRegistry.set(subsystem, "quarantined");
  }
}
