/**
 * Track 21A Portable Verified Compute JS Contract & Client Bridge.
 * Additive companion to sdk/zcc_client.mjs (maintains 100% backward compatibility).
 */

import { ZccApiError, ZccClient } from "./zcc_client.mjs";

export { ZccApiError, ZccClient };

export const ZCC_COMPUTE_ABI_VERSION = 0x00010000;
export const ZCC_COMPUTE_CONTRACT_VERSION = "1.0.0";
export const ZCC_COMPUTE_RECEIPT_SCHEMA = "track21a.receipt.v1";

export const ZccBackendId = Object.freeze({
  AUTO: 0,
  SCALAR_C99: 1,
  AVX2_FMA: 2,
  PTX_SM120: 3
});

export const ZccOpId = Object.freeze({
  VEC_ADD_F32: 1,
  VEC_MUL_F32: 2,
  VEC_FMA_F32: 3,
  DOT_F32: 4,
  GEMM_F32: 5,
  RMSNORM_F32: 6,
  SOFTMAX_F32: 7,
  DEQUANT_Q8_0: 8,
  MATVEC_Q8_0: 9,
  DEQUANT_Q4_0: 10,
  MATVEC_Q4_0: 11,
  DEQUANT_Q4_K: 12,
  MATVEC_Q4_K: 13
});

/**
 * Validate client-side structural expectations for a Track 21A receipt JSON object.
 * Authoritative verification remains in tools/verify_track21a_release.py (fail-closed).
 * @param {object} receipt
 */
export function validateComputeReceiptShape(receipt) {
  if (!receipt || typeof receipt !== "object") {
    return { valid: false, reason: "receipt_not_object" };
  }
  const required = [
    "schema_version",
    "contract_version",
    "backend_requested",
    "backend_executed",
    "op_name",
    "inputs_descriptor_sha256",
    "input_buffers_sha256",
    "output_descriptor_sha256",
    "output_buffer_sha256",
    "status_code",
    "verification_status",
    "integrity_kind",
    "receipt_canonical_sha256"
  ];
  for (const k of required) {
    if (!(k in receipt)) return { valid: false, reason: `missing_${k}` };
  }
  if (receipt.schema_version !== ZCC_COMPUTE_RECEIPT_SCHEMA) {
    return { valid: false, reason: "schema_mismatch" };
  }
  if (receipt.verification_status !== "UNVERIFIED_RUNTIME_ONLY") {
    return { valid: false, reason: "illegal_self_certification" };
  }
  if (receipt.zk_proof_present === true) {
    return { valid: false, reason: "unexpected_zk_claim" };
  }
  return { valid: true, reason: "shape_ok" };
}
