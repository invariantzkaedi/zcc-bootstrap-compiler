#ifndef ZCC_IR_H
#define ZCC_IR_H

#include <stdint.h>
#include <stdbool.h>
#include "../prelude.h"

#define ty ir_type
#define src1 src[0]
#define src2 src[1]
#define src_is_float is_float

#define OP_ICMP_EQ OP_EQ
#define OP_ICMP_NE OP_NE
#define OP_SDIV OP_DIV
#define OP_UDIV OP_DIV
#define OP_ASHR OP_SHR
#define OP_AND OP_BAND
#define OP_OR OP_BOR
#define OP_XOR OP_BXOR

typedef IRType Type;

#endif
