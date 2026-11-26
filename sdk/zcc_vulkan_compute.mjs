// ZCC Track 21B Vulkan & SPIR-V Compute SDK Contract (additive extension over sdk/zcc_compute.mjs)
import { validateComputeReceiptShape } from "./zcc_compute.mjs";
export * from "./zcc_compute.mjs";

export const ZCC_COMPUTE_V2_ABI_VERSION = 0x00010100;
export const ZCC_COMPUTE_V2_CONTRACT_VERSION = "1.1.0";
export const ZCC_COMPUTE_V2_SCHEMA = "track21b.receipt.v1";

export const ZccBackendV2Id = Object.freeze({
  AUTO: 0,
  SCALAR_C99: 1,
  AVX2_FMA: 2,
  PTX_SM120: 3,
  VULKAN_SPV: 4,
  VULKAN_GPU: 5,
});

export const ZccVulkanDeviceType = Object.freeze({
  OTHER: "OTHER",
  INTEGRATED_GPU: "INTEGRATED_GPU",
  DISCRETE_GPU: "DISCRETE_GPU",
  VIRTUAL_GPU: "VIRTUAL_GPU",
  CPU: "CPU",
});

export function validateVulkanReceiptShape(receipt) {
  if (!receipt || typeof receipt !== "object") {
    return { valid: false, reason: "receipt_not_object" };
  }
  const required = [
    "v2_schema_version",
    "v2_abi_version",
    "v2_contract_version",
    "backend_requested",
    "backend_executed",
    "status_code",
    "v2_verification_status",
    "v2_integrity_kind",
    "v2_canonical_sha256",
  ];
  for (const k of required) {
    if (!(k in receipt)) return { valid: false, reason: `missing_${k}` };
  }
  if (receipt.v2_schema_version !== ZCC_COMPUTE_V2_SCHEMA) {
    return { valid: false, reason: "schema_mismatch" };
  }
  if (receipt.v2_verification_status !== "UNVERIFIED_RUNTIME_ONLY") {
    return { valid: false, reason: "illegal_self_certification" };
  }
  if (receipt.v2_zk_proof_present !== false) {
    return { valid: false, reason: "unexpected_zk_claim" };
  }
  if (receipt.backend_executed === "VULKAN_SPV" || receipt.backend_executed === "VULKAN_GPU") {
    if (!receipt.vulkan_caps || !receipt.vulkan_caps.device_type) {
      return { valid: false, reason: "missing_vulkan_device_type" };
    }
    if (!receipt.sync_barrier_count || receipt.sync_barrier_count < 2) {
      return { valid: false, reason: "missing_sync_barriers" };
    }
    if (receipt.backend_executed === "VULKAN_GPU" && receipt.vulkan_caps.device_type === "CPU") {
      return { valid: false, reason: "cpu_device_claiming_gpu_backend" };
    }
  }
  if ("base_receipt" in receipt && receipt.base_receipt !== null) {
    const baseChk = validateComputeReceiptShape(receipt.base_receipt);
    if (!baseChk.valid) {
      return { valid: false, reason: `base_${baseChk.reason}` };
    }
  }
  return { valid: true, reason: "shape_ok" };
}
