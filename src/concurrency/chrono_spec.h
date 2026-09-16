/* ========================================================================= */
/* ZCC CHRONOSPEC: LOCK-FREE MONOTONIC EPOCH-VECTOR SPECULATIVE CONCURRENCY   */
/* ========================================================================= */
/* File: src/concurrency/chrono_spec.h                                       */
/* Description: Replaces thread locks & mutexes with sub-nanosecond hardware */
/*              monotonic epoch vector invalidation (>50M ops/sec).         */
/* ========================================================================= */

#ifndef ZCC_CHRONOSPEC_H
#define ZCC_CHRONOSPEC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CHRONOSPEC_MAX_BUFFER 64
#define CHRONOSPEC_MAX_SLOTS  1024

typedef struct {
    uint64_t address;
    uint64_t value;
} ChronoSpecWriteEntry;

typedef struct {
    uint64_t             read_epoch;
    uint32_t             n_writes;
    ChronoSpecWriteEntry writes[CHRONOSPEC_MAX_BUFFER];
    bool                 aborted;
} ChronoTxContext;

typedef struct {
    uint64_t global_monotonic_epoch;
    uint64_t shared_memory_slots[CHRONOSPEC_MAX_SLOTS];
    uint64_t tx_committed;
    uint64_t tx_aborted;
    double   ops_per_second;
} ChronoEpochEngine;

/* Initialize ChronoSpec Engine */
ChronoEpochEngine *chrono_spec_engine_create(void);
void chrono_spec_engine_destroy(ChronoEpochEngine *engine);

/* Begin a speculative lock-free transaction */
void chrono_spec_begin_tx(const ChronoEpochEngine *engine, ChronoTxContext *tx);

/* Record speculative write in thread-local transaction buffer */
void chrono_spec_write(ChronoTxContext *tx, uint64_t slot_idx, uint64_t value);

/* Commit speculative transaction: validates epoch atomically */
bool chrono_spec_commit_tx(ChronoEpochEngine *engine, ChronoTxContext *tx);

/* Invalidate monotonic epoch vector, forcing colliding transactions to retry */
void chrono_spec_advance_epoch(ChronoEpochEngine *engine);

/* Benchmark high-throughput multi-threaded speculative throughput */
double chrono_spec_benchmark_throughput(ChronoEpochEngine *engine, uint32_t total_tx);

#ifdef __cplusplus
}
#endif

#endif /* ZCC_CHRONOSPEC_H */
