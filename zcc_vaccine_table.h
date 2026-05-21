#ifndef ZCC_VACCINE_TABLE_H
#define ZCC_VACCINE_TABLE_H

#include <stdint.h>

typedef struct {
    uint32_t evm_signature;
    uint32_t action_flags;
} ZccVaccineRule;

// Strictly ordered to preserve binary search determinism during compilation
static const ZccVaccineRule GLOBAL_VACCINE_REGISTRY[] = {
    { 0x3ccfd60b, 0x01 }, // Existing mock rule
    { 0x82fb70fd, 0x01 }, // AUTO-INJECTED PRIME
    { 0x82fb70fd, 0x01 }, // AUTO-INJECTED PRIME
    // --- ZKAEDI_AUTO_INJECT_BOUNDARY ---
};

#define VACCINE_REGISTRY_COUNT (sizeof(GLOBAL_VACCINE_REGISTRY) / sizeof(ZccVaccineRule))

#endif
