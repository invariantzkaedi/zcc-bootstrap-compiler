#include "../../evm_lifter.h"
#include "../../ir.h"
#include <stdio.h>

extern void ir_pm_run_default(void *mod_ptr, int verbose);
extern void evm_lifter_init(evm_lifter_t *ls, const unsigned char *bytecode, int length, ir_module_t *module);
extern evm_lift_result_t evm_lift_bytecode(evm_lifter_t *ls);
extern void evm_lifter_destroy(evm_lifter_t *ls);

/* Read hex string into byte array */
int hex2bin(const char *hex, unsigned char *out) {
    int len = 0;
    while (hex[0] && hex[1]) {
        unsigned int b;
        if (sscanf(hex, "%2x", &b) != 1) break;
        out[len++] = b;
        hex += 2;
    }
    return len;
}

int main(int argc, char **argv) {
    if (argc < 2) return 1;
    unsigned char bytecode[1024];
    int len = hex2bin(argv[1], bytecode);
    
    ir_module_t *mod = ir_module_create();
    evm_lifter_t ls;
    evm_lifter_init(&ls, bytecode, len, mod);
    evm_lift_bytecode(&ls);
    
    ir_pm_run_default(mod, 0);
    
    ir_module_emit_text(mod, stdout);
    
    evm_lifter_destroy(&ls);
    ir_module_free(mod);
    return 0;
}
