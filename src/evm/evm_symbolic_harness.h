#ifndef EVM_SYMBOLIC_HARNESS_H
#define EVM_SYMBOLIC_HARNESS_H

#include "../../ir.h"
#include "../../evm_lifter.h"

void evm_run_symbolic(ir_module_t* mod);

#endif
