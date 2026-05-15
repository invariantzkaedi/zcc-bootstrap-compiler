#pragma once
#include "ir.h"
#include <stdint.h>

typedef struct {
    uint8_t* code;
    size_t   len;
    size_t   cap;
    uint64_t gas_used;
} evm_builder_t;

void ir_to_evm(ir_func_t* func, evm_builder_t* out);
