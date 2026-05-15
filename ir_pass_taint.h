#pragma once
#include "ir.h"
#include "ir_pass_manager.h"
#include <string.h>
#include <stdlib.h>

typedef struct {
    char **names;
    int count;
    int capacity;
} ir_bitset_t;

static inline ir_bitset_t *ir_bitset_create(int capacity) {
    ir_bitset_t *bs = (ir_bitset_t*)malloc(sizeof(ir_bitset_t));
    bs->capacity = capacity;
    bs->count = 0;
    bs->names = (char**)malloc(sizeof(char*) * capacity);
    return bs;
}

static inline void ir_bitset_free(ir_bitset_t *bs) {
    for (int i = 0; i < bs->count; i++) free(bs->names[i]);
    free(bs->names);
    free(bs);
}

static inline void ir_bitset_set(ir_bitset_t *bs, const char *name) {
    if (!name || !name[0]) return;
    for (int i = 0; i < bs->count; i++) {
        if (strcmp(bs->names[i], name) == 0) return;
    }
    if (bs->count >= bs->capacity) {
        bs->capacity *= 2;
        bs->names = (char**)realloc(bs->names, sizeof(char*) * bs->capacity);
    }
    bs->names[bs->count++] = strdup(name);
}

static inline int ir_bitset_test(ir_bitset_t *bs, const char *name) {
    if (!name || !name[0]) return 0;
    for (int i = 0; i < bs->count; i++) {
        if (strcmp(bs->names[i], name) == 0) return 1;
    }
    return 0;
}

ir_pass_result_t ir_pass_taint_propagate(void* fn_ptr);
