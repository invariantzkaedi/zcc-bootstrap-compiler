#include "zcc_glb_ingest.h"
#include <string.h>

#pragma pack(push, 1)
typedef struct { uint32_t magic; uint32_t version; uint32_t length; } GlbHeader;
typedef struct { uint32_t chunkLength; uint32_t chunkType; } ChunkHeader;
#pragma pack(pop)

#define GLB_MAGIC 0x46546C67u
#define CHUNK_JSON 0x4E4F534Au
#define CHUNK_BIN  0x004E4942u

int zcc_parse_glb(const uint8_t* data, size_t size, ZccGlbParsed* out) {
    if (!data || size < 12 || !out) return 0;
    memset(out, 0, sizeof(*out));

    const GlbHeader* hdr = (const GlbHeader*)data;
    if (hdr->magic != GLB_MAGIC || hdr->version != 2) return 0;

    size_t offset = 12;
    while (offset + 8 <= size) {
        const ChunkHeader* ch = (const ChunkHeader*)(data + offset);
        uint32_t clen = ch->chunkLength;
        if (offset + 8 + clen > size) break;

        if (ch->chunkType == CHUNK_JSON) {
            out->json_data = data + offset + 8;
            out->json_length = clen;
        } else if (ch->chunkType == CHUNK_BIN) {
            out->bin_data = data + offset + 8;
            out->bin_length = clen;
        }
        offset += 8 + clen;
    }
    out->valid = (out->json_data && out->bin_data);
    return out->valid;
}
