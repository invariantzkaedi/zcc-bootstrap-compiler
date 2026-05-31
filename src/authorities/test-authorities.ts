// src/authorities/test-authorities.ts
import { ProtocolAuthority } from "./protocol-authority";
import { StateAuthority } from "./state-authority";
import { TimeAuthority } from "./time-authority";
import { PolicyAuthority } from "./policy-authority";
import type { ProposedAction } from "../sovereignty/action-types";

const protocol = new ProtocolAuthority();
const state = new StateAuthority();
const time = new TimeAuthority();
const policy = new PolicyAuthority();

console.log("🔱 INITIALIZING TIER 0 SOVEREIGN INFRASTRUCTURE AUTHORITIES TEST SWEEP");
console.log("---------------------------------------------------------------------");

// 1. Test Protocol Authority
console.log("\n[1/4] Testing Protocol Schema Validation:");
const validPayload = {
  id: "evt-001",
  ts: Date.now(),
  domain: "zcc",
  subsystem: "resource-oracle",
  event: "zcc.oracle.event",
  severity: "info",
  metrics: { peakHeapMb: 10.0 }
};

const protoCheck1 = protocol.validatePayload(validPayload);
console.log(`- Valid Payload Check: ${protoCheck1.valid ? "✅ VALID" : "❌ INVALID: " + protoCheck1.error}`);

const invalidPayload = {
  id: "evt-002",
  ts: Date.now(),
  domain: "invalid-domain-xyz", // Unsupported domain
  subsystem: "test",
  event: "test.event",
  severity: "info"
};

const protoCheck2 = protocol.validatePayload(invalidPayload);
console.log(`- Invalid Payload Check: ${protoCheck2.valid ? "✅ VALID" : "❌ INVALID: " + protoCheck2.error}`);


// 2. Test State Authority
console.log("\n[2/4] Testing State Reconciliation & Consensus:");
const r1 = state.reconcileState("compiler-intelligence-directorate", "healthy", "kernel");
console.log(`- Initial healthy state reconciled: ${r1.reconciled ? "✅ YES" : "❌ NO"} (Target: ${r1.targetState})`);

// Reconcile a failed state
const r2 = state.reconcileState("compiler-intelligence-directorate", "failed", "lyapunov-stability-guardian", "Out of heap bounds memory corruption detected.");
console.log(`- Failed state override reconciled: ${r2.reconciled ? "✅ YES" : "❌ NO"} (Target: ${r2.targetState})`);

// Attempt a healthy override (must fail because it's in failed state)
const r3 = state.reconcileState("compiler-intelligence-directorate", "healthy", "rebalancer");
console.log(`- Healthy override rejected (in failed state): ${!r3.reconciled ? "✅ CORRECT" : "❌ WRONG"} (Reconciled Target: ${r3.targetState})`);


// 3. Test Time Authority
console.log("\n[3/4] Testing Time Sequencing & Temporal Drift:");
const baseEvent = {
  domain: "hamiltonian" as const,
  subsystem: "lyapunov-guardian",
  event: "stability.update",
  severity: "info" as const
};

const seq1 = time.sequenceEvent(baseEvent);
const seq2 = time.sequenceEvent(baseEvent);
console.log(`- Contiguous sequencing indexes: Event 1 logical sequence ID: ${seq1.logicalSequence} | Event 2 logical sequence ID: ${seq2.logicalSequence}`);

const currentPhysical = Date.now();
const driftResult = time.detectClockDrift("cloudflare-edge-east", currentPhysical - 6000); // 6 seconds drift (limit is 5 seconds)
console.log(`- Clock Drift Detection: Drifted: ${driftResult.hasDrift ? "⚠️ YES" : "✅ NO"} (Drift value: ${driftResult.driftMs}ms)`);


// 4. Test Policy Authority
console.log("\n[4/4] Testing Policy Compliance Auditing:");
const nonCompliantCompilerAction: ProposedAction = {
  id: "act-201",
  ts: Date.now(),
  domain: "compiler",
  subsystem: "codegen",
  action: "optimization_pass",
  proposedBy: "genetic-sweep",
  payload: {
    skipVerifications: true // Violated! Compiler stability policy forbids skipping verifications
  }
};

const policyCheck1 = policy.evaluateAction(nonCompliantCompilerAction);
console.log(`- Non-compliant Compiler Action: Compliant: ${policyCheck1.compliant ? "✅ YES" : "❌ NO (Violated Policy: " + policyCheck1.violatedPolicyId + ")"}`);

const compliantCompilerAction: ProposedAction = {
  id: "act-202",
  ts: Date.now(),
  domain: "compiler",
  subsystem: "codegen",
  action: "optimization_pass",
  proposedBy: "genetic-sweep",
  payload: {
    skipVerifications: false
  }
};

const policyCheck2 = policy.evaluateAction(compliantCompilerAction);
console.log(`- Compliant Compiler Action: Compliant: ${policyCheck2.compliant ? "✅ YES" : "❌ NO"}`);

console.log("\n---------------------------------------------------------------------");
console.log("🔱 TIER 0 SOVEREIGN INFRASTRUCTURE AUTHORITIES TEST SWEEP COMPLETED");
