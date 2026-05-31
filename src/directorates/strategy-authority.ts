// src/directorates/strategy-authority.ts

export interface StrategyPlan {
  planId: string;
  expectedPerformanceGain: number; // Percent
  estimatedRisk: number; // 0 to 1.0
  recommended: boolean;
}

export class StrategyAuthority {
  public selectOptimalPlan(plans: StrategyPlan[]): StrategyPlan {
    // Strategy selection: chooses lower risk/acceptable performance over extreme risk
    let bestPlan = plans[0];
    let highestUtility = -999;

    for (const plan of plans) {
      // Simple utility function: Performance Gain - (Risk * 50)
      const utility = plan.expectedPerformanceGain - (plan.estimatedRisk * 50);
      if (utility > highestUtility) {
        highestUtility = utility;
        bestPlan = plan;
      }
    }

    // Set recommendation flag
    for (const plan of plans) {
      plan.recommended = (plan.planId === bestPlan.planId);
    }

    return bestPlan;
  }
}
