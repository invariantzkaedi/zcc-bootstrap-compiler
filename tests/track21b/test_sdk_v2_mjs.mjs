import assert from "node:assert/strict";
import {
  ZccClient,
  ZccApiError,
  ZccBackendV2Id,
  ZccVulkanDeviceType,
  ZCC_COMPUTE_V2_ABI_VERSION,
  ZCC_COMPUTE_V2_CONTRACT_VERSION,
  validateComputeReceiptShape,
  validateVulkanReceiptShape,
} from "../../sdk/zcc_vulkan_compute.mjs";

assert.equal(typeof ZccClient, "function");
assert.equal(typeof ZccApiError, "function");
assert.equal(ZccBackendV2Id.VULKAN_SPV, 4);
assert.equal(ZccBackendV2Id.VULKAN_GPU, 5);
assert.equal(ZccVulkanDeviceType.CPU, "CPU");
assert.equal(ZCC_COMPUTE_V2_ABI_VERSION, 0x00010100);
assert.equal(ZCC_COMPUTE_V2_CONTRACT_VERSION, "1.1.0");

const validBaseReceipt = {
  schema_version: "track21a.receipt.v1",
  contract_version: "1.0.0",
  backend_requested: "SCALAR_C99",
  backend_executed: "SCALAR_C99",
  op_name: "VEC_ADD_F32",
  inputs_descriptor_sha256: "b".repeat(64),
  input_buffers_sha256: "c".repeat(64),
  output_descriptor_sha256: "d".repeat(64),
  output_buffer_sha256: "e".repeat(64),
  status_code: 0,
  verification_status: "UNVERIFIED_RUNTIME_ONLY",
  integrity_kind: "unkeyed_sha256_integrity",
  zk_proof_present: false,
  receipt_canonical_sha256: "a".repeat(64),
};

assert.deepEqual(validateComputeReceiptShape(validBaseReceipt), { valid: true, reason: "shape_ok" });

assert.deepEqual(
  validateVulkanReceiptShape({
    v2_schema_version: "track21b.receipt.v1",
    v2_abi_version: "0x00010100",
    v2_contract_version: "1.1.0",
    backend_requested: "VULKAN_SPV",
    backend_executed: "VULKAN_SPV",
    vulkan_caps: { device_type: "CPU" },
    sync_barrier_count: 2,
    status_code: 0,
    v2_verification_status: "UNVERIFIED_RUNTIME_ONLY",
    v2_integrity_kind: "unkeyed_sha256_integrity",
    v2_zk_proof_present: false,
    v2_canonical_sha256: "f".repeat(64),
    base_receipt: validBaseReceipt,
  }),
  { valid: true, reason: "shape_ok" }
);

assert.equal(
  validateVulkanReceiptShape({
    v2_schema_version: "track21b.receipt.v1",
    v2_abi_version: "0x00010100",
    v2_contract_version: "1.1.0",
    backend_requested: "VULKAN_SPV",
    backend_executed: "VULKAN_SPV",
    vulkan_caps: { device_type: "CPU" },
    sync_barrier_count: 0,
    status_code: 0,
    v2_verification_status: "UNVERIFIED_RUNTIME_ONLY",
    v2_integrity_kind: "unkeyed_sha256_integrity",
    v2_zk_proof_present: false,
    v2_canonical_sha256: "f".repeat(64),
  }).valid,
  false
);

assert.equal(
  validateVulkanReceiptShape({
    v2_schema_version: "track21b.receipt.v1",
    v2_abi_version: "0x00010100",
    v2_contract_version: "1.1.0",
    backend_requested: "VULKAN_GPU",
    backend_executed: "VULKAN_GPU",
    vulkan_caps: { device_type: "CPU" },
    sync_barrier_count: 2,
    status_code: 0,
    v2_verification_status: "UNVERIFIED_RUNTIME_ONLY",
    v2_integrity_kind: "unkeyed_sha256_integrity",
    v2_zk_proof_present: false,
    v2_canonical_sha256: "f".repeat(64),
  }).reason,
  "cpu_device_claiming_gpu_backend"
);

console.log("MJS_V2_SDK_CONTRACT_OK");
