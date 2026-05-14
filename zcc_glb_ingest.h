#ifndef ZCC_GLB_INGEST_H
#define ZCC_GLB_INGEST_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    const uint8_t* json_data;
    uint32_t       json_length;
    const uint8_t* bin_data;
    uint32_t       bin_length;
    int            valid;
} ZccGlbParsed;

int zcc_parse_glb(const uint8_t* data, size_t size, ZccGlbParsed* out);

#endif
