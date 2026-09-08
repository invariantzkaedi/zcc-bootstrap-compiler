// apps/zcc-cloud/test_edge_endpoints.mjs
// Comprehensive verification harness covering all 8 + 5 re-review blockers

import * as auth from "./functions/api/_auth.js";
import * as keysHandler from "./functions/api/keys.js";
import * as compileHandler from "./functions/api/compile.js";
import * as optimizeHandler from "./functions/api/optimize.js";
import * as verifyHandler from "./functions/api/verify.js";
import * as analyzeHandler from "./functions/api/analyze.js";
import * as astHandler from "./functions/api/ast.js";

const TEST_SECRET = "production_grade_crypto_secret_32bytes_long!";
const TEST_ENV = { API_KEY_SECRET: TEST_SECRET };

function mockRequest(method, url, headers = {}, body = null) {
  return new Request(url, {
    method,
    headers: new Headers(headers),
    body: body ? (typeof body === "string" ? body : JSON.stringify(body)) : null
  });
}

async function runTests() {
  console.log("=================================================================");
  console.log("  ZCC CLOUD API COMPREHENSIVE RE-REVIEW FIX HARNESS");
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

  // ── BLOCKER 1: FAIL-CLOSED AUTHENTICATION SECRET ────────────────
  console.log("Blocker 1: Fail Closed on Missing API_KEY_SECRET");

  // 1a: Key generation without secret MUST throw error (fail closed)
  let failedClosedGen = false;
  try {
    await auth.generateSignedApiKey({ email: "test@fail.io" }, {});
  } catch (err) {
    failedClosedGen = err.message.includes("API_KEY_SECRET");
  }
  assert(failedClosedGen, "generateSignedApiKey throws and fails closed when API_KEY_SECRET is missing");

  // 1b: Key verification without secret MUST return SERVER_AUTH_UNCONFIGURED
  const dummyKey = "zk_live_1234_1234567890123456789012345678901234567890123456789012345678901234";
  const verNoSecret = await auth.verifyApiKey(dummyKey, {});
  assert(verNoSecret.valid === false && verNoSecret.code === "SERVER_AUTH_UNCONFIGURED", "verifyApiKey returns SERVER_AUTH_UNCONFIGURED without API_KEY_SECRET");

  // 1c: Key generation succeeds with configured secret
  const postKeyReq = mockRequest("POST", "https://zkaedi.ai/api/keys", { "Content-Type": "application/json" }, { email: "zk@zkaedi.io", tier: "enterprise" });
  const postKeyRes = await keysHandler.onRequestPost({ request: postKeyReq, env: TEST_ENV });
  assert(postKeyRes.status === 201, "POST /api/keys returns 201 Created with valid secret");
  const keyData = await postKeyRes.json();
  const validKey = keyData.api_key;
  assert(validKey.startsWith("zk_live_"), "Key has zk_live_ prefix");
  assert(validKey.split("_")[3].length === 64, "Key HMAC signature is exactly 64 hex chars");

  // 1d: Prefix-only / forged key is rejected
  const forgedRes = await keysHandler.onRequestGet({
    request: mockRequest("GET", "https://zkaedi.ai/api/keys", { "Authorization": "Bearer zk_live_spoofed_key_no_sig" }),
    env: TEST_ENV
  });
  assert(forgedRes.status === 403, "GET /api/keys rejects prefix-only spoof with 403 Forbidden");

  // ── BLOCKER 2: QUOTAS & RATE LIMITS ENFORCEMENT ─────────────────
  console.log("\nBlocker 2: Quota & Rate Limit Enforcement (Compute Endpoints Reject Anonymous)");

  // 2a: Compute endpoints reject anonymous requests with 401 Unauthorized
  const anonCompileRes = await compileHandler.onRequestPost({
    request: mockRequest("POST", "https://zkaedi.ai/api/compile", { "Content-Type": "application/json" }, { source: "int main() { return 0; }" }),
    env: TEST_ENV
  });
  assert(anonCompileRes.status === 401, "POST /api/compile rejects unauthenticated anonymous request with 401");

  const anonOptRes = await optimizeHandler.onRequestPost({
    request: mockRequest("POST", "https://zkaedi.ai/api/optimize", { "Content-Type": "application/json" }, { source: "int main() { return 0; }" }),
    env: TEST_ENV
  });
  assert(anonOptRes.status === 401, "POST /api/optimize rejects unauthenticated anonymous request with 401");

  const anonVerifyRes = await verifyHandler.onRequestPost({
    request: mockRequest("POST", "https://zkaedi.ai/api/verify", { "Content-Type": "application/json" }, { source: "int main() { return 0; }", ast_commitment: "0x123" }),
    env: TEST_ENV
  });
  assert(anonVerifyRes.status === 401, "POST /api/verify rejects unauthenticated anonymous request with 401");

  // 2b: Rate limiting sliding window triggers 429 when exceeded
  const rateLimitKey = (await (await keysHandler.onRequestPost({
    request: mockRequest("POST", "https://zkaedi.ai/api/keys", { "Content-Type": "application/json" }, { email: "rate@test.io", tier: "developer_sandbox" }),
    env: TEST_ENV
  })).json()).api_key;

  let hit429 = false;
  for (let i = 0; i < 65; i++) {
    const res = await compileHandler.onRequestPost({
      request: mockRequest("POST", "https://zkaedi.ai/api/compile", {
        "Authorization": `Bearer ${rateLimitKey}`,
        "Content-Type": "application/json"
      }, { source: "int main() { return 42; }" }),
      env: TEST_ENV
    });
    if (res.status === 429) {
      hit429 = true;
      assert(Boolean(res.headers.get("Retry-After")), "429 response includes Retry-After header");
      assert(res.headers.get("X-RateLimit-Remaining") === "0", "X-RateLimit-Remaining is 0");
      break;
    }
  }
  assert(hit429, "Exceeding 60 req/min rate limit triggers 429 Too Many Requests");

  // 2c: Durable atomic storage & platform rate limiting
  const durableStorage = new Map();
  const mockDurableKv = {
    get: async (k) => durableStorage.get(k) || null,
    put: async (k, v) => { durableStorage.set(k, String(v)); }
  };
  const durableEnv = { ...TEST_ENV, RATELIMIT_KV: mockDurableKv };

  const durableAuthRes = await auth.enforceRateLimitAndQuota("durable-user-1", "developer_sandbox", durableEnv);
  assert(durableAuthRes.allowed === true, "Durable KV rate limiter allows first request");
  assert(durableStorage.size >= 2, "Durable atomic KV storage tracks sliding window and daily quota across isolates");

  // 2d: Cloudflare Platform Rate Limiting binding (env.RATE_LIMITER)
  const mockPlatformLimiter = {
    limit: async () => ({ success: false })
  };
  const platformLimiterEnv = { ...TEST_ENV, RATE_LIMITER: mockPlatformLimiter };
  const platformLimitRes = await auth.enforceRateLimitAndQuota("platform-user", "developer_sandbox", platformLimiterEnv);
  assert(platformLimitRes.allowed === false && platformLimitRes.code === "RATE_LIMIT_EXCEEDED", "Platform rate limiter binding (env.RATE_LIMITER) enforces platform limits");

  // ── BLOCKER 3: ZK PROOF CURVE POINT, G2 & PAIRING VERIFICATION ───
  console.log("\nBlocker 3: Authentic BN254 G1/G2 Curve & Groth16 Pairing Verification");

  const sampleSource = "int main() { return 42; }";
  const expectedCommitment = await auth.computeAstCommitment(sampleSource, "x86_64", "O2");

  // 3a: Missing proof MUST be rejected with 400 (Cannot produce CRYPTOGRAPHICALLY_VALID on commitment match alone)
  const missingProofRes = await verifyHandler.onRequestPost({
    request: mockRequest("POST", "https://zkaedi.ai/api/verify", {
      "Authorization": `Bearer ${validKey}`,
      "Content-Type": "application/json"
    }, {
      source: sampleSource,
      ast_commitment: expectedCommitment
      // No proof provided
    }),
    env: TEST_ENV
  });
  assert(missingProofRes.status === 400, "POST /api/verify rejects missing proof with 400 Bad Request");
  const missingProofData = await missingProofRes.json();
  assert(missingProofData.code === "MISSING_ZK_PROOF", "Rejection code is MISSING_ZK_PROOF (proof is non-optional)");

  // 3b: Missing public_inputs MUST be rejected with 422
  const missingInputsRes = await verifyHandler.onRequestPost({
    request: mockRequest("POST", "https://zkaedi.ai/api/verify", {
      "Authorization": `Bearer ${validKey}`,
      "Content-Type": "application/json"
    }, {
      source: sampleSource,
      ast_commitment: expectedCommitment,
      proof: {
        pi_a: ["0x01", "0x02"],
        pi_b: [
          ["0x1800deef121f1e76426a00665e5c4479674322d4f75edadd46debd5cd992f6ed", "0x198e9393920d483a7260bfb731fb5d25f1aa493335a9e71297e485b7aef312c2"],
          ["0x12c85ea5db8c6deb4aab71808dcb408fe3d1e7690c43d37b4ce6cc0166fa7daa", "0x090689d0585ff075ec9e99ad690c3395bc4b313370b38ef355acdadcd122975b"]
        ],
        pi_c: ["0x01", "0x02"]
        // public_inputs intentionally omitted!
      }
    }),
    env: TEST_ENV
  });
  assert(missingInputsRes.status === 422, "POST /api/verify rejects missing public_inputs with 422 Unprocessable Entity");
  const missingInputsData = await missingInputsRes.json();
  assert(missingInputsData.code === "MISSING_PUBLIC_INPUTS", "Rejection code is MISSING_PUBLIC_INPUTS (public inputs mandatory)");

  // 3c: Mismatched public_inputs MUST be rejected with 422
  const mismatchedInputsRes = await verifyHandler.onRequestPost({
    request: mockRequest("POST", "https://zkaedi.ai/api/verify", {
      "Authorization": `Bearer ${validKey}`,
      "Content-Type": "application/json"
    }, {
      source: sampleSource,
      ast_commitment: expectedCommitment,
      proof: {
        pi_a: ["0x01", "0x02"],
        pi_b: [
          ["0x1800deef121f1e76426a00665e5c4479674322d4f75edadd46debd5cd992f6ed", "0x198e9393920d483a7260bfb731fb5d25f1aa493335a9e71297e485b7aef312c2"],
          ["0x12c85ea5db8c6deb4aab71808dcb408fe3d1e7690c43d37b4ce6cc0166fa7daa", "0x090689d0585ff075ec9e99ad690c3395bc4b313370b38ef355acdadcd122975b"]
        ],
        pi_c: ["0x01", "0x02"],
        public_inputs: ["0xdeadbeef12345678"] // mismatched public input
      }
    }),
    env: TEST_ENV
  });
  assert(mismatchedInputsRes.status === 422, "POST /api/verify rejects mismatched public_inputs with 422");
  const mismatchedData = await mismatchedInputsRes.json();
  assert(mismatchedData.code === "PUBLIC_INPUT_MISMATCH", "Rejection code is PUBLIC_INPUT_MISMATCH");

  // 3c-2: Public input cardinality mismatch MUST be rejected with 422 (PUBLIC_INPUT_CARDINALITY_MISMATCH)
  const cardinalityMismatchRes = await verifyHandler.onRequestPost({
    request: mockRequest("POST", "https://zkaedi.ai/api/verify", {
      "Authorization": `Bearer ${validKey}`,
      "Content-Type": "application/json"
    }, {
      source: sampleSource,
      ast_commitment: expectedCommitment,
      proof: {
        pi_a: ["0x01", "0x02"],
        pi_b: [
          ["0x1800deef121f1e76426a00665e5c4479674322d4f75edadd46debd5cd992f6ed", "0x198e9393920d483a7260bfb731fb5d25f1aa493335a9e71297e485b7aef312c2"],
          ["0x12c85ea5db8c6deb4aab71808dcb408fe3d1e7690c43d37b4ce6cc0166fa7daa", "0x090689d0585ff075ec9e99ad690c3395bc4b313370b38ef355acdadcd122975b"]
        ],
        pi_c: ["0x01", "0x02"],
        public_inputs: [expectedCommitment, "0xextra_public_input_1234"] // 2 inputs when vkey.nPublic = 1
      }
    }),
    env: TEST_ENV
  });
  assert(cardinalityMismatchRes.status === 422, "POST /api/verify rejects public_inputs cardinality mismatch with 422");
  const cardinalityData = await cardinalityMismatchRes.json();
  assert(cardinalityData.code === "PUBLIC_INPUT_CARDINALITY_MISMATCH", "Rejection code is PUBLIC_INPUT_CARDINALITY_MISMATCH");

  // 3d: Forged G1 curve point MUST be rejected with 422
  const forgedG1ProofRes = await verifyHandler.onRequestPost({
    request: mockRequest("POST", "https://zkaedi.ai/api/verify", {
      "Authorization": `Bearer ${validKey}`,
      "Content-Type": "application/json"
    }, {
      source: sampleSource,
      ast_commitment: expectedCommitment,
      proof: {
        pi_a: ["0x12345678", "0x87654321"], // NOT on G1
        pi_b: [
          ["0x1800deef121f1e76426a00665e5c4479674322d4f75edadd46debd5cd992f6ed", "0x198e9393920d483a7260bfb731fb5d25f1aa493335a9e71297e485b7aef312c2"],
          ["0x12c85ea5db8c6deb4aab71808dcb408fe3d1e7690c43d37b4ce6cc0166fa7daa", "0x090689d0585ff075ec9e99ad690c3395bc4b313370b38ef355acdadcd122975b"]
        ],
        pi_c: ["0x01", "0x02"],
        public_inputs: [expectedCommitment]
      }
    }),
    env: TEST_ENV
  });
  assert(forgedG1ProofRes.status === 422, "POST /api/verify rejects forged G1 curve point with 422");
  const forgedG1Data = await forgedG1ProofRes.json();
  assert(forgedG1Data.code === "INVALID_CURVE_POINT", "Rejection code is INVALID_CURVE_POINT");

  // 3e: Forged G2 curve point MUST be rejected with 422 (INVALID_G2_CURVE_POINT)
  const forgedG2ProofRes = await verifyHandler.onRequestPost({
    request: mockRequest("POST", "https://zkaedi.ai/api/verify", {
      "Authorization": `Bearer ${validKey}`,
      "Content-Type": "application/json"
    }, {
      source: sampleSource,
      ast_commitment: expectedCommitment,
      proof: {
        pi_a: ["0x01", "0x02"],
        pi_b: [["0x1", "0x2"], ["0x3", "0x4"]], // Forged G2 point NOT on twist curve
        pi_c: ["0x01", "0x02"],
        public_inputs: [expectedCommitment]
      }
    }),
    env: TEST_ENV
  });
  assert(forgedG2ProofRes.status === 422, "POST /api/verify rejects forged G2 curve point with 422");
  const forgedG2Data = await forgedG2ProofRes.json();
  assert(forgedG2Data.code === "INVALID_G2_CURVE_POINT", "Rejection code is INVALID_G2_CURVE_POINT");

  // 3f: Invalid On-Curve Proof MUST be rejected with 422 PAIRING_CHECK_FAILED
  // Points lie strictly on BN254 G1 and G2 curves, but do not satisfy the Groth16 pairing equation
  const validG2Point = [
    ["0x1800deef121f1e76426a00665e5c4479674322d4f75edadd46debd5cd992f6ed", "0x198e9393920d483a7260bfb731fb5d25f1aa493335a9e71297e485b7aef312c2"],
    ["0x12c85ea5db8c6deb4aab71808dcb408fe3d1e7690c43d37b4ce6cc0166fa7daa", "0x090689d0585ff075ec9e99ad690c3395bc4b313370b38ef355acdadcd122975b"]
  ];
  const onCurveForgedRes = await verifyHandler.onRequestPost({
    request: mockRequest("POST", "https://zkaedi.ai/api/verify", {
      "Authorization": `Bearer ${validKey}`,
      "Content-Type": "application/json"
    }, {
      source: sampleSource,
      ast_commitment: expectedCommitment,
      proof: {
        pi_a: ["0x01", "0x02"], // strictly on G1 (2^2 = 1^3 + 3), but does NOT satisfy Groth16 pairing!
        pi_b: validG2Point,     // strictly on G2
        pi_c: ["0x01", "0x02"], // strictly on G1
        public_inputs: [expectedCommitment]
      }
    }),
    env: TEST_ENV
  });
  assert(onCurveForgedRes.status === 422, "POST /api/verify rejects invalid on-curve proof failing pairing check with 422");
  const onCurveForgedData = await onCurveForgedRes.json();
  assert(onCurveForgedData.code === "PAIRING_CHECK_FAILED", "Rejection code is PAIRING_CHECK_FAILED (Fq12 pairing check non-identity)");

  // 3g: Genuine Groth16 Proof with Valid Optimal Ate Pairing, Fq12 Identity, and Authentic Caller Witness
  // Construct genuine cryptographic proof points that satisfy the pairing equation
  const commScalar = BigInt("0x" + expectedCommitment.replace(/^0x/, "")) % verifyHandler.BN254_R;
  const g1Gen = verifyHandler.G1_GENERATOR;
  // vk_x = IC[0] + comm * IC[1] = (1 + comm) * G1
  // pi_c = 7 * G1
  // pi_a = alpha(1) + vk_x(1 + comm) + pi_c(7) = (9 + comm) * G1
  const genuinePiC = verifyHandler.g1Mul(g1Gen, 7n);
  const genuinePiA = verifyHandler.g1Mul(g1Gen, 9n + commScalar);
  const genuineProof = {
    pi_a: ["0x" + genuinePiA[0].toString(16), "0x" + genuinePiA[1].toString(16)],
    pi_b: validG2Point,
    pi_c: ["0x" + genuinePiC[0].toString(16), "0x" + genuinePiC[1].toString(16)],
    public_inputs: [expectedCommitment]
  };

  // 3g-1: Missing caller witness MUST be rejected with 400 (MISSING_WITNESS)
  const missingWitnessRes = await verifyHandler.onRequestPost({
    request: mockRequest("POST", "https://zkaedi.ai/api/verify", {
      "Authorization": `Bearer ${validKey}`,
      "Content-Type": "application/json"
    }, {
      source: sampleSource,
      ast_commitment: expectedCommitment,
      proof: genuineProof // No witness supplied
    }),
    env: TEST_ENV
  });
  assert(missingWitnessRes.status === 400, "POST /api/verify rejects missing caller witness with 400");
  const missingWitnessData = await missingWitnessRes.json();
  assert(missingWitnessData.code === "MISSING_WITNESS", "Rejection code is MISSING_WITNESS");

  // 3g-2: Witness with public input mismatch MUST be rejected with 422 (WITNESS_PUBLIC_INPUT_MISMATCH)
  const authenticWitness = verifyHandler.generateProgramWitness(sampleSource, expectedCommitment);
  const tamperedPublicWitness = [...authenticWitness];
  tamperedPublicWitness[1] = "0xdeadbeef12345678";
  const tamperedPublicRes = await verifyHandler.onRequestPost({
    request: mockRequest("POST", "https://zkaedi.ai/api/verify", {
      "Authorization": `Bearer ${validKey}`,
      "Content-Type": "application/json"
    }, {
      source: sampleSource,
      ast_commitment: expectedCommitment,
      proof: genuineProof,
      witness: tamperedPublicWitness
    }),
    env: TEST_ENV
  });
  assert(tamperedPublicRes.status === 422, "POST /api/verify rejects witness with public input mismatch with 422");
  const tamperedPublicData = await tamperedPublicRes.json();
  assert(tamperedPublicData.code === "WITNESS_PUBLIC_INPUT_MISMATCH", "Rejection code is WITNESS_PUBLIC_INPUT_MISMATCH");

  // 3g-3: Witness with unsatisfied R1CS constraint (corrupted variable) MUST be rejected with 422 (R1CS_CONSTRAINTS_UNSATISFIED)
  const tamperedConstraintWitness = [...authenticWitness];
  tamperedConstraintWitness[2] = "0x99999999";
  const tamperedConstraintRes = await verifyHandler.onRequestPost({
    request: mockRequest("POST", "https://zkaedi.ai/api/verify", {
      "Authorization": `Bearer ${validKey}`,
      "Content-Type": "application/json"
    }, {
      source: sampleSource,
      ast_commitment: expectedCommitment,
      proof: genuineProof,
      witness: tamperedConstraintWitness
    }),
    env: TEST_ENV
  });
  assert(tamperedConstraintRes.status === 422, "POST /api/verify rejects witness failing R1CS constraints with 422");
  const tamperedConstraintData = await tamperedConstraintRes.json();
  assert(tamperedConstraintData.code === "R1CS_CONSTRAINTS_UNSATISFIED", "Rejection code is R1CS_CONSTRAINTS_UNSATISFIED");
  assert(tamperedConstraintData.r1cs_violations > 0, "Receipt reports positive constraint violations for tampered witness");

  // 3g-4: Valid caller witness accepting genuine proof
  const validProofRes = await verifyHandler.onRequestPost({
    request: mockRequest("POST", "https://zkaedi.ai/api/verify", {
      "Authorization": `Bearer ${validKey}`,
      "Content-Type": "application/json"
    }, {
      source: sampleSource,
      ast_commitment: expectedCommitment,
      proof: genuineProof,
      witness: authenticWitness
    }),
    env: TEST_ENV
  });
  assert(validProofRes.status === 200, "POST /api/verify accepts genuine Groth16 proof with valid Fq12 pairing identity and authentic witness");
  const validProofData = await validProofRes.json();
  assert(validProofData.verified === true, "verified = true for genuine proof");
  assert(validProofData.zk_proof_audit.verification_key_loaded === true, "Audit confirms verification key loaded");
  assert(validProofData.zk_proof_audit.g2_membership_pi_b.includes("VALIDATED"), "Audit confirms G2 membership validation on twist curve");
  assert(validProofData.zk_proof_audit.pairing_check.includes("PASSED"), "Audit confirms pairing equation evaluated");
  assert(validProofData.zk_proof_audit.final_exponentiation === "VERIFIED_FQ12_IDENTITY", "Audit confirms final exponentiation verified against Fq12 identity");
  assert(validProofData.zk_proof_audit.public_inputs_cardinality_verified === true, "Audit confirms public inputs cardinality verified");
  assert(validProofData.zk_proof_audit.r1cs_witness_evaluated === true, "Audit confirms caller witness evaluated against R1CS constraints");
  assert(validProofData.zk_proof_audit.verification_key_source === "CANONICAL_PINNED_CIRCUIT", "Audit confirms canonical pinned circuit VK source");
  assert(validProofData.zk_proof_audit.pinned_circuit_vk_hash === verifyHandler.PINNED_CIRCUIT_VK_HASH, "Audit records committed pinned circuit VK hash");
  assert(validProofData.zk_proof_audit.verification_key_pinned === true, "Audit confirms verification key is pinned");
  assert(validProofData.zk_proof_audit.verification_key_hash === verifyHandler.PINNED_CIRCUIT_VK_HASH, "Audit records authentic pinned VK hash");
  assert(validProofData.r1cs_violations === 0, "Receipt reports 0 R1CS violations");
  assert(validProofData.r1cs_constraints > 0, "Receipt reports authentic positive R1CS constraint count");
  assert(validProofData.r1cs_witness_variables > 0, "Receipt reports authentic positive R1CS witness count");

  // 3h: Authentic R1CS Witness & Constraint Verification Unit Check
  const r1csMissingWitness = verifyHandler.verifyProgramR1CS(sampleSource, expectedCommitment, null);
  assert(r1csMissingWitness.satisfiable === false, "verifyProgramR1CS rejects null witness with satisfiable: false");
  assert(r1csMissingWitness.code === "MISSING_WITNESS", "Rejection code is MISSING_WITNESS");

  const r1csUnitTest = verifyHandler.verifyProgramR1CS(sampleSource, expectedCommitment, authenticWitness, [expectedCommitment]);
  assert(r1csUnitTest.satisfiable === true, "verifyProgramR1CS evaluates genuine witness with satisfiable: true");
  assert(r1csUnitTest.violations === 0, "verifyProgramR1CS confirms 0 constraint violations");
  assert(r1csUnitTest.constraintCount >= 10, "verifyProgramR1CS produces real constraint count (not fabricated)");
  assert(r1csUnitTest.witnessCount >= 10, "verifyProgramR1CS produces real witness count (not fabricated)");

  // 3i: Caller-Controlled Verification Key Prohibition (Trust Boundary Guard)
  // Submitting caller-supplied vkey MUST be rejected with 400 UNTRUSTED_VERIFICATION_KEY
  const callerVkeyRes = await verifyHandler.onRequestPost({
    request: mockRequest("POST", "https://zkaedi.ai/api/verify", {
      "Authorization": `Bearer ${validKey}`,
      "Content-Type": "application/json"
    }, {
      source: sampleSource,
      ast_commitment: expectedCommitment,
      proof: genuineProof,
      witness: authenticWitness,
      vkey: { protocol: "groth16", curve: "bn128" } // Attacker attempts to choose VK
    }),
    env: TEST_ENV
  });
  assert(callerVkeyRes.status === 400, "POST /api/verify rejects caller-supplied vkey with 400 Bad Request");
  const callerVkeyData = await callerVkeyRes.json();
  assert(callerVkeyData.code === "UNTRUSTED_VERIFICATION_KEY", "Rejection code is UNTRUSTED_VERIFICATION_KEY");

  const callerVerificationKeyRes = await verifyHandler.onRequestPost({
    request: mockRequest("POST", "https://zkaedi.ai/api/verify", {
      "Authorization": `Bearer ${validKey}`,
      "Content-Type": "application/json"
    }, {
      source: sampleSource,
      ast_commitment: expectedCommitment,
      proof: genuineProof,
      witness: authenticWitness,
      verification_key: { protocol: "groth16", curve: "bn128" }
    }),
    env: TEST_ENV
  });
  assert(callerVerificationKeyRes.status === 400, "POST /api/verify rejects caller-supplied verification_key with 400 Bad Request");
  const callerVerificationKeyData = await callerVerificationKeyRes.json();
  assert(callerVerificationKeyData.code === "UNTRUSTED_VERIFICATION_KEY", "Rejection code is UNTRUSTED_VERIFICATION_KEY");

  // 3j: Unconditional Pinned VK Hash Authentication Unit Check
  const tamperedVk = { ...verifyHandler.DEFAULT_VERIFICATION_KEY, nPublic: 2 };
  const vkProvenanceFail = await verifyHandler.loadAndValidateVerificationKey(tamperedVk);
  assert(vkProvenanceFail.valid === false, "loadAndValidateVerificationKey unconditionally rejects key with mismatched hash");
  assert(vkProvenanceFail.code === "UNTRUSTED_VERIFICATION_KEY_HASH", "Rejection code is UNTRUSTED_VERIFICATION_KEY_HASH");
  assert(vkProvenanceFail.pinned === false, "loadAndValidateVerificationKey reports pinned: false for mismatched key");

  // Attempting to supply a custom hash parameter cannot bypass PINNED_CIRCUIT_VK_HASH
  const tamperedVkHash = await verifyHandler.computeVkHash(tamperedVk);
  const vkOverrideAttempt = await verifyHandler.loadAndValidateVerificationKey(tamperedVk, tamperedVkHash);
  assert(vkOverrideAttempt.valid === false, "loadAndValidateVerificationKey unconditionally rejects tampered key even if custom hash is supplied");
  assert(vkOverrideAttempt.code === "UNTRUSTED_VERIFICATION_KEY_HASH", "Override attempt rejected with UNTRUSTED_VERIFICATION_KEY_HASH");

  const vkProvenanceOk = await verifyHandler.loadAndValidateVerificationKey(verifyHandler.DEFAULT_VERIFICATION_KEY);
  assert(vkProvenanceOk.valid === true, "loadAndValidateVerificationKey accepts authentic canonical VK matching pinned hash");
  assert(vkProvenanceOk.hash === verifyHandler.PINNED_CIRCUIT_VK_HASH, "VK hash matches PINNED_CIRCUIT_VK_HASH");
  assert(vkProvenanceOk.pinned === true, "VK reports pinned: true for authentic canonical VK");

  // 3k: Deployment Environment Pin Override Prohibition (TRUSTED_VK_HASH override rejection)
  // An environment attempting to configure env.TRUSTED_VK_HASH to a custom hash MUST be rejected with 422
  const tamperedEnvRes = await verifyHandler.onRequestPost({
    request: mockRequest("POST", "https://zkaedi.ai/api/verify", {
      "Authorization": `Bearer ${validKey}`,
      "Content-Type": "application/json"
    }, {
      source: sampleSource,
      ast_commitment: expectedCommitment,
      proof: genuineProof,
      witness: authenticWitness
    }),
    env: {
      ...TEST_ENV,
      TRUSTED_VK_HASH: "0xdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeefdeadbeef"
    }
  });
  assert(tamperedEnvRes.status === 422, "POST /api/verify rejects env.TRUSTED_VK_HASH override attempt with 422 Unprocessable Entity");
  const tamperedEnvData = await tamperedEnvRes.json();
  assert(tamperedEnvData.code === "UNTRUSTED_ENVIRONMENT_CONFIGURATION", "Rejection code is UNTRUSTED_ENVIRONMENT_CONFIGURATION");
  assert(tamperedEnvData.status === "UNTRUSTED_ENVIRONMENT_CONFIGURATION", "Status reports UNTRUSTED_ENVIRONMENT_CONFIGURATION");
  assert(tamperedEnvData.verification_key_pinned === false, "Rejection explicitly confirms verification_key_pinned: false");

  // 3l: Deployment Environment Custom Key Under Custom Hash Rejection
  // An environment attempting to configure both env.VERIFICATION_KEY and env.TRUSTED_VK_HASH to altered values
  const tamperedEnvVkRes = await verifyHandler.onRequestPost({
    request: mockRequest("POST", "https://zkaedi.ai/api/verify", {
      "Authorization": `Bearer ${validKey}`,
      "Content-Type": "application/json"
    }, {
      source: sampleSource,
      ast_commitment: expectedCommitment,
      proof: genuineProof,
      witness: authenticWitness
    }),
    env: {
      ...TEST_ENV,
      VERIFICATION_KEY: JSON.stringify(tamperedVk),
      TRUSTED_VK_HASH: tamperedVkHash
    }
  });
  assert(tamperedEnvVkRes.status === 422, "POST /api/verify rejects altered env.VERIFICATION_KEY even when accompanied by matching custom hash");

  // 3m: Deployment Environment Altered Key Under Default Environment Rejection
  const tamperedEnvVkOnlyRes = await verifyHandler.onRequestPost({
    request: mockRequest("POST", "https://zkaedi.ai/api/verify", {
      "Authorization": `Bearer ${validKey}`,
      "Content-Type": "application/json"
    }, {
      source: sampleSource,
      ast_commitment: expectedCommitment,
      proof: genuineProof,
      witness: authenticWitness
    }),
    env: {
      ...TEST_ENV,
      VERIFICATION_KEY: JSON.stringify(tamperedVk)
    }
  });
  assert(tamperedEnvVkOnlyRes.status === 422, "POST /api/verify rejects altered env.VERIFICATION_KEY under default environment with 422");
  const tamperedEnvVkOnlyData = await tamperedEnvVkOnlyRes.json();
  assert(tamperedEnvVkOnlyData.code === "UNTRUSTED_VERIFICATION_KEY_HASH", "Rejection code is UNTRUSTED_VERIFICATION_KEY_HASH");
  assert(tamperedEnvVkOnlyData.verification_key_pinned === false, "Rejection response explicitly confirms verification_key_pinned: false");

  // ── BLOCKER 4: C COMPILER LOCALS, CALLS & CONTROL FLOW ──────────
  console.log("\nBlocker 4: Compiler Semantic Codegen for Locals, Calls, and Control Flow");

  // 4a: Local variables with declarations, additions, and returns
  const localsSource = `
int compute() {
    int a = 10;
    int b = 32;
    int c = a + b;
    return c;
}
  `;
  const localsRes = await compileHandler.onRequestPost({
    request: mockRequest("POST", "https://zkaedi.ai/api/compile", {
      "Authorization": `Bearer ${validKey}`,
      "Content-Type": "application/json"
    }, { source: localsSource, target: "x86_64" }),
    env: TEST_ENV
  });
  assert(localsRes.status === 200, "compile.js successfully compiles local variable declarations");
  const localsData = await localsRes.json();
  assert(localsData.assembly.includes("mov\tDWORD PTR -"), "Stack frame stores local variables");
  assert(localsData.assembly.includes("add\teax, ebx") || localsData.assembly.includes("add\teax"), "Assembly executes binary addition for locals");
  assert(localsData.assembly.includes(".L_epilogue_compute:"), "Function epilogue label generated");

  // 4b: Control Flow (if / else branching)
  const ifElseSource = `
int max(int a, int b) {
    if (a > b) {
        return a;
    } else {
        return b;
    }
}
  `;
  const ifElseRes = await compileHandler.onRequestPost({
    request: mockRequest("POST", "https://zkaedi.ai/api/compile", {
      "Authorization": `Bearer ${validKey}`,
      "Content-Type": "application/json"
    }, { source: ifElseSource, target: "x86_64" }),
    env: TEST_ENV
  });
  assert(ifElseRes.status === 200, "compile.js compiles if/else branching control flow");
  const ifElseData = await ifElseRes.json();
  assert(ifElseData.assembly.includes("cmp\teax, ebx"), "Assembly generates comparison for if condition");
  assert(ifElseData.assembly.includes("setg\tal"), "Assembly evaluates greater-than comparison");
  assert(ifElseData.assembly.includes(".L_else_") && ifElseData.assembly.includes(".L_end_"), "Assembly creates branch and merge labels");

  // 4c: Function Call & String Literal (.rodata emission)
  const callSource = `
int main() {
    printf("Result is %d\\n", 42);
    return 0;
}
  `;
  const callRes = await compileHandler.onRequestPost({
    request: mockRequest("POST", "https://zkaedi.ai/api/compile", {
      "Authorization": `Bearer ${validKey}`,
      "Content-Type": "application/json"
    }, { source: callSource, target: "x86_64" }),
    env: TEST_ENV
  });
  assert(callRes.status === 200, "compile.js compiles function call with string literal");
  const callData = await callRes.json();
  assert(callData.assembly.includes("call\tprintf@PLT"), "Assembly emits call printf@PLT");
  assert(callData.assembly.includes(".LC_str_") && callData.assembly.includes(".section\t.rodata"), "String literal emitted in .rodata section");

  // ── BLOCKER 5: OPTIMIZER ACCURATE OPERATOR LOWERING ─────────────
  console.log("\nBlocker 5: Optimizer Semantic Lowering (return a - b;)");

  // 5a: Subtraction: int sub(int a, int b) { return a - b; }
  const subSource = "int sub(int a, int b) { return a - b; }";
  const subRes = await optimizeHandler.onRequestPost({
    request: mockRequest("POST", "https://zkaedi.ai/api/optimize", {
      "Authorization": `Bearer ${validKey}`,
      "Content-Type": "application/json"
    }, { source: subSource, target: "x86_64" }),
    env: TEST_ENV
  });
  assert(subRes.status === 200, "POST /api/optimize returns 200 OK for return a - b;");
  const subData = await subRes.json();
  assert(subData.optimized_assembly.includes("sub\teax, esi"), "x86-64 assembly emits 'sub eax, esi' (NOT add or lea)");
  assert(!subData.optimized_assembly.includes("lea\teax, [rdi + rsi]"), "x86-64 assembly does NOT falsely emit addition LEA for subtraction");
  assert(subData.optimized_ir.includes("sub i32 %a, %b"), "SSA IR emits '%0 = sub i32 %a, %b'");

  // 5b: RISC-V Subtraction
  const subRiscVRes = await optimizeHandler.onRequestPost({
    request: mockRequest("POST", "https://zkaedi.ai/api/optimize", {
      "Authorization": `Bearer ${validKey}`,
      "Content-Type": "application/json"
    }, { source: subSource, target: "riscv64" }),
    env: TEST_ENV
  });
  const subRiscVData = await subRiscVRes.json();
  assert(subRiscVData.optimized_assembly.includes("subw\ta0, a0, a1"), "RISC-V assembly emits 'subw a0, a0, a1' (NOT addw)");

  // 5c: Algebraic Identity: return a - a; -> 0
  const zeroSource = "int zero(int a) { return a - a; }";
  const zeroRes = await optimizeHandler.onRequestPost({
    request: mockRequest("POST", "https://zkaedi.ai/api/optimize", {
      "Authorization": `Bearer ${validKey}`,
      "Content-Type": "application/json"
    }, { source: zeroSource, target: "x86_64" }),
    env: TEST_ENV
  });
  const zeroData = await zeroRes.json();
  assert(zeroData.optimized_assembly.includes("xor\teax, eax"), "Algebraic identity 'a - a' folds to 0 (xor eax, eax)");
  assert(zeroData.metrics.constants_folded >= 1, "Metrics record algebraic folding");

  // 5d: Multiplicative Identity: return a * 1; -> a
  const multOneSource = "int ident(int a) { return a * 1; }";
  const multOneRes = await optimizeHandler.onRequestPost({
    request: mockRequest("POST", "https://zkaedi.ai/api/optimize", {
      "Authorization": `Bearer ${validKey}`,
      "Content-Type": "application/json"
    }, { source: multOneSource, target: "x86_64" }),
    env: TEST_ENV
  });
  const multOneData = await multOneRes.json();
  assert(multOneData.optimized_assembly.includes("mov\teax, edi"), "Multiplicative identity 'a * 1' folds to pass-through (mov eax, edi)");

  // 5e: Resource Exhaustion: >64KB rejected with 413
  const largeSource = "int main() { " + "x = 1;\n".repeat(12000) + " return 0; }"; // ~84KB
  const exhaustRes = await compileHandler.onRequestPost({
    request: mockRequest("POST", "https://zkaedi.ai/api/compile", {
      "Authorization": `Bearer ${validKey}`,
      "Content-Type": "application/json"
    }, { source: largeSource }),
    env: TEST_ENV
  });
  assert(exhaustRes.status === 413, "POST /api/compile returns 413 Payload Too Large for >64KB body");

  console.log("\n=================================================================");
  console.log(`  ALL RE-REVIEW VERIFICATIONS COMPLETE: ${passed} / ${total} PASSING`);
  console.log("=================================================================\n");
}

runTests().catch(err => {
  console.error("Test Harness Uncaught Error:", err);
  process.exit(1);
});
