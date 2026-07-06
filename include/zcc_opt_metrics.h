#ifndef ZCC_OPT_METRICS_H
#define ZCC_OPT_METRICS_H

#include <stdbool.h>
#include <stdint.h>

typedef struct Function Function;

typedef struct {
    const char *pass_name;
    const char *fn_name;
    int instr_before;
    int instr_after;
    int blocks_before;
    int blocks_after;
    int64_t pass_time_us;
    bool changed;
} OptPassMetricRow;

typedef struct {
    OptPassMetricRow *rows;
    int n_rows;
    int cap_rows;
} OptMetricsSink;

void opt_metrics_init(OptMetricsSink *s);
void opt_metrics_push(OptMetricsSink *s, OptPassMetricRow row);
void opt_metrics_dump_csv(const OptMetricsSink *s, const char *path);

// utilities
int fn_count_instructions(const Function *fn);
int fn_count_blocks(const Function *fn);
int64_t zcc_now_us(void);

#endif
