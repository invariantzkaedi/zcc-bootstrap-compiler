/* SPDX-License-Identifier: MIT */
#define _POSIX_C_SOURCE 200809L
#include "zcc_vulkan_compute.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static uint64_t t21b_monotonic_ns(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0ULL;
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

void zcc_vulkan_exec_options_init(zcc_vulkan_exec_options_t *opts) {
    if (!opts) return;
    memset(opts, 0, sizeof(*opts));
    zcc_exec_options_init(&opts->base);
    opts->abi_v2_version = ZCC_COMPUTE_V2_ABI_VERSION;
    opts->struct_size = (uint32_t)sizeof(*opts);
    opts->require_validation_layer = 0U;
    opts->require_hardware_gpu = 0U;
}

const char *zcc_backend_v2_str(uint32_t backend_id) {
    if (backend_id == ZCC_BACKEND_VULKAN_SPV) return "VULKAN_SPV";
    if (backend_id == ZCC_BACKEND_VULKAN_GPU) return "VULKAN_GPU";
    return zcc_backend_str(backend_id);
}

static int vk_caps_hw_gpu_ready(const zcc_vulkan_caps_t *vk_caps) {
    if (!vk_caps || !vk_caps->vulkan_available || !vk_caps->has_compute_queue) return 0;
    if (vk_caps->max_push_constants_size < sizeof(zcc_spv_push_t)) return 0;
    return (vk_caps->physical_device_type == ZCC_VK_DEVTYPE_DISCRETE_GPU ||
            vk_caps->physical_device_type == ZCC_VK_DEVTYPE_INTEGRATED_GPU) ? 1 : 0;
}

static int vk_caps_spv_ready(const zcc_vulkan_caps_t *vk_caps, uint32_t require_hw_gpu) {
    if (!vk_caps || !vk_caps->vulkan_available || !vk_caps->has_compute_queue) return 0;
    if (vk_caps->max_push_constants_size < sizeof(zcc_spv_push_t)) return 0;
    if (require_hw_gpu && !vk_caps_hw_gpu_ready(vk_caps)) return 0;
    return 1;
}

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
) {
    if (!cpu_caps || !out_selected_backend || !out_fallback_occurred) {
        return ZCC_STATUS_ERR_NULL_PTR;
    }
    if (selection_policy != ZCC_POLICY_EXACT_REQUIRED &&
        selection_policy != ZCC_POLICY_AUTO_FALLBACK) {
        return ZCC_STATUS_ERR_INVALID_ARG;
    }

    if (requested_backend <= ZCC_BACKEND_PTX_SM120) {
        return zcc_negotiate_backend_with_caps(
            cpu_caps, ptx_caps, requested_backend, selection_policy,
            out_selected_backend, out_fallback_occurred, reason_buf, reason_cap
        );
    }

    *out_fallback_occurred = 0U;
    *out_selected_backend = ZCC_BACKEND_SCALAR_C99;
    int avx2_ok = zcc_cpu_has_avx2_fma(cpu_caps);
    int spv_ok  = vk_caps_spv_ready(vk_caps, require_hardware_gpu);
    int gpu_ok  = vk_caps_hw_gpu_ready(vk_caps);

    if (requested_backend == ZCC_BACKEND_VULKAN_GPU) {
        if (gpu_ok) {
            *out_selected_backend = ZCC_BACKEND_VULKAN_GPU;
            if (reason_buf && reason_cap) snprintf(reason_buf, reason_cap, "explicit_vulkan_hardware_gpu_verified");
            return ZCC_STATUS_OK;
        }
        if (selection_policy == ZCC_POLICY_AUTO_FALLBACK) {
            *out_fallback_occurred = 1U;
            if (vk_caps_spv_ready(vk_caps, 0U)) {
                *out_selected_backend = ZCC_BACKEND_VULKAN_SPV;
                if (reason_buf && reason_cap) snprintf(reason_buf, reason_cap, "fallback_vulkan_cpu_no_hw_gpu");
                return ZCC_STATUS_OK;
            }
            if (avx2_ok) {
                *out_selected_backend = ZCC_BACKEND_AVX2_FMA;
                if (reason_buf && reason_cap) snprintf(reason_buf, reason_cap, "fallback_avx2_no_vulkan_gpu");
                return ZCC_STATUS_OK;
            }
            *out_selected_backend = ZCC_BACKEND_SCALAR_C99;
            if (reason_buf && reason_cap) snprintf(reason_buf, reason_cap, "fallback_scalar_no_vulkan_gpu");
            return ZCC_STATUS_OK;
        }
        if (reason_buf && reason_cap) snprintf(reason_buf, reason_cap, "rejected_vulkan_gpu_only_cpu_or_unavailable");
        return (vk_caps && vk_caps->vulkan_available) ? ZCC_STATUS_ERR_UNSUPPORTED_HW : ZCC_STATUS_ERR_BACKEND_UNAVAILABLE;
    }

    if (requested_backend == ZCC_BACKEND_VULKAN_SPV) {
        if (spv_ok) {
            *out_selected_backend = ZCC_BACKEND_VULKAN_SPV;
            if (reason_buf && reason_cap) {
                snprintf(reason_buf, reason_cap, "explicit_vulkan_spv_verified_%s",
                         vk_caps ? vk_caps->device_type_str : "UNKNOWN");
            }
            return ZCC_STATUS_OK;
        }
        if (selection_policy == ZCC_POLICY_AUTO_FALLBACK) {
            *out_fallback_occurred = 1U;
            if (avx2_ok) {
                *out_selected_backend = ZCC_BACKEND_AVX2_FMA;
                if (reason_buf && reason_cap) snprintf(reason_buf, reason_cap, "fallback_avx2_missing_vulkan_spv");
                return ZCC_STATUS_OK;
            }
            *out_selected_backend = ZCC_BACKEND_SCALAR_C99;
            if (reason_buf && reason_cap) snprintf(reason_buf, reason_cap, "fallback_scalar_missing_vulkan_spv");
            return ZCC_STATUS_OK;
        }
        if (reason_buf && reason_cap) snprintf(reason_buf, reason_cap, "rejected_vulkan_spv_unavailable_or_restricted");
        return (vk_caps && vk_caps->vulkan_available) ? ZCC_STATUS_ERR_UNSUPPORTED_HW : ZCC_STATUS_ERR_BACKEND_UNAVAILABLE;
    }

    if (reason_buf && reason_cap) snprintf(reason_buf, reason_cap, "invalid_v2_backend_id");
    return ZCC_STATUS_ERR_INVALID_ARG;
}

static void clamp_vk_caps(zcc_vulkan_caps_t *eff, const zcc_vulkan_caps_t *live, const zcc_vulkan_caps_t *restr) {
    *eff = *live;
    if (!restr) return;
    eff->vulkan_available &= restr->vulkan_available;
    eff->has_compute_queue &= restr->has_compute_queue;
    eff->validation_layer_available &= restr->validation_layer_available;
    if (restr->max_push_constants_size < eff->max_push_constants_size) {
        eff->max_push_constants_size = restr->max_push_constants_size;
    }
    if (restr->max_storage_buffer_range < eff->max_storage_buffer_range) {
        eff->max_storage_buffer_range = restr->max_storage_buffer_range;
    }
    if (restr->physical_device_type == ZCC_VK_DEVTYPE_CPU && eff->physical_device_type != ZCC_VK_DEVTYPE_CPU) {
        eff->physical_device_type = ZCC_VK_DEVTYPE_CPU;
        snprintf(eff->device_type_str, sizeof(eff->device_type_str), "CPU");
    }
}

static void clamp_cpu(zcc_cpu_caps_t *eff, const zcc_cpu_caps_t *live, const zcc_cpu_caps_t *restr) {
    *eff = *live;
    if (!restr) return;
    eff->has_cpuid &= restr->has_cpuid;
    eff->has_sse2 &= restr->has_sse2;
    eff->has_osxsave &= restr->has_osxsave;
    eff->xcr0_xmm &= restr->xcr0_xmm;
    eff->xcr0_ymm &= restr->xcr0_ymm;
    eff->has_avx &= restr->has_avx;
    eff->has_f16c &= restr->has_f16c;
    eff->has_fma &= restr->has_fma;
    eff->has_avx2 &= restr->has_avx2;
    eff->has_avx512f &= restr->has_avx512f;
    eff->xcr0_raw &= restr->xcr0_raw;
}

static void hash_io_meta_and_bufs(
    zcc_op_t op,
    const zcc_tensor_desc_t *inputs,
    uint32_t num_inputs,
    const zcc_tensor_desc_t *output,
    zcc_compute_receipt_t *rec
) {
    zcc_sha256_ctx_t ctx_d, ctx_b;
    zcc_sha256_init(&ctx_d);
    zcc_sha256_init(&ctx_b);
    for (uint32_t i = 0U; i < num_inputs; i++) {
        zcc_tensor_desc_t meta = inputs[i];
        meta.data = NULL;
        zcc_sha256_update(&ctx_d, &meta, sizeof(meta));
        if (inputs[i].data && inputs[i].nbytes > 0ULL) {
            zcc_sha256_update(&ctx_b, inputs[i].data, (size_t)inputs[i].nbytes);
        }
    }
    uint8_t dig[32];
    zcc_sha256_final(&ctx_d, dig);
    zcc_sha256_digest_to_hex(dig, rec->inputs_descriptor_sha256);
    zcc_sha256_final(&ctx_b, dig);
    zcc_sha256_digest_to_hex(dig, rec->input_buffers_sha256);

    zcc_tensor_desc_t ometa = *output;
    ometa.data = NULL;
    zcc_sha256_hex(&ometa, sizeof(ometa), rec->output_descriptor_sha256);

    rec->input_count = num_inputs;
    rec->primary_dtype = inputs[0].dtype;
    if (op == ZCC_OP_GEMM_F32) {
        rec->dim_m = inputs[0].shape[0];
        rec->dim_k = inputs[0].shape[1];
        rec->dim_n = inputs[1].shape[1];
    } else if (op == ZCC_OP_DOT_F32) {
        rec->dim_m = 1; rec->dim_n = 1; rec->dim_k = inputs[0].shape[0];
    } else if (inputs[0].rank == 1U) {
        rec->dim_m = 1; rec->dim_n = inputs[0].shape[0]; rec->dim_k = inputs[0].shape[0];
    } else {
        rec->dim_m = inputs[0].shape[0]; rec->dim_n = inputs[0].shape[1]; rec->dim_k = inputs[0].shape[1];
    }
}

static int build_v2_canonical_core(const zcc_compute_v2_receipt_t *r, char *out, size_t cap) {
    return snprintf(
        out, cap,
        "v2schema=%s|v2abi=%u|v2contract=%s|basecanon=%s|breq=%u|bexec=%u|"
        "spvsha=%s|spvwords=%u|vkavail=%u|vktype=%s|vkname=%s|vkvendor=0x%04x|"
        "vkapi=%u|vkval=%u|bar=%u|fl=%u|inv=%u|valerr=%u|status=%d|outb=%s|v2stat=%s|v2ikind=%s|v2zk=%u",
        r->v2_schema_version,
        r->v2_abi_version,
        r->v2_contract_version,
        r->base.receipt_canonical_sha256,
        r->base.backend_requested,
        r->base.backend_executed,
        r->spirv_module_sha256_hex,
        r->spirv_word_count,
        r->vulkan_caps.vulkan_available,
        r->vulkan_caps.device_type_str,
        r->vulkan_caps.device_name,
        r->vulkan_caps.vendor_id,
        r->vulkan_caps.api_version,
        r->vulkan_caps.validation_layer_enabled,
        r->sync_barrier_count,
        r->mem_flush_count,
        r->mem_invalidate_count,
        r->validation_error_count,
        r->base.status_code,
        r->base.output_buffer_sha256,
        r->v2_verification_status,
        r->v2_integrity_kind,
        r->v2_zk_proof_present
    );
}

zcc_status_t zcc_receipt_v2_seal(zcc_compute_v2_receipt_t *receipt, const uint8_t *hmac_key, size_t hmac_key_len) {
    if (!receipt) return ZCC_STATUS_ERR_NULL_PTR;
    if (strcmp(receipt->v2_schema_version, ZCC_COMPUTE_V2_RECEIPT_SCHEMA) != 0) {
        return ZCC_STATUS_ERR_RECEIPT_TAMPER;
    }
    if (hmac_key && hmac_key_len > 0U) {
        snprintf(receipt->v2_integrity_kind, sizeof(receipt->v2_integrity_kind), "hmac_sha256_attested");
    } else {
        snprintf(receipt->v2_integrity_kind, sizeof(receipt->v2_integrity_kind), "unkeyed_sha256_integrity");
        receipt->v2_hmac_sha256_hex[0] = '\0';
    }
    char core[3072];
    int n = build_v2_canonical_core(receipt, core, sizeof(core));
    if (n <= 0 || (size_t)n >= sizeof(core)) return ZCC_STATUS_ERR_OVERFLOW;
    zcc_sha256_hex(core, (size_t)n, receipt->v2_canonical_sha256_hex);
    if (hmac_key && hmac_key_len > 0U) {
        zcc_hmac_sha256_hex(hmac_key, hmac_key_len, core, (size_t)n, receipt->v2_hmac_sha256_hex);
    }
    return ZCC_STATUS_OK;
}

zcc_status_t zcc_receipt_v2_verify_canonical(const zcc_compute_v2_receipt_t *receipt, const uint8_t *hmac_key, size_t hmac_key_len) {
    if (!receipt) return ZCC_STATUS_ERR_NULL_PTR;
    if (receipt->v2_struct_size != sizeof(*receipt) ||
        receipt->v2_abi_version != ZCC_COMPUTE_V2_ABI_VERSION ||
        strcmp(receipt->v2_schema_version, ZCC_COMPUTE_V2_RECEIPT_SCHEMA) != 0 ||
        strcmp(receipt->v2_contract_version, ZCC_COMPUTE_V2_CONTRACT_VER) != 0) {
        return ZCC_STATUS_ERR_RECEIPT_TAMPER;
    }
    if (strcmp(receipt->v2_verification_status, "UNVERIFIED_RUNTIME_ONLY") != 0 ||
        receipt->v2_zk_proof_present != 0U) {
        return ZCC_STATUS_ERR_RECEIPT_TAMPER;
    }
    if (zcc_receipt_verify_canonical(&receipt->base, hmac_key, hmac_key_len) != ZCC_STATUS_OK) {
        return ZCC_STATUS_ERR_RECEIPT_TAMPER;
    }
    char core[3072];
    int n = build_v2_canonical_core(receipt, core, sizeof(core));
    if (n <= 0 || (size_t)n >= sizeof(core)) return ZCC_STATUS_ERR_RECEIPT_TAMPER;
    char exp_sha[65];
    zcc_sha256_hex(core, (size_t)n, exp_sha);
    if (strcmp(exp_sha, receipt->v2_canonical_sha256_hex) != 0) {
        return ZCC_STATUS_ERR_RECEIPT_TAMPER;
    }
    if (hmac_key && hmac_key_len > 0U) {
        if (strcmp(receipt->v2_integrity_kind, "hmac_sha256_attested") != 0) return ZCC_STATUS_ERR_RECEIPT_TAMPER;
        char exp_hmac[65];
        zcc_hmac_sha256_hex(hmac_key, hmac_key_len, core, (size_t)n, exp_hmac);
        if (strcmp(exp_hmac, receipt->v2_hmac_sha256_hex) != 0) return ZCC_STATUS_ERR_RECEIPT_TAMPER;
    }
    return ZCC_STATUS_OK;
}

static void json_esc2(const char *src, char *dst, size_t cap) {
    size_t w = 0U;
    if (!dst || cap == 0U) return;
    if (!src) { dst[0] = '\0'; return; }
    for (size_t i = 0U; src[i] != '\0' && w + 3U < cap; i++) {
        unsigned char c = (unsigned char)src[i];
        if (c == '"' || c == '\\') { dst[w++] = '\\'; dst[w++] = (char)c; }
        else if (c < 0x20U) { dst[w++] = ' '; }
        else { dst[w++] = (char)c; }
    }
    dst[w] = '\0';
}

int zcc_receipt_v2_to_json(const zcc_compute_v2_receipt_t *r, char *buf, size_t cap) {
    if (!r || !buf || cap == 0U) return -1;
    char base_json[3584], vk_name_esc[384], err_esc[256], reason_esc[256];
    zcc_receipt_to_json(&r->base, base_json, sizeof(base_json));
    json_esc2(r->vulkan_caps.device_name, vk_name_esc, sizeof(vk_name_esc));
    json_esc2(r->base.error_message, err_esc, sizeof(err_esc));
    json_esc2(r->base.selection_reason, reason_esc, sizeof(reason_esc));
    return snprintf(
        buf, cap,
        "{\n"
        "  \"v2_schema_version\": \"%s\",\n"
        "  \"v2_abi_version\": \"0x%08x\",\n"
        "  \"v2_contract_version\": \"%s\",\n"
        "  \"backend_requested\": \"%s\",\n"
        "  \"backend_executed\": \"%s\",\n"
        "  \"selection_reason\": \"%s\",\n"
        "  \"spirv_module_sha256\": \"%s\",\n"
        "  \"spirv_word_count\": %u,\n"
        "  \"vulkan_caps\": {\n"
        "    \"vulkan_available\": %u,\n"
        "    \"device_type\": \"%s\",\n"
        "    \"device_name\": \"%s\",\n"
        "    \"vendor_id\": \"0x%04x\",\n"
        "    \"api_version\": %u,\n"
        "    \"validation_layer_enabled\": %u,\n"
        "    \"max_push_constants_size\": %u,\n"
        "    \"max_storage_buffer_range\": %llu\n"
        "  },\n"
        "  \"sync_barrier_count\": %u,\n"
        "  \"mem_flush_count\": %u,\n"
        "  \"mem_invalidate_count\": %u,\n"
        "  \"validation_error_count\": %u,\n"
        "  \"status_code\": %d,\n"
        "  \"error_message\": \"%s\",\n"
        "  \"output_buffer_sha256\": \"%s\",\n"
        "  \"v2_verification_status\": \"%s\",\n"
        "  \"v2_integrity_kind\": \"%s\",\n"
        "  \"v2_zk_proof_present\": %s,\n"
        "  \"v2_canonical_sha256\": \"%s\",\n"
        "  \"v2_hmac_sha256\": \"%s\",\n"
        "  \"base_receipt\": %s\n"
        "}",
        r->v2_schema_version,
        r->v2_abi_version,
        r->v2_contract_version,
        zcc_backend_v2_str(r->base.backend_requested),
        r->base.backend_executed ? zcc_backend_v2_str(r->base.backend_executed) : "NONE",
        reason_esc,
        r->spirv_module_sha256_hex,
        r->spirv_word_count,
        r->vulkan_caps.vulkan_available,
        r->vulkan_caps.device_type_str,
        vk_name_esc,
        r->vulkan_caps.vendor_id,
        r->vulkan_caps.api_version,
        r->vulkan_caps.validation_layer_enabled,
        r->vulkan_caps.max_push_constants_size,
        (unsigned long long)r->vulkan_caps.max_storage_buffer_range,
        r->sync_barrier_count,
        r->mem_flush_count,
        r->mem_invalidate_count,
        r->validation_error_count,
        r->base.status_code,
        err_esc,
        r->base.output_buffer_sha256,
        r->v2_verification_status,
        r->v2_integrity_kind,
        r->v2_zk_proof_present ? "true" : "false",
        r->v2_canonical_sha256_hex,
        r->v2_hmac_sha256_hex[0] ? r->v2_hmac_sha256_hex : "none",
        base_json
    );
}

zcc_status_t zcc_compute_exec_v2_with_caps(
    zcc_op_t op,
    const zcc_tensor_desc_t *inputs,
    uint32_t num_inputs,
    zcc_tensor_desc_t *output,
    const zcc_vulkan_exec_options_t *opts,
    const zcc_cpu_caps_t *restricted_cpu_caps,
    const zcc_vulkan_caps_t *restricted_vk_caps,
    zcc_compute_v2_receipt_t *out_receipt
) {
    if (!opts) return ZCC_STATUS_ERR_NULL_PTR;
    if (opts->abi_v2_version != ZCC_COMPUTE_V2_ABI_VERSION || opts->struct_size != sizeof(*opts)) {
        return ZCC_STATUS_ERR_ABI_MISMATCH;
    }

    zcc_compute_v2_receipt_t local_rec;
    zcc_compute_v2_receipt_t *rec = out_receipt ? out_receipt : &local_rec;
    memset(rec, 0, sizeof(*rec));
    rec->v2_struct_size = (uint32_t)sizeof(*rec);
    rec->v2_abi_version = ZCC_COMPUTE_V2_ABI_VERSION;
    snprintf(rec->v2_schema_version, sizeof(rec->v2_schema_version), "%s", ZCC_COMPUTE_V2_RECEIPT_SCHEMA);
    snprintf(rec->v2_contract_version, sizeof(rec->v2_contract_version), "%s", ZCC_COMPUTE_V2_CONTRACT_VER);
    snprintf(rec->spirv_module_sha256_hex, sizeof(rec->spirv_module_sha256_hex), "none");

    zcc_vulkan_caps_t live_vk, eff_vk;
    zcc_probe_vulkan_caps(&live_vk, 1);
    clamp_vk_caps(&eff_vk, &live_vk, restricted_vk_caps);

    if (opts->base.requested_backend <= ZCC_BACKEND_PTX_SM120) {
        zcc_status_t st = zcc_compute_exec_with_caps(
            op, inputs, num_inputs, output, &opts->base, restricted_cpu_caps, &rec->base
        );
        rec->vulkan_caps = eff_vk;
        snprintf(rec->v2_verification_status, sizeof(rec->v2_verification_status), "UNVERIFIED_RUNTIME_ONLY");
        rec->v2_zk_proof_present = 0U;
        if (opts->base.emit_receipt) {
            zcc_receipt_v2_seal(rec, opts->base.hmac_key, opts->base.hmac_key_len);
        }
        return st;
    }

    uint64_t t0 = t21b_monotonic_ns();
    char err_msg[192];
    memset(err_msg, 0, sizeof(err_msg));

    zcc_cpu_caps_t live_cpu, eff_cpu;
    zcc_ptx_caps_t ptx_caps;
    memset(&ptx_caps, 0, sizeof(ptx_caps));
    ptx_caps.struct_size = (uint32_t)sizeof(ptx_caps);
    zcc_probe_cpu_caps(&live_cpu);
    clamp_cpu(&eff_cpu, &live_cpu, restricted_cpu_caps);

    zcc_status_t st = zcc_compute_validate(op, inputs, num_inputs, output, &opts->base, err_msg, sizeof(err_msg));
    uint32_t sel_bk = 0U, fb = 0U;
    char reason[128] = {0};

    if (st == ZCC_STATUS_OK) {
        st = zcc_negotiate_backend_v2_with_caps(
            &eff_cpu, &ptx_caps, &eff_vk,
            opts->base.requested_backend, opts->base.selection_policy,
            opts->require_hardware_gpu, &sel_bk, &fb, reason, sizeof(reason)
        );
        if (st == ZCC_STATUS_OK && (sel_bk == ZCC_BACKEND_VULKAN_SPV || sel_bk == ZCC_BACKEND_VULKAN_GPU) &&
            opts->require_validation_layer && !eff_vk.validation_layer_available) {
            st = ZCC_STATUS_ERR_UNSUPPORTED_HW;
            snprintf(err_msg, sizeof(err_msg), "VK_LAYER_KHRONOS_validation required but unavailable or restricted");
        }
        if (st == ZCC_STATUS_OK && (sel_bk == ZCC_BACKEND_VULKAN_SPV || sel_bk == ZCC_BACKEND_VULKAN_GPU) &&
            opts->base.accum_policy == ZCC_ACCUM_FP64_REF) {
            if (opts->base.selection_policy == ZCC_POLICY_AUTO_FALLBACK) {
                sel_bk = ZCC_BACKEND_SCALAR_C99;
                fb = 1U;
                snprintf(reason, sizeof(reason), "fallback_scalar_for_fp64_accum");
            } else {
                st = ZCC_STATUS_ERR_UNSUPPORTED_HW;
                snprintf(err_msg, sizeof(err_msg), "VULKAN_SPV does not assume Float64 capability for FP64_REF");
            }
        }
    }

    /* Handle fallback to CPU backends (Scalar/AVX2) cleanly when negotiated */
    if (st == ZCC_STATUS_OK && (sel_bk == ZCC_BACKEND_SCALAR_C99 || sel_bk == ZCC_BACKEND_AVX2_FMA)) {
        zcc_exec_options_t fb_opts = opts->base;
        fb_opts.requested_backend = sel_bk;
        fb_opts.selection_policy = ZCC_POLICY_EXACT_REQUIRED;
        st = zcc_compute_exec_with_caps(op, inputs, num_inputs, output, &fb_opts, restricted_cpu_caps, &rec->base);
        rec->base.backend_requested = opts->base.requested_backend;
        rec->base.fallback_occurred = fb;
        snprintf(rec->base.selection_reason, sizeof(rec->base.selection_reason), "%s", reason);
        zcc_receipt_seal(&rec->base, opts->base.hmac_key, opts->base.hmac_key_len);
        rec->vulkan_caps = eff_vk;
        snprintf(rec->v2_verification_status, sizeof(rec->v2_verification_status), "UNVERIFIED_RUNTIME_ONLY");
        rec->v2_zk_proof_present = 0U;
        if (opts->base.emit_receipt) {
            zcc_receipt_v2_seal(rec, opts->base.hmac_key, opts->base.hmac_key_len);
        }
        return st;
    }

    rec->base.struct_size = (uint32_t)sizeof(rec->base);
    rec->base.abi_version = ZCC_COMPUTE_ABI_VERSION;
    snprintf(rec->base.schema_version, sizeof(rec->base.schema_version), "%s", ZCC_COMPUTE_RECEIPT_SCHEMA);
    snprintf(rec->base.contract_version, sizeof(rec->base.contract_version), "%s", ZCC_COMPUTE_CONTRACT_VERSION);
    snprintf(rec->base.compiler_id, sizeof(rec->base.compiler_id), "%s",
             opts->base.compiler_id ? opts->base.compiler_id : "zcc-spirv-vulkan-21b");
    snprintf(rec->base.compiler_sha256_hex, sizeof(rec->base.compiler_sha256_hex), "%s",
             opts->base.compiler_sha256_hex ? opts->base.compiler_sha256_hex : "unrecorded");
    snprintf(rec->base.executable_sha256_hex, sizeof(rec->base.executable_sha256_hex), "%s",
             opts->base.executable_sha256_hex ? opts->base.executable_sha256_hex : "unrecorded");
    snprintf(rec->base.ptx_module_sha256_hex, sizeof(rec->base.ptx_module_sha256_hex), "none");
    rec->base.cpu_caps = eff_cpu;
    rec->base.ptx_caps = ptx_caps;
    rec->vulkan_caps = eff_vk;
    rec->base.op_id = (uint32_t)op;
    rec->base.backend_requested = opts->base.requested_backend;
    rec->base.selection_policy = opts->base.selection_policy;
    rec->base.accum_policy = opts->base.accum_policy;
    rec->base.determinism_mode = opts->base.determinism_mode;
    rec->base.seed = opts->base.seed;

    if (inputs && num_inputs > 0U && output) {
        hash_io_meta_and_bufs(op, inputs, num_inputs, output, &rec->base);
    }

    if (st == ZCC_STATUS_OK && opts->base.alloc_fn) {
        void *guard = opts->base.alloc_fn(64U, opts->base.alloc_ctx);
        if (!guard) {
            st = ZCC_STATUS_ERR_OOM;
            snprintf(err_msg, sizeof(err_msg), "Workspace allocation failed (OOM)");
        } else if (opts->base.free_fn) {
            opts->base.free_fn(guard, opts->base.alloc_ctx);
        } else {
            free(guard);
        }
    }

    if (st == ZCC_STATUS_OK) {
        rec->base.backend_executed = sel_bk;
        rec->base.fallback_occurred = fb;
        snprintf(rec->base.selection_reason, sizeof(rec->base.selection_reason), "%s", reason);
        st = zcc_vulkan_exec(
            op, inputs, num_inputs, output, opts, &eff_vk,
            rec->spirv_module_sha256_hex, &rec->spirv_word_count,
            &rec->sync_barrier_count, &rec->mem_flush_count,
            &rec->mem_invalidate_count, &rec->validation_error_count,
            err_msg, sizeof(err_msg)
        );
        if (st != ZCC_STATUS_OK) {
            rec->base.backend_executed = 0U;
        }
    } else {
        rec->base.backend_executed = 0U;
        rec->base.fallback_occurred = fb;
        snprintf(rec->base.selection_reason, sizeof(rec->base.selection_reason), "%s",
                 reason[0] ? reason : "validation_or_negotiation_failed");
    }

    if (st == ZCC_STATUS_OK && output && output->data && output->nbytes > 0ULL) {
        zcc_sha256_hex(output->data, (size_t)output->nbytes, rec->base.output_buffer_sha256);
    } else {
        snprintf(rec->base.output_buffer_sha256, sizeof(rec->base.output_buffer_sha256),
                 "0000000000000000000000000000000000000000000000000000000000000000");
    }

    rec->base.status_code = (int32_t)st;
    snprintf(rec->base.execution_status, sizeof(rec->base.execution_status), "%s", zcc_status_str(st));
    snprintf(rec->base.error_message, sizeof(rec->base.error_message), "%s", err_msg);
    snprintf(rec->base.verification_status, sizeof(rec->base.verification_status), "UNVERIFIED_RUNTIME_ONLY");
    rec->base.zk_proof_present = 0U;
    rec->base.elapsed_ns = t21b_monotonic_ns() - t0;

    snprintf(rec->v2_verification_status, sizeof(rec->v2_verification_status), "UNVERIFIED_RUNTIME_ONLY");
    rec->v2_zk_proof_present = 0U;

    if (opts->base.emit_receipt) {
        zcc_receipt_seal(&rec->base, opts->base.hmac_key, opts->base.hmac_key_len);
        zcc_receipt_v2_seal(rec, opts->base.hmac_key, opts->base.hmac_key_len);
    }
    return st;
}

zcc_status_t zcc_compute_exec_v2(
    zcc_op_t op,
    const zcc_tensor_desc_t *inputs,
    uint32_t num_inputs,
    zcc_tensor_desc_t *output,
    const zcc_vulkan_exec_options_t *opts,
    zcc_compute_v2_receipt_t *out_receipt
) {
    return zcc_compute_exec_v2_with_caps(op, inputs, num_inputs, output, opts, NULL, NULL, out_receipt);
}
