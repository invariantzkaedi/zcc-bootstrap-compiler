#include "ir_pass_manager.h"
#include <stddef.h>

__attribute__((weak))
ir_pass_result_t ir_pass_auto_heal(void *fn_ptr) {
    ir_pass_result_t r = {0, 0, 0, 0, 0};
    (void)fn_ptr;
    return r;
}

__attribute__((weak))
void evm_run_symbolic_from_bytecode(const unsigned char *bytecode,
                                     size_t len, int smt_mode) {
    (void)bytecode; (void)len; (void)smt_mode;
}
