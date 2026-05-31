// src/directorates/test-directorates.ts
import { DeploymentAuthority } from "./deployment-authority";
import { RecoveryAuthority } from "./recovery-authority";
import { ResourceAuthority } from "./resource-authority";
import { IdentityAuthority } from "./identity-authority";
import { AuditAuthority } from "./audit-authority";
import { ExecutionAuthority } from "./execution-authority";
import { RollbackAuthority } from "./rollback-authority";
import { HealthAuthority } from "./health-authority";
import { QuarantineAuthority } from "./quarantine-authority";
import { RuntimeCoordinator } from "./runtime-coordinator";
import { DependencyGraphAuthority } from "./dependency-graph";
import { WorkflowAuthority } from "./workflow-authority";
import { SimulationAuthority } from "./simulation-authority";
import { ObservabilityAuthority } from "./observability-authority";
import { LearningAuthority } from "./learning-authority";
import { ForecastAuthority } from "./forecast-authority";
import { PolicyEvolutionAuthority } from "./policy-evolution-authority";
import { CapacityAuthority } from "./capacity-authority";
import { ChaosAuthority } from "./chaos-authority";
import { PatternAuthority } from "./pattern-authority";
import { AnomalyAuthority } from "./anomaly-authority";
import { PredictionAuthority } from "./prediction-authority";
import { StrategyAuthority } from "./strategy-authority";
import { KnowledgeAuthority } from "./knowledge-authority";
import { SovereignDecisionEngine } from "../sovereignty/sovereign-decision-engine";
import type { ProposedAction } from "../sovereignty/action-types";
import * as fs from "fs";

const deployment = new DeploymentAuthority();
const recovery = new RecoveryAuthority();
const resource = new ResourceAuthority();

console.log("🔱 INITIALIZING TIER 1 SOVEREIGN OPERATIONAL & GOVERNANCE DIRECTORATES TEST SWEEP");
console.log("--------------------------------------------------------------------------------");

// 1. Test Deployment Authority
console.log("\n[1/5] Testing Deployment Rollout Boundaries & Canaries:");
const badDeployment: ProposedAction = {
  id: "dep-001",
  ts: Date.now(),
  domain: "deployment",
  subsystem: "cloudflare-edge",
  action: "canary_rollout",
  proposedBy: "deployment-commander",
  payload: {
    errorRate: 0.025, // 2.5% error rate (violates 1% limit)
    latencyMs: 120,
    smokeTestPassed: true
  }
};

const dCheck1 = deployment.evaluateRollout(badDeployment);
console.log(`- High Error Canary Rollout Check: Approved: ${dCheck1.approved ? "✅ YES" : "❌ NO: " + dCheck1.reason}`);

const goodDeployment: ProposedAction = {
  id: "dep-002",
  ts: Date.now(),
  domain: "deployment",
  subsystem: "cloudflare-edge",
  action: "canary_rollout",
  proposedBy: "deployment-commander",
  payload: {
    errorRate: 0.002, // 0.2% error rate
    latencyMs: 95,
    smokeTestPassed: true
  }
};

const dCheck2 = deployment.evaluateRollout(goodDeployment);
console.log(`- Stable Canary Rollout Check: Approved: ${dCheck2.approved ? "✅ YES" : "❌ NO"}`);


// 2. Test Recovery Authority
console.log("\n[2/5] Testing Recovery Cascade & Loop Protections:");
const mockRecoveryAction = (node: string): ProposedAction => ({
  id: `rec-${Math.random().toString(36).substring(2, 7)}`,
  ts: Date.now(),
  domain: "recovery",
  subsystem: node,
  action: "restart_service",
  proposedBy: "recovery-commander",
  payload: {}
});

// Run 3 recoveries for subsystem 'zcc-oracle'
const rec1 = recovery.evaluateRecovery(mockRecoveryAction("zcc-oracle"));
const rec2 = recovery.evaluateRecovery(mockRecoveryAction("zcc-oracle"));
const rec3 = recovery.evaluateRecovery(mockRecoveryAction("zcc-oracle"));
console.log(`- Recovery 1: Allowed: ${rec1.allowed ? "✅ YES" : "❌ NO"}`);
console.log(`- Recovery 2: Allowed: ${rec2.allowed ? "✅ YES" : "❌ NO"}`);
console.log(`- Recovery 3: Allowed: ${rec3.allowed ? "✅ YES" : "❌ NO"}`);

// Attempt 4th recovery for subsystem 'zcc-oracle' (violates 3 recoveries / 5-min threshold)
const rec4 = recovery.evaluateRecovery(mockRecoveryAction("zcc-oracle"));
console.log(`- Recovery 4 (Threshold Exceeded): Allowed: ${rec4.allowed ? "✅ YES" : "❌ NO: " + rec4.reason}`);


// 3. Test Resource Authority
console.log("\n[3/5] Testing Resource Budgets & Artifact Limits:");
const excessiveAction: ProposedAction = {
  id: "res-001",
  ts: Date.now(),
  domain: "optimization",
  subsystem: "zcc-opt",
  action: "optimize_loop",
  proposedBy: "evolution-commander",
  payload: {
    projectedMemoryMb: 20480, // 20GB memory (exceeds 16GB limit)
    projectedCpuPercent: 65,
    projectedBinSizeBytes: 5 * 1024 * 1024
  }
};

const rCheck1 = resource.auditResourceConsumption(excessiveAction);
console.log(`- Excessive Memory Footprint Audit: In-Budget: ${rCheck1.inBudget ? "✅ YES" : "❌ NO: " + rCheck1.details}`);

const compliantAction: ProposedAction = {
  id: "res-002",
  ts: Date.now(),
  domain: "optimization",
  subsystem: "zcc-opt",
  action: "optimize_loop",
  proposedBy: "evolution-commander",
  payload: {
    projectedMemoryMb: 4096, // 4GB
    projectedCpuPercent: 70, // 70% CPU
    projectedBinSizeBytes: 12 * 1024 * 1024 // 12MB binary
  }
};

const rCheck2 = resource.auditResourceConsumption(compliantAction);
console.log(`- Compliant Footprint Audit: In-Budget: ${rCheck2.inBudget ? "✅ YES" : "❌ NO"}`);


// 4. Test Identity Cryptographic Signatures, Rotations, & Revocations
console.log("\n[4/5] Testing Cryptographic Agent Identity Provenance:");
const identity = new IdentityAuthority();

const authAction: ProposedAction = {
  id: "act-401",
  ts: Date.now(),
  domain: "compiler",
  subsystem: "zcc-backend",
  action: "trigger_selfhost",
  proposedBy: "compiler-intelligence-directorate",
  payload: {
    stageParityVerified: true,
    alignmentPadBytes: 8
  }
};

// Generate valid signature with active key
const validSignature = identity.generateSignatureForTest("compiler-intelligence-directorate", authAction);
const vSigCheck = identity.verifySignature("compiler-intelligence-directorate", authAction, validSignature);
console.log(`- Valid Cryptographic Signature Verification: Passed: ${vSigCheck ? "✅ YES" : "❌ NO"}`);

// Verification fails with invalid/modified payload or wrong signature
const badSigCheck = identity.verifySignature("compiler-intelligence-directorate", authAction, "bad-sig-hash-xyz");
console.log(`- Invalid Cryptographic Signature Verification: Passed (Expected Failure): ${badSigCheck ? "❌ YES" : "✅ NO"}`);

// Key Rotation Sweep
console.log("\n  Rotating key for 'compiler-intelligence-directorate'...");
identity.rotateKey("compiler-intelligence-directorate", "new-compiler-secret-key-2");

// Signature signed with the prior key must now fail
const priorSigCheck = identity.verifySignature("compiler-intelligence-directorate", authAction, validSignature);
console.log(`- Verification using Rotated Prior Key (Expected Failure): Passed: ${priorSigCheck ? "❌ YES" : "✅ NO"}`);

// Signature signed with the newly active rotated key must pass
const rotatedSignature = identity.generateSignatureForTest("compiler-intelligence-directorate", authAction);
const rotatedSigCheck = identity.verifySignature("compiler-intelligence-directorate", authAction, rotatedSignature);
console.log(`- Verification using Active Rotated Key: Passed: ${rotatedSigCheck ? "✅ YES" : "❌ NO"}`);

// Agent Revocation Sweep
console.log("\n  Revoking 'compiler-intelligence-directorate'...");
identity.revokeAgent("compiler-intelligence-directorate");
const revokedSigCheck = identity.verifySignature("compiler-intelligence-directorate", authAction, rotatedSignature);
console.log(`- Verification under Revoked Agent Status (Expected Failure): Passed: ${revokedSigCheck ? "❌ YES" : "✅ NO"}`);


// 5. Test Sovereign Decision Engine Integrated Audit Ledger
console.log("\n[5/5] Testing Sovereign Decision Engine Integrated Chained Ledger:");
const decisionEngine = new SovereignDecisionEngine();
const engineIdentity = decisionEngine.getIdentityAuthority();
const audit = decisionEngine.getAuditAuthority();

const compilerAction: ProposedAction = {
  id: "act-501",
  ts: Date.now(),
  domain: "compiler",
  subsystem: "zcc-backend",
  action: "trigger_selfhost",
  proposedBy: "compiler-intelligence-directorate",
  payload: {
    stageParityVerified: true,
    alignmentPadBytes: 16,
    hasSmokeTest: true
  }
};

// Re-register active key since we revoked it in the isolated test above
engineIdentity.registerAgent("compiler-intelligence-directorate", "compiler-secret-key-1");
const cleanSig = engineIdentity.generateSignatureForTest("compiler-intelligence-directorate", compilerAction);

console.log("\n  Dispatching valid proposal to decision engine...");
const decision1 = decisionEngine.authorize(compilerAction, cleanSig);
console.log(`- Decision 1 Outcome: Approved: ${decision1.approved ? "✅ YES" : "❌ NO"}`);

// Reject Case
const failingAction: ProposedAction = {
  id: "act-502",
  ts: Date.now(),
  domain: "compiler",
  subsystem: "zcc-backend",
  action: "trigger_selfhost",
  proposedBy: "compiler-intelligence-directorate",
  payload: {
    stageParityVerified: false, // Violates ZCC_BOOTSTRAP_PARITY_GUARD
    alignmentPadBytes: 16,
    hasSmokeTest: true
  }
};
const failingSig = engineIdentity.generateSignatureForTest("compiler-intelligence-directorate", failingAction);

console.log("\n  Dispatching invalid proposal to decision engine...");
const decision2 = decisionEngine.authorize(failingAction, failingSig);
console.log(`- Decision 2 Outcome: Approved: ${decision2.approved ? "✅ YES" : "❌ NO: " + decision2.reason}`);

// Check that sovereign_audit_ledger.jsonl contains both records in a chained sequence
console.log("\n  Auditing ledger integrity and SHA-256 chained blocks...");
const ledgerPath = audit.getLedgerPath();
const ledgerContent = fs.readFileSync(ledgerPath, "utf-8").trim();
const lines = ledgerContent.split("\n").filter((l: string) => l !== "");
console.log(`- Chained blocks recorded in ledger on disk: ${lines.length}`);

for (let i = 0; i < lines.length; i++) {
  const block = JSON.parse(lines[i]);
  console.log(`  * Block ${block.index} [Hash: ${block.hash.substring(0, 16)}...] -> Prev: ${block.previousHash.substring(0, 16)}...`);
}

// Tamper Detection Verification
console.log("\n  Simulating audit ledger tampering...");
const corruptedContent = ledgerContent.replace("genesis-000", "genesis-tampered-000");
const tempLedgerPath = ledgerPath + ".temp";
fs.writeFileSync(tempLedgerPath, corruptedContent);

console.log("- Loading corrupted audit ledger (Expected to throw Integrity Error):");
try {
  // Back up the official ledger, copy the corrupted one in place
  fs.copyFileSync(ledgerPath, ledgerPath + ".bak");
  fs.writeFileSync(ledgerPath, corruptedContent);
  
  // Re-instantiate the AuditAuthority to trigger startup verification
  new AuditAuthority();
  console.log("❌ ERROR: Ledger loaded tampered contents without detection!");
} catch (e) {
  console.log(`✅ SUCCESS: Ledger verification successfully caught tampering: "${String(e).split("\n")[0]}"`);
} finally {
  // Restore the original correct ledger
  fs.copyFileSync(ledgerPath + ".bak", ledgerPath);
  fs.unlinkSync(ledgerPath + ".bak");
  fs.unlinkSync(tempLedgerPath);
}


// 6. Test Tier 2 Runtime Enforcement Plane
console.log("\n🔱 INITIALIZING TIER 2 SOVEREIGN RUNTIME ENFORCEMENT PLANE TEST SWEEP");
console.log("--------------------------------------------------------------------------------");

const executionAuth = new ExecutionAuthority();
const rollbackAuth = new RollbackAuthority();
const healthAuth = new HealthAuthority();
const quarantineAuth = new QuarantineAuthority();

console.log("\n[1/4] Testing Cryptographic Execution Permits & Replay Prevention:");
// Issue a permit for the approved decision1
const permit = executionAuth.issuePermit(decision1);
console.log(`- Execution Permit Issued: ID: '${permit.permitId}' | Decision Hash: '${permit.decisionHash.substring(0, 16)}...'`);

// Validate and consume the permit
const firstConsume = executionAuth.validateAndConsumePermit(permit.permitId);
console.log(`- Validate and Consume Permit (First Try): Valid: ${firstConsume.valid ? "✅ YES" : "❌ NO: " + firstConsume.reason}`);

// Try to consume the permit again (replay attack)
const secondConsume = executionAuth.validateAndConsumePermit(permit.permitId);
console.log(`- Replay Consumption Attempt (Expected Failure): Valid: ${secondConsume.valid ? "❌ YES" : "✅ NO: " + secondConsume.reason}`);

// Try to issue permit for rejected decision
try {
  executionAuth.issuePermit(decision2);
  console.log("❌ ERROR: Issued permit for an unapproved decision!");
} catch (e) {
  console.log(`- Issuance for Rejected Decision Blocked: Passed: ✅ YES ("${String(e).split("\n")[0]}")`);
}


console.log("\n[2/4] Testing State Checkpointing & Rollback Boundaries:");
const mockState = { configVersion: "v1.2.0", activeOptimizationFlags: ["-O2", "-fcg"] };
console.log(`  Initial Subsystem 'zcc-codegen' State: configVersion: ${mockState.configVersion}`);

// Create a snapshot before execution
const checkpoint = rollbackAuth.createCheckpoint("zcc-codegen", mockState);
console.log(`- Pre-Execution Checkpoint Saved: ID: '${checkpoint.snapshotId}'`);

// Simulate corrupted/mutated state change during execution failure
const mutatedState = { configVersion: "v1.3.0-failed-leak", activeOptimizationFlags: [] };
console.log(`  Mutated State during failure: configVersion: ${mutatedState.configVersion}`);

// Revert to checkpoint
const revertResult = rollbackAuth.revertToLastCheckpoint("zcc-codegen");
console.log(`- Revert Transaction: Reverted: ${revertResult.success ? "✅ YES" : "❌ NO"}`);
console.log(`  Restored State Data: configVersion: ${revertResult.revertedState?.configVersion}`);


console.log("\n[3/4] Testing Continuous Health Auditing & State Classification:");
// Evaluate healthy metrics
const health1 = healthAuth.evaluateSubsystemHealth("zcc-codegen", { cpuPercent: 45, errorRate: 0.001, heapFootprintMb: 2048 });
console.log(`- Healthy Metrics Audit: Computed Status: ${health1 === "healthy" ? "✅ healthy" : "❌ " + health1}`);

// Evaluate degraded metrics
const health2 = healthAuth.evaluateSubsystemHealth("zcc-codegen", { cpuPercent: 82, errorRate: 0.025, heapFootprintMb: 4096 });
console.log(`- Degraded Metrics Audit: Computed Status: ${health2 === "degraded" ? "⚠️ degraded" : "❌ " + health2}`);

// Evaluate failing metrics
const health3 = healthAuth.evaluateSubsystemHealth("zcc-codegen", { cpuPercent: 95, errorRate: 0.08, heapFootprintMb: 14000 });
console.log(`- Failing Metrics Audit: Computed Status: ${health3 === "failed" ? "❌ failed" : "❌ " + health3}`);


console.log("\n[4/4] Testing Quarantine Isolation & Containment Boundaries:");
// Quarantine the failed subsystem
quarantineAuth.quarantineSubsystem("zcc-codegen", "Continuous failing health states.");
console.log(`- Subsystem Quarantined check: IsQuarantined: ${quarantineAuth.isQuarantined("zcc-codegen") ? "✅ YES" : "❌ NO"}`);

// Attempt master registry mutation while quarantined
const mutationAttempt = quarantineAuth.authorizeStateMutation("zcc-codegen", "Update active optimizations");
console.log(`- Mutation Gating while Quarantined (Expected Rejection): Allowed: ${mutationAttempt.allowed ? "❌ YES" : "✅ NO: " + mutationAttempt.reason}`);

// Lift quarantine and check again
quarantineAuth.liftQuarantine("zcc-codegen");
const postMutationAttempt = quarantineAuth.authorizeStateMutation("zcc-codegen", "Update active optimizations");
console.log(`- Mutation Gating after Quarantine Lifted: Allowed: ${postMutationAttempt.allowed ? "✅ YES" : "❌ NO"}`);

console.log("\n--------------------------------------------------------------------------------");
console.log("🔱 TIER 2 SOVEREIGN RUNTIME ENFORCEMENT PLANE TEST SWEEP COMPLETED");


// 7. Test Tier 3 Orchestration Control Plane
console.log("\n🔱 INITIALIZING TIER 3 SOVEREIGN ORCHESTRATION CONTROL PLANE TEST SWEEP");
console.log("--------------------------------------------------------------------------------");

const coordinator = new RuntimeCoordinator();
const depGraph = new DependencyGraphAuthority();
const workflow = new WorkflowAuthority();
const simulation = new SimulationAuthority();
const observability = new ObservabilityAuthority();

console.log("\n[1/4] Testing Pre-Execution Simulation & Blast-Radius Calculation:");
const simulationResult = simulation.runSimulation(compilerAction);
console.log(`- Simulation Result: Safe: ${simulationResult.isSafe ? "✅ YES" : "❌ NO"} | Projected Memory: ${simulationResult.simulatedMemoryMb}MB | Blast Radius Score: ${simulationResult.blastRadiusScore.toFixed(2)}`);

console.log("\n[2/4] Testing Topological Dependency Graph & Health Propagation:");
// Setup topological graph: compiler -> oracle -> telemetry -> audit -> deployment
depGraph.addDependency("compiler", "oracle");
depGraph.addDependency("oracle", "telemetry");
depGraph.addDependency("telemetry", "audit");
depGraph.addDependency("audit", "deployment");

// Test integrity under healthy dependencies
const integrityCheck1 = depGraph.verifySubsystemIntegrity("compiler", healthAuth, quarantineAuth);
console.log(`- Transitive Dependency Health Check (Healthy Base): Safe: ${integrityCheck1.safe ? "✅ YES" : "❌ NO: " + integrityCheck1.reason}`);

// Quarantine 'telemetry' to simulate dependency failure propagation
console.log("\n  Simulating transitive dependency quarantine ('telemetry' quarantined)...");
quarantineAuth.quarantineSubsystem("telemetry", "Failure detected during data serialization.");

const integrityCheck2 = depGraph.verifySubsystemIntegrity("compiler", healthAuth, quarantineAuth);
console.log(`- Transitive Dependency Health Check (Quarantined Dependency): Safe: ${integrityCheck2.safe ? "❌ YES" : "✅ NO: " + integrityCheck2.reason}`);

// Lift quarantine
quarantineAuth.liftQuarantine("telemetry");


console.log("\n[3/4] Testing Deterministic Workflow DAG Orchestration:");
const wfid = "wf-success-001";
console.log(`  Initiating success flow: '${wfid}'...`);
workflow.startWorkflow(wfid, "success");

// State sequence: Authorize -> IssuePermit -> Snapshot -> Execute -> Validate -> Commit
workflow.transitionTo(wfid, "IssuePermit");
workflow.transitionTo(wfid, "Snapshot");
workflow.transitionTo(wfid, "Execute");
workflow.transitionTo(wfid, "Validate");
const finalTransition = workflow.transitionTo(wfid, "Commit");
const successState = workflow.getWorkflowState(wfid);

console.log(`- Success Flow Transitions completed: ${finalTransition.success ? "✅ YES" : "❌ NO"}`);
console.log(`  Active Workflow Path: ${successState?.path.join(" ──> ")}`);

// Rollback sequence flow
const wfid2 = "wf-rollback-001";
console.log(`\n  Initiating rollback flow: '${wfid2}'...`);
workflow.startWorkflow(wfid2, "rollback");

// State sequence: Authorize -> Snapshot -> Execute -> Failure -> Rollback -> Quarantine
workflow.transitionTo(wfid2, "Snapshot");
workflow.transitionTo(wfid2, "Execute");
workflow.transitionTo(wfid2, "Failure");
workflow.transitionTo(wfid2, "Rollback");
workflow.transitionTo(wfid2, "Quarantine");
const rollbackState = workflow.getWorkflowState(wfid2);
console.log(`- Rollback Flow Transitions completed: ${rollbackState?.currentStep === "Quarantine" ? "✅ YES" : "❌ NO"}`);
console.log(`  Active Workflow Path: ${rollbackState?.path.join(" ──> ")}`);


console.log("\n[4/4] Testing Trace Provenance Lineage Correlation:");
// Generate permit for the compilerAction Decision
const enginePermit = executionAuth.issuePermit(decision1);
const lineage = observability.registerDecisionAndPermit(decision1, enginePermit, "compiler");

// Bind snapshot
const mockSnapshot = rollbackAuth.createCheckpoint("compiler", { optimizationLevel: "-O3" });
observability.recordSnapshotBinding(enginePermit.permitId, mockSnapshot);

// Update status to success
observability.updateExecutionStatus(enginePermit.permitId, "success");

// Fetch and display correlated lineage graph trace
const trace = observability.getTrace(enginePermit.permitId);
console.log(`- Lineage Tracing Graph Correlated:`);
console.log(`  * Decision ID:   ${trace?.decisionId}`);
console.log(`  * Decision Hash: ${trace?.decisionHash.substring(0, 20)}...`);
console.log(`  * Permit ID:     ${trace?.permitId}`);
console.log(`  * Snapshot ID:   ${trace?.snapshotId}`);
console.log(`  * Status:        ${trace?.executedStatus === "success" ? "✅ success" : "❌ " + trace?.executedStatus}`);

console.log("\n--------------------------------------------------------------------------------");
console.log("🔱 TIER 3 SOVEREIGN ORCHESTRATION CONTROL PLANE TEST SWEEP COMPLETED");


// 8. Test Tier 4 Adaptive Governance
console.log("\n🔱 INITIALIZING TIER 4 SOVEREIGN ADAPTIVE GOVERNANCE TEST SWEEP");
console.log("--------------------------------------------------------------------------------");

const learning = new LearningAuthority();
const forecast = new ForecastAuthority();
const policyEvolution = new PolicyEvolutionAuthority();
const capacity = new CapacityAuthority();
const chaos = new ChaosAuthority();

console.log("\n[1/4] Testing Learning & Historical Outcome Tracking:");
learning.logOutcome(decision1, true);
learning.logOutcome(decision1, true);
learning.logOutcome(decision1, false);
console.log(`- Historical Success Rate for Authorized Agent: ${(learning.getHistoricalSuccessRate("authorized-agent") * 100).toFixed(1)}%`);

console.log("\n[2/4] Testing Dynamic Forecast Modeling & Saturation Limits:");
const mockMetricsHistory = [
  { cpuPercent: 40, errorRate: 0.001, heapFootprintMb: 2048 },
  { cpuPercent: 55, errorRate: 0.002, heapFootprintMb: 4096 } // Growth detected!
];
const projections = forecast.projectExhaustion(mockMetricsHistory);
projections.forEach(p => {
  console.log(`  * Resource: ${p.resource} | Exhaustion Probability: ${(p.exhaustionProbability * 100).toFixed(0)}% | Time to Saturation: ${p.secondsToSaturation.toFixed(0)}s`);
});

console.log("\n[3/4] Testing Policy Violation Audits & Evolution Suggestions:");
// Simulate multiple constitutional rejections
policyEvolution.auditViolations(decision2);
policyEvolution.auditViolations(decision2);
const refinements = policyEvolution.auditViolations(decision2); // Third violation!
console.log(`- Policy Evolution Suggestion: Count: ${refinements.length}`);
if (refinements.length > 0) {
  console.log(`  * Violations reached evolution ceiling. Recommended: "${refinements[0].recommendedAdjustment}"`);
}

console.log("\n[4/4] Testing Capacity Envelope Alerts & Chaos Drills:");
const capacityAlerts = capacity.auditEnvelopes({ cpuPercent: 78, errorRate: 0.01, heapFootprintMb: 15000 });
console.log(`- Capacity Envelope Warnings Emitted: ${capacityAlerts.length}`);
capacityAlerts.forEach(c => {
  console.log(`  * [${c.alertLevel.toUpperCase()}] ${c.resource} utilization at ${c.utilizationPercent.toFixed(1)}%`);
});

// Chaos Drill
const drill = chaos.triggerDrill("dependency_failure", healthAuth, quarantineAuth);
console.log(`- Chaos Drill Outcome: Drill: '${drill.drillType}' | Recovery State: '${drill.outcome}' | Remediation: "${drill.remediationPath}"`);
// Clean up quarantine after chaos drill
quarantineAuth.liftQuarantine("telemetry");


// 9. Test Tier 5 Cognitive Operations Layer
console.log("\n🔱 INITIALIZING TIER 5 SOVEREIGN COGNITIVE OPERATIONS TEST SWEEP");
console.log("--------------------------------------------------------------------------------");

const pattern = new PatternAuthority();
const anomaly = new AnomalyAuthority();
const predictor = new PredictionAuthority();
const strategy = new StrategyAuthority();
const knowledge = new KnowledgeAuthority();

console.log("\n[1/4] Testing Lineage Pattern Extraction & Operational Fingerprinting:");
const traceHistory = [
  observability.getTrace(enginePermit.permitId)!,
  observability.getTrace(enginePermit.permitId)!
];
const detectedPatterns = pattern.auditLineagePatterns(traceHistory);
console.log(`- Fingerprinted Patterns: Identified: ${detectedPatterns.length}`);
if (detectedPatterns.length > 0) {
  console.log(`  * Pattern ID: '${detectedPatterns[0].patternId}' | Description: "${detectedPatterns[0].description}"`);
}

console.log("\n[2/4] Testing Behavioral Anomaly Detection & Prediction Forecasting:");
const anomalies = anomaly.detectAnomalies("compiler", { cpuPercent: 95, errorRate: 0.08, heapFootprintMb: 14000 });
console.log(`- Behavioral Anomalies Identified: ${anomalies.length}`);
if (anomalies.length > 0) {
  console.log(`  * Warning: "${anomalies[0].message}"`);
}

// Prediction forecast before execution
const compileForecast = predictor.predictActionProbability(compilerAction);
console.log(`- Pre-Execution Prediction Estimate:`);
console.log(`  * Success Probability:  ${(compileForecast.successProbability * 100).toFixed(1)}%`);
console.log(`  * Rollback Probability: ${(compileForecast.rollbackProbability * 100).toFixed(1)}%`);

console.log("\n[3/4] Testing Strategic Plan Optimization (Plan-B Selection):");
const planA = { planId: "plan-a-high-optimization", expectedPerformanceGain: 8.0, estimatedRisk: 0.40, recommended: false };
const planB = { planId: "plan-b-resilient-base", expectedPerformanceGain: 5.5, estimatedRisk: 0.05, recommended: false };
const selectedPlan = strategy.selectOptimalPlan([planA, planB]);
console.log(`- Strategic Plan Approved: '${selectedPlan.planId}' (Expected Performance: +${selectedPlan.expectedPerformanceGain}% | Risk: ${selectedPlan.estimatedRisk})`);

console.log("\n[4/4] Testing Sovereign Knowledge Graph Memory:");
knowledge.addRelation("compiler", "oracle", "depends_on");
knowledge.addRelation("oracle", "telemetry", "depends_on");
knowledge.addRelation("compiler", "memory_pressure", "failed_due_to");
knowledge.addRelation("memory_pressure", "rollback_v1.2.0", "reverted_by");

console.log(`- Knowledge Graph Relationships Sealed: ${knowledge.getGraphEdgeCount()}`);
const compilerMemory = knowledge.getRelationsForNode("compiler");
compilerMemory.forEach(e => {
  console.log(`  * Semantic Memory: '${e.source}' ──[${e.relation}]──> '${e.target}'`);
});

console.log("\n--------------------------------------------------------------------------------");
console.log("🔱 TIER 5 SOVEREIGN COGNITIVE OPERATIONS TEST SWEEP COMPLETED");
