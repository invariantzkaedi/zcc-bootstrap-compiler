/*
 * evm_cfg_driver.c — StateHealer_runtime.evm CFG Extraction Driver
 * =================================================================
 * Reads an ASCII-hex .evm file (as produced by the StateHealer deployment
 * toolchain), decodes it to raw bytes, and runs evm_cfg_dump() to emit
 * the flat JSON edge list to stdout (or a named output file).
 *
 * Usage:
 *   ./evm_cfg_driver <input.evm> [output.json]
 *
 * Compile:
 *   gcc -I../.. -I. -O2 -o evm_cfg_driver evm_cfg_driver.c evm_cfg_dump.c
 */

#include "evm_cfg_dump.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static int hex_nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/*
 * load_evm_file() — Auto-detecting EVM loader.
 *
 * Supports two formats:
 *   RAW BINARY : file contains non-printable bytes (actual opcode bytes).
 *                Loaded verbatim.
 *   ASCII HEX  : file contains printable hex digits (0-9, a-f, A-F),
 *                optional 0x prefix / whitespace.  Decoded to bytes.
 *
 * Detection: sniff first min(16, fsize) bytes.  If any byte falls outside
 * the printable ASCII range [0x09, 0x7E] (allowing TAB/LF/CR), treat as
 * binary.  This correctly handles EVM payloads starting with PUSH1 (0x60).
 */
static unsigned char *load_evm_file(const char *path, int *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "[DRIVER] Cannot open: %s\n", path); return NULL; }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    rewind(f);
    if (fsize <= 0) { fclose(f); return NULL; }

    unsigned char *raw = (unsigned char *)malloc((size_t)fsize + 1);
    if (!raw) { fclose(f); return NULL; }
    long nread = (long)fread(raw, 1, (size_t)fsize, f);
    fclose(f);
    raw[nread] = '\0';

    /* ── Format sniff: check first 16 bytes ── */
    int is_binary = 0;
    long sniff = nread < 16 ? nread : 16;
    for (long i = 0; i < sniff; i++) {
        unsigned char b = raw[i];
        /* Printable ASCII + TAB/LF/CR are text; anything else → binary */
        if (b != 0x09 && b != 0x0a && b != 0x0d && (b < 0x20 || b > 0x7e)) {
            is_binary = 1;
            break;
        }
    }

    if (is_binary) {
        /* Raw binary — return as-is */
        fprintf(stderr, "[DRIVER] Format: RAW BINARY (%ld bytes)\n", nread);
        *out_len = (int)nread;
        return raw;
    }

    /* ASCII hex — decode */
    fprintf(stderr, "[DRIVER] Format: ASCII HEX (%ld chars)\n", nread);
    char *src = (char *)raw;
    if (src[0] == '0' && (src[1] == 'x' || src[1] == 'X')) src += 2;

    int hex_len = 0;
    for (int i = 0; src[i]; i++)
        if (isxdigit((unsigned char)src[i])) hex_len++;

    int byte_count = hex_len / 2;
    unsigned char *buf = (unsigned char *)malloc((size_t)byte_count + 1);
    if (!buf) { free(raw); return NULL; }

    int bi = 0, ni = 0, hi = -1;
    for (int i = 0; src[i] && bi < byte_count; i++) {
        int n = hex_nibble(src[i]);
        if (n < 0) continue;
        if (ni == 0) { hi = n; ni = 1; }
        else         { buf[bi++] = (unsigned char)((hi << 4) | n); ni = 0; }
    }

    free(raw);
    *out_len = byte_count;
    return buf;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <input.evm> [output.json]\n", argv[0]);
        return 1;
    }

    int len = 0;
    unsigned char *bc = load_evm_file(argv[1], &len);
    if (!bc || len == 0) {
        fprintf(stderr, "[DRIVER] Failed to load: %s\n", argv[1]);
        return 1;
    }
    fprintf(stderr, "[DRIVER] Decoded %d bytes from %s\n", len, argv[1]);

    FILE *out = stdout;
    if (argc >= 3) {
        out = fopen(argv[2], "w");
        if (!out) {
            fprintf(stderr, "[DRIVER] Cannot write: %s\n", argv[2]);
            free(bc);
            return 1;
        }
    }

    int edges = evm_cfg_dump(bc, len, out);
    if (argc >= 3) fclose(out);
    free(bc);

    if (edges < 0) {
        fprintf(stderr, "[DRIVER] CFG extraction failed.\n");
        return 1;
    }
    fprintf(stderr, "[DRIVER] Emitted %d CFG edges.\n", edges);
    return 0;
}
