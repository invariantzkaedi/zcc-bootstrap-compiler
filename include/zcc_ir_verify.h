#ifndef ZCC_IR_VERIFY_H
#define ZCC_IR_VERIFY_H

#include <stdbool.h>

typedef struct Function Function;
typedef struct Module {
    Function *funcs[100];
    int n_funcs;
} Module;

typedef struct VerifyError {
    const char *kind;      // "CFG", "SSA", "PHI", "DOM", ...
    int bb_id;
    int inst_id;
    int reg_id;
    const char *message;
} VerifyError;

typedef struct VerifyReport {
    bool ok;
    VerifyError *errors;
    int n_errors;
    int cap_errors;
} VerifyReport;

// Verify one function
bool verify_cfg(Function *fn, VerifyReport *out);
bool verify_ssa(Function *fn, VerifyReport *out);
bool verify_phi_wellformed(Function *fn, VerifyReport *out);
bool verify_terminators(Function *fn, VerifyReport *out);

// Convenient full function verify
bool verify_function_ir(Function *fn, VerifyReport *out);

// Optional whole-module verify
bool verify_module_ir(Module *m, VerifyReport *out);

#endif
