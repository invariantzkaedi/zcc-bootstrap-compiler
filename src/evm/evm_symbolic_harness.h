#ifndef EVM_SYMBOLIC_HARNESS_H
#define EVM_SYMBOLIC_HARNESS_H

#include "../../ir.h"
#include "../../evm_lifter.h"

void evm_run_symbolic(ir_module_t* mod);
void evm_run_symbolic_from_bytecode(const unsigned char* bytecode, size_t len, int smt_mode);
int evm_symbolic_check_reentrancy_invariant(const unsigned char* bytecode, size_t len);

#endif

