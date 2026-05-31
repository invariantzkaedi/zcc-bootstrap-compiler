// src/directorates/resource-authority.ts
import type { ProposedAction } from "../sovereignty/action-types";

export class ResourceAuthority {
  // Budget boundaries
  private memoryLimitMb = 16384; // 16GB
  private cpuPercentLimit = 85.0; // 85% cpu cap
  private binSizeLimitBytes = 50 * 1024 * 1024; // 50MB ELF limit

  public auditResourceConsumption(action: ProposedAction): { inBudget: boolean; details?: string } {
    const memory = action.payload?.projectedMemoryMb ?? 0;
    const cpu = action.payload?.projectedCpuPercent ?? 0;
    const binSize = action.payload?.projectedBinSizeBytes ?? 0;

    if (memory > this.memoryLimitMb) {
      return {
        inBudget: false,
        details: `Memory budget exceeded: Projected ${memory}MB is over the limit of ${this.memoryLimitMb}MB.`
      };
    }

    if (cpu > this.cpuPercentLimit) {
      return {
        inBudget: false,
        details: `CPU budget exceeded: Projected ${cpu}% is over the safety ceiling of ${this.cpuPercentLimit}%.`
      };
    }

    if (binSize > this.binSizeLimitBytes) {
      return {
        inBudget: false,
        details: `ELF Size budget exceeded: Emitted binary size ${(binSize / 1024 / 1024).toFixed(2)}MB exceeds ${this.binSizeLimitBytes / 1024 / 1024}MB budget.`
      };
    }

    return { inBudget: true };
  }
}
