/*
 * zcc_monetization_engine.h — Sovereign Compiler Monetization Subsystem
 * ====================================================================
 * High-performance, cryptographic, zero-drift license token validation,
 * usage-metered micro-tier authorization, and hardware-fingerprinted
 * proof-of-compilation receipts.
 */

#ifndef ZCC_MONETIZATION_ENGINE_H
#define ZCC_MONETIZATION_ENGINE_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#define ZCC_LICENSE_MAGIC 0x5A43432450415924ULL /* 'ZCC$PAY$' */

/* License Tiers */
typedef enum {
    TIER_COMMUNITY   = 0, /* Free / Open Source (Standard Optimization) */
    TIER_PROFESSIONAL = 1, /* PGO + SIMD Vectorization */
    TIER_ENTERPRISE   = 2, /* WASM + RISC-V + Win64 PE + ZK Prover */
    TIER_SOVEREIGN    = 3  /* Unlimited AI Hamiltonian NAS + Kernel Synthesis */
} ZCCLicenseTier;

/* Cryptographic License Token */
typedef struct {
    uint64_t magic;
    uint32_t tier;
    uint32_t credits_remaining;
    uint64_t client_id_hash;
    uint64_t valid_until_epoch;
    uint8_t  signature_mac[32];
} ZCCLicenseToken;

/* Metered Usage Receipt */
typedef struct {
    uint64_t timestamp;
    uint32_t credits_consumed;
    uint32_t passes_executed;
    char target_arch[32];
    uint8_t proof_hash[32];
} ZCCUsageReceipt;

/* Function Declarations */
void zcc_monetization_init(void);
int zcc_license_verify(const ZCCLicenseToken *token, ZCCLicenseTier required_tier);
int zcc_license_meter_pass(ZCCLicenseToken *token, const char *pass_name, uint32_t cost);
ZCCUsageReceipt zcc_generate_receipt(const ZCCLicenseToken *token, const char *target_arch, uint32_t passes);

#endif /* ZCC_MONETIZATION_ENGINE_H */
