/*
 * zcc_monetization_engine.c — Sovereign Compiler Monetization Subsystem
 * ====================================================================
 */

#include "include/zcc_monetization_engine.h"

void zcc_monetization_init(void) {
    /* Initialization hook */
}

int zcc_license_verify(const ZCCLicenseToken *token, ZCCLicenseTier required_tier) {
    if (!token) return -1;
    if (token->magic != ZCC_LICENSE_MAGIC) return -2;

    /* Check epoch validity */
    uint64_t current_time = (uint64_t)time(NULL);
    if (token->valid_until_epoch > 0 && current_time > token->valid_until_epoch) {
        return -3; /* Expired */
    }

    /* Check tier hierarchy */
    if (token->tier < (uint32_t)required_tier) {
        return -4; /* Insufficient Tier */
    }

    return 0; /* Verified Valid */
}

int zcc_license_meter_pass(ZCCLicenseToken *token, const char *pass_name, uint32_t cost) {
    if (!token) return -1;
    if (token->tier == TIER_SOVEREIGN) {
        return 0; /* Unlimited passes */
    }
    if (token->credits_remaining < cost) {
        return -5; /* Insufficient credits */
    }
    token->credits_remaining -= cost;
    return 0;
}

ZCCUsageReceipt zcc_generate_receipt(const ZCCLicenseToken *token, const char *target_arch, uint32_t passes) {
    ZCCUsageReceipt receipt;
    memset(&receipt, 0, sizeof(receipt));
    receipt.timestamp = (uint64_t)time(NULL);
    receipt.passes_executed = passes;
    receipt.credits_consumed = (token && token->tier != TIER_SOVEREIGN) ? passes * 10 : 0;
    
    if (target_arch) {
        strncpy(receipt.target_arch, target_arch, sizeof(receipt.target_arch) - 1);
    } else {
        strcpy(receipt.target_arch, "x86_64-linux");
    }

    /* Compute proof hash */
    for (int i = 0; i < 32; i++) {
        receipt.proof_hash[i] = (uint8_t)((receipt.timestamp >> (i % 8)) ^ (passes + i));
    }

    return receipt;
}
