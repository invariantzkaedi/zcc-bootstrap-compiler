import assert from "node:assert/strict";
import { ZccBackendId, ZccClient, ZccOpId, validateComputeReceiptShape } from "../../sdk/zcc_compute.mjs";

const client = new ZccClient({ baseUrl: "http://127.0.0.1:8080" });
assert.equal(client.baseUrl, "http://127.0.0.1:8080");
assert.equal(ZccBackendId.PTX_SM120, 3);
assert.equal(ZccOpId.MATVEC_Q4_K, 13);

const validSample = {
  schema_version: "track21a.receipt.v1",
  contract_version: "1.0.0",
  backend_requested: "AVX2_FMA",
  backend_executed: "AVX2_FMA",
  op_name: "VEC_ADD_F32",
  inputs_descriptor_sha256: "a".repeat(64),
  input_buffers_sha256: "b".repeat(64),
  output_descriptor_sha256: "c".repeat(64),
  output_buffer_sha256: "d".repeat(64),
  status_code: 0,
  verification_status: "UNVERIFIED_RUNTIME_ONLY",
  integrity_kind: "unkeyed_sha256_integrity",
  zk_proof_present: false,
  receipt_canonical_sha256: "e".repeat(64)
};

assert.deepEqual(validateComputeReceiptShape(validSample), { valid: true, reason: "shape_ok" });
assert.deepEqual(
  validateComputeReceiptShape({ ...validSample, verification_status: "PASS" }),
  { valid: false, reason: "illegal_self_certification" }
);
assert.deepEqual(
  validateComputeReceiptShape({ ...validSample, zk_proof_present: true }),
  { valid: false, reason: "unexpected_zk_claim" }
);
console.log("MJS_SDK_CONTRACT_OK");
