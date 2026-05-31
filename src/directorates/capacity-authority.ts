// src/directorates/capacity-authority.ts
import type { SystemMetrics } from "./health-authority";

export interface CapacityAlert {
  alertLevel: "warning" | "critical";
  resource: string;
  utilizationPercent: number;
}

export class CapacityAuthority {
  private memCapMb = 16384;
  private cpuCapPercent = 85.0;

  public auditEnvelopes(metrics: SystemMetrics): CapacityAlert[] {
    const alerts: CapacityAlert[] = [];

    const memUsage = (metrics.heapFootprintMb / this.memCapMb) * 100;
    if (memUsage > 90) {
      alerts.push({ alertLevel: "critical", resource: "memory", utilizationPercent: memUsage });
    } else if (memUsage > 75) {
      alerts.push({ alertLevel: "warning", resource: "memory", utilizationPercent: memUsage });
    }

    if (metrics.cpuPercent > this.cpuCapPercent) {
      alerts.push({ alertLevel: "critical", resource: "cpu", utilizationPercent: metrics.cpuPercent });
    } else if (metrics.cpuPercent > 70) {
      alerts.push({ alertLevel: "warning", resource: "cpu", utilizationPercent: metrics.cpuPercent });
    }

    return alerts;
  }
}
