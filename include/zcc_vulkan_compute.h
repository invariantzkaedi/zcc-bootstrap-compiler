/* SPDX-License-Identifier: MIT */
#ifndef ZCC_VULKAN_COMPUTE_H
#define ZCC_VULKAN_COMPUTE_H

#include "zcc_compute.h"
#include "zcc_spirv.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ZCC_COMPUTE_V2_ABI_VERSION    0x00010100U
#define ZCC_COMPUTE_V2_CONTRACT_VER   "1.1.0"
#define ZCC_COMPUTE_V2_RECEIPT_SCHEMA "track21b.receipt.v1"

/* Additive backend IDs extending zcc_backend_id_t (Track 21A 0..3 preserved verbatim) */
#define ZCC_BACKEND_VULKAN_SPV 4U /* Any capability-verified Vulkan compute device (explicitly records CPU vs GPU) */
#define ZCC_BACKEND_VULKAN_GPU 5U /* Hardware GPU required (DISCRETE_GPU or INTEGRATED_GPU; rejects CPU-only llvmpipe) */

/* Vulkan VkPhysicalDeviceType numeric values */
#define ZCC_VK_DEVTYPE_OTHER          0U
#define ZCC_VK_DEVTYPE_INTEGRATED_GPU 1U
#define ZCC_VK_DEVTYPE_DISCRETE_GPU   2U
#define ZCC_VK_DEVTYPE_VIRTUAL_GPU    3U
#define ZCC_VK_DEVTYPE_CPU            4U

typedef struct zcc_vulkan_caps_s {
    uint32_t struct_size;
    uint32_t vulkan_available;
    uint32_t validation_layer_available;
    uint32_t validation_layer_enabled;
    uint32_t physical_device_count;
    uint32_t selected_device_index;
    uint32_t physical_device_type;     /* ZCC_VK_DEVTYPE_* */
    uint32_t api_version;
    uint32_t driver_version;
    uint32_t vendor_id;
    uint32_t device_id;
    uint32_t compute_queue_family;
    uint32_t has_compute_queue;
    uint32_t max_push_constants_size;
    uint32_t max_compute_shared_mem;
    uint32_t max_compute_invocations;
    uint32_t max_workgroup_size[3];
    uint32_t max_workgroup_count[3];
    uint64_t max_storage_buffer_range;
    uint64_t non_coherent_atom_size;
    uint32_t host_coherent_mem_available;
    uint32_t float16_int8_supported;
    char     device_name[256];
    char     device_type_str[32];
} zcc_vulkan_caps_t;

typedef struct zcc_vulkan_exec_options_s {
    zcc_exec_options_t base;           /* Preserves Track 21A zcc_exec_options_t verbatim */
    uint32_t abi_v2_version;           /* Must equal ZCC_COMPUTE_V2_ABI_VERSION */
    uint32_t struct_size;              /* Must equal sizeof(zcc_vulkan_exec_options_t) */
    uint32_t require_validation_layer; /* 1 = fail closed if VK_LAYER_KHRONOS_validation missing */
    uint32_t require_hardware_gpu;     /* 1 = forbid VK_PHYSICAL_DEVICE_TYPE_CPU (llvmpipe) */
    const uint32_t *spirv_override_words;
    size_t   spirv_override_word_count;
} zcc_vulkan_exec_options_t;

typedef struct zcc_compute_v2_receipt_s {
    zcc_compute_receipt_t base;        /* Nested Track 21A receipt fields */
    uint32_t v2_struct_size;
    uint32_t v2_abi_version;
    char     v2_schema_version[32];
    char     v2_contract_version[16];
    zcc_vulkan_caps_t vulkan_caps;
    char     spirv_module_sha256_hex[65];
    uint32_t spirv_word_count;
    uint32_t sync_barrier_count;
    uint32_t mem_flush_count;
    uint32_t mem_invalidate_count;
    uint32_t validation_error_count;
    char     v2_verification_status[32]; /* Always "UNVERIFIED_RUNTIME_ONLY" from runtime */
    char     v2_integrity_kind[40];
    uint32_t v2_zk_proof_present;        /* Always 0 */
    char     v2_canonical_sha256_hex[65];
    char     v2_hmac_sha256_hex[65];
} zcc_compute_v2_receipt_t;

/* Option initializer & device type strings */
void         zcc_vulkan_exec_options_init(zcc_vulkan_exec_options_t *opts);
const char  *zcc_vulkan_device_type_str(uint32_t dev_type);
const char  *zcc_backend_v2_str(uint32_t backend_id);

/* Live probing & restricted capability negotiation (CPU + PTX + Vulkan) */
zcc_status_t zcc_probe_vulkan_caps(zcc_vulkan_caps_t *out_caps, int enable_validation_layer);
zcc_status_t zcc_negotiate_backend_v2_with_caps(
    const zcc_cpu_caps_t *cpu_caps,
    const zcc_ptx_caps_t *ptx_caps,
    const zcc_vulkan_caps_t *vk_caps,
    uint32_t requested_backend,
    uint32_t selection_policy,
    uint32_t require_hardware_gpu,
    uint32_t *out_selected_backend,
    uint32_t *out_fallback_occurred,
    char *reason_buf,
    size_t reason_cap
);

/* Direct Vulkan execution entrypoint */
zcc_status_t zcc_vulkan_exec(
    zcc_op_t op,
    const zcc_tensor_desc_t *inputs,
    uint32_t num_inputs,
    zcc_tensor_desc_t *output,
    const zcc_vulkan_exec_options_t *opts,
    const zcc_vulkan_caps_t *caps,
    char *spirv_sha_out,
    uint32_t *out_word_count,
    uint32_t *out_barriers,
    uint32_t *out_flushes,
    uint32_t *out_invalidates,
    uint32_t *out_val_errors,
    char *err_buf,
    size_t err_cap
);

/* Unified Track 21B dispatch supporting backends 0..5 with full provenance receipt */
zcc_status_t zcc_compute_exec_v2(
    zcc_op_t op,
    const zcc_tensor_desc_t *inputs,
    uint32_t num_inputs,
    zcc_tensor_desc_t *output,
    const zcc_vulkan_exec_options_t *opts,
    zcc_compute_v2_receipt_t *out_receipt
);

/* Down-gated test entrypoint accepting explicit restricted CPU + Vulkan capability caps */
zcc_status_t zcc_compute_exec_v2_with_caps(
    zcc_op_t op,
    const zcc_tensor_desc_t *inputs,
    uint32_t num_inputs,
    zcc_tensor_desc_t *output,
    const zcc_vulkan_exec_options_t *opts,
    const zcc_cpu_caps_t *restricted_cpu_caps,
    const zcc_vulkan_caps_t *restricted_vk_caps,
    zcc_compute_v2_receipt_t *out_receipt
);

/* Track 21B receipt sealing, serialization, and independent canonical verification */
zcc_status_t zcc_receipt_v2_seal(zcc_compute_v2_receipt_t *receipt, const uint8_t *hmac_key, size_t hmac_key_len);
int          zcc_receipt_v2_to_json(const zcc_compute_v2_receipt_t *receipt, char *buf, size_t cap);
zcc_status_t zcc_receipt_v2_verify_canonical(const zcc_compute_v2_receipt_t *receipt, const uint8_t *hmac_key, size_t hmac_key_len);

#ifdef __cplusplus
}
#endif

#endif /* ZCC_VULKAN_COMPUTE_H */
