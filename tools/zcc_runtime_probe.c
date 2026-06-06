/*
 * zcc_runtime_probe.c — D-28: Runtime Behavioral Probe
 *
 * A zero-heap-allocation instrumentation library that records actual
 * function call behavior at runtime using GCC/Clang's
 * -finstrument-functions hook mechanism.
 *
 * Link this file alongside any binary compiled with -finstrument-functions.
 * At program exit, the atexit-registered handler emits a runtime_genome.json
 * capturing the observed call-graph: function addresses, call counts, and
 * peak call-stack depth.
 *
 * Output path controlled by environment variable ZCC_PROBE_OUT.
 * Default: runtime_genome.json in current directory.
 *
 * Memory discipline:
 *   - All state lives in static arrays (no malloc, no free needed).
 *   - No heap allocations during instrumentation hot path.
 *   - atexit handler writes JSON once and returns — no dangling state.
 *   - No phantom closures.
 *
 * Symbol resolution:
 *   - Raw function addresses are emitted as hex strings.
 *   - Offline resolution via zcc_behavioral_diff using zcc_elf_parser.h.
 *   - No dladdr(), no -rdynamic required.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/* ── Configuration ───────────────────────────────────────────────────── */

#define PROBE_MAX_FUNCS   4096   /* distinct function addresses tracked    */
#define PROBE_HASH_SLOTS  8192   /* open-address hash table (must be 2^n) */
#define PROBE_HASH_MASK   (PROBE_HASH_SLOTS - 1)

/* ── Per-function runtime record ─────────────────────────────────────── */

typedef struct {
    uintptr_t address;        /* raw function pointer (void* cast)     */
    uint64_t  enter_count;    /* total times entered                    */
    uint32_t  max_depth;      /* maximum observed call depth at entry   */
    uint32_t  active;         /* 1 = slot in use                        */
} ProbeFunc;

/* ── Static storage (zero heap) ──────────────────────────────────────── */

static ProbeFunc probe_table[PROBE_HASH_SLOTS];  /* open-address hash  */
static int       probe_func_count  = 0;           /* distinct functions  */
static int       probe_depth       = 0;           /* live call depth    */
static int       probe_peak_depth  = 0;           /* max depth ever     */
static uint64_t  probe_total_calls = 0;           /* total enter events */
static int       probe_registered  = 0;           /* atexit guard       */
static int       probe_overflow    = 0;           /* table full flag    */

/* ── Hash lookup ─────────────────────────────────────────────────────── */
/*
 * All internal probe functions are annotated with no_instrument_function.
 * This prevents -finstrument-functions from injecting hooks INTO the probe
 * itself, which would cause infinite recursion.
 */
__attribute__((no_instrument_function))
static int probe_find_slot(uintptr_t addr) {
    uint32_t h = 2166136261u;
    uintptr_t a = addr;
    for (int i = 0; i < (int)sizeof(uintptr_t); i++) {
        h ^= (uint8_t)(a & 0xff);
        h *= 16777619u;
        a >>= 8;
    }
    int slot = (int)(h & PROBE_HASH_MASK);
    for (int i = 0; i < PROBE_HASH_SLOTS; i++) {
        int s = (slot + i) & PROBE_HASH_MASK;
        if (!probe_table[s].active) return s;           /* empty slot */
        if (probe_table[s].address == addr) return s;   /* found      */
    }
    return -1; /* table full — should not happen within PROBE_MAX_FUNCS */
}

/* ── atexit emitter ──────────────────────────────────────────────────── */

__attribute__((no_instrument_function))
static void zcc_probe_emit_genome(void) {
    const char *out_path = getenv("ZCC_PROBE_OUT");
    if (!out_path || out_path[0] == '\0') out_path = "runtime_genome.json";

    FILE *f = fopen(out_path, "w");
    if (!f) {
        fprintf(stderr, "[zcc_probe] cannot write to %s\n", out_path);
        return;
    }

    /* Collect active slots into a compact linear array for sorting */
    ProbeFunc *slots[PROBE_MAX_FUNCS];
    int count = 0;
    for (int i = 0; i < PROBE_HASH_SLOTS && count < PROBE_MAX_FUNCS; i++) {
        if (probe_table[i].active)
            slots[count++] = &probe_table[i];
    }

    /* Sort by call count descending (simple insertion sort — count <= 4096) */
    for (int i = 1; i < count; i++) {
        ProbeFunc *key = slots[i];
        int j = i - 1;
        while (j >= 0 && slots[j]->enter_count < key->enter_count) {
            slots[j + 1] = slots[j];
            j--;
        }
        slots[j + 1] = key;
    }

    fprintf(f, "{\n");
    fprintf(f, "  \"schema\": \"zcc.runtime_genome.v1\",\n");
    fprintf(f, "  \"observed_functions\": %d,\n", count);
    fprintf(f, "  \"peak_call_depth\": %d,\n", probe_peak_depth);
    fprintf(f, "  \"total_calls\": %llu,\n", (unsigned long long)probe_total_calls);
    fprintf(f, "  \"table_overflow\": %s,\n", probe_overflow ? "true" : "false");
    fprintf(f, "  \"functions\": [\n");

    for (int i = 0; i < count; i++) {
        fprintf(f, "    { \"address\": \"0x%llx\", \"calls\": %llu, \"max_depth\": %u }%s\n",
                (unsigned long long)slots[i]->address,
                (unsigned long long)slots[i]->enter_count,
                slots[i]->max_depth,
                (i == count - 1) ? "" : ",");
    }

    fprintf(f, "  ]\n");
    fprintf(f, "}\n");
    fclose(f);

    /* Brief summary to stderr so it's visible in gate output */
    fprintf(stderr,
            "[zcc_probe] Runtime genome written: %s\n"
            "[zcc_probe]   observed_functions=%d  peak_depth=%d  total_calls=%llu\n",
            out_path, count, probe_peak_depth,
            (unsigned long long)probe_total_calls);
}

/* ── Instrumentation hooks ───────────────────────────────────────────── */
/*
 * Called by the compiler at every function entry/exit when the binary
 * is compiled with -finstrument-functions.
 *
 * Hot path: must be fast. Uses only static storage and integer ops.
 * The atexit registration is done lazily on first entry (no init overhead
 * for binaries that never call into instrumented code).
 */

void __cyg_profile_func_enter(void *this_fn,
                               void *call_site __attribute__((unused)))
    __attribute__((no_instrument_function));

void __cyg_profile_func_exit(void *this_fn __attribute__((unused)),
                              void *call_site __attribute__((unused)))
    __attribute__((no_instrument_function));

void __cyg_profile_func_enter(void *this_fn,
                               void *call_site __attribute__((unused))) {
    /* Register atexit emitter once */
    if (!probe_registered) {
        atexit(zcc_probe_emit_genome);
        probe_registered = 1;
    }

    probe_depth++;
    probe_total_calls++;
    if (probe_depth > probe_peak_depth) probe_peak_depth = probe_depth;

    if (probe_overflow) return; /* table full — still track depth */

    uintptr_t addr = (uintptr_t)this_fn;
    int slot = probe_find_slot(addr);
    if (slot < 0) {
        probe_overflow = 1;
        return;
    }

    if (!probe_table[slot].active) {
        /* New function */
        if (probe_func_count >= PROBE_MAX_FUNCS) { probe_overflow = 1; return; }
        probe_table[slot].address     = addr;
        probe_table[slot].enter_count = 0;
        probe_table[slot].max_depth   = 0;
        probe_table[slot].active      = 1;
        probe_func_count++;
    }

    probe_table[slot].enter_count++;
    if ((uint32_t)probe_depth > probe_table[slot].max_depth)
        probe_table[slot].max_depth = (uint32_t)probe_depth;
}

void __cyg_profile_func_exit(void *this_fn __attribute__((unused)),
                              void *call_site __attribute__((unused))) {
    if (probe_depth > 0) probe_depth--;
}
