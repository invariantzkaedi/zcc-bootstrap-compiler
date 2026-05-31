// src/directorates/forecast-authority.ts
import type { SystemMetrics } from "./health-authority";

export interface CapacityForecast {
  resource: "cpu" | "memory" | "binarySize";
  exhaustionProbability: number; // 0 to 1.0
  secondsToSaturation: number;
}

export class ForecastAuthority {
  public projectExhaustion(history: SystemMetrics[]): CapacityForecast[] {
    if (history.length < 2) {
      return [
        { resource: "memory", exhaustionProbability: 0.0, secondsToSaturation: 99999 },
        { resource: "cpu", exhaustionProbability: 0.0, secondsToSaturation: 99999 }
      ];
    }

    const last = history[history.length - 1];
    const prev = history[history.length - 2];

    const memGrowth = last.heapFootprintMb - prev.heapFootprintMb;
    const cpuGrowth = last.cpuPercent - prev.cpuPercent;

    const memoryForecast: CapacityForecast = {
      resource: "memory",
      exhaustionProbability: memGrowth > 1000 ? 0.85 : 0.05,
      secondsToSaturation: memGrowth > 0 ? (16384 - last.heapFootprintMb) / memGrowth : 99999
    };

    const cpuForecast: CapacityForecast = {
      resource: "cpu",
      exhaustionProbability: cpuGrowth > 20 ? 0.70 : 0.10,
      secondsToSaturation: cpuGrowth > 0 ? (85 - last.cpuPercent) / cpuGrowth : 99999
    };

    return [memoryForecast, cpuForecast];
  }
}
