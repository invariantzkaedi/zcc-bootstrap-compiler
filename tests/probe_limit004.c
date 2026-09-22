#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * tests/probe_limit004.c
 * Minimal probe for LIMIT-004:
 * 1. Multi-archive (.a) static library ingestion & symbol resolution.
 * 2. Freestanding static ELF64 executable execution via synthetic _start CRT0.
 */

int main(void) {
    int rc;

    /* Step 1: Create source for library archive A (arithmetic) */
    FILE *f = fopen("/tmp/probe_l4_math.c", "w");
    if (!f) return 1;
    fprintf(f, "int probe_add(int a, int b) { return a + b; }\n");
    fprintf(f, "int probe_sub(int a, int b) { return a - b; }\n");
    fclose(f);

    /* Step 2: Create source for library archive B (multiplier) */
    f = fopen("/tmp/probe_l4_mul.c", "w");
    if (!f) return 1;
    fprintf(f, "int probe_mul(int a, int b) { return a * b; }\n");
    fclose(f);

    /* Step 3: Create source for main program (no _start, standard main) */
    f = fopen("/tmp/probe_l4_main.c", "w");
    if (!f) return 1;
    fprintf(f,
        "extern int probe_add(int, int);\n"
        "extern int probe_mul(int, int);\n"
        "int main(int argc, char **argv) {\n"
        "    int s = probe_add(10, 20);\n"    /* 30 */
        "    int p = probe_mul(s, 2);\n"      /* 60 */
        "    if (p == 60) return 0;\n"
        "    return 1;\n"
        "}\n"
    );
    fclose(f);

    /* Compile components with ZCC */
    system("./zcc -c /tmp/probe_l4_math.c -o /tmp/probe_l4_math.o >/dev/null 2>&1");
    system("./zcc -c /tmp/probe_l4_mul.c -o /tmp/probe_l4_mul.o >/dev/null 2>&1");
    system("./zcc -c /tmp/probe_l4_main.c -o /tmp/probe_l4_main.o >/dev/null 2>&1");

    /* Create static archives with ar */
    system("ar rcs /tmp/libprobe_math.a /tmp/probe_l4_math.o >/dev/null 2>&1");
    system("ar rcs /tmp/libprobe_mul.a /tmp/probe_l4_mul.o >/dev/null 2>&1");

    /* Attempt to link main with both archives using ZCC -zld */
    rc = system("./zcc -zld /tmp/probe_l4_main.o /tmp/libprobe_math.a /tmp/libprobe_mul.a -o /tmp/probe_l4_exe 2>/tmp/probe_l4_link.log");
    if (rc != 0) {
        printf("PROBE_LIMIT004 FAIL: Linker failed (rc=%d)\n", rc);
        system("cat /tmp/probe_l4_link.log");
        return 1;
    }

    /* Execute the linked binary */
    system("chmod +x /tmp/probe_l4_exe");
    rc = system("/tmp/probe_l4_exe");
    if (rc != 0) {
        printf("PROBE_LIMIT004 FAIL: Binary execution failed with rc=%d\n", rc);
        return 2;
    }

    printf("PROBE_LIMIT004 PASS: Multi-archive linking and freestanding execution verified!\n");
    return 0;
}
