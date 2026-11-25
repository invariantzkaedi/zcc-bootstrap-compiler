/*
 * Win64 Sovereign Minimal DLL CRT Shim for ZCC PE32+ Libraries
 * ============================================================
 * Provides DllMain stdio stream initialization (__iob_func offsets 0/48/96
 * in System32 msvcrt.dll) and C99 aliases without pulling in main() args.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>

extern void *__iob_func(void);
extern long long _strtoi64(const char *nptr, char **endptr, int base);
extern unsigned long long _strtoui64(const char *nptr, char **endptr, int base);
extern long long _atoi64(const char *s);
extern int _getpid(void);
extern char *_getcwd(char *buf, int size);
extern int *_errno(void);

void *stdin;
void *stdout;
void *stderr;

int __stdcall DllMain(void *hinstDLL, uint32_t fdwReason, void *lpvReserved) {
    (void)hinstDLL;
    (void)lpvReserved;
    if (fdwReason == 1U) {
        char *iob = (char *)__iob_func();
        stdin = (void *)iob;
        stdout = (void *)(iob + 48);
        stderr = (void *)(iob + 96);
    }
    return 1;
}

long long strtoll(const char *nptr, char **endptr, int base) {
    return _strtoi64(nptr, endptr, base);
}

unsigned long long strtoull(const char *nptr, char **endptr, int base) {
    return _strtoui64(nptr, endptr, base);
}

long long atoll(const char *s) {
    return _atoi64(s);
}

int getpid(void) {
    return _getpid();
}

char *getcwd(char *buf, int size) {
    return _getcwd(buf, size);
}

int *__errno_location(void) {
    return _errno();
}
