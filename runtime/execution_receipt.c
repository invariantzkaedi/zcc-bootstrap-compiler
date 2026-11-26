/* SPDX-License-Identifier: MIT */
#include "zcc_compute.h"
#include <stdio.h>
#include <string.h>

static void json_escape(const char *src, char *dst, size_t cap) {
    if (!dst || cap == 0U) return;
    if (!src) { dst[0] = '\0'; return; }
    size_t w = 0U;
    for (size_t i = 0U; src[i] != '\0' && w + 2U < cap; i++) {
        unsigned char c = (unsigned char)src[i];
        if (c == '"' || c == '\\') {
            if (w + 3U >= cap) break;
            dst[w++] = '\\';
            dst[w++] = (char)c;
        } else if (c == '\n') {
            if (w + 3U >= cap) break;
            dst[w++] = '\\'; dst[w++] = 'n';
        } else if (c == '\r') {
            if (w + 3U >= cap) break;
            dst[w++] = '\\'; dst[w++] = 'r';
        } else if (c == '\t') {
            if (w + 3U >= cap) break;
            dst[w++] = '\\'; dst[w++] = 't';
        } else if (c < 0x20U) {
            dst[w++] = ' ';
        } else {
            dst[w++] = (char)c;
        }
    }
    dst[w] = '\0';
}

static int build_canonical_core(const zcc_compute_receipt_t *r, char *out, size_t cap) {
    return snprintf(
        out, cap,
        "schema=%s|abi=%u|contract=%s|compiler=%s|csha=%s|esha=%s|ptxsha=%s|"
        "breq=%u|bexec=%u|pol=%u|fb=%u|reason=%s|op=%u|dt=%u|acc=%u|det=%u|seed=%llu|"
        "in=%u|m=%lld|n=%lld|k=%lld|ind=%s|inb=%s|outd=%s|outb=%s|"
        "status=%d|estat=%s|vstat=%s|ikind=%s|zk=%u|ns=%llu|"
        "avx2=%u|osx=%u|xmm=%u|ymm=%u|fma=%u|xcr0=%llu|cuda=%u|cc=%d.%d",
        r->schema_version,
        r->abi_version,
        r->contract_version,
        r->compiler_id,
        r->compiler_sha256_hex,
        r->executable_sha256_hex,
        r->ptx_module_sha256_hex,
        r->backend_requested,
        r->backend_executed,
        r->selection_policy,
        r->fallback_occurred,
        r->selection_reason,
        r->op_id,
        r->primary_dtype,
        r->accum_policy,
        r->determinism_mode,
        (unsigned long long)r->seed,
        r->input_count,
        (long long)r->dim_m,
        (long long)r->dim_n,
        (long long)r->dim_k,
        r->inputs_descriptor_sha256,
        r->input_buffers_sha256,
        r->output_descriptor_sha256,
        r->output_buffer_sha256,
        r->status_code,
        r->execution_status,
        r->verification_status,
        r->integrity_kind,
        r->zk_proof_present,
        (unsigned long long)r->elapsed_ns,
        r->cpu_caps.has_avx2,
        r->cpu_caps.has_osxsave,
        r->cpu_caps.xcr0_xmm,
        r->cpu_caps.xcr0_ymm,
        r->cpu_caps.has_fma,
        (unsigned long long)r->cpu_caps.xcr0_raw,
        r->ptx_caps.cuda_available,
        r->ptx_caps.cc_major,
        r->ptx_caps.cc_minor
    );
}

zcc_status_t zcc_receipt_seal(zcc_compute_receipt_t *receipt, const uint8_t *hmac_key, size_t hmac_key_len) {
    if (!receipt) return ZCC_STATUS_ERR_NULL_PTR;
    if (strcmp(receipt->schema_version, ZCC_COMPUTE_RECEIPT_SCHEMA) != 0) {
        return ZCC_STATUS_ERR_RECEIPT_TAMPER;
    }
    if (hmac_key && hmac_key_len > 0U) {
        snprintf(receipt->integrity_kind, sizeof(receipt->integrity_kind), "hmac_sha256_attested");
    } else {
        snprintf(receipt->integrity_kind, sizeof(receipt->integrity_kind), "unkeyed_sha256_integrity");
        receipt->hmac_sha256_hex[0] = '\0';
    }

    char core[2048];
    int n = build_canonical_core(receipt, core, sizeof(core));
    if (n <= 0 || (size_t)n >= sizeof(core)) return ZCC_STATUS_ERR_OVERFLOW;

    zcc_sha256_hex(core, (size_t)n, receipt->receipt_canonical_sha256);
    if (hmac_key && hmac_key_len > 0U) {
        zcc_hmac_sha256_hex(hmac_key, hmac_key_len, core, (size_t)n, receipt->hmac_sha256_hex);
    }
    return ZCC_STATUS_OK;
}

zcc_status_t zcc_receipt_verify_canonical(const zcc_compute_receipt_t *receipt, const uint8_t *hmac_key, size_t hmac_key_len) {
    if (!receipt) return ZCC_STATUS_ERR_NULL_PTR;
    if (receipt->struct_size != sizeof(zcc_compute_receipt_t) ||
        receipt->abi_version != ZCC_COMPUTE_ABI_VERSION ||
        strcmp(receipt->schema_version, ZCC_COMPUTE_RECEIPT_SCHEMA) != 0 ||
        strcmp(receipt->contract_version, ZCC_COMPUTE_CONTRACT_VERSION) != 0) {
        return ZCC_STATUS_ERR_RECEIPT_TAMPER;
    }
    /* Per ZCC-EF-0, runtime-issued receipt MUST NOT self-certify PASS */
    if (strcmp(receipt->verification_status, "UNVERIFIED_RUNTIME_ONLY") != 0) {
        return ZCC_STATUS_ERR_RECEIPT_TAMPER;
    }
    if (receipt->zk_proof_present != 0U) {
        return ZCC_STATUS_ERR_RECEIPT_TAMPER;
    }

    char core[2048];
    int n = build_canonical_core(receipt, core, sizeof(core));
    if (n <= 0 || (size_t)n >= sizeof(core)) return ZCC_STATUS_ERR_RECEIPT_TAMPER;

    char expected_sha[65];
    zcc_sha256_hex(core, (size_t)n, expected_sha);
    if (strcmp(expected_sha, receipt->receipt_canonical_sha256) != 0) {
        return ZCC_STATUS_ERR_RECEIPT_TAMPER;
    }

    if (hmac_key && hmac_key_len > 0U) {
        if (strcmp(receipt->integrity_kind, "hmac_sha256_attested") != 0) {
            return ZCC_STATUS_ERR_RECEIPT_TAMPER;
        }
        char expected_hmac[65];
        zcc_hmac_sha256_hex(hmac_key, hmac_key_len, core, (size_t)n, expected_hmac);
        if (strcmp(expected_hmac, receipt->hmac_sha256_hex) != 0) {
            return ZCC_STATUS_ERR_RECEIPT_TAMPER;
        }
    }
    return ZCC_STATUS_OK;
}

int zcc_receipt_to_json(const zcc_compute_receipt_t *r, char *buf, size_t cap) {
    if (!r || !buf || cap == 0U) return -1;
    char esc_comp[128], esc_reason[256], esc_gpu[256], esc_err[384];
    json_escape(r->compiler_id, esc_comp, sizeof(esc_comp));
    json_escape(r->selection_reason, esc_reason, sizeof(esc_reason));
    json_escape(r->ptx_caps.device_name, esc_gpu, sizeof(esc_gpu));
    json_escape(r->error_message, esc_err, sizeof(esc_err));
    return snprintf(
        buf, cap,
        "{\n"
        "  \"schema_version\": \"%s\",\n"
        "  \"abi_version\": \"0x%08x\",\n"
        "  \"contract_version\": \"%s\",\n"
        "  \"compiler_id\": \"%s\",\n"
        "  \"compiler_sha256\": \"%s\",\n"
        "  \"executable_sha256\": \"%s\",\n"
        "  \"ptx_module_sha256\": \"%s\",\n"
        "  \"backend_requested\": \"%s\",\n"
        "  \"backend_executed\": \"%s\",\n"
        "  \"selection_policy\": %u,\n"
        "  \"fallback_occurred\": %s,\n"
        "  \"selection_reason\": \"%s\",\n"
        "  \"op_id\": %u,\n"
        "  \"op_name\": \"%s\",\n"
        "  \"dtype\": \"%s\",\n"
        "  \"accum_policy\": %u,\n"
        "  \"determinism_mode\": %u,\n"
        "  \"seed\": %llu,\n"
        "  \"input_count\": %u,\n"
        "  \"dims\": {\"m\": %lld, \"n\": %lld, \"k\": %lld},\n"
        "  \"hardware_caps\": {\n"
        "    \"cpuid\": %u, \"osxsave\": %u, \"xcr0_xmm\": %u, \"xcr0_ymm\": %u,\n"
        "    \"avx\": %u, \"avx2\": %u, \"fma\": %u, \"f16c\": %u,\n"
        "    \"xcr0_hex\": \"0x%llx\",\n"
        "    \"gpu_available\": %u, \"gpu_name\": \"%s\", \"gpu_cc\": \"%d.%d\"\n"
        "  },\n"
        "  \"inputs_descriptor_sha256\": \"%s\",\n"
        "  \"input_buffers_sha256\": \"%s\",\n"
        "  \"output_descriptor_sha256\": \"%s\",\n"
        "  \"output_buffer_sha256\": \"%s\",\n"
        "  \"status_code\": %d,\n"
        "  \"execution_status\": \"%s\",\n"
        "  \"error_message\": \"%s\",\n"
        "  \"verification_status\": \"%s\",\n"
        "  \"integrity_kind\": \"%s\",\n"
        "  \"zk_proof_present\": %s,\n"
        "  \"elapsed_ns\": %llu,\n"
        "  \"receipt_canonical_sha256\": \"%s\",\n"
        "  \"hmac_sha256\": \"%s\"\n"
        "}",
        r->schema_version,
        r->abi_version,
        r->contract_version,
        esc_comp,
        r->compiler_sha256_hex,
        r->executable_sha256_hex,
        r->ptx_module_sha256_hex,
        zcc_backend_str(r->backend_requested),
        r->backend_executed ? zcc_backend_str(r->backend_executed) : "NONE",
        r->selection_policy,
        r->fallback_occurred ? "true" : "false",
        esc_reason,
        r->op_id,
        zcc_op_str(r->op_id),
        zcc_dtype_str(r->primary_dtype),
        r->accum_policy,
        r->determinism_mode,
        (unsigned long long)r->seed,
        r->input_count,
        (long long)r->dim_m, (long long)r->dim_n, (long long)r->dim_k,
        r->cpu_caps.has_cpuid, r->cpu_caps.has_osxsave, r->cpu_caps.xcr0_xmm, r->cpu_caps.xcr0_ymm,
        r->cpu_caps.has_avx, r->cpu_caps.has_avx2, r->cpu_caps.has_fma, r->cpu_caps.has_f16c,
        (unsigned long long)r->cpu_caps.xcr0_raw,
        r->ptx_caps.cuda_available, esc_gpu, r->ptx_caps.cc_major, r->ptx_caps.cc_minor,
        r->inputs_descriptor_sha256,
        r->input_buffers_sha256,
        r->output_descriptor_sha256,
        r->output_buffer_sha256,
        r->status_code,
        r->execution_status,
        esc_err,
        r->verification_status,
        r->integrity_kind,
        r->zk_proof_present ? "true" : "false",
        (unsigned long long)r->elapsed_ns,
        r->receipt_canonical_sha256,
        r->hmac_sha256_hex[0] ? r->hmac_sha256_hex : "none"
    );
}
