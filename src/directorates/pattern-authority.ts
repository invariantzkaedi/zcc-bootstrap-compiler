// src/directorates/pattern-authority.ts
import type { LineageTrace } from "./observability-authority";

export interface OperationalPattern {
  patternId: string;
  frequency: number;
  confidence: number;
  affectedSubsystems: string[];
  description: string;
}

export class PatternAuthority {
  private patterns = new Map<string, OperationalPattern>();

  public auditLineagePatterns(traces: LineageTrace[]): OperationalPattern[] {
    const matched: OperationalPattern[] = [];

    const compiledTraces = traces.filter(t => t.subsystem === "compiler");
    if (compiledTraces.length >= 2) {
      const p: OperationalPattern = {
        patternId: "pat-compiler-sequential",
        frequency: compiledTraces.length,
        confidence: 0.95,
        affectedSubsystems: ["compiler"],
        description: "Normal compilation pipeline with consecutive deterministic sweeps."
      };
      this.patterns.set(p.patternId, p);
      matched.push(p);
    }

    return matched;
  }

  public getPattern(patternId: string): OperationalPattern | undefined {
    return this.patterns.get(patternId);
  }
}
