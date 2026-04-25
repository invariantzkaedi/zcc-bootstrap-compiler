#ifndef ZCC_LAYOUT_H
#define ZCC_LAYOUT_H

#include <stddef.h>
#include <stdbool.h>
#include "zcc_types.h"
#include "zcc_source.h"

typedef enum {
    LAYOUT_PHASE_INIT,
    LAYOUT_PHASE_SIZEOF,
    LAYOUT_PHASE_ALIGNOF,
    LAYOUT_PHASE_STRUCT,
    LAYOUT_PHASE_UNION,
    LAYOUT_PHASE_CODEGEN
} LayoutPhase;

typedef struct {
    size_t size;
    size_t align;
    size_t padded_size;
    bool valid;
} TypeLayout;

TypeLayout zcc_get_layout(Type *type, LayoutPhase phase);
size_t zcc_sizeof(Type *type);
size_t zcc_alignof(Type *type);

#endif