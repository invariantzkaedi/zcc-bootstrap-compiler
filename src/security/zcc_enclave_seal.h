/* ========================================================================= */
/* ZCC ENCLAVESEAL: HARDWARE ATTESTATION & ZERO-TRUST SECURITY (E1-E5)       */
/* ========================================================================= */
/* File: src/security/zcc_enclave_seal.h                                     */
/* Description: Hardware-Rooted Enclave Confidential Computing:              */
/*              E1: Cryptographic Enclave Measurement (MRENCLAVE / MRTD)     */
/*              E2: AMD SEV-SNP VCEK Certificate & Guest Policy Audit        */
/*              E3: Intel TDX ECDSA Quote & RTMR Extended Hash Chain         */
/*              E4: 256-bit Nonce Challenge-Response & Replay Protection     */
/*              E5: Master Zero-Trust Attestation Emitter & Seal Receipts    */
/* ========================================================================= */

#ifndef ZCC_ENCLAVE_SEAL_H
#define ZCC_ENCLAVE_SEAL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ENCLAVE_DIGEST_LEN      48 /* SHA-384 384-bit Measurement Digest */
#define ENCLAVE_NONCE_LEN       32 /* 256-bit Fresh Attestation Challenge Nonce */
#define ENCLAVE_SIG_LEN         96 /* ECDSA P-384 Signature Length */

typedef enum {
    ENCLAVE_ARCH_AMD_SEV_SNP = 1,
    ENCLAVE_ARCH_INTEL_TDX = 2,
    ENCLAVE_ARCH_AWS_NITRO = 3
} EnclaveArchitecture;

typedef struct {
    EnclaveArchitecture arch;
    uint8_t             measurement_mrenclave[ENCLAVE_DIGEST_LEN];
    uint8_t             rtmr_registers[4][ENCLAVE_DIGEST_LEN];
    uint32_t            tcb_version;
    uint32_t            guest_policy_flags; /* Bit 0: Debug Disabled, Bit 1: SMT Off */
    bool                migration_disabled;
} EnclaveDescriptor;

typedef struct {
    EnclaveArchitecture arch;
    uint8_t             report_digest[ENCLAVE_DIGEST_LEN];
    uint8_t             challenge_nonce[ENCLAVE_NONCE_LEN];
    uint8_t             hardware_signature[ENCLAVE_SIG_LEN];
    uint32_t            chip_id;
    bool                quote_valid;
    bool                tcb_current;
} EnclaveAttestationReport;

typedef struct {
    uint8_t             sealed_data[128];
    uint32_t            sealed_len;
    uint8_t             key_id[32];
    bool                bound_to_mrenclave;
} EnclaveSealedBlob;

typedef struct {
    EnclaveArchitecture verified_arch;
    uint32_t            security_version_tcb;
    bool                measurement_verified;
    bool                vcek_chain_trusted;
    bool                nonce_freshness_verified;
    bool                zero_trust_hardware_sealed;
} EnclaveSealReceipt;

/* ------------------------------------------------------------------------- */
/* FUNCTION PROTOTYPES (E1 - E5)                                             */
/* ------------------------------------------------------------------------- */

/* E1: Enclave Measurement Initialization & Code Hash Computation */
bool enclave_init_descriptor(EnclaveDescriptor *desc, EnclaveArchitecture arch);
bool enclave_measure_code_page(EnclaveDescriptor *desc, const uint8_t *page_data, size_t page_len);

/* E2: AMD SEV-SNP Guest Policy & VCEK Audit */
bool enclave_verify_sev_snp_policy(const EnclaveDescriptor *desc, uint32_t required_policy);

/* E3: Intel TDX RTMR Extended Hash Chain Audit */
bool enclave_extend_rtmr(EnclaveDescriptor *desc, uint8_t rtmr_idx, const uint8_t *data, size_t len);

/* E4: Nonce Challenge-Response & Hardware Secret Sealing */
bool enclave_generate_attestation_report(
    const EnclaveDescriptor *desc,
    const uint8_t nonce[ENCLAVE_NONCE_LEN],
    EnclaveAttestationReport *out_report
);
bool enclave_seal_secret(
    const EnclaveDescriptor *desc,
    const uint8_t *plaintext,
    size_t plain_len,
    EnclaveSealedBlob *out_blob
);
bool enclave_unseal_secret(
    const EnclaveDescriptor *desc,
    const EnclaveSealedBlob *blob,
    uint8_t *out_plaintext,
    size_t *out_len
);

/* E5: Master Zero-Trust Attestation Verifier & Assembly Emitter */
bool enclave_verify_attestation(
    const EnclaveAttestationReport *report,
    const uint8_t expected_nonce[ENCLAVE_NONCE_LEN],
    EnclaveSealReceipt *out_receipt
);

int32_t enclave_emit_attestation_assembly(
    const EnclaveAttestationReport *report,
    char *out_buf,
    size_t buf_len
);

#ifdef __cplusplus
}
#endif

#endif /* ZCC_ENCLAVE_SEAL_H */
