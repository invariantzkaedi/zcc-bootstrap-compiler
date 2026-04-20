#define MINIZ_NO_ARCHIVE_APIS
#define MINIZ_NO_ARCHIVE_WRITING_APIS

/* include miniz.c directly if we want to compile as a single unit or just let gcc/zcc compile miniz.c and link it */
/* But the prompt says "Include miniz.c" */
#include "miniz.c"

#define CHUNK_SIZE (64 * 1024)

int main() {
    unsigned char *in_buf = (unsigned char *)malloc(CHUNK_SIZE);
    unsigned char *comp_buf = (unsigned char *)malloc(CHUNK_SIZE * 2); /* generous buffer for compression */
    unsigned char *decomp_buf = (unsigned char *)malloc(CHUNK_SIZE);
    
    if (!in_buf || !comp_buf || !decomp_buf) {
        printf("FAIL: out of memory\n");
        return 1;
    }

    /* Fill with a known pattern */
    size_t i;
    for (i = 0; i < CHUNK_SIZE; i++) {
        in_buf[i] = (unsigned char)(i % 251);
    }

    unsigned long comp_len = CHUNK_SIZE * 2;
    int status = mz_compress(comp_buf, &comp_len, in_buf, CHUNK_SIZE);
    if (status != MZ_OK) {
        printf("FAIL: compression error %d\n", status);
        return 1;
    }

    unsigned long decomp_len = CHUNK_SIZE;
    status = mz_uncompress(decomp_buf, &decomp_len, comp_buf, comp_len);
    if (status != MZ_OK) {
        printf("FAIL: decompression error %d\n", status);
        return 1;
    }

    if (decomp_len != CHUNK_SIZE) {
        printf("FAIL: size mismatch %lu vs %d\n", decomp_len, CHUNK_SIZE);
        return 1;
    }

    /* Verification */
    for (i = 0; i < CHUNK_SIZE; i++) {
        if (in_buf[i] != decomp_buf[i]) {
            printf("FAIL: data corruption at byte %lu\n", (unsigned long)i);
            return 1;
        }
    }

    /* calculate simple checksum for the bonus */
    unsigned long long checksum = 0;
    for (i = 0; i < comp_len; i++) {
        checksum = (checksum + comp_buf[i]) * 31;
    }

    printf("PASS (compressed %d bytes down to %lu bytes, checksum %llu)\n", CHUNK_SIZE, comp_len, checksum);

    free(in_buf);
    free(comp_buf);
    free(decomp_buf);

    return 0;
}
