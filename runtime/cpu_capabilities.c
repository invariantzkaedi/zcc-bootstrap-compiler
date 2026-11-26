/* SPDX-License-Identifier: MIT */
#include "zcc_compute.h"
#include <stdio.h>
#include <string.h>

#if defined(__x86_64__) || defined(_M_X64)
static void cpuid_query(uint32_t leaf, uint32_t subleaf, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx) {
#if defined(_MSC_VER) && !defined(__GNUC__)
    int regs[4];
    __cpuidex(regs, (int)leaf, (int)subleaf);
    *eax = (uint32_t)regs[0];
    *ebx = (uint32_t)regs[1];
    *ecx = (uint32_t)regs[2];
    *edx = (uint32_t)regs[3];
#else
    __asm__ __volatile__("cpuid"
                         : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
                         : "a"(leaf), "c"(subleaf));
#endif
}

static uint64_t xgetbv_query(uint32_t xcr_idx) {
#if defined(_MSC_VER) && !defined(__GNUC__)
    return (uint64_t)_xgetbv(xcr_idx);
#else
    uint32_t lo = 0U, hi = 0U;
    __asm__ __volatile__("xgetbv" : "=a"(lo), "=d"(hi) : "c"(xcr_idx));
    return ((uint64_t)hi << 32) | (uint64_t)lo;
#endif
}
#endif

zcc_status_t zcc_probe_cpu_caps(zcc_cpu_caps_t *out_caps) {
    if (!out_caps) return ZCC_STATUS_ERR_NULL_PTR;
    memset(out_caps, 0, sizeof(*out_caps));
    out_caps->struct_size = (uint32_t)sizeof(*out_caps);

#if defined(__x86_64__) || defined(_M_X64)
    uint32_t eax = 0U, ebx = 0U, ecx = 0U, edx = 0U;
    out_caps->has_cpuid = 1U;
    cpuid_query(0U, 0U, &eax, &ebx, &ecx, &edx);
    out_caps->max_basic_leaf = eax;

    if (out_caps->max_basic_leaf >= 1U) {
        cpuid_query(1U, 0U, &eax, &ebx, &ecx, &edx);
        out_caps->has_sse2 = ((edx >> 26) & 1U);
        out_caps->has_osxsave = ((ecx >> 27) & 1U);
        uint32_t cpuid_fma  = ((ecx >> 12) & 1U);
        uint32_t cpuid_avx  = ((ecx >> 28) & 1U);
        uint32_t cpuid_f16c = ((ecx >> 29) & 1U);

        if (out_caps->has_osxsave) {
            uint64_t xcr0 = xgetbv_query(0U);
            out_caps->xcr0_raw = xcr0;
            out_caps->xcr0_xmm = ((xcr0 & 0x02ULL) != 0ULL) ? 1U : 0U;
            out_caps->xcr0_ymm = ((xcr0 & 0x04ULL) != 0ULL) ? 1U : 0U;
        }

        uint32_t os_avx_state = (out_caps->has_osxsave && out_caps->xcr0_xmm && out_caps->xcr0_ymm) ? 1U : 0U;
        out_caps->has_avx  = (cpuid_avx && os_avx_state) ? 1U : 0U;
        out_caps->has_fma  = (cpuid_fma && os_avx_state) ? 1U : 0U;
        out_caps->has_f16c = (cpuid_f16c && os_avx_state) ? 1U : 0U;

        if (out_caps->max_basic_leaf >= 7U) {
            cpuid_query(7U, 0U, &eax, &ebx, &ecx, &edx);
            uint32_t cpuid_avx2    = ((ebx >> 5) & 1U);
            uint32_t cpuid_avx512f = ((ebx >> 16) & 1U);
            uint32_t os_avx512_state = (os_avx_state && ((out_caps->xcr0_raw & 0xE0ULL) == 0xE0ULL)) ? 1U : 0U;
            out_caps->has_avx2    = (cpuid_avx2 && out_caps->has_avx) ? 1U : 0U;
            out_caps->has_avx512f = (cpuid_avx512f && os_avx512_state) ? 1U : 0U;
        }
    }
#endif
    return ZCC_STATUS_OK;
}

int zcc_cpu_has_avx2_fma(const zcc_cpu_caps_t *caps) {
    if (!caps || caps->struct_size < sizeof(zcc_cpu_caps_t)) return 0;
    return (caps->has_cpuid &&
            caps->has_osxsave &&
            caps->xcr0_xmm &&
            caps->xcr0_ymm &&
            caps->has_avx &&
            caps->has_avx2 &&
            caps->has_fma) ? 1 : 0;
}

static void write_reason(char *buf, size_t cap, const char *msg) {
    if (!buf || cap == 0U) return;
    snprintf(buf, cap, "%s", msg ? msg : "");
}

zcc_status_t zcc_negotiate_backend_with_caps(
    const zcc_cpu_caps_t *cpu_caps,
    const zcc_ptx_caps_t *ptx_caps,
    uint32_t requested_backend,
    uint32_t selection_policy,
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

    int avx2_ok = zcc_cpu_has_avx2_fma(cpu_caps);
    int ptx_ok  = (ptx_caps && ptx_caps->cuda_available && ptx_caps->cc_major >= 8) ? 1 : 0;

    *out_fallback_occurred = 0U;
    *out_selected_backend = ZCC_BACKEND_SCALAR_C99;

    if (requested_backend == ZCC_BACKEND_SCALAR_C99) {
        *out_selected_backend = ZCC_BACKEND_SCALAR_C99;
        write_reason(reason_buf, reason_cap, "explicit_scalar_c99");
        return ZCC_STATUS_OK;
    }

    if (requested_backend == ZCC_BACKEND_AVX2_FMA) {
        if (avx2_ok) {
            *out_selected_backend = ZCC_BACKEND_AVX2_FMA;
            write_reason(reason_buf, reason_cap, "explicit_avx2_fma_verified");
            return ZCC_STATUS_OK;
        }
        if (selection_policy == ZCC_POLICY_AUTO_FALLBACK) {
            *out_selected_backend = ZCC_BACKEND_SCALAR_C99;
            *out_fallback_occurred = 1U;
            write_reason(reason_buf, reason_cap, "fallback_scalar_missing_avx2_osxsave_xcr0_or_fma");
            return ZCC_STATUS_OK;
        }
        write_reason(reason_buf, reason_cap, "rejected_exact_avx2_fma_unsupported_hw");
        return ZCC_STATUS_ERR_UNSUPPORTED_HW;
    }

    if (requested_backend == ZCC_BACKEND_PTX_SM120) {
        if (ptx_ok) {
            *out_selected_backend = ZCC_BACKEND_PTX_SM120;
            write_reason(reason_buf, reason_cap, "explicit_ptx_sm120_verified");
            return ZCC_STATUS_OK;
        }
        if (selection_policy == ZCC_POLICY_AUTO_FALLBACK) {
            if (avx2_ok) {
                *out_selected_backend = ZCC_BACKEND_AVX2_FMA;
                *out_fallback_occurred = 1U;
                write_reason(reason_buf, reason_cap, "fallback_avx2_fma_missing_ptx");
                return ZCC_STATUS_OK;
            }
            *out_selected_backend = ZCC_BACKEND_SCALAR_C99;
            *out_fallback_occurred = 1U;
            write_reason(reason_buf, reason_cap, "fallback_scalar_missing_ptx_and_avx2");
            return ZCC_STATUS_OK;
        }
        write_reason(reason_buf, reason_cap, "rejected_exact_ptx_unavailable");
        return ZCC_STATUS_ERR_BACKEND_UNAVAILABLE;
    }

    if (requested_backend == ZCC_BACKEND_AUTO) {
        if (ptx_ok) {
            *out_selected_backend = ZCC_BACKEND_PTX_SM120;
            write_reason(reason_buf, reason_cap, "auto_selected_ptx_sm120");
            return ZCC_STATUS_OK;
        }
        if (avx2_ok) {
            *out_selected_backend = ZCC_BACKEND_AVX2_FMA;
            write_reason(reason_buf, reason_cap, "auto_selected_avx2_fma");
            return ZCC_STATUS_OK;
        }
        *out_selected_backend = ZCC_BACKEND_SCALAR_C99;
        write_reason(reason_buf, reason_cap, "auto_selected_scalar_c99");
        return ZCC_STATUS_OK;
    }

    write_reason(reason_buf, reason_cap, "invalid_backend_id");
    return ZCC_STATUS_ERR_INVALID_ARG;
}
