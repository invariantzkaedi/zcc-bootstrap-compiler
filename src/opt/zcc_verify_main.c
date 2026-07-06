#include "../prelude.h"
#include "zcc_ir_verify.h"
#include <stdio.h>
#include <stdlib.h>

Module *parse_ir_module(const char *filename);
void free_ir_module(Module *m);

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file.ir>\n", argv[0]);
        return 1;
    }

    const char *filename = argv[1];
    Module *m = parse_ir_module(filename);
    if (!m) {
        fprintf(stderr, "Failed to parse IR module %s\n", filename);
        return 1;
    }

    VerifyError errors[100];
    VerifyReport report = {
        .ok = true,
        .errors = errors,
        .n_errors = 0,
        .cap_errors = 100
    };

    bool ok = verify_module_ir(m, &report);

    free_ir_module(m);

    if (!ok || !report.ok) {
        // Errors were already printed to stderr by report_error in ir_verify.c
        return 1;
    }

    return 0;
}
