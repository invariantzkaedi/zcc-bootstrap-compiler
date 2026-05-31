// src/directorates/dependency-graph.ts
import type { HealthAuthority } from "./health-authority";
import type { QuarantineAuthority } from "./quarantine-authority";

export class DependencyGraphAuthority {
  private adjacencyList = new Map<string, string[]>(); // node -> dependencies

  public addDependency(node: string, dependency: string): void {
    const list = this.adjacencyList.get(node) ?? [];
    if (!list.includes(dependency)) {
      list.push(dependency);
      this.adjacencyList.set(node, list);
    }
  }

  public getDependencies(node: string): string[] {
    return this.adjacencyList.get(node) ?? [];
  }

  public verifySubsystemIntegrity(
    node: string,
    healthAuth: HealthAuthority,
    quarantineAuth: QuarantineAuthority
  ): { safe: boolean; reason?: string } {
    // 1. Check direct node quarantine
    if (quarantineAuth.isQuarantined(node)) {
      return { safe: false, reason: `Subsystem '${node}' integrity check failed: Target node is quarantined.` };
    }

    // 2. Transitive dependency resolution (DFS)
    const visited = new Set<string>();
    const stack = [node];

    while (stack.length > 0) {
      const current = stack.pop()!;
      if (visited.has(current)) continue;
      visited.add(current);

      // Check current node dependency health states
      if (current !== node) {
        if (quarantineAuth.isQuarantined(current)) {
          return {
            safe: false,
            reason: `Dependency boundary violation: Subsystem '${node}' depends on quarantined node '${current}'.`
          };
        }

        const status = healthAuth.getSubsystemStatus(current);
        if (status === "failed") {
          return {
            safe: false,
            reason: `Dependency boundary violation: Subsystem '${node}' depends on failed node '${current}'.`
          };
        }
      }

      const dependencies = this.getDependencies(current);
      for (const dep of dependencies) {
        if (!visited.has(dep)) {
          stack.push(dep);
        }
      }
    }

    return { safe: true };
  }
}
