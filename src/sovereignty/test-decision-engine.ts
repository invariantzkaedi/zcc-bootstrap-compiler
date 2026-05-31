// src/sovereignty/test-decision-engine.ts
import { SovereignDecisionEngine } from "./sovereign-decision-engine";
import type { ProposedAction } from "./action-types";

const engine = new SovereignDecisionEngine();

console.log("🔱 INITIALIZING SOVEREIGN DECISION ENGINE TESTING HARNESS");
console.log("---------------------------------------------------------");

// Case 1: Valid high-trust compiler adjustment proposed by the Compiler directorate
const action1: ProposedAction = {
  id: "act-101",
  ts: Date.now(),
  domain: "compiler",
  subsystem: "zcc-backend",
  action: "adjust_stack_alignment",
  proposedBy: "compiler-intelligence-directorate",
  payload: {
    alignmentPadBytes: 16,
    stageParityVerified: true,
    estimatedBlastRadius: 0.1
  }
};

const decision1 = engine.authorize(action1);
console.log(`\nTest Case 1: [compiler] Valid action by high-trust agent`);
console.log(`Approved: ${decision1.approved ? '✅ YES' : '❌ NO'}`);
console.log(`Reason: ${decision1.reason}`);
console.log(`Metrics: Trust: ${decision1.metrics.trustScore.toFixed(2)} | Risk: ${decision1.metrics.riskScore.toFixed(2)}`);

// Case 2: Invalid action violating constitutional ABI alignment
const action2: ProposedAction = {
  id: "act-102",
  ts: Date.now(),
  domain: "compiler",
  subsystem: "zcc-backend",
  action: "adjust_stack_alignment",
  proposedBy: "compiler-intelligence-directorate",
  payload: {
    alignmentPadBytes: 13, // Fails alignment check (13 is not divisible by 8)
    stageParityVerified: true,
    estimatedBlastRadius: 0.1
  }
};

const decision2 = engine.authorize(action2);
console.log(`\nTest Case 2: [compiler] ABI alignment rule violation`);
console.log(`Approved: ${decision2.approved ? '✅ YES' : '❌ NO'}`);
console.log(`Reason: ${decision2.reason}`);

// Case 3: Action proposed by low-trust/unknown agent
const action3: ProposedAction = {
  id: "act-103",
  ts: Date.now(),
  domain: "deployment",
  subsystem: "cloudflare-edge",
  action: "promote_canary",
  proposedBy: "unverified-temp-agent", // Low trust agent (defaults to 0.50, threshold is 0.70)
  payload: {
    hasSmokeTest: true,
    estimatedBlastRadius: 0.1
  }
};

const decision3 = engine.authorize(action3);
console.log(`\nTest Case 3: [deployment] Proposal by low-trust agent`);
console.log(`Approved: ${decision3.approved ? '✅ YES' : '❌ NO'}`);
console.log(`Reason: ${decision3.reason}`);

// Case 4: High-risk action exceeding safety thresholds
const action4: ProposedAction = {
  id: "act-104",
  ts: Date.now(),
  domain: "security",
  subsystem: "secrets-vault",
  action: "disable_vault_guards",
  proposedBy: "zkaedi-prime-orchestrator", // Highly trusted agent
  payload: {
    disablesSecurityPolicies: true, // Increases risk by 0.50
    estimatedBlastRadius: 0.4
  }
};

const decision4 = engine.authorize(action4);
console.log(`\nTest Case 4: [security] High-risk bypass proposed by highly trusted agent`);
console.log(`Approved: ${decision4.approved ? '✅ YES' : '❌ NO'}`);
console.log(`Reason: ${decision4.reason}`);
console.log(`Metrics: Trust: ${decision4.metrics.trustScore.toFixed(2)} | Risk: ${decision4.metrics.riskScore.toFixed(2)}`);

console.log("\n---------------------------------------------------------");
console.log("🔱 SOVEREIGN DECISION ENGINE TESTING HARNESS COMPLETED");
