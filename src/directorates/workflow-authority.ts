// src/directorates/workflow-authority.ts

export type WorkflowStep =
  | "Authorize"
  | "IssuePermit"
  | "Snapshot"
  | "Execute"
  | "Validate"
  | "Commit"
  | "Failure"
  | "Rollback"
  | "Quarantine";

export class WorkflowAuthority {
  private activeWorkflows = new Map<string, { currentStep: WorkflowStep; flowType: "success" | "rollback"; path: WorkflowStep[] }>();

  public startWorkflow(workflowId: string, flowType: "success" | "rollback"): void {
    this.activeWorkflows.set(workflowId, {
      currentStep: "Authorize",
      flowType,
      path: ["Authorize"]
    });
  }

  public transitionTo(workflowId: string, nextStep: WorkflowStep): { success: boolean; reason?: string } {
    const wf = this.activeWorkflows.get(workflowId);
    if (!wf) {
      return { success: false, reason: `Workflow '${workflowId}' not found.` };
    }

    const current = wf.currentStep;
    const flow = wf.flowType;

    // Validate deterministic transitions
    let validTransition = false;

    if (flow === "success") {
      if (current === "Authorize" && nextStep === "IssuePermit") validTransition = true;
      else if (current === "IssuePermit" && nextStep === "Snapshot") validTransition = true;
      else if (current === "Snapshot" && nextStep === "Execute") validTransition = true;
      else if (current === "Execute" && nextStep === "Validate") validTransition = true;
      else if (current === "Validate" && nextStep === "Commit") validTransition = true;
    } else { // rollback flow
      if (current === "Authorize" && nextStep === "Snapshot") validTransition = true;
      else if (current === "Snapshot" && nextStep === "Execute") validTransition = true;
      else if (current === "Execute" && nextStep === "Failure") validTransition = true;
      else if (current === "Failure" && nextStep === "Rollback") validTransition = true;
      else if (current === "Rollback" && nextStep === "Quarantine") validTransition = true;
    }

    if (!validTransition) {
      return {
        success: false,
        reason: `Invalid transition mapping: Cannot move from '${current}' to '${nextStep}' in '${flow}' flow.`
      };
    }

    wf.currentStep = nextStep;
    wf.path.push(nextStep);
    this.activeWorkflows.set(workflowId, wf);

    return { success: true };
  }

  public getWorkflowState(workflowId: string): { currentStep: WorkflowStep; path: WorkflowStep[] } | undefined {
    const wf = this.activeWorkflows.get(workflowId);
    return wf ? { currentStep: wf.currentStep, path: wf.path } : undefined;
  }
}
