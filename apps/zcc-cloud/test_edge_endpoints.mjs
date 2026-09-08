// apps/zcc-cloud/test_edge_endpoints.mjs
// Comprehensive verification harness for all 8 review fixes

import * as auth from "./functions/api/_auth.js";
import * as keysHandler from "./functions/api/keys.js";
import * as compileHandler from "./functions/api/compile.js";
import * as optimizeHandler from "./functions/api/optimize.js";
import * as verifyHandler from "./functions/api/verify.js";
import * as analyzeHandler from "./functions/api/analyze.js";
import * as astHandler from "./functions/api/ast.js";

function mockRequest(method, url, headers = {}, body = null) {
  return new Request(url, {
    method,
    headers: new Headers(headers),
    body: body ? (typeof body === "string" ? body : JSON.stringify(body)) : null
  });
}

async function runTests() {
  console.log("=================================================================");
  console.log("  ZCC CLOUD API & CODE REVIEW FIX HARNESS");
  console.log("=================================================================\n");

  let passed = 0;
  let total = 0;

  function assert(cond, name) {
    total++;
    if (cond) {
      console.log(`  [PASS] ${name}`);
      passed++;
    } else {
      console.error(`  [FAIL] ${name}`);
      process.exitCode = 1;
    }
  }

  // TEST 1: POST /api/keys generates HMAC signed key
  console.log("Test 1: Cryptographic API Key Generation & Signing");
  const postKeyReq = mockRequest("POST", "https://zkaedi.ai/api/keys", { "Content-Type": "application/json" }, { email: "zk@test.io", tier: "enterprise" });
  const postKeyRes = await keysHandler.onRequestPost({ request: postKeyReq, env: {} });
  const keyData = await postKeyRes.json();
  assert(postKeyRes.status === 201, "POST /api/keys returns 201 Created");
  assert(keyData.success === true, "Key response success = true");
  assert(keyData.api_key.startsWith("zk_live_"), "Key has zk_live_ prefix");
  const parts = keyData.api_key.split("_");
  assert(parts.length === 4, "Key contains zk, live, payloadHex, and 64-char HMAC signature");
  assert(parts[3].length === 64, "HMAC signature is exactly 64 hex characters (SHA-256)");
  const validKey = keyData.api_key;

  // TEST 2: GET /api/keys rejects prefix-only forged key
  console.log("\nTest 2: Rejection of Prefix-Only and Forged Keys");
  const forgedKeyReq = mockRequest("GET", "https://zkaedi.ai/api/keys", { "Authorization": "Bearer zk_live_forged_key_without_signature" });
  const forgedRes = await keysHandler.onRequestGet({ request: forgedKeyReq, env: {} });
  assert(forgedRes.status === 403, "GET /api/keys with prefix-only spoof key returns 403 Forbidden");
  const forgedData = await forgedRes.json();
  assert(forgedData.success === false, "Forged key success = false");

  // TEST 3: GET /api/keys accepts authentic key
  console.log("\nTest 3: Authentication of Cryptographically Signed Key");
  const validKeyReq = mockRequest("GET", "https://zkaedi.ai/api/keys", { "Authorization": `Bearer ${validKey}` });
  const validRes = await keysHandler.onRequestGet({ request: validKeyReq, env: {} });
  assert(validRes.status === 200, "GET /api/keys with authentic key returns 200 OK");
  const validData = await validRes.json();
  assert(validData.status === "ACTIVE", "Authentic key status = ACTIVE");
  assert(validData.developer_email === "zk@test.io", "Developer email preserved in key payload");

  // TEST 4: POST /api/compile with 'return 42' emits correct 'mov eax, 42'
  console.log("\nTest 4: Semantic Expression & Return 42 Codegen");
  const compileReq = mockRequest("POST", "https://zkaedi.ai/api/compile", {
    "Authorization": `Bearer ${validKey}`,
    "Content-Type": "application/json"
  }, {
    source: "int main() { return 42; }",
    target: "x86_64",
    prove_zk: true
  });
  const compileRes = await compileHandler.onRequestPost({ request: compileReq, env: {} });
  assert(compileRes.status === 200, "POST /api/compile returns 200 OK");
  const compileData = await compileRes.json();
  assert(compileData.assembly.includes("mov\teax, 42"), "Assembly emits 'mov eax, 42' (NOT xor eax, eax)");
  assert(!compileData.assembly.includes("xor\teax, eax\n\tpop\trbp\n\tret"), "Assembly does not return 0 for return 42");
  assert(Boolean(compileData.ast_commitment), "AST commitment is generated");
  const generatedCommitment = compileData.ast_commitment;

  // TEST 4b: RISC-V compilation of return 42 emits 'li a0, 42'
  const compileRiscVReq = mockRequest("POST", "https://zkaedi.ai/api/compile", {
    "Authorization": `Bearer ${validKey}`,
    "Content-Type": "application/json"
  }, {
    source: "int main() { return 42; }",
    target: "riscv64"
  });
  const compileRiscVRes = await compileHandler.onRequestPost({ request: compileRiscVReq, env: {} });
  const riscvData = await compileRiscVRes.json();
  assert(riscvData.assembly.includes("li\ta0, 42"), "RISC-V emits 'li a0, 42'");

  // TEST 5: POST /api/optimize semantic optimization (no hardcoded x*x+1)
  console.log("\nTest 5: Semantic Optimizer (Elimination of Hardcoded x*x+1)");
  const optReq = mockRequest("POST", "https://zkaedi.ai/api/optimize", {
    "Authorization": `Bearer ${validKey}`,
    "Content-Type": "application/json"
  }, {
    source: "int main() { int dead = 99; return 40 + 2; }",
    target: "x86_64",
    opt_level: "O2"
  });
  const optRes = await optimizeHandler.onRequestPost({ request: optReq, env: {} });
  assert(optRes.status === 200, "POST /api/optimize returns 200 OK");
  const optData = await optRes.json();
  assert(!optData.optimized_ir.includes("mul i32 %v1, %v1"), "Optimizer DOES NOT synthesize hardcoded x*x+1");
  assert(!optData.optimized_assembly.includes("imull\t%edi, %edi"), "Optimized assembly does not contain hardcoded x*x+1 imull");
  assert(optData.optimized_ir.includes("ret i32 42"), "Optimized IR folds 40 + 2 -> 42");
  assert(optData.optimized_assembly.includes("mov\teax, 42"), "Optimized assembly returns constant 42");
  assert(optData.metrics.constants_folded >= 1, "Metrics record constant folding");

  // TEST 6: POST /api/verify rejects missing commitment
  console.log("\nTest 6: Verification Endpoint Commitment Enforcement");
  const verifyMissingReq = mockRequest("POST", "https://zkaedi.ai/api/verify", {
    "Authorization": `Bearer ${validKey}`,
    "Content-Type": "application/json"
  }, {
    source: "int main() { return 42; }"
  });
  const verifyMissingRes = await verifyHandler.onRequestPost({ request: verifyMissingReq, env: {} });
  assert(verifyMissingRes.status === 400, "POST /api/verify returns 400 Bad Request when ast_commitment is missing");
  const missingData = await verifyMissingRes.json();
  assert(missingData.code === "MISSING_COMMITMENT", "Error code is MISSING_COMMITMENT");

  // TEST 7: POST /api/verify rejects mismatched/tampered commitment
  console.log("\nTest 7: Tamper Detection on Mismatched Commitment");
  const verifyTamperReq = mockRequest("POST", "https://zkaedi.ai/api/verify", {
    "Authorization": `Bearer ${validKey}`,
    "Content-Type": "application/json"
  }, {
    source: "int main() { return 42; }",
    ast_commitment: "0x0000000000000000000000000000000000000000000000000000000000000000"
  });
  const verifyTamperRes = await verifyHandler.onRequestPost({ request: verifyTamperReq, env: {} });
  assert(verifyTamperRes.status === 422, "POST /api/verify returns 422 Unprocessable Entity on tampered commitment");
  const tamperData = await verifyTamperRes.json();
  assert(tamperData.status === "COMMITMENT_MISMATCH", "Status is COMMITMENT_MISMATCH");

  // TEST 8: POST /api/verify succeeds with commitment from /api/compile
  console.log("\nTest 8: Synchronized Commitment Verification Between Compile & Verify");
  const verifyValidReq = mockRequest("POST", "https://zkaedi.ai/api/verify", {
    "Authorization": `Bearer ${validKey}`,
    "Content-Type": "application/json"
  }, {
    source: "int main() { return 42; }",
    target: "x86_64",
    opt_level: "O2",
    ast_commitment: generatedCommitment
  });
  const verifyValidRes = await verifyHandler.onRequestPost({ request: verifyValidReq, env: {} });
  assert(verifyValidRes.status === 200, "POST /api/verify returns 200 OK with valid matching commitment");
  const verifyValidData = await verifyValidRes.json();
  assert(verifyValidData.verified === true, "verified = true");
  assert(verifyValidData.status === "CRYPTOGRAPHICALLY_VALID", "Status is CRYPTOGRAPHICALLY_VALID");

  // TEST 9: Resource exhaustion protection: payload > 64KB
  console.log("\nTest 9: Resource Exhaustion Protection (64KB Ceiling)");
  const largeSource = "int main() { " + "x = 1;\n".repeat(12000) + " return 0; }"; // ~84KB
  const exhaustReq = mockRequest("POST", "https://zkaedi.ai/api/compile", {
    "Authorization": `Bearer ${validKey}`,
    "Content-Type": "application/json"
  }, {
    source: largeSource
  });
  const exhaustRes = await compileHandler.onRequestPost({ request: exhaustReq, env: {} });
  assert(exhaustRes.status === 413, "POST /api/compile returns 413 Payload Too Large for >64KB body");

  console.log("\n=================================================================");
  console.log(`  ALL API VERIFICATIONS COMPLETE: ${passed} / ${total} PASSING`);
  console.log("=================================================================\n");
}

runTests().catch(err => {
  console.error("Test Harness Uncaught Error:", err);
  process.exit(1);
});
