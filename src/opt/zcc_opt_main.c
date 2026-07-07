#include "../prelude.h"
#include "zcc_ir_verify.h"
#include "zcc_opt_metrics.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Module *parse_ir_module(const char *filename);
void free_ir_module(Module *m);
void print_ir_function(FILE *out, Function *fn);

// Declarations of optimization passes
bool opt_instcombine_pass(Function *fn, OptMetricsSink *metrics);
bool opt_sccp_pass(Function *fn, OptMetricsSink *metrics);
bool opt_cfg_simplify_pass(Function *fn, OptMetricsSink *metrics);

enum PassKind {
    PASS_INSTCOMBINE,
    PASS_SCCP,
    PASS_CFG_SIMPLIFY
};

int main(int argc, char **argv) {
    int passes[100];
    int n_passes = 0;
    const char *infile = NULL;
    const char *outfile = NULL;
    const char *metrics_path = NULL;

    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--pass=", 7) == 0) {
            const char *pname = argv[i] + 7;
            if (strcmp(pname, "instcombine") == 0) {
                passes[n_passes++] = PASS_INSTCOMBINE;
            } else if (strcmp(pname, "sccp") == 0) {
                passes[n_passes++] = PASS_SCCP;
            } else if (strcmp(pname, "cfg_simplify") == 0) {
                passes[n_passes++] = PASS_CFG_SIMPLIFY;
            }
        } else if (strcmp(argv[i], "--opt-metrics-out") == 0 && i + 1 < argc) {
            metrics_path = argv[i+1];
            i++;
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            outfile = argv[i+1];
            i++;
        } else if (argv[i][0] != '-') {
            infile = argv[i];
        }
    }

    if (!infile) {
        fprintf(stderr, "Usage: %s [--pass=passname] [--opt-metrics-out path] <file.ir> [-o outfile]\n", argv[0]);
        return 1;
    }

    Module *m = parse_ir_module(infile);
    if (!m) {
        fprintf(stderr, "Failed to parse IR module %s\n", infile);
        return 1;
    }

    // Verify input IR first
    VerifyError errors[100];
    VerifyReport report = {
        .ok = true,
        .errors = errors,
        .n_errors = 0,
        .cap_errors = 100
    };
    if (!verify_module_ir(m, &report)) {
        fprintf(stderr, "Input IR verification failed!\n");
        free_ir_module(m);
        return 1;
    }

    OptMetricsSink metrics_sink;
    opt_metrics_init(&metrics_sink);

    // Run passes
    for (int f = 0; f < m->n_funcs; f++) {
        Function *fn = m->funcs[f];
        for (int p = 0; p < n_passes; p++) {
            switch (passes[p]) {
                case PASS_INSTCOMBINE:
                    opt_instcombine_pass(fn, &metrics_sink);
                    break;
                case PASS_SCCP:
                    opt_sccp_pass(fn, &metrics_sink);
                    break;
                case PASS_CFG_SIMPLIFY:
                    opt_cfg_simplify_pass(fn, &metrics_sink);
                    break;
            }
        }
    }

    // Verify output IR
    report.ok = true;
    report.n_errors = 0;
    if (!verify_module_ir(m, &report)) {
        fprintf(stderr, "Optimized IR verification failed!\n");
        if (metrics_sink.rows) free(metrics_sink.rows);
        free_ir_module(m);
        return 1;
    }

    // Print optimized module
    FILE *out = stdout;
    if (outfile) {
        out = fopen(outfile, "w");
        if (!out) {
            fprintf(stderr, "Failed to open output file %s\n", outfile);
            if (metrics_sink.rows) free(metrics_sink.rows);
            free_ir_module(m);
            return 1;
        }
    }

    for (int f = 0; f < m->n_funcs; f++) {
        print_ir_function(out, m->funcs[f]);
    }

    if (outfile) {
        fclose(out);
    }

    if (metrics_path) {
        opt_metrics_dump_csv(&metrics_sink, metrics_path);
    }
    if (metrics_sink.rows) {
        free(metrics_sink.rows);
    }

    free_ir_module(m);
    return 0;
}

