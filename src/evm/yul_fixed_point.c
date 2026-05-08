/*
 * yul_fixed_point.c — EVM Wad/Ray Fixed-Point Emulation
 * =====================================================
 * Emulates double-precision floating-point arithmetic (IR_FADD, IR_FSUB,
 * IR_FMUL, IR_FDIV) using 256-bit Yul fixed-point math scaled by 1e18.
 * High-precision MEV math parameters preventing precision loss and overflows.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../ir.h"

#define WAD 1000000000000000000ULL

/* Emits the pure-Yul helper functions into the output stream */
void yul_emit_fixed_point_helpers(FILE *out) {
    fprintf(out, "        function wad_add(x, y) -> z {\n");
    fprintf(out, "            z := add(x, y)\n");
    fprintf(out, "            if lt(z, x) { revert(0, 0) } // overflow\n");
    fprintf(out, "        }\n");
    fprintf(out, "        function wad_sub(x, y) -> z {\n");
    fprintf(out, "            if lt(x, y) { revert(0, 0) } // underflow\n");
    fprintf(out, "            z := sub(x, y)\n");
    fprintf(out, "        }\n");
    fprintf(out, "        function wad_mul(x, y) -> z {\n");
    fprintf(out, "            if iszero(y) { z := 0 leave }\n");
    fprintf(out, "            let wad := 1000000000000000000\n");
    fprintf(out, "            z := mul(x, y)\n");
    fprintf(out, "            if iszero(eq(div(z, y), x)) { revert(0, 0) } // overflow\n");
    fprintf(out, "            z := div(z, wad)\n");
    fprintf(out, "        }\n");
    fprintf(out, "        function wad_div(x, y) -> z {\n");
    fprintf(out, "            if iszero(y) { revert(0, 0) } // divide by zero\n");
    fprintf(out, "            let wad := 1000000000000000000\n");
    fprintf(out, "            let x_wad := mul(x, wad)\n");
    fprintf(out, "            if iszero(eq(div(x_wad, wad), x)) { revert(0, 0) } // overflow\n");
    fprintf(out, "            z := div(x_wad, y)\n");
    fprintf(out, "        }\n");
}

/* Lowers a floating-point IR node to its Yul equivalent string */
const char *yul_lower_float_op(ir_op_t op) {
    switch (op) {
        case IR_FADD: return "wad_add";
        case IR_FSUB: return "wad_sub";
        case IR_FMUL: return "wad_mul";
        case IR_FDIV: return "wad_div";
        default: return NULL;
    }
}
