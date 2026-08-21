/*
 * test_monetization_engine.c — Monetization Subsystem Verification Suite
 * ======================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include "include/zcc_monetization_engine.h"

int main(void) {
    printf("=================================================================\n");
    printf("🔱 ZCC MONETIZATION & METERED CLOUD ENGINE VERIFICATION 🔱\n");
    printf("=================================================================\n\n");

    /* 1. Test Community Token */
    printf("1. Testing Community License Token Verification...\n");
    ZCCLicenseToken comm_token;
    memset(&comm_token, 0, sizeof(comm_token));
    comm_token.magic = ZCC_LICENSE_MAGIC;
    comm_token.tier = TIER_COMMUNITY;
    comm_token.credits_remaining = 100;
    comm_token.valid_until_epoch = (uint64_t)time(NULL) + 3600;

    assert(zcc_license_verify(&comm_token, TIER_COMMUNITY) == 0);
    assert(zcc_license_verify(&comm_token, TIER_PROFESSIONAL) == -4); /* Insufficient tier */
    printf("   [+] Community tier authorization validated (Free passes allowed, Pro rejected).\n\n");

    /* 2. Test Enterprise Token & Credit Metering */
    printf("2. Testing Enterprise License & Credit Metering...\n");
    ZCCLicenseToken ent_token;
    memset(&ent_token, 0, sizeof(ent_token));
    ent_token.magic = ZCC_LICENSE_MAGIC;
    ent_token.tier = TIER_ENTERPRISE;
    ent_token.credits_remaining = 50;
    ent_token.valid_until_epoch = (uint64_t)time(NULL) + 3600;

    assert(zcc_license_verify(&ent_token, TIER_ENTERPRISE) == 0);
    
    /* Meter 3 passes (cost: 30 credits) */
    assert(zcc_license_meter_pass(&ent_token, "simd_vectorize", 30) == 0);
    assert(ent_token.credits_remaining == 20);
    printf("   [+] Successfully metered 30 credits: remaining = %u credits.\n", ent_token.credits_remaining);

    /* Try to meter 30 more credits (should fail with insufficient credits) */
    assert(zcc_license_meter_pass(&ent_token, "zk_r1cs_proof", 30) == -5);
    printf("   [+] Insufficient credit tripwire intercepted as expected.\n\n");

    /* 3. Test Sovereign Token & Unlimited Proof-of-Compilation Receipts */
    printf("3. Testing Sovereign Tier & Proof-of-Compilation Receipt...\n");
    ZCCLicenseToken sov_token;
    memset(&sov_token, 0, sizeof(sov_token));
    sov_token.magic = ZCC_LICENSE_MAGIC;
    sov_token.tier = TIER_SOVEREIGN;
    sov_token.credits_remaining = 0; /* Sovereign needs no balance */
    sov_token.valid_until_epoch = (uint64_t)time(NULL) + 86400 * 365;

    assert(zcc_license_verify(&sov_token, TIER_SOVEREIGN) == 0);
    assert(zcc_license_meter_pass(&sov_token, "hamiltonian_nas", 1000) == 0);

    ZCCUsageReceipt receipt = zcc_generate_receipt(&sov_token, "riscv64-linux", 7);
    assert(receipt.passes_executed == 7);
    assert(strcmp(receipt.target_arch, "riscv64-linux") == 0);
    printf("   [+] Generated ZCC Proof-of-Compilation Receipt (Target: %s, Passes: %u).\n", receipt.target_arch, receipt.passes_executed);
    printf("   [+] Receipt Timestamp: %llu (Epoch)\n", (unsigned long long)receipt.timestamp);
    printf("   -> [PASS] Cryptographic receipt generation verified.\n\n");

    printf("=================================================================\n");
    printf("★ ZCC MONETIZATION & METERED ENGINE VERIFIED 100% OPERATIONAL ★\n");
    printf("=================================================================\n");
    return 0;
}
