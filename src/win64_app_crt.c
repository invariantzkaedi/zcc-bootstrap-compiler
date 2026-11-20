/*
 * ZCC Standalone Win64 Application CRT Runtime (`src/win64_app_crt.c`)
 * =====================================================================
 * Minimal Microsoft x64 PE32+ startup & C99/POSIX MSVCRT bridge for
 * standalone native Windows console executables (zero MSVC/MinGW/binutils,
 * zero compiler-internal stubs).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern FILE *__iob_func(void);
extern int *_errno(void);
extern int __getmainargs(int *argc, char ***argv, char ***envp, int doWildCard, void *startupinfo);
extern int main(int argc, char **argv);

FILE *stdin = 0;
FILE *stdout = 0;
FILE *stderr = 0;

void __zcc_win64_app_init(void) {
    static int initialized = 0;
    if (initialized) return;
    initialized = 1;
    char *iob = (char *)__iob_func();
    if (iob) {
        stdin = (FILE *)(iob + 0);
        stdout = (FILE *)(iob + 48);
        stderr = (FILE *)(iob + 96);
    }
}

void mainCRTStartup(void) {
    __zcc_win64_app_init();
    int argc = 0;
    char **argv = NULL;
    char **envp = NULL;
    int si[16] = {0};
    __getmainargs(&argc, &argv, &envp, 0, si);
    static char *default_argv[2] = {"app.exe", NULL};
    if (argc <= 0 || !argv) {
        argc = 1;
        argv = default_argv;
    }
    int ret = main(argc, argv);
    exit(ret);
}

/* MSVCRT 64-bit integer conversion and POSIX compatibility wrappers */
extern long long _strtoi64(const char *nptr, char **endptr, int base);
extern unsigned long long _strtoui64(const char *nptr, char **endptr, int base);
extern long long _atoi64(const char *nptr);
extern int _getpid(void);
extern char *_getcwd(char *buf, int size);

long long strtoll(const char *nptr, char **endptr, int base) { return _strtoi64(nptr, endptr, base); }
unsigned long long strtoull(const char *nptr, char **endptr, int base) { return _strtoui64(nptr, endptr, base); }
long long atoll(const char *nptr) { return _atoi64(nptr); }
int getpid(void) { return _getpid(); }
char *getcwd(char *buf, size_t size) { return _getcwd(buf, (int)size); }
int *__errno_location(void) { return _errno(); }
