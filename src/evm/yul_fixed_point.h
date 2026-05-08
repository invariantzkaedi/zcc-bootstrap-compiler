#ifndef ZCC_YUL_FIXED_POINT_H
#define ZCC_YUL_FIXED_POINT_H

#include <stdio.h>
#include "../../ir.h"

void yul_emit_fixed_point_helpers(FILE *out);
const char *yul_lower_float_op(ir_op_t op);

#endif
