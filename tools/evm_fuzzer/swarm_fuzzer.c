// tools/evm_fuzzer/swarm_fuzzer.c
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

typedef struct {
    uint8_t* code;
    size_t len;
    int stack_depth;
} ContractBuilder;

void append_push(ContractBuilder* b, uint64_t value, int bytes) {
    b->code[b->len++] = 0x60 + bytes - 1;  // PUSHn
    for (int i = bytes-1; i >= 0; i--) {
        b->code[b->len++] = (value >> (i*8)) & 0xFF;
    }
    b->stack_depth++;
}

void generate_valid_dispatcher(ContractBuilder* b, int heavy_abi) {
    if (heavy_abi) {
        for(int j=0; j<5; j++) {
            append_push(b, 0, 4);
            b->code[b->len++] = 0x36; // CALLDATASIZE
            b->stack_depth++;
            b->code[b->len++] = 0x14; // EQ
            b->stack_depth--;
        }
    } else {
        // Standard 4-byte selector pattern
        append_push(b, 0, 4);           // PUSH4 selector
        // Add dummy EQ jump pattern
        b->code[b->len++] = 0x36; // CALLDATASIZE
        b->stack_depth++;
        b->code[b->len++] = 0x14; // EQ
        b->stack_depth--;
    }
}

void swarm_fuzz_generate(const char* out_path, int seed, int size, int heavy_abi, int heavy_memory, int heavy_calldata) {
    srand(seed);
    ContractBuilder b = { .code = malloc(size + 4096), .len = 0, .stack_depth = 0 };
    
    // Always start with valid structure
    generate_valid_dispatcher(&b, heavy_abi);
    
    if (heavy_calldata) {
        b.code[b.len++] = 0x35; // CALLDATALOAD
        b.stack_depth++;
        b.code[b.len++] = 0x35; // CALLDATALOAD
        b.stack_depth++;
    }

    // Random but constrained body
    for (int i = 0; i < size; i++) {
        if (heavy_memory && rand() % 5 == 0 && b.stack_depth >= 2) {
            b.code[b.len++] = 0x52; // MSTORE
            b.stack_depth -= 2;
        } else if (heavy_memory && rand() % 5 == 0 && b.stack_depth >= 1) {
            b.code[b.len++] = 0x51; // MLOAD
        } else if (heavy_memory && rand() % 5 == 0 && b.stack_depth >= 2) {
            b.code[b.len++] = 0x55; // SSTORE
            b.stack_depth -= 2;
        } else if (rand() % 10 == 0 && b.stack_depth >= 2) {
            b.code[b.len++] = 0x01; // ADD (safe)
            b.stack_depth--;
        } else if (rand() % 5 == 0) {
            append_push(&b, rand(), 1 + (rand() % 8));
        } else if (rand() % 4 == 0 && b.stack_depth >= 1) {
            b.code[b.len++] = 0x50; // POP
            b.stack_depth--;
        }
        // Just append some math ops randomly
        else if (b.stack_depth >= 2) {
            b.code[b.len++] = 0x02 + (rand() % 5); // MUL, SUB, DIV, SDIV, MOD
            b.stack_depth--;
        }
    }
    
    // End with STOP or RETURN
    if (rand() % 2 == 0 && b.stack_depth >= 2) {
        b.code[b.len++] = 0xf3; // RETURN
        b.stack_depth -= 2;
    } else {
        b.code[b.len++] = 0x00; // STOP
    }
    
    FILE* f = fopen(out_path, "wb");
    if (f) {
        fwrite(b.code, 1, b.len, f);
        fclose(f);
    }
    free(b.code);
}

int main(int argc, char** argv) {
    if (argc < 2) return 1;
    int seed = 0;
    int size = 512;
    int heavy_abi = 0, heavy_memory = 0, heavy_calldata = 0;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--heavy-abi") == 0) heavy_abi = 1;
        else if (strcmp(argv[i], "--heavy-memory") == 0) heavy_memory = 1;
        else if (strcmp(argv[i], "--heavy-calldata") == 0) heavy_calldata = 1;
        else if (strcmp(argv[i], "--seed") == 0 && i+1 < argc) seed = atoi(argv[++i]);
        else if (strcmp(argv[i], "--size") == 0 && i+1 < argc) size = atoi(argv[++i]);
        else seed = atoi(argv[i]); // legacy fallback
    }

    swarm_fuzz_generate("/dev/stdout", seed, size, heavy_abi, heavy_memory, heavy_calldata);
    return 0;
}
