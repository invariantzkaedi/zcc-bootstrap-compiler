/*
 * zld.c — ZKAEDI Freestanding ELF-64 Static Linker (Driver Wrapper)
 * Includes the main static linker logic from src/zld.c and wraps it for standalone CLI execution.
 */

#include "src/zld.c"

/* ── main ───────────────────────────────────────────────────────────────── */
int main(int argc, char *argv[]) {
    char *ld_script = NULL;
    const char **obj_files = NULL;
    int obj_count = 0;
    int obj_cap = 0;
    int i;

    char *env_verb = getenv("ZLD_VERBOSE");
    g_verbose = env_verb ? atoi(env_verb) : 0;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-T") == 0 && i + 1 < argc) {
            ld_script = argv[++i];
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            strncpy(g_output, argv[++i], sizeof(g_output) - 1);
            g_output[sizeof(g_output) - 1] = '\0';
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            g_verbose = 1;
        } else if (argv[i][0] != '-') {
            if (obj_count >= obj_cap) {
                obj_cap = obj_cap ? obj_cap * 2 : 16;
                obj_files = xrealloc(obj_files, obj_cap * sizeof(char *));
            }
            obj_files[obj_count++] = argv[i];
        }
    }

    if (!obj_count) {
        fprintf(stderr, "usage: zld [-T linker.ld] [-o output] [-v|--verbose] file.o...\n");
        if (obj_files) free(obj_files);
        return 1;
    }

    zld_link(obj_files, obj_count, g_output, ld_script);

    free(obj_files);
    return 0;
}
