// src/evm/abi_extractor.c
#include "../../evm_lifter.h"
#include "../../ir.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

typedef struct {
    uint32_t selector;
    char name[32];        // "func_0x..."
    int param_count;
} AbiFunction;

void evm_calldata_load(void* mem, const char* offset_tmp) {
    (void)mem;
    (void)offset_tmp;
    // placeholder for dynamic calldata tracking
}

static const char* infer_type_from_pattern(uint64_t offset, void* mem) {
    (void)mem;
    // Simple heuristics based on load patterns
    if (offset % 32 == 0) return "uint256";
    if (offset == 4) return "address";
    // extend with bytes32, string, etc.
    return "bytes";
}

void abi_extract(void* mem, const unsigned char* bytecode, size_t len, FILE* decomp_out) {
    if (!decomp_out) return;
    fprintf(decomp_out, "// === RECONSTRUCTED ABI ===\n");

    // Scan for standard dispatch pattern
    for (size_t i = 0; i + 5 < len; i++) {
        if (bytecode[i] == 0x63) {  // typical PUSH4 pattern (0x63)
            uint32_t selector = 
                (bytecode[i+1] << 24) | (bytecode[i+2] << 16) |
                (bytecode[i+3] << 8)  | bytecode[i+4];

            AbiFunction fn;
            fn.selector = selector;
            snprintf(fn.name, sizeof(fn.name), "func_0x%08x", selector);
            fn.param_count = 0;

            // Infer params from calldata loads after this selector
            fprintf(decomp_out, "function %s(", fn.name);
            for (int p = 0; p < 4; p++) {  // max 4 params for now
                uint64_t offset = 4 + p * 32;
                const char* type = infer_type_from_pattern(offset, mem);
                fprintf(decomp_out, "%s param%d%s", type, p, (p < 3) ? ", " : "");
                fn.param_count++;
            }
            fprintf(decomp_out, ") external;\n");
        }
    }

    fprintf(decomp_out, "// =========================\n\n");
}
