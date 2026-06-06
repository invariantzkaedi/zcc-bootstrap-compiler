/*
 * zcc_build_ledger.c — D-27: Sovereign Build Ledger
 *
 * An append-only provenance chain. Each "build event" consists of:
 *   - a build_id (from genome)
 *   - a timestamp (unix epoch stored as string for portability)
 *   - a genome version tag
 *   - a SHA-256 over (prev_hash || build_id || version || stability_score)
 *
 * The ledger is stored as a flat text file (NDJSON) where each line is
 * one immutable entry. The chain is validated by re-computing each entry's
 * hash from its predecessor.
 *
 * Operations:
 *   append   --ledger <file> --genome <file> --version <tag> [--stability N]
 *   verify   --ledger <file>
 *   dump     --ledger <file>
 *
 * SHA-256 implementation: pure C, self-contained (same approach as
 * zcc_sha256.h in the existing toolchain). No external crypto dependency.
 *
 * Memory discipline: all buffers are stack-allocated or malloc/free paired.
 * No phantom closures.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

/* ── Inline SHA-256 (FIPS 180-4, pure C) ─────────────────────────────── */

static const uint32_t K256[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,
    0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,
    0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,
    0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,
    0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,
    0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,
    0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,
    0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,
    0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

#define ROTR(x,n) (((x)>>(n))|((x)<<(32-(n))))
#define S0(x)  (ROTR(x,2)^ROTR(x,13)^ROTR(x,22))
#define S1(x)  (ROTR(x,6)^ROTR(x,11)^ROTR(x,25))
#define S2(x)  (ROTR(x,7)^ROTR(x,18)^((x)>>3))
#define S3(x)  (ROTR(x,17)^ROTR(x,19)^((x)>>10))
#define CH(x,y,z) (((x)&(y))^((~(x))&(z)))
#define MAJ(x,y,z)(((x)&(y))^((x)&(z))^((y)&(z)))

typedef struct {
    uint32_t state[8];
    uint64_t bitlen;
    uint32_t datalen;
    uint8_t  data[64];
} SHA256_CTX;

static void sha256_transform(SHA256_CTX *ctx, const uint8_t *d) {
    uint32_t a,b,c,e,f,g,h,t1,t2,W[64];
    int i;
    for (i=0;i<16;i++) W[i]=((uint32_t)d[i*4]<<24)|((uint32_t)d[i*4+1]<<16)|((uint32_t)d[i*4+2]<<8)|(d[i*4+3]);
    for (;i<64;i++) W[i]=S3(W[i-2])+W[i-7]+S2(W[i-15])+W[i-16];
    a=ctx->state[0]; b=ctx->state[1]; c=ctx->state[2]; uint32_t dd=ctx->state[3];
    e=ctx->state[4]; f=ctx->state[5]; g=ctx->state[6]; h=ctx->state[7];
    for (i=0;i<64;i++){
        t1=h+S1(e)+CH(e,f,g)+K256[i]+W[i];
        t2=S0(a)+MAJ(a,b,c);
        h=g;g=f;f=e;e=dd+t1;dd=c;c=b;b=a;a=t1+t2;
    }
    ctx->state[0]+=a; ctx->state[1]+=b; ctx->state[2]+=c; ctx->state[3]+=dd;
    ctx->state[4]+=e; ctx->state[5]+=f; ctx->state[6]+=g; ctx->state[7]+=h;
}

static void sha256_init(SHA256_CTX *ctx) {
    ctx->datalen=0; ctx->bitlen=0;
    ctx->state[0]=0x6a09e667; ctx->state[1]=0xbb67ae85;
    ctx->state[2]=0x3c6ef372; ctx->state[3]=0xa54ff53a;
    ctx->state[4]=0x510e527f; ctx->state[5]=0x9b05688c;
    ctx->state[6]=0x1f83d9ab; ctx->state[7]=0x5be0cd19;
}

static void sha256_update(SHA256_CTX *ctx, const uint8_t *data, size_t len) {
    for (size_t i=0;i<len;i++){
        ctx->data[ctx->datalen++]=data[i];
        if(ctx->datalen==64){ sha256_transform(ctx,ctx->data); ctx->bitlen+=512; ctx->datalen=0; }
    }
}

static void sha256_final(SHA256_CTX *ctx, uint8_t *hash) {
    uint32_t i=ctx->datalen;
    ctx->data[i++]=0x80;
    if(i>56){ while(i<64) ctx->data[i++]=0; sha256_transform(ctx,ctx->data); i=0; }
    while(i<56) ctx->data[i++]=0;
    ctx->bitlen+=ctx->datalen*8;
    ctx->data[63]=(uint8_t)(ctx->bitlen);       ctx->data[62]=(uint8_t)(ctx->bitlen>>8);
    ctx->data[61]=(uint8_t)(ctx->bitlen>>16);   ctx->data[60]=(uint8_t)(ctx->bitlen>>24);
    ctx->data[59]=(uint8_t)(ctx->bitlen>>32);   ctx->data[58]=(uint8_t)(ctx->bitlen>>40);
    ctx->data[57]=(uint8_t)(ctx->bitlen>>48);   ctx->data[56]=(uint8_t)(ctx->bitlen>>56);
    sha256_transform(ctx,ctx->data);
    for(i=0;i<4;i++){
        hash[i]   =(ctx->state[0]>>(24-i*8))&0xff;
        hash[i+4] =(ctx->state[1]>>(24-i*8))&0xff;
        hash[i+8] =(ctx->state[2]>>(24-i*8))&0xff;
        hash[i+12]=(ctx->state[3]>>(24-i*8))&0xff;
        hash[i+16]=(ctx->state[4]>>(24-i*8))&0xff;
        hash[i+20]=(ctx->state[5]>>(24-i*8))&0xff;
        hash[i+24]=(ctx->state[6]>>(24-i*8))&0xff;
        hash[i+28]=(ctx->state[7]>>(24-i*8))&0xff;
    }
}

static void sha256_hash_str(const char *input, char *hex_out) {
    /* hex_out must be at least 65 bytes */
    SHA256_CTX ctx;
    uint8_t hash[32];
    sha256_init(&ctx);
    sha256_update(&ctx, (const uint8_t *)input, strlen(input));
    sha256_final(&ctx, hash);
    for (int i = 0; i < 32; i++)
        sprintf(hex_out + i * 2, "%02x", hash[i]);
    hex_out[64] = '\0';
}

/* ── JSON helpers ────────────────────────────────────────────────────── */

static void die(const char *msg) {
    fprintf(stderr, "zcc_build_ledger: fatal: %s\n", msg);
    exit(1);
}

static uint8_t *load_file(const char *path, size_t *sz) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    *sz = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *buf = malloc(*sz + 1);
    if (!buf) die("out of memory");
    if (fread(buf, 1, *sz, f) != *sz) die("file read error");
    fclose(f);
    buf[*sz] = 0;
    return buf;
}

static int find_json_string_scoped(const char *scope, const char *key,
                                   char *out, int max) {
    if (!scope) return 0;
    const char *p = strstr(scope, key);
    if (!p) return 0;
    p += strlen(key);
    p = strchr(p, ':');
    if (!p) return 0;
    p++;
    p = strchr(p, '"');
    if (!p) return 0;
    p++;
    int len = 0;
    while (*p && *p != '"' && len < max - 1) out[len++] = *p++;
    out[len] = '\0';
    return 1;
}

/* ── Ledger entry ─────────────────────────────────────────────────────── */
/* One line in the ledger file is a compact JSON object. */

#define MAX_ENTRIES 4096

typedef struct {
    char entry_hash[65];   /* SHA-256 of (prev_hash||build_id||version||score_str) */
    char prev_hash[65];
    char build_id[65];
    char version[64];
    char timestamp[32];
    int  stability_score;
    long line_number;
} LedgerEntry;

/* ── Append mode ─────────────────────────────────────────────────────── */

static void compute_entry_hash(const char *prev_hash,
                                const char *build_id,
                                const char *version,
                                int stability_score,
                                char *out_hex) {
    char buf[512];
    snprintf(buf, sizeof(buf), "%s|%s|%s|%d", prev_hash, build_id, version, stability_score);
    sha256_hash_str(buf, out_hex);
}

static int do_append(const char *ledger_path, const char *genome_path,
                     const char *version, int stability_score) {
    /* Extract build_id from genome JSON */
    char build_id[65] = "unknown";
    if (genome_path) {
        size_t sz = 0;
        uint8_t *content = load_file(genome_path, &sz);
        if (content) {
            const char *meta = strstr((const char *)content, "\"metadata\"");
            if (meta) find_json_string_scoped(meta, "\"build_id\"", build_id, 65);
            /* fallback: try top-level */
            if (build_id[0] == '\0' || strcmp(build_id, "unknown") == 0)
                find_json_string_scoped((const char *)content, "\"build_id\"", build_id, 65);
            free(content);
        } else {
            fprintf(stderr, "warning: cannot read genome %s — using 'unknown' build_id\n", genome_path);
        }
    }

    /* Read the last entry's hash from the ledger (if it exists) */
    char prev_hash[65];
    strcpy(prev_hash, "0000000000000000000000000000000000000000000000000000000000000000");

    FILE *rf = fopen(ledger_path, "r");
    if (rf) {
        /* Scan to last non-empty line */
        char line[1024];
        char last_line[1024];
        last_line[0] = '\0';
        while (fgets(line, sizeof(line), rf)) {
            if (line[0] && line[0] != '\n' && line[0] != '\r')
                strncpy(last_line, line, sizeof(last_line) - 1);
        }
        fclose(rf);

        /* Extract entry_hash from last line JSON */
        if (last_line[0]) {
            char eh[65] = "";
            find_json_string_scoped(last_line, "\"entry_hash\"", eh, 65);
            if (eh[0]) strncpy(prev_hash, eh, 65);
        }
    }

    /* Timestamp (seconds since epoch as decimal string) */
    char timestamp[32];
    snprintf(timestamp, sizeof(timestamp), "%lld", (long long)time(NULL));

    /* Compute new entry hash */
    char new_hash[65];
    compute_entry_hash(prev_hash, build_id, version, stability_score, new_hash);

    /* Append to ledger */
    FILE *wf = fopen(ledger_path, "a");
    if (!wf) { fprintf(stderr, "error: cannot open ledger %s for append\n", ledger_path); return 1; }

    fprintf(wf, "{\"entry_hash\":\"%s\",\"prev_hash\":\"%s\","
            "\"build_id\":\"%s\",\"version\":\"%s\","
            "\"timestamp\":\"%s\",\"stability_score\":%d}\n",
            new_hash, prev_hash, build_id, version, timestamp, stability_score);
    fclose(wf);

    printf("Appended entry to ledger: %s\n", ledger_path);
    printf("  version:          %s\n", version);
    printf("  build_id:         %s\n", build_id);
    printf("  stability_score:  %d\n", stability_score);
    printf("  entry_hash:       %s\n", new_hash);
    printf("  prev_hash:        %.16s...\n", prev_hash);
    return 0;
}

/* ── Verify mode ─────────────────────────────────────────────────────── */

static int do_verify(const char *ledger_path) {
    FILE *rf = fopen(ledger_path, "r");
    if (!rf) { fprintf(stderr, "error: cannot open ledger %s\n", ledger_path); return 1; }

    printf("=== ZCC Sovereign Build Ledger Verification ===\n");
    printf("Ledger: %s\n\n", ledger_path);

    int line_num = 0;
    int errors   = 0;
    char expected_prev[65];
    strcpy(expected_prev, "0000000000000000000000000000000000000000000000000000000000000000");

    char line[2048];
    while (fgets(line, sizeof(line), rf)) {
        /* Strip trailing newline */
        int llen = (int)strlen(line);
        while (llen > 0 && (line[llen-1] == '\n' || line[llen-1] == '\r'))
            line[--llen] = '\0';
        if (llen == 0) continue;

        line_num++;
        char entry_hash[65]="", prev_hash[65]="", build_id[65]="";
        char version[64]="", timestamp[32]="";
        int  stability_score = 0;

        find_json_string_scoped(line, "\"entry_hash\"",    entry_hash, 65);
        find_json_string_scoped(line, "\"prev_hash\"",     prev_hash,  65);
        find_json_string_scoped(line, "\"build_id\"",      build_id,   65);
        find_json_string_scoped(line, "\"version\"",       version,    64);
        find_json_string_scoped(line, "\"timestamp\"",     timestamp,  32);

        /* Extract stability_score (integer) */
        {
            const char *p = strstr(line, "\"stability_score\":");
            if (p) {
                p += strlen("\"stability_score\":");
                while (*p == ' ') p++;
                stability_score = atoi(p);
            }
        }

        /* Verify prev_hash chain */
        if (strcmp(prev_hash, expected_prev) != 0) {
            printf("  [CHAIN BREAK] line %d: prev_hash mismatch\n", line_num);
            printf("    expected: %.32s...\n", expected_prev);
            printf("    found:    %.32s...\n", prev_hash);
            errors++;
        }

        /* Recompute entry_hash */
        char computed[65];
        compute_entry_hash(prev_hash, build_id, version, stability_score, computed);
        if (strcmp(entry_hash, computed) != 0) {
            printf("  [HASH FAIL]   line %d (%s %s): entry hash mismatch\n",
                   line_num, version, build_id);
            printf("    stored:   %s\n", entry_hash);
            printf("    computed: %s\n", computed);
            errors++;
        } else {
            printf("  [OK] line %-4d  %-10s  score=%-3d  hash=%.16s...\n",
                   line_num, version, stability_score, entry_hash);
        }

        strncpy(expected_prev, entry_hash, 65);
    }
    fclose(rf);

    printf("\n");
    printf("Ledger entries:  %d\n", line_num);
    printf("Errors detected: %d\n", errors);
    printf("Chain integrity: %s\n", errors == 0 ? "VERIFIED" : "BROKEN");
    return errors == 0 ? 0 : 1;
}

/* ── Dump mode ───────────────────────────────────────────────────────── */

static int do_dump(const char *ledger_path) {
    FILE *rf = fopen(ledger_path, "r");
    if (!rf) { fprintf(stderr, "error: cannot open ledger %s\n", ledger_path); return 1; }

    printf("=== ZCC Sovereign Build Ledger ===\n");
    printf("Ledger: %s\n\n", ledger_path);
    printf("%-6s  %-10s  %-8s  %-32s  %s\n",
           "Entry", "Version", "Score", "Hash (first 32)", "Build ID");
    printf("------  ----------  --------  --------------------------------  --------\n");

    int line_num = 0;
    char line[2048];
    while (fgets(line, sizeof(line), rf)) {
        int llen = (int)strlen(line);
        while (llen > 0 && (line[llen-1] == '\n' || line[llen-1] == '\r'))
            line[--llen] = '\0';
        if (llen == 0) continue;
        line_num++;

        char entry_hash[65]="", build_id[65]="", version[64]="";
        int  stability_score = 0;
        find_json_string_scoped(line, "\"entry_hash\"", entry_hash, 65);
        find_json_string_scoped(line, "\"build_id\"",   build_id,   65);
        find_json_string_scoped(line, "\"version\"",    version,    64);
        {
            const char *p = strstr(line, "\"stability_score\":");
            if (p) { p += strlen("\"stability_score\":"); while(*p==' ')p++; stability_score = atoi(p); }
        }

        printf("%-6d  %-10s  %-8d  %.32s  %s\n",
               line_num, version, stability_score, entry_hash, build_id);
    }
    fclose(rf);
    printf("\nTotal entries: %d\n", line_num);
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════ */

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage:\n");
        printf("  zcc_build_ledger append --ledger <file> --genome <genome.json> "
               "--version <tag> [--stability N]\n");
        printf("  zcc_build_ledger verify --ledger <file>\n");
        printf("  zcc_build_ledger dump   --ledger <file>\n");
        return 2;
    }

    const char *op          = argv[1];
    const char *ledger_path = NULL;
    const char *genome_path = NULL;
    const char *version_tag = NULL;
    int stability_score     = 80; /* default */

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--ledger") == 0 && i + 1 < argc)
            ledger_path = argv[++i];
        else if (strcmp(argv[i], "--genome") == 0 && i + 1 < argc)
            genome_path = argv[++i];
        else if (strcmp(argv[i], "--version") == 0 && i + 1 < argc)
            version_tag = argv[++i];
        else if (strcmp(argv[i], "--stability") == 0 && i + 1 < argc)
            stability_score = atoi(argv[++i]);
    }

    if (!ledger_path) {
        fprintf(stderr, "error: --ledger is required\n");
        return 2;
    }

    if (strcmp(op, "append") == 0) {
        if (!version_tag) {
            fprintf(stderr, "error: --version is required for append\n");
            return 2;
        }
        return do_append(ledger_path, genome_path, version_tag, stability_score);
    } else if (strcmp(op, "verify") == 0) {
        return do_verify(ledger_path);
    } else if (strcmp(op, "dump") == 0) {
        return do_dump(ledger_path);
    } else {
        fprintf(stderr, "error: unknown operation '%s'\n", op);
        return 2;
    }
}
