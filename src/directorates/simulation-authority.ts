// src/directorates/simulation-authority.ts
import type { ProposedAction } from "../sovereignty/action-types";

export interface SimulationResult {
  simulatedMemoryMb: number;
  simulatedCpuPercent: number;
  simulatedBinSizeBytes: number;
  blastRadiusScore: number; // 0 to 1.0
  isSafe: boolean;
}

export class SimulationAuthority {
  private memoryLimitMb = 16384;
  private cpuLimitPercent = 85.0;

  public runSimulation(action: ProposedAction): SimulationResult {
    // Project simulated reality based on proposed payload
    const memory = action.payload?.projectedMemoryMb ?? 2048;
    const cpu = action.payload?.projectedCpuPercent ?? 30;
    const binSize = action.payload?.projectedBinSizeBytes ?? 1024 * 1024;

    // Calculate blast radius dynamically
    let blastRadiusScore = 0.05; // Base risk weight
    if (memory > 8192) blastRadiusScore += 0.10;
    if (cpu > 60) blastRadiusScore += 0.10;
    if (action.domain === "security") blastRadiusScore += 0.20;

    const isSafe = memory <= this.memoryLimitMb && cpu <= this.cpuLimitPercent;

    return {
      simulatedMemoryMb: memory,
      simulatedCpuPercent: cpu,
      simulatedBinSizeBytes: binSize,
      blastRadiusScore,
      isSafe
    };
  }
}
