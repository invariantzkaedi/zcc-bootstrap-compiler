#ifndef ZCC_POINTER_SSA_H
#define ZCC_POINTER_SSA_H

#include <stdint.h>

struct Function;
uint32_t opt_pointer_ssa_rewrite_pass(struct Function *fn);

#endif /* ZCC_POINTER_SSA_H */
