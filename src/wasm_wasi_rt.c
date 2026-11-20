/*
 * ZCC Freestanding WASI Preview1 C Runtime (src/wasm_wasi_rt.c)
 * ============================================================
 * Provides `_start`, buffered WASI Preview1 FILE* stdio, formatting/scanf,
 * string/ctype/stdlib/math helpers, and optional side-pass stubs so ZCC
 * compiles itself and C programs to standalone WASI .wasm modules with zero
 * `env.*` imports (strictly `wasi_snapshot_preview1.*` + synthesized heap).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <stddef.h>

/* -------------------------------------------------------------------------
 * 1. WASI Preview1 Imports (`__wasi_*` -> `wasi_snapshot_preview1.*`)
 * ------------------------------------------------------------------------- */
extern int __wasi_args_sizes_get(uint32_t *argc, uint32_t *argv_buf_size);
extern int __wasi_args_get(uint32_t *argv, uint8_t *argv_buf);
extern int __wasi_environ_sizes_get(uint32_t *envc, uint32_t *env_buf_size);
extern int __wasi_environ_get(uint32_t *env, uint8_t *env_buf);
extern int __wasi_clock_time_get(uint32_t id, uint64_t precision, uint64_t *out_time);
extern int __wasi_fd_close(int fd);
extern int __wasi_fd_fdstat_get(int fd, void *stat_buf);
extern int __wasi_fd_prestat_get(int fd, void *prestat_buf);
extern int __wasi_fd_prestat_dir_name(int fd, uint8_t *path, uint32_t path_len);
extern int __wasi_fd_read(int fd, const void *iovs, uint32_t iovs_len, uint32_t *nread);
extern int __wasi_fd_seek(int fd, int64_t offset, uint32_t whence, uint64_t *newoffset);
extern int __wasi_fd_write(int fd, const void *iovs, uint32_t iovs_len, uint32_t *nwritten);
extern int __wasi_path_open(int dirfd, uint32_t dirflags, const char *path, uint32_t path_len,
                            uint32_t oflags, uint64_t fs_rights_base, uint64_t fs_rights_inheriting,
                            uint32_t fdflags, int *out_fd);
extern int __wasi_path_unlink_file(int dirfd, const char *path, uint32_t path_len);
extern void __wasi_proc_exit(int rval);

/* Synthesized allocator declarations provided by ZCC WASM backend */
extern void *malloc(size_t size);
extern void *calloc(size_t nmemb, size_t size);
extern void *realloc(void *ptr, size_t size);
extern void free(void *ptr);

extern int main(int argc, char **argv);

typedef struct {
    uint32_t buf;
    uint32_t len;
} _wasi_rt_iovec_t;

#define _WASI_RT_O_CREAT      1U
#define _WASI_RT_O_TRUNC      8U
#define _WASI_RT_FDFLAG_APP   1U

#define _WASI_RT_RIGHT_FD_DATASYNC          (1ULL << 0)
#define _WASI_RT_RIGHT_FD_READ              (1ULL << 1)
#define _WASI_RT_RIGHT_FD_SEEK              (1ULL << 2)
#define _WASI_RT_RIGHT_FD_FDSTAT_SET_FLAGS  (1ULL << 3)
#define _WASI_RT_RIGHT_FD_SYNC              (1ULL << 4)
#define _WASI_RT_RIGHT_FD_TELL              (1ULL << 5)
#define _WASI_RT_RIGHT_FD_WRITE             (1ULL << 6)
#define _WASI_RT_RIGHT_FD_ALLOCATE          (1ULL << 8)
#define _WASI_RT_RIGHT_FD_FILESTAT_GET      (1ULL << 21)
#define _WASI_RT_RIGHT_FD_FILESTAT_SET_SIZE (1ULL << 22)
#define _WASI_RT_RIGHT_POLL_FD_READWRITE    (1ULL << 27)
#define _WASI_RT_RIGHTS_READ  (_WASI_RT_RIGHT_FD_READ | _WASI_RT_RIGHT_FD_SEEK | \
                               _WASI_RT_RIGHT_FD_TELL | _WASI_RT_RIGHT_FD_FILESTAT_GET | \
                               _WASI_RT_RIGHT_POLL_FD_READWRITE)
#define _WASI_RT_RIGHTS_WRITE (_WASI_RT_RIGHTS_READ | _WASI_RT_RIGHT_FD_DATASYNC | \
                               _WASI_RT_RIGHT_FD_FDSTAT_SET_FLAGS | _WASI_RT_RIGHT_FD_SYNC | \
                               _WASI_RT_RIGHT_FD_WRITE | _WASI_RT_RIGHT_FD_ALLOCATE | \
                               _WASI_RT_RIGHT_FD_FILESTAT_SET_SIZE)

/* -------------------------------------------------------------------------
 * 2. Environment, Preopen Discovery, and Error State
 * ------------------------------------------------------------------------- */
static int _wasi_rt_errno_val = 0;
static char **_wasi_rt_environ = NULL;
static int _wasi_rt_envc = 0;
static int _wasi_rt_preopen_fd = 3;
static uint64_t _wasi_rt_preopen_rights_inh = _WASI_RT_RIGHTS_WRITE;
static char _wasi_rt_preopen_prefix[256] = ".";
static unsigned int _wasi_rt_rand_state = 1U;
static void (*_wasi_rt_atexit_fns[32])(void);
static int _wasi_rt_atexit_count = 0;

int *__errno_location(void) {
    return &_wasi_rt_errno_val;
}

static void _wasi_rt_discover_preopen(void) {
    uint8_t pstat[8];
    uint8_t fdstat[24];
    int fd;
    for (fd = 3; fd < 16; fd++) {
        memset(pstat, 0, sizeof(pstat));
        if (__wasi_fd_prestat_get(fd, pstat) == 0) {
            uint32_t tag = *(uint32_t *)(pstat);
            uint32_t nlen = *(uint32_t *)(pstat + 4);
            if (tag == 0U && nlen < sizeof(_wasi_rt_preopen_prefix)) {
                memset(_wasi_rt_preopen_prefix, 0, sizeof(_wasi_rt_preopen_prefix));
                if (nlen > 0U) {
                    __wasi_fd_prestat_dir_name(fd, (uint8_t *)_wasi_rt_preopen_prefix, nlen);
                }
                _wasi_rt_preopen_fd = fd;
                memset(fdstat, 0, sizeof(fdstat));
                if (__wasi_fd_fdstat_get(fd, fdstat) == 0) {
                    uint64_t inh = *(uint64_t *)(fdstat + 16);
                    if (inh != 0ULL) _wasi_rt_preopen_rights_inh = inh;
                }
                return;
            }
        }
    }
}

static const char *_wasi_rt_relpath(const char *path) {
    if (!path) return "";
    if (_wasi_rt_preopen_prefix[0] && _wasi_rt_preopen_prefix[0] != '.' ) {
        size_t plen = 0;
        while (_wasi_rt_preopen_prefix[plen]) plen++;
        if (plen > 0) {
            int match = 1;
            size_t i;
            for (i = 0; i < plen; i++) {
                if (path[i] != _wasi_rt_preopen_prefix[i]) { match = 0; break; }
            }
            if (match) {
                const char *rest = path + plen;
                while (*rest == '/') rest++;
                if (*rest) return rest;
            }
        }
    }
    while (path[0] == '.' && path[1] == '/') path += 2;
    while (path[0] == '/') path++;
    return path[0] ? path : ".";
}

/* -------------------------------------------------------------------------
 * 3. Memory & String Primitives
 * ------------------------------------------------------------------------- */
void *memcpy(void *dst, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    size_t i;
    for (i = 0; i < n; i++) d[i] = s[i];
    return dst;
}

void *memmove(void *dst, const void *src, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    if (d == s || n == 0) return dst;
    if (d < s) {
        size_t i;
        for (i = 0; i < n; i++) d[i] = s[i];
    } else {
        size_t i = n;
        while (i > 0) {
            i--;
            d[i] = s[i];
        }
    }
    return dst;
}

void *memset(void *dst, int c, size_t n) {
    uint8_t *d = (uint8_t *)dst;
    uint8_t v = (uint8_t)c;
    size_t i;
    for (i = 0; i < n; i++) d[i] = v;
    return dst;
}

int memcmp(const void *a, const void *b, size_t n) {
    const uint8_t *pa = (const uint8_t *)a;
    const uint8_t *pb = (const uint8_t *)b;
    size_t i;
    for (i = 0; i < n; i++) {
        if (pa[i] != pb[i]) return (int)pa[i] - (int)pb[i];
    }
    return 0;
}

void *memchr(const void *s, int c, size_t n) {
    const uint8_t *p = (const uint8_t *)s;
    uint8_t ch = (uint8_t)c;
    size_t i;
    for (i = 0; i < n; i++) {
        if (p[i] == ch) return (void *)(p + i);
    }
    return NULL;
}

size_t strlen(const char *s) {
    if (!s) return 0;
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

size_t strnlen(const char *s, size_t maxlen) {
    if (!s) return 0;
    size_t n = 0;
    while (n < maxlen && s[n]) n++;
    return n;
}

int strcmp(const char *a, const char *b) {
    const unsigned char *pa = (const unsigned char *)a;
    const unsigned char *pb = (const unsigned char *)b;
    while (*pa && *pa == *pb) { pa++; pb++; }
    return (int)*pa - (int)*pb;
}

int strncmp(const char *a, const char *b, size_t n) {
    const unsigned char *pa = (const unsigned char *)a;
    const unsigned char *pb = (const unsigned char *)b;
    size_t i;
    for (i = 0; i < n; i++) {
        if (pa[i] != pb[i] || pa[i] == 0) return (int)pa[i] - (int)pb[i];
    }
    return 0;
}

int strcoll(const char *a, const char *b) {
    return strcmp(a, b);
}

size_t strxfrm(char *dst, const char *src, size_t n) {
    size_t len = strlen(src);
    if (dst && n > 0) {
        size_t copy = (len < n - 1) ? len : (n - 1);
        memcpy(dst, src, copy);
        dst[copy] = '\0';
    }
    return len;
}

char *strcpy(char *dst, const char *src) {
    char *r = dst;
    while ((*dst++ = *src++) != 0) {}
    return r;
}

char *strncpy(char *dst, const char *src, size_t n) {
    size_t i = 0;
    for (; i < n && src[i]; i++) dst[i] = src[i];
    for (; i < n; i++) dst[i] = '\0';
    return dst;
}

char *strcat(char *dst, const char *src) {
    char *p = dst + strlen(dst);
    while ((*p++ = *src++) != 0) {}
    return dst;
}

char *strncat(char *dst, const char *src, size_t n) {
    char *p = dst + strlen(dst);
    size_t i = 0;
    while (i < n && src[i]) { *p++ = src[i++]; }
    *p = '\0';
    return dst;
}

char *strchr(const char *s, int c) {
    char ch = (char)c;
    while (*s) {
        if (*s == ch) return (char *)s;
        s++;
    }
    return (ch == '\0') ? (char *)s : NULL;
}

char *strrchr(const char *s, int c) {
    char ch = (char)c;
    const char *last = NULL;
    while (*s) {
        if (*s == ch) last = s;
        s++;
    }
    if (ch == '\0') return (char *)s;
    return (char *)last;
}

char *strstr(const char *hay, const char *needle) {
    if (!needle[0]) return (char *)hay;
    size_t nlen = strlen(needle);
    for (; *hay; hay++) {
        if (*hay == *needle && strncmp(hay, needle, nlen) == 0) return (char *)hay;
    }
    return NULL;
}

size_t strspn(const char *s, const char *accept) {
    size_t n = 0;
    while (s[n] && strchr(accept, s[n])) n++;
    return n;
}

size_t strcspn(const char *s, const char *reject) {
    size_t n = 0;
    while (s[n] && !strchr(reject, s[n])) n++;
    return n;
}

char *strpbrk(const char *s, const char *accept) {
    while (*s) {
        if (strchr(accept, *s)) return (char *)s;
        s++;
    }
    return NULL;
}

char *strtok_r(char *s, const char *delim, char **saveptr) {
    if (!s) s = *saveptr;
    if (!s) return NULL;
    s += strspn(s, delim);
    if (!*s) { *saveptr = NULL; return NULL; }
    char *end = s + strcspn(s, delim);
    if (*end) {
        *end = '\0';
        *saveptr = end + 1;
    } else {
        *saveptr = NULL;
    }
    return s;
}

static char *_wasi_rt_strtok_state = NULL;
char *strtok(char *s, const char *delim) {
    return strtok_r(s, delim, &_wasi_rt_strtok_state);
}

char *strdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *d = (char *)malloc(n);
    if (d) memcpy(d, s, n);
    return d;
}

char *strndup(const char *s, size_t n) {
    if (!s) return NULL;
    size_t l = strnlen(s, n);
    char *d = (char *)malloc(l + 1);
    if (!d) return NULL;
    memcpy(d, s, l);
    d[l] = '\0';
    return d;
}

int strcasecmp(const char *a, const char *b) {
    if (!a && !b) return 0;
    if (!a) return -1;
    if (!b) return 1;
    while (*a || *b) {
        int ca = (unsigned char)*a++;
        int cb = (unsigned char)*b++;
        if (ca >= 'A' && ca <= 'Z') ca += 32;
        if (cb >= 'A' && cb <= 'Z') cb += 32;
        if (ca != cb) return ca - cb;
    }
    return 0;
}

int strncasecmp(const char *a, const char *b, size_t n) {
    if (n == 0) return 0;
    if (!a && !b) return 0;
    if (!a) return -1;
    if (!b) return 1;
    while (n-- > 0) {
        int ca = (unsigned char)*a++;
        int cb = (unsigned char)*b++;
        if (ca >= 'A' && ca <= 'Z') ca += 32;
        if (cb >= 'A' && cb <= 'Z') cb += 32;
        if (ca != cb || ca == 0) return ca - cb;
    }
    return 0;
}

char *strerror(int errnum) {
    static char _wasi_rt_errbuf[32];
    int pos = 0;
    _wasi_rt_errbuf[pos++] = 'e'; _wasi_rt_errbuf[pos++] = 'r'; _wasi_rt_errbuf[pos++] = 'r'; _wasi_rt_errbuf[pos++] = ' ';
    if (errnum < 0) { _wasi_rt_errbuf[pos++] = '-'; errnum = -errnum; }
    char tmp[16]; int ti = 0;
    do { tmp[ti++] = (char)('0' + (errnum % 10)); errnum /= 10; } while (errnum > 0);
    while (ti > 0) _wasi_rt_errbuf[pos++] = tmp[--ti];
    _wasi_rt_errbuf[pos] = '\0';
    return _wasi_rt_errbuf;
}

/* -------------------------------------------------------------------------
 * 4. Character Classification & Numeric Conversion
 * ------------------------------------------------------------------------- */
int isspace(int c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f'; }
int isdigit(int c) { return c >= '0' && c <= '9'; }
int isupper(int c) { return c >= 'A' && c <= 'Z'; }
int islower(int c) { return c >= 'a' && c <= 'z'; }
int isalpha(int c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); }
int isalnum(int c) { return isalpha(c) || isdigit(c); }
int isxdigit(int c) { return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
int isprint(int c) { return c >= 0x20 && c <= 0x7E; }
int tolower(int c) { return isupper(c) ? (c + 32) : c; }
int toupper(int c) { return islower(c) ? (c - 32) : c; }
int abs(int n) { return n < 0 ? -n : n; }
long labs(long n) { return n < 0 ? -n : n; }
long long llabs(long long n) { return n < 0 ? -n : n; }

static int _wasi_rt_digit_val(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'z') return c - 'a' + 10;
    if (c >= 'A' && c <= 'Z') return c - 'A' + 10;
    return -1;
}

unsigned long long strtoull(const char *nptr, char **endptr, int base) {
    const char *p = nptr;
    while (isspace((unsigned char)*p)) p++;
    int neg = 0;
    if (*p == '-') { neg = 1; p++; }
    else if (*p == '+') { p++; }

    if ((base == 0 || base == 16) && p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
        if (base == 0 || _wasi_rt_digit_val((unsigned char)p[2]) < 16) {
            int d = _wasi_rt_digit_val((unsigned char)p[2]);
            if (d >= 0 && d < 16) { base = 16; p += 2; }
        }
    }
    if (base == 0) {
        if (*p == '0') base = 8;
        else base = 10;
    }

    unsigned long long acc = 0;
    int any = 0;
    while (*p) {
        int d = _wasi_rt_digit_val((unsigned char)*p);
        if (d < 0 || d >= base) break;
        acc = acc * (unsigned long long)base + (unsigned long long)d;
        any = 1;
        p++;
    }
    if (endptr) *endptr = (char *)(any ? p : nptr);
    return neg ? (unsigned long long)(-(long long)acc) : acc;
}

long long strtoll(const char *nptr, char **endptr, int base) {
    const char *p = nptr;
    while (isspace((unsigned char)*p)) p++;
    int neg = 0;
    if (*p == '-') { neg = 1; p++; }
    else if (*p == '+') { p++; }
    char *ep = NULL;
    unsigned long long u = strtoull(p, &ep, base);
    if (endptr) *endptr = (ep == p) ? (char *)nptr : ep;
    return neg ? -(long long)u : (long long)u;
}

long strtol(const char *nptr, char **endptr, int base) {
    return (long)strtoll(nptr, endptr, base);
}

unsigned long strtoul(const char *nptr, char **endptr, int base) {
    return (unsigned long)strtoull(nptr, endptr, base);
}

long long atoll(const char *s) { return strtoll(s, NULL, 10); }
long atol(const char *s) { return (long)strtoll(s, NULL, 10); }
int atoi(const char *s) { return (int)strtoll(s, NULL, 10); }

static uint64_t _wasi_f64_to_u64(double x) {
    union { double d; uint64_t u; } v;
    v.d = x;
    return v.u;
}

static double _wasi_u64_to_f64(uint64_t u) {
    union { double d; uint64_t u; } v;
    v.u = u;
    return v.d;
}

static int _wasi_bi_mul_u32(uint32_t *a, int n, int max_n, uint32_t mul) {
    uint64_t carry = 0ULL;
    int i;
    for (i = 0; i < n; i++) {
        uint64_t v = (uint64_t)a[i] * (uint64_t)mul + carry;
        a[i] = (uint32_t)(v & 0xFFFFFFFFULL);
        carry = v >> 32;
    }
    while (carry != 0ULL && n < max_n) {
        a[n++] = (uint32_t)(carry & 0xFFFFFFFFULL);
        carry >>= 32;
    }
    return n;
}

static int _wasi_bi_shl(uint32_t *a, int n, int max_n, int sh) {
    if (sh <= 0 || n <= 0) return n;
    int w = sh >> 5;
    int b = sh & 31;
    int i;
    if (w > 0) {
        int new_n = n + w;
        if (new_n > max_n) new_n = max_n;
        for (i = new_n - 1; i >= w; i--) a[i] = a[i - w];
        for (i = 0; i < w && i < max_n; i++) a[i] = 0U;
        n = new_n;
    }
    if (b > 0) {
        uint32_t carry = 0U;
        int rb = 32 - b;
        for (i = w; i < n; i++) {
            uint32_t cur = a[i];
            a[i] = (cur << b) | carry;
            carry = cur >> rb;
        }
        if (carry != 0U && n < max_n) {
            a[n++] = carry;
        }
    }
    return n;
}

static int _wasi_bi_bitlen(const uint32_t *a, int n) {
    while (n > 0 && a[n - 1] == 0U) n--;
    if (n <= 0) return 0;
    uint32_t top = a[n - 1];
    int bits = 0;
    while (top != 0U) { top >>= 1; bits++; }
    return (n - 1) * 32 + bits;
}

static int _wasi_bi_cmp(const uint32_t *a, int na, const uint32_t *b, int nb) {
    while (na > 0 && a[na - 1] == 0U) na--;
    while (nb > 0 && b[nb - 1] == 0U) nb--;
    if (na != nb) return (na > nb) ? 1 : -1;
    int i;
    for (i = na - 1; i >= 0; i--) {
        if (a[i] != b[i]) return (a[i] > b[i]) ? 1 : -1;
    }
    return 0;
}

static int _wasi_bi_sub(uint32_t *a, int na, const uint32_t *b, int nb) {
    uint64_t borrow = 0ULL;
    int i;
    for (i = 0; i < na; i++) {
        uint64_t av = (uint64_t)a[i];
        uint64_t bv = (i < nb) ? (uint64_t)b[i] : 0ULL;
        uint64_t sub = bv + borrow;
        if (av < sub) {
            a[i] = (uint32_t)((av + (1ULL << 32)) - sub);
            borrow = 1ULL;
        } else {
            a[i] = (uint32_t)(av - sub);
            borrow = 0ULL;
        }
    }
    while (na > 0 && a[na - 1] == 0U) na--;
    return na;
}

static int _wasi_bi_get_bit(const uint32_t *a, int n, int pos) {
    if (pos < 0) return 0;
    int w = pos >> 5;
    if (w >= n) return 0;
    return (int)((a[w] >> (pos & 31)) & 1U);
}

static int _wasi_bi_has_nonzero_below(const uint32_t *a, int n, int pos) {
    if (pos <= 0) return 0;
    int w = pos >> 5;
    int b = pos & 31;
    int i;
    int lim = (w < n) ? w : n;
    for (i = 0; i < lim; i++) {
        if (a[i] != 0U) return 1;
    }
    if (w < n && b > 0) {
        uint32_t mask = (1U << b) - 1U;
        if ((a[w] & mask) != 0U) return 1;
    }
    return 0;
}

static int _wasi_ci_match3(const char *p, const char *lit) {
    int i;
    for (i = 0; lit[i]; i++) {
        if ((char)tolower((unsigned char)p[i]) != lit[i]) return 0;
    }
    return 1;
}

double strtod(const char *nptr, char **endptr) {
    static const double p10[23] = {
        1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7,
        1e8, 1e9, 1e10, 1e11, 1e12, 1e13, 1e14, 1e15,
        1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22
    };
    static const uint32_t pow5_tab[14] = {
        1U, 5U, 25U, 125U, 625U, 3125U, 15625U, 78125U,
        390625U, 1953125U, 9765625U, 48828125U, 244140625U, 1220703125U
    };
    const char *p = nptr;
    while (isspace((unsigned char)*p)) p++;
    int neg = 0;
    if (*p == '-') { neg = 1; p++; }
    else if (*p == '+') { p++; }

    if (_wasi_ci_match3(p, "inf")) {
        p += 3;
        if (_wasi_ci_match3(p, "inity")) p += 5;
        if (endptr) *endptr = (char *)p;
        double infv = 1.0 / 0.0;
        return neg ? -infv : infv;
    }
    if (_wasi_ci_match3(p, "nan")) {
        p += 3;
        if (endptr) *endptr = (char *)p;
        return _wasi_u64_to_f64(0x7FF8000000000000ULL | (neg ? (1ULL << 63) : 0ULL));
    }

    uint64_t mant = 0ULL;
    int any = 0;
    int sig_digits = 0;
    int eval = 0;
    int extra_nz = 0;
    while (isdigit((unsigned char)*p)) {
        int d = *p - '0';
        if (mant != 0ULL || d != 0) {
            if (sig_digits < 19) {
                mant = mant * 10ULL + (uint64_t)d;
                sig_digits++;
            } else {
                if (d != 0) extra_nz = 1;
                if (eval < 10000) eval++;
            }
        }
        any = 1;
        p++;
    }
    if (*p == '.') {
        p++;
        while (isdigit((unsigned char)*p)) {
            int d = *p - '0';
            if (mant != 0ULL || d != 0) {
                if (sig_digits < 19) {
                    mant = mant * 10ULL + (uint64_t)d;
                    sig_digits++;
                    if (eval > -10000) eval--;
                } else {
                    if (d != 0) extra_nz = 1;
                }
            } else if (sig_digits == 0) {
                if (eval > -10000) eval--;
            }
            any = 1;
            p++;
        }
    }
    if (any && (*p == 'e' || *p == 'E')) {
        const char *ep = p + 1;
        int eneg = 0;
        if (*ep == '-') { eneg = 1; ep++; }
        else if (*ep == '+') { ep++; }
        if (isdigit((unsigned char)*ep)) {
            int ev = 0;
            while (isdigit((unsigned char)*ep)) {
                if (ev < 10000) ev = ev * 10 + (*ep - '0');
                ep++;
            }
            eval += eneg ? -ev : ev;
            p = ep;
        }
    }

    if (!any) {
        if (endptr) *endptr = (char *)nptr;
        return 0.0;
    }

    if (mant == 0ULL) {
        if (endptr) *endptr = (char *)p;
        return neg ? -0.0 : 0.0;
    }

    double val = 0.0;
    /* Clinger exact IEEE-754 fast path when mantissa and power of 10 are both exact */
    if (!extra_nz && mant < (1ULL << 53) && eval >= -22 && eval <= 22) {
        val = (double)mant;
        if (eval > 0) val *= p10[eval];
        else if (eval < 0) val /= p10[-eval];
    } else if (eval > 350) {
        val = 1.0 / 0.0;
    } else if (eval < -350) {
        val = 0.0;
    } else {
        uint32_t num[48];
        int nlen, i;
        for (i = 0; i < 48; i++) num[i] = 0U;
        num[0] = (uint32_t)(mant & 0xFFFFFFFFULL);
        num[1] = (uint32_t)((mant >> 32) & 0xFFFFFFFFULL);
        nlen = (num[1] != 0U) ? 2 : 1;

        if (eval >= 0) {
            int rem = eval;
            while (rem > 0) {
                int k = (rem > 13) ? 13 : rem;
                nlen = _wasi_bi_mul_u32(num, nlen, 48, pow5_tab[k]);
                rem -= k;
            }
            int bl = _wasi_bi_bitlen(num, nlen);
            int shift = bl - 53;
            int be = eval + shift;
            uint64_t q = 0ULL;
            if (shift > 0) {
                int bi;
                for (bi = 52; bi >= 0; bi--) {
                    q = (q << 1) | (uint64_t)_wasi_bi_get_bit(num, nlen, shift + bi);
                }
                int rbit = _wasi_bi_get_bit(num, nlen, shift - 1);
                int sticky = _wasi_bi_has_nonzero_below(num, nlen, shift - 1) || extra_nz;
                if (rbit && (sticky || (q & 1ULL))) {
                    q++;
                    if (q == (1ULL << 53)) { q >>= 1; be++; }
                }
            } else {
                q = ((uint64_t)num[0]) | ((nlen > 1) ? ((uint64_t)num[1] << 32) : 0ULL);
                if (shift < 0) q <<= (-shift);
            }
            int exp_bits = be + 1075;
            if (exp_bits >= 0x7FF) {
                val = 1.0 / 0.0;
            } else {
                uint64_t u = ((uint64_t)exp_bits << 52) | (q & 0xFFFFFFFFFFFFFULL);
                val = _wasi_u64_to_f64(u);
            }
        } else {
            uint32_t den[48];
            int dlen;
            for (i = 0; i < 48; i++) den[i] = 0U;
            den[0] = 1U;
            dlen = 1;
            int rem = -eval;
            while (rem > 0) {
                int k = (rem > 13) ? 13 : rem;
                dlen = _wasi_bi_mul_u32(den, dlen, 48, pow5_tab[k]);
                rem -= k;
            }
            int bl_m = _wasi_bi_bitlen(num, nlen);
            int bl_d = _wasi_bi_bitlen(den, dlen);
            int k = bl_d - bl_m;
            if (k > 0) nlen = _wasi_bi_shl(num, nlen, 48, k);
            else if (k < 0) dlen = _wasi_bi_shl(den, dlen, 48, -k);
            if (_wasi_bi_cmp(num, nlen, den, dlen) < 0) {
                nlen = _wasi_bi_shl(num, nlen, 48, 1);
                k++;
            }
            int e_real = eval - k;
            if (e_real > 1023) {
                val = 1.0 / 0.0;
            } else if (e_real < -1075) {
                val = 0.0;
            } else {
                int prec_bits, be;
                if (e_real >= -1022) { prec_bits = 53; be = e_real - 52; }
                else { prec_bits = e_real + 1075; be = -1074; }
                uint64_t q = 0ULL;
                for (i = 0; i < prec_bits; i++) {
                    q <<= 1;
                    if (_wasi_bi_cmp(num, nlen, den, dlen) >= 0) {
                        nlen = _wasi_bi_sub(num, nlen, den, dlen);
                        q |= 1ULL;
                    }
                    nlen = _wasi_bi_shl(num, nlen, 48, 1);
                }
                int cmp_r = _wasi_bi_cmp(num, nlen, den, dlen);
                if (cmp_r > 0 || (cmp_r == 0 && (extra_nz || (q & 1ULL)))) {
                    q++;
                    if (prec_bits == 53 && q == (1ULL << 53)) {
                        q >>= 1;
                        be++;
                    } else if (prec_bits < 53 && q == (1ULL << 52)) {
                        val = _wasi_u64_to_f64(1ULL << 52);
                        goto strtod_done;
                    }
                }
                if (prec_bits < 53) {
                    val = _wasi_u64_to_f64(q & 0xFFFFFFFFFFFFFULL);
                } else {
                    int exp_bits = be + 1075;
                    if (exp_bits >= 0x7FF) val = 1.0 / 0.0;
                    else val = _wasi_u64_to_f64(((uint64_t)exp_bits << 52) | (q & 0xFFFFFFFFFFFFFULL));
                }
            }
        }
    }
strtod_done:
    if (endptr) *endptr = (char *)p;
    return neg ? -val : val;
}

double atof(const char *s) { return strtod(s, NULL); }
float strtof(const char *nptr, char **endptr) { return (float)strtod(nptr, endptr); }

/* -------------------------------------------------------------------------
 * 5. Sorting & Searching
 * ------------------------------------------------------------------------- */
static void _wasi_rt_swap_bytes(uint8_t *a, uint8_t *b, size_t sz) {
    size_t i;
    for (i = 0; i < sz; i++) {
        uint8_t t = a[i];
        a[i] = b[i];
        b[i] = t;
    }
}

void qsort(void *base, size_t n, size_t sz, int (*cmp)(const void *a, const void *b)) {
    if (!base || n < 2 || sz == 0 || !cmp) return;
    uint8_t *arr = (uint8_t *)base;
    /* Gap (Shell) sort with Tokuda/Knuth gaps: O(n log^2 n) with zero recursion stack risk */
    size_t gap = 1;
    while (gap < n / 3) gap = gap * 3 + 1;
    while (gap >= 1) {
        size_t i;
        for (i = gap; i < n; i++) {
            size_t j = i;
            while (j >= gap && cmp(arr + (j - gap) * sz, arr + j * sz) > 0) {
                _wasi_rt_swap_bytes(arr + (j - gap) * sz, arr + j * sz, sz);
                j -= gap;
            }
        }
        gap /= 3;
    }
}

void *bsearch(const void *key, const void *base, size_t n, size_t sz, int (*cmp)(const void *a, const void *b)) {
    const uint8_t *arr = (const uint8_t *)base;
    size_t lo = 0, hi = n;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        int c = cmp(key, arr + mid * sz);
        if (c == 0) return (void *)(arr + mid * sz);
        if (c < 0) hi = mid;
        else lo = mid + 1;
    }
    return NULL;
}

int rand(void) {
    _wasi_rt_rand_state = _wasi_rt_rand_state * 1103515245U + 12345U;
    return (int)((_wasi_rt_rand_state >> 16) & 0x7FFFU);
}

void srand(unsigned int seed) {
    _wasi_rt_rand_state = seed ? seed : 1U;
}

/* -------------------------------------------------------------------------
 * 6. Buffered WASI Preview1 FILE* Stdio & Memstream
 * ------------------------------------------------------------------------- */
#define _WASI_RT_MAX_FILES 64
#define _WASI_RT_IO_BUF    4096

struct _IO_FILE {
    int in_use;
    int fd;
    int eof;
    int err;
    int has_unget;
    int unget_ch;
    int can_read;
    int can_write;
    int is_append;
    int is_mem;
    int is_memstream;
    int is_std;
    uint8_t *rbuf;
    size_t rpos;
    size_t rlen;
    uint8_t *wbuf;
    size_t wlen;
    uint8_t *membuf;
    size_t memsize;
    size_t memcap;
    size_t mempos;
    char **out_bufloc;
    size_t *out_sizeloc;
};

static struct _IO_FILE _wasi_rt_files[_WASI_RT_MAX_FILES];
static uint8_t _wasi_rt_wbufs[_WASI_RT_MAX_FILES][_WASI_RT_IO_BUF];
static uint8_t _wasi_rt_rbufs[_WASI_RT_MAX_FILES][_WASI_RT_IO_BUF];

FILE *stdin = NULL;
FILE *stdout = NULL;
FILE *stderr = NULL;

static int _wasi_rt_flush_wbuf(FILE *f) {
    if (!f || !f->in_use) return -1;
    if (f->is_memstream) {
        if (f->out_bufloc) {
            if (!f->membuf) {
                f->membuf = (uint8_t *)calloc(1, 64);
                f->memcap = 64;
            }
            f->membuf[f->memsize] = '\0';
            *f->out_bufloc = (char *)f->membuf;
        }
        if (f->out_sizeloc) *f->out_sizeloc = f->mempos;
        return 0;
    }
    if (f->is_mem || f->wlen == 0) return 0;
    size_t sent = 0;
    while (sent < f->wlen) {
        _wasi_rt_iovec_t iov;
        iov.buf = (uint32_t)(uintptr_t)(f->wbuf + sent);
        iov.len = (uint32_t)(f->wlen - sent);
        uint32_t nw = 0;
        int rc = __wasi_fd_write(f->fd, &iov, 1U, &nw);
        if (rc != 0 || nw == 0U) {
            f->err = 1;
            f->wlen = 0;
            return -1;
        }
        sent += (size_t)nw;
    }
    f->wlen = 0;
    return 0;
}

int fflush(FILE *stream) {
    if (!stream) {
        int i, rc = 0;
        for (i = 0; i < _WASI_RT_MAX_FILES; i++) {
            if (_wasi_rt_files[i].in_use && _wasi_rt_flush_wbuf(&_wasi_rt_files[i]) != 0) rc = -1;
        }
        return rc;
    }
    return _wasi_rt_flush_wbuf(stream);
}

static FILE *_wasi_rt_alloc_file(void) {
    int i;
    for (i = 3; i < _WASI_RT_MAX_FILES; i++) {
        if (!_wasi_rt_files[i].in_use) {
            memset(&_wasi_rt_files[i], 0, sizeof(struct _IO_FILE));
            _wasi_rt_files[i].in_use = 1;
            _wasi_rt_files[i].fd = -1;
            _wasi_rt_files[i].wbuf = _wasi_rt_wbufs[i];
            _wasi_rt_files[i].rbuf = _wasi_rt_rbufs[i];
            return &_wasi_rt_files[i];
        }
    }
    return NULL;
}

static int _wasi_rt_inited = 0;

void _wasi_rt_ensure_init(void) {
    if (_wasi_rt_inited) return;
    _wasi_rt_inited = 1;
    memset(_wasi_rt_files, 0, sizeof(_wasi_rt_files));
    stdin = &_wasi_rt_files[0];
    stdout = &_wasi_rt_files[1];
    stderr = &_wasi_rt_files[2];

    stdin->in_use = 1; stdin->fd = 0; stdin->can_read = 1; stdin->is_std = 1; stdin->rbuf = _wasi_rt_rbufs[0];
    stdout->in_use = 1; stdout->fd = 1; stdout->can_write = 1; stdout->is_std = 1; stdout->wbuf = _wasi_rt_wbufs[1];
    stderr->in_use = 1; stderr->fd = 2; stderr->can_write = 1; stderr->is_std = 1; stderr->wbuf = _wasi_rt_wbufs[2];

    _wasi_rt_discover_preopen();

    uint32_t raw_envc = 0, raw_env_sz = 0;
    if (__wasi_environ_sizes_get(&raw_envc, &raw_env_sz) == 0 && raw_envc > 0) {
        uint32_t *e32 = (uint32_t *)malloc((size_t)(raw_envc + 1U) * 4U);
        uint8_t *ebuf = (uint8_t *)malloc((size_t)(raw_env_sz + 1U));
        if (e32 && ebuf && __wasi_environ_get(e32, ebuf) == 0) {
            _wasi_rt_environ = (char **)malloc((size_t)(raw_envc + 1U) * sizeof(char *));
            if (_wasi_rt_environ) {
                uint32_t ei;
                for (ei = 0; ei < raw_envc; ei++) {
                    _wasi_rt_environ[ei] = (char *)(uintptr_t)e32[ei];
                }
                _wasi_rt_environ[raw_envc] = NULL;
                _wasi_rt_envc = (int)raw_envc;
            }
        }
        free(e32);
    }
}

FILE *fopen(const char *path, const char *mode) {
    _wasi_rt_ensure_init();
    if (!path || !mode) return NULL;
    int r = 0, w = 0, ap = 0, plus = 0;
    const char *m = mode;
    while (*m) {
        if (*m == 'r') r = 1;
        else if (*m == 'w') w = 1;
        else if (*m == 'a') ap = 1;
        else if (*m == '+') plus = 1;
        m++;
    }
    uint32_t oflags = 0;
    uint32_t fdflags = 0;
    if (w) oflags = _WASI_RT_O_CREAT | _WASI_RT_O_TRUNC;
    else if (ap) { oflags = _WASI_RT_O_CREAT; fdflags = _WASI_RT_FDFLAG_APP; }

    const char *rel = _wasi_rt_relpath(path);
    int new_fd = -1;
    uint64_t want_rights = (w || ap || plus) ? _WASI_RT_RIGHTS_WRITE : _WASI_RT_RIGHTS_READ;
    uint64_t base_rights = want_rights & _wasi_rt_preopen_rights_inh;
    if (base_rights == 0ULL) base_rights = want_rights;
    int rc = __wasi_path_open(_wasi_rt_preopen_fd, 0U, rel, (uint32_t)strlen(rel),
                              oflags, base_rights, base_rights, fdflags, &new_fd);
    if (rc != 0 || new_fd < 0) {
        _wasi_rt_errno_val = rc;
        return NULL;
    }
    FILE *f = _wasi_rt_alloc_file();
    if (!f) {
        __wasi_fd_close(new_fd);
        return NULL;
    }
    f->fd = new_fd;
    f->can_read = (r || plus) ? 1 : 0;
    f->can_write = (w || ap || plus) ? 1 : 0;
    f->is_append = ap ? 1 : 0;
    return f;
}

FILE *freopen(const char *path, const char *mode, FILE *stream) {
    if (!stream || !path) return NULL;
    fflush(stream);
    if (!stream->is_std && stream->fd >= 0) __wasi_fd_close(stream->fd);
    FILE *nf = fopen(path, mode);
    if (!nf) return NULL;
    int keep_std = stream->is_std;
    uint8_t *keep_rb = stream->rbuf;
    uint8_t *keep_wb = stream->wbuf;
    stream->fd = nf->fd;
    stream->eof = 0;
    stream->err = 0;
    stream->has_unget = 0;
    stream->can_read = nf->can_read;
    stream->can_write = nf->can_write;
    stream->is_append = nf->is_append;
    stream->rpos = 0; stream->rlen = 0; stream->wlen = 0;
    stream->is_std = keep_std;
    stream->rbuf = keep_rb;
    stream->wbuf = keep_wb;
    nf->in_use = 0;
    return stream;
}

FILE *fdopen(int fd, const char *mode) {
    (void)mode;
    if (fd < 0) return NULL;
    FILE *f = _wasi_rt_alloc_file();
    if (!f) return NULL;
    f->fd = fd;
    f->can_read = 1;
    f->can_write = 1;
    return f;
}

FILE *open_memstream(char **bufloc, size_t *sizeloc) {
    if (!bufloc || !sizeloc) return NULL;
    FILE *f = _wasi_rt_alloc_file();
    if (!f) return NULL;
    f->membuf = (uint8_t *)calloc(1, 256);
    if (!f->membuf) { f->in_use = 0; return NULL; }
    f->memcap = 256;
    f->memsize = 0;
    f->mempos = 0;
    f->can_write = 1;
    f->is_mem = 1;
    f->is_memstream = 1;
    f->out_bufloc = bufloc;
    f->out_sizeloc = sizeloc;
    *bufloc = (char *)f->membuf;
    *sizeloc = 0;
    return f;
}

FILE *fmemopen(void *buf, size_t size, const char *mode) {
    (void)mode;
    FILE *f = _wasi_rt_alloc_file();
    if (!f) return NULL;
    f->membuf = (uint8_t *)buf;
    f->memsize = size;
    f->memcap = size;
    f->mempos = 0;
    f->can_read = 1;
    f->can_write = 0;
    f->is_mem = 1;
    return f;
}

int fclose(FILE *stream) {
    if (!stream || !stream->in_use) return -1;
    int rc = _wasi_rt_flush_wbuf(stream);
    if (!stream->is_mem && !stream->is_std && stream->fd >= 0) {
        if (__wasi_fd_close(stream->fd) != 0) rc = -1;
    }
    if (!stream->is_std) stream->in_use = 0;
    return rc;
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream) {
    if (!stream || !ptr || size == 0 || nmemb == 0) return 0;
    size_t total = size * nmemb;
    const uint8_t *src = (const uint8_t *)ptr;

    if (stream->is_memstream) {
        size_t need = stream->mempos + total + 1;
        if (need > stream->memcap) {
            size_t nc = stream->memcap ? stream->memcap * 2 : 256;
            while (nc < need) nc *= 2;
            uint8_t *nb = (uint8_t *)realloc(stream->membuf, nc);
            if (!nb) { stream->err = 1; return 0; }
            stream->membuf = nb;
            stream->memcap = nc;
        }
        memcpy(stream->membuf + stream->mempos, src, total);
        stream->mempos += total;
        if (stream->mempos > stream->memsize) stream->memsize = stream->mempos;
        stream->membuf[stream->mempos] = '\0';
        return nmemb;
    }

    size_t left = total;
    size_t off = 0;
    while (left > 0) {
        if (stream->wlen == _WASI_RT_IO_BUF) {
            if (_wasi_rt_flush_wbuf(stream) != 0) return off / size;
        }
        size_t space = _WASI_RT_IO_BUF - stream->wlen;
        size_t take = (left < space) ? left : space;
        memcpy(stream->wbuf + stream->wlen, src + off, take);
        stream->wlen += take;
        off += take;
        left -= take;
    }
    if (stream == stderr) _wasi_rt_flush_wbuf(stream);
    return nmemb;
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    if (!stream || !ptr || size == 0 || nmemb == 0) return 0;
    size_t total = size * nmemb;
    uint8_t *dst = (uint8_t *)ptr;
    size_t got = 0;

    if (stream->has_unget && got < total) {
        dst[got++] = (uint8_t)stream->unget_ch;
        stream->has_unget = 0;
    }
    if (stream->is_mem) {
        size_t avail = (stream->mempos < stream->memsize) ? (stream->memsize - stream->mempos) : 0;
        size_t take = ((total - got) < avail) ? (total - got) : avail;
        if (take > 0) {
            memcpy(dst + got, stream->membuf + stream->mempos, take);
            stream->mempos += take;
            got += take;
        }
        if (got < total) stream->eof = 1;
        return got / size;
    }
    while (got < total) {
        if (stream->rpos < stream->rlen) {
            size_t avail = stream->rlen - stream->rpos;
            size_t take = ((total - got) < avail) ? (total - got) : avail;
            memcpy(dst + got, stream->rbuf + stream->rpos, take);
            stream->rpos += take;
            got += take;
            continue;
        }
        if ((total - got) >= _WASI_RT_IO_BUF) {
            _wasi_rt_iovec_t iov;
            iov.buf = (uint32_t)(uintptr_t)(dst + got);
            iov.len = (uint32_t)(total - got);
            uint32_t nr = 0;
            int rc = __wasi_fd_read(stream->fd, &iov, 1U, &nr);
            if (rc != 0) { stream->err = 1; break; }
            if (nr == 0U) { stream->eof = 1; break; }
            got += (size_t)nr;
        } else {
            _wasi_rt_iovec_t iov;
            iov.buf = (uint32_t)(uintptr_t)stream->rbuf;
            iov.len = (uint32_t)_WASI_RT_IO_BUF;
            uint32_t nr = 0;
            int rc = __wasi_fd_read(stream->fd, &iov, 1U, &nr);
            if (rc != 0) { stream->err = 1; break; }
            if (nr == 0U) { stream->eof = 1; break; }
            stream->rpos = 0;
            stream->rlen = (size_t)nr;
        }
    }
    return got / size;
}

int fgetc(FILE *stream) {
    if (!stream) return -1;
    if (stream->has_unget) {
        int ch = stream->unget_ch;
        stream->has_unget = 0;
        return ch;
    }
    if (!stream->is_mem && stream->rpos < stream->rlen) {
        return (int)stream->rbuf[stream->rpos++];
    }
    uint8_t b = 0;
    if (fread(&b, 1, 1, stream) != 1) return -1;
    return (int)b;
}

int getc(FILE *stream) { return fgetc(stream); }
int getchar(void) { return fgetc(stdin); }

int ungetc(int c, FILE *stream) {
    if (!stream || c < 0) return -1;
    stream->has_unget = 1;
    stream->unget_ch = c & 0xFF;
    stream->eof = 0;
    return stream->unget_ch;
}

char *fgets(char *s, int size, FILE *stream) {
    if (!s || size <= 0 || !stream) return NULL;
    int i = 0;
    while (i + 1 < size) {
        int c = fgetc(stream);
        if (c < 0) {
            if (i == 0) return NULL;
            break;
        }
        s[i++] = (char)c;
        if (c == '\n') break;
    }
    s[i] = '\0';
    return s;
}

int fputc(int c, FILE *stream) {
    uint8_t b = (uint8_t)c;
    if (!stream) return -1;
    if (fwrite(&b, 1, 1, stream) != 1) return -1;
    if (stream == stdout && b == '\n') _wasi_rt_flush_wbuf(stream);
    return (int)b;
}

int putc(int c, FILE *stream) { return fputc(c, stream); }
int putchar(int c) { _wasi_rt_ensure_init(); return fputc(c, stdout); }

int fputs(const char *s, FILE *stream) {
    if (!s || !stream) return -1;
    size_t len = strlen(s);
    if (len == 0) return 0;
    return (fwrite(s, 1, len, stream) == len) ? 0 : -1;
}

int puts(const char *s) {
    _wasi_rt_ensure_init();
    if (fputs(s ? s : "(null)", stdout) < 0) return -1;
    if (fputc('\n', stdout) < 0) return -1;
    return 0;
}

int fseek(FILE *stream, long offset, int whence) {
    if (!stream) return -1;
    _wasi_rt_flush_wbuf(stream);
    if (stream->rpos < stream->rlen && whence == 1) {
        offset -= (long)(stream->rlen - stream->rpos);
    }
    stream->rpos = 0;
    stream->rlen = 0;
    stream->has_unget = 0;
    stream->eof = 0;

    if (stream->is_mem) {
        long npos = 0;
        if (whence == 0) npos = offset;
        else if (whence == 1) npos = (long)stream->mempos + offset;
        else if (whence == 2) npos = (long)stream->memsize + offset;
        if (npos < 0) return -1;
        stream->mempos = (size_t)npos;
        return 0;
    }
    uint64_t newoff = 0;
    int rc = __wasi_fd_seek(stream->fd, (int64_t)offset, (uint32_t)whence, &newoff);
    if (rc != 0) return -1;
    return 0;
}

long ftell(FILE *stream) {
    if (!stream) return -1L;
    _wasi_rt_flush_wbuf(stream);
    if (stream->is_mem) return (long)stream->mempos;
    uint64_t cur = 0;
    if (__wasi_fd_seek(stream->fd, 0LL, 1U, &cur) != 0) return -1L;
    long buffered_unread = (long)(stream->rlen - stream->rpos) + (stream->has_unget ? 1L : 0L);
    return (long)cur - buffered_unread;
}

void rewind(FILE *stream) {
    if (stream) {
        fseek(stream, 0L, 0);
        stream->err = 0;
    }
}

int feof(FILE *stream) { return stream ? stream->eof : 1; }
int ferror(FILE *stream) { return stream ? stream->err : 1; }
void clearerr(FILE *stream) { if (stream) { stream->eof = 0; stream->err = 0; } }
int setvbuf(FILE *stream, char *buf, int mode, size_t size) { (void)stream; (void)buf; (void)mode; (void)size; return 0; }
void setbuf(FILE *stream, char *buf) { (void)stream; (void)buf; }
FILE *tmpfile(void) { return NULL; }

int remove(const char *path) {
    if (!path) return -1;
    const char *rel = _wasi_rt_relpath(path);
    return (__wasi_path_unlink_file(_wasi_rt_preopen_fd, rel, (uint32_t)strlen(rel)) == 0) ? 0 : -1;
}

int write(int fd, const void *buf, size_t count) {
    if (fd < 0 || (!buf && count > 0)) return -1;
    _wasi_rt_iovec_t iov;
    iov.buf = (uint32_t)(uintptr_t)buf;
    iov.len = (uint32_t)count;
    uint32_t nw = 0;
    int rc = __wasi_fd_write(fd, &iov, 1U, &nw);
    return (rc == 0) ? (int)nw : -1;
}

int read(int fd, void *buf, size_t count) {
    if (fd < 0 || (!buf && count > 0)) return -1;
    _wasi_rt_iovec_t iov;
    iov.buf = (uint32_t)(uintptr_t)buf;
    iov.len = (uint32_t)count;
    uint32_t nr = 0;
    int rc = __wasi_fd_read(fd, &iov, 1U, &nr);
    return (rc == 0) ? (int)nr : -1;
}

int close(int fd) {
    if (fd < 0) return -1;
    return (__wasi_fd_close(fd) == 0) ? 0 : -1;
}

/* -------------------------------------------------------------------------
 * 7. Formatting (vsnprintf / snprintf / sprintf / vfprintf / fprintf / printf)
 * ------------------------------------------------------------------------- */
typedef struct {
    char *buf;
    size_t cap;
    size_t count;
    FILE *fp;
} _wasi_rt_fmt_sink;

static void _wasi_rt_sink_putc(_wasi_rt_fmt_sink *sk, char c) {
    if (sk->fp) {
        fputc((unsigned char)c, sk->fp);
    } else if (sk->buf && sk->count + 1 < sk->cap) {
        sk->buf[sk->count] = c;
    }
    sk->count++;
}

static void _wasi_rt_sink_puts(_wasi_rt_fmt_sink *sk, const char *s, size_t n) {
    size_t i;
    for (i = 0; i < n; i++) _wasi_rt_sink_putc(sk, s[i]);
}

/* Extract exact top `nd` decimal digits of finite positive double `dv` with IEEE RNE */
static int _wasi_rt_f64_exact_dec(double dv, int nd, char *out_digits, int *out_exp10) {
    static const uint32_t pow5_dec[14] = {
        1U, 5U, 25U, 125U, 625U, 3125U, 15625U, 78125U,
        390625U, 1953125U, 9765625U, 48828125U, 244140625U, 1220703125U
    };
    if (nd < 1) nd = 1;
    if (nd > 26) nd = 26;
    uint64_t u = _wasi_f64_to_u64(dv);
    int eb = (int)((u >> 52) & 0x7FFULL);
    uint64_t mant = u & 0xFFFFFFFFFFFFFULL;
    int be;
    if (eb == 0) {
        be = -1074;
    } else {
        mant |= (1ULL << 52);
        be = eb - 1075;
    }
    uint32_t d[128];
    int dlen = 0, i;
    while (mant != 0ULL && dlen < 128) {
        d[dlen++] = (uint32_t)(mant % 1000000000ULL);
        mant /= 1000000000ULL;
    }
    if (dlen == 0) d[dlen++] = 0U;

    int exp_shift = 0;
    if (be > 0) {
        int rem = be;
        while (rem > 0) {
            int sh = (rem > 29) ? 29 : rem;
            uint64_t carry = 0ULL;
            for (i = 0; i < dlen; i++) {
                uint64_t v = ((uint64_t)d[i] << sh) + carry;
                d[i] = (uint32_t)(v % 1000000000ULL);
                carry = v / 1000000000ULL;
            }
            while (carry != 0ULL && dlen < 128) {
                d[dlen++] = (uint32_t)(carry % 1000000000ULL);
                carry /= 1000000000ULL;
            }
            rem -= sh;
        }
    } else if (be < 0) {
        int rem = -be;
        while (rem > 0) {
            int k = (rem > 13) ? 13 : rem;
            uint64_t mul = (uint64_t)pow5_dec[k];
            uint64_t carry = 0ULL;
            for (i = 0; i < dlen; i++) {
                uint64_t v = (uint64_t)d[i] * mul + carry;
                d[i] = (uint32_t)(v % 1000000000ULL);
                carry = v / 1000000000ULL;
            }
            while (carry != 0ULL && dlen < 128) {
                d[dlen++] = (uint32_t)(carry % 1000000000ULL);
                carry /= 1000000000ULL;
            }
            rem -= k;
        }
        exp_shift = be;
    }

    char top_buf[16];
    uint32_t tv = d[dlen - 1];
    int top_len = 0;
    do {
        top_buf[top_len++] = (char)('0' + (int)(tv % 10U));
        tv /= 10U;
    } while (tv > 0U && top_len < 16);

    int exp10 = top_len + (dlen - 1) * 9 - 1 + exp_shift;

    char stream[64];
    int slen = 0;
    for (i = top_len - 1; i >= 0 && slen < 63; i--) {
        stream[slen++] = top_buf[i] - '0';
    }
    int idx = dlen - 2;
    while (slen <= nd && idx >= 0 && slen + 9 <= 64) {
        uint32_t lv = d[idx--];
        int k;
        for (k = 8; k >= 0; k--) {
            stream[slen + k] = (char)(lv % 10U);
            lv /= 10U;
        }
        slen += 9;
    }
    int sticky = 0;
    for (i = idx; i >= 0; i--) {
        if (d[i] != 0U) { sticky = 1; break; }
    }
    if (!sticky && slen > nd + 1) {
        for (i = nd + 1; i < slen; i++) {
            if (stream[i] != 0) { sticky = 1; break; }
        }
    }

    for (i = 0; i < nd; i++) {
        out_digits[i] = (i < slen) ? stream[i] : 0;
    }
    int guard = (nd < slen) ? (int)stream[nd] : 0;
    if (guard > 5 || (guard == 5 && (sticky || (out_digits[nd - 1] & 1)))) {
        int ci = nd - 1;
        out_digits[ci]++;
        while (ci > 0 && out_digits[ci] >= 10) {
            out_digits[ci] = 0;
            ci--;
            out_digits[ci]++;
        }
        if (out_digits[0] >= 10) {
            for (ci = 1; ci < nd; ci++) out_digits[ci] = 0;
            out_digits[0] = 1;
            exp10++;
        }
    }
    *out_exp10 = exp10;
    return 0;
}

static int _wasi_rt_vformat(_wasi_rt_fmt_sink *sk, const char *fmt, va_list ap) {
    if (!fmt) return 0;
    va_list aq;
    va_copy(aq, ap);
    while (*fmt) {
        if (*fmt != '%') {
            _wasi_rt_sink_putc(sk, *fmt++);
            continue;
        }
        fmt++;
        if (*fmt == '%') { _wasi_rt_sink_putc(sk, *fmt++); continue; }

        int left = 0, plus = 0, space = 0, alt = 0, zpad = 0;
        for (;;) {
            if (*fmt == '-') left = 1;
            else if (*fmt == '+') plus = 1;
            else if (*fmt == ' ') space = 1;
            else if (*fmt == '#') alt = 1;
            else if (*fmt == '0') zpad = 1;
            else break;
            fmt++;
        }
        if (left) zpad = 0;

        int width = 0;
        if (*fmt == '*') {
            width = va_arg(aq, int);
            if (width < 0) { left = 1; zpad = 0; width = -width; }
            fmt++;
        } else {
            while (*fmt >= '0' && *fmt <= '9') {
                width = width * 10 + (*fmt - '0');
                fmt++;
            }
        }

        int prec = -1;
        if (*fmt == '.') {
            fmt++;
            prec = 0;
            if (*fmt == '*') {
                prec = va_arg(aq, int);
                if (prec < 0) prec = -1;
                fmt++;
            } else {
                while (*fmt >= '0' && *fmt <= '9') {
                    prec = prec * 10 + (*fmt - '0');
                    fmt++;
                }
            }
        }

        int lmod = 0; /* 0=int, 1=long, 2=long long */
        if (*fmt == 'l') {
            lmod = 1; fmt++;
            if (*fmt == 'l') { lmod = 2; fmt++; }
        } else if (*fmt == 'z' || *fmt == 't' || *fmt == 'j') {
            lmod = 1; fmt++;
        } else if (*fmt == 'h') {
            fmt++;
            if (*fmt == 'h') fmt++;
        }

        char spec = *fmt ? *fmt++ : '\0';
        char tmp[128];
        if (spec == 'c') {
            char ch = (char)va_arg(aq, int);
            int pad = (width > 1) ? (width - 1) : 0;
            if (!left) while (pad-- > 0) _wasi_rt_sink_putc(sk, ' ');
            _wasi_rt_sink_putc(sk, ch);
            if (left) while (pad-- > 0) _wasi_rt_sink_putc(sk, ' ');
        } else if (spec == 's') {
            const char *s = va_arg(aq, const char *);
            if (!s) s = "(null)";
            size_t len = strlen(s);
            if (prec >= 0 && (size_t)prec < len) len = (size_t)prec;
            int pad = (width > (int)len) ? (width - (int)len) : 0;
            if (!left) while (pad-- > 0) _wasi_rt_sink_putc(sk, ' ');
            _wasi_rt_sink_puts(sk, s, len);
            if (left) while (pad-- > 0) _wasi_rt_sink_putc(sk, ' ');
        } else if (spec == 'd' || spec == 'i' || spec == 'u' || spec == 'x' || spec == 'X' || spec == 'o' || spec == 'p') {
            unsigned long long uv = 0;
            int neg = 0;
            unsigned base = 10U;
            const char *digits = "0123456789abcdef";
            if (spec == 'X') digits = "0123456789ABCDEF";
            if (spec == 'x' || spec == 'X' || spec == 'p') base = 16U;
            else if (spec == 'o') base = 8U;

            if (spec == 'p') {
                uintptr_t pv = (uintptr_t)va_arg(aq, void *);
                uv = (unsigned long long)pv;
                alt = 1;
                if (width == 0 && sizeof(void *) == 8) zpad = 0;
            } else if (spec == 'd' || spec == 'i') {
                long long sv = (lmod >= 1) ? va_arg(aq, long long) : (long long)va_arg(aq, int);
                if (sv < 0) { neg = 1; uv = (unsigned long long)(-sv); }
                else uv = (unsigned long long)sv;
            } else {
                uv = (lmod >= 1) ? va_arg(aq, unsigned long long) : (unsigned long long)va_arg(aq, unsigned int);
            }

            int ti = 0;
            if (uv == 0 && prec == 0) {
                ti = 0;
            } else {
                do {
                    tmp[ti++] = digits[uv % base];
                    uv /= base;
                } while (uv > 0 && ti < (int)sizeof(tmp));
            }
            while (prec > ti && ti < (int)sizeof(tmp)) tmp[ti++] = '0';

            char pfx[4];
            int plen = 0;
            if (neg) pfx[plen++] = '-';
            else if ((spec == 'd' || spec == 'i') && plus) pfx[plen++] = '+';
            else if ((spec == 'd' || spec == 'i') && space) pfx[plen++] = ' ';
            if (alt && (spec == 'p' || ((spec == 'x' || spec == 'X') && ti > 0 && tmp[ti - 1] != '0'))) {
                pfx[plen++] = '0';
                pfx[plen++] = (spec == 'X') ? 'X' : 'x';
            }

            int total_len = plen + ti;
            int pad = (width > total_len) ? (width - total_len) : 0;
            if (!left && !zpad) while (pad-- > 0) _wasi_rt_sink_putc(sk, ' ');
            _wasi_rt_sink_puts(sk, pfx, (size_t)plen);
            if (!left && zpad) while (pad-- > 0) _wasi_rt_sink_putc(sk, '0');
            while (ti > 0) _wasi_rt_sink_putc(sk, tmp[--ti]);
            if (left) while (pad-- > 0) _wasi_rt_sink_putc(sk, ' ');
        } else if (spec == 'f' || spec == 'F' || spec == 'e' || spec == 'E' || spec == 'g' || spec == 'G') {
            double dv = va_arg(aq, double);
            union { double d; uint64_t u; } du;
            du.d = dv;
            int neg = ((du.u >> 63) != 0);
            if (neg) dv = -dv;
            int upper_fp = (spec == 'F' || spec == 'E' || spec == 'G');
            if (dv != dv) {
                if (neg) _wasi_rt_sink_putc(sk, '-');
                _wasi_rt_sink_puts(sk, upper_fp ? "NAN" : "nan", 3);
                continue;
            }
            if (dv > 1.7976931348623157e+308) {
                if (neg) _wasi_rt_sink_putc(sk, '-');
                else if (plus) _wasi_rt_sink_putc(sk, '+');
                else if (space) _wasi_rt_sink_putc(sk, ' ');
                _wasi_rt_sink_puts(sk, upper_fp ? "INF" : "inf", 3);
                continue;
            }
            int is_e = (spec == 'e' || spec == 'E');
            int is_g = (spec == 'g' || spec == 'G');
            int p = (prec >= 0) ? prec : 6;
            if (is_g && p == 0) p = 1;
            if (p > 24) p = 24;

            if (is_e || is_g) {
                int nd = is_e ? (p + 1) : p;
                char digits[32];
                int exp10 = 0;
                int di;
                for (di = 0; di < 32; di++) digits[di] = 0;
                if (dv > 0.0) {
                    _wasi_rt_f64_exact_dec(dv, nd, digits, &exp10);
                }

                int use_sci = is_e || (dv > 0.0 && (exp10 < -4 || exp10 >= p));
                if (use_sci) {
                    int frac_len = nd - 1;
                    if (is_g && !alt) {
                        while (frac_len > 0 && digits[frac_len] == 0) frac_len--;
                    }
                    char ebuf[16];
                    int ei = 0;
                    ebuf[ei++] = upper_fp ? 'E' : 'e';
                    int eabs = (exp10 < 0) ? -exp10 : exp10;
                    ebuf[ei++] = (exp10 < 0) ? '-' : '+';
                    if (eabs >= 100) {
                        ebuf[ei++] = (char)('0' + (eabs / 100) % 10);
                        ebuf[ei++] = (char)('0' + (eabs / 10) % 10);
                        ebuf[ei++] = (char)('0' + (eabs % 10));
                    } else {
                        ebuf[ei++] = (char)('0' + (eabs / 10));
                        ebuf[ei++] = (char)('0' + (eabs % 10));
                    }
                    int plen = (neg || plus || space) ? 1 : 0;
                    int dotlen = (frac_len > 0 || alt) ? (1 + frac_len) : 0;
                    int total_len = plen + 1 + dotlen + ei;
                    int pad = (width > total_len) ? (width - total_len) : 0;
                    if (!left && !zpad) while (pad-- > 0) _wasi_rt_sink_putc(sk, ' ');
                    if (neg) _wasi_rt_sink_putc(sk, '-');
                    else if (plus) _wasi_rt_sink_putc(sk, '+');
                    else if (space) _wasi_rt_sink_putc(sk, ' ');
                    if (!left && zpad) while (pad-- > 0) _wasi_rt_sink_putc(sk, '0');
                    _wasi_rt_sink_putc(sk, (char)('0' + digits[0]));
                    if (frac_len > 0 || alt) {
                        _wasi_rt_sink_putc(sk, '.');
                        for (di = 1; di <= frac_len; di++) _wasi_rt_sink_putc(sk, (char)('0' + digits[di]));
                    }
                    _wasi_rt_sink_puts(sk, ebuf, (size_t)ei);
                    if (left) while (pad-- > 0) _wasi_rt_sink_putc(sk, ' ');
                    continue;
                }

                /* Fixed %g / %G directly from exact decimal digits & exp10 (-4 <= exp10 < p) */
                int plen = (neg || plus || space) ? 1 : 0;
                if (dv == 0.0) {
                    int fzeros = alt ? (p - 1) : 0;
                    int total_len = plen + 1 + ((fzeros > 0 || alt) ? (1 + fzeros) : 0);
                    int pad = (width > total_len) ? (width - total_len) : 0;
                    if (!left && !zpad) while (pad-- > 0) _wasi_rt_sink_putc(sk, ' ');
                    if (neg) _wasi_rt_sink_putc(sk, '-');
                    else if (plus) _wasi_rt_sink_putc(sk, '+');
                    else if (space) _wasi_rt_sink_putc(sk, ' ');
                    if (!left && zpad) while (pad-- > 0) _wasi_rt_sink_putc(sk, '0');
                    _wasi_rt_sink_putc(sk, '0');
                    if (fzeros > 0 || alt) {
                        _wasi_rt_sink_putc(sk, '.');
                        for (di = 0; di < fzeros; di++) _wasi_rt_sink_putc(sk, '0');
                    }
                    if (left) while (pad-- > 0) _wasi_rt_sink_putc(sk, ' ');
                    continue;
                }
                int sig_len = nd;
                int min_sig = (exp10 >= 0) ? (exp10 + 1) : 1;
                if (!alt) {
                    while (sig_len > min_sig && digits[sig_len - 1] == 0) sig_len--;
                }
                int int_len = (exp10 >= 0) ? (exp10 + 1) : 1;
                int lead_zeros = (exp10 < 0) ? (-exp10 - 1) : 0;
                int frac_len = (exp10 >= 0) ? (sig_len - (exp10 + 1)) : (lead_zeros + sig_len);
                int dotlen = (frac_len > 0 || alt) ? (1 + frac_len) : 0;
                int total_len = plen + int_len + dotlen;
                int pad = (width > total_len) ? (width - total_len) : 0;
                if (!left && !zpad) while (pad-- > 0) _wasi_rt_sink_putc(sk, ' ');
                if (neg) _wasi_rt_sink_putc(sk, '-');
                else if (plus) _wasi_rt_sink_putc(sk, '+');
                else if (space) _wasi_rt_sink_putc(sk, ' ');
                if (!left && zpad) while (pad-- > 0) _wasi_rt_sink_putc(sk, '0');
                if (exp10 >= 0) {
                    for (di = 0; di <= exp10; di++) _wasi_rt_sink_putc(sk, (char)('0' + digits[di]));
                    if (frac_len > 0 || alt) {
                        _wasi_rt_sink_putc(sk, '.');
                        for (di = exp10 + 1; di < sig_len; di++) _wasi_rt_sink_putc(sk, (char)('0' + digits[di]));
                    }
                } else {
                    _wasi_rt_sink_putc(sk, '0');
                    _wasi_rt_sink_putc(sk, '.');
                    for (di = 0; di < lead_zeros; di++) _wasi_rt_sink_putc(sk, '0');
                    for (di = 0; di < sig_len; di++) _wasi_rt_sink_putc(sk, (char)('0' + digits[di]));
                }
                if (left) while (pad-- > 0) _wasi_rt_sink_putc(sk, ' ');
                continue;
            }

            int eff_p = p;
            double round_add = 0.5;
            int rp;
            for (rp = 0; rp < eff_p; rp++) round_add *= 0.1;
            dv += round_add;
            unsigned long long ipart = (unsigned long long)dv;
            double fpart = dv - (double)ipart;
            int ti = 0;
            do {
                tmp[ti++] = (char)('0' + (int)(ipart % 10ULL));
                ipart /= 10ULL;
            } while (ipart > 0 && ti < 64);

            char fbuf[32];
            int fi = 0;
            for (fi = 0; fi < eff_p; fi++) {
                fpart *= 10.0;
                int d = (int)fpart;
                if (d < 0) d = 0;
                if (d > 9) d = 9;
                fbuf[fi] = (char)('0' + d);
                fpart -= (double)d;
            }
            int out_p = eff_p;
            if (is_g && !alt) {
                while (out_p > 0 && fbuf[out_p - 1] == '0') out_p--;
            }
            int plen = (neg || plus || space) ? 1 : 0;
            int dotlen = (out_p > 0 || alt) ? (1 + out_p) : 0;
            int pad = (width > (plen + ti + dotlen)) ? (width - (plen + ti + dotlen)) : 0;
            if (!left && !zpad) while (pad-- > 0) _wasi_rt_sink_putc(sk, ' ');
            if (neg) _wasi_rt_sink_putc(sk, '-');
            else if (plus) _wasi_rt_sink_putc(sk, '+');
            else if (space) _wasi_rt_sink_putc(sk, ' ');
            if (!left && zpad) while (pad-- > 0) _wasi_rt_sink_putc(sk, '0');
            while (ti > 0) _wasi_rt_sink_putc(sk, tmp[--ti]);
            if (out_p > 0 || alt) {
                _wasi_rt_sink_putc(sk, '.');
                if (out_p > 0) _wasi_rt_sink_puts(sk, fbuf, (size_t)out_p);
            }
            if (left) while (pad-- > 0) _wasi_rt_sink_putc(sk, ' ');
        } else if (spec == 'n') {
            int *np = va_arg(aq, int *);
            if (np) *np = (int)sk->count;
        } else {
            _wasi_rt_sink_putc(sk, '%');
            if (spec) _wasi_rt_sink_putc(sk, spec);
        }
    }
    va_end(aq);
    if (!sk->fp && sk->buf && sk->cap > 0) {
        size_t z = (sk->count < sk->cap) ? sk->count : (sk->cap - 1);
        sk->buf[z] = '\0';
    }
    return (int)sk->count;
}

int vsnprintf(char *str, size_t size, const char *format, va_list ap) {
    _wasi_rt_fmt_sink sk;
    sk.buf = str;
    sk.cap = size;
    sk.count = 0;
    sk.fp = NULL;
    return _wasi_rt_vformat(&sk, format, ap);
}

int snprintf(char *str, size_t size, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int r = vsnprintf(str, size, format, ap);
    va_end(ap);
    return r;
}

int vsprintf(char *str, const char *format, va_list ap) {
    return vsnprintf(str, 0x7FFFFFFFU, format, ap);
}

int sprintf(char *str, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int r = vsnprintf(str, 0x7FFFFFFFU, format, ap);
    va_end(ap);
    return r;
}

int vfprintf(FILE *stream, const char *format, va_list ap) {
    _wasi_rt_ensure_init();
    _wasi_rt_fmt_sink sk;
    sk.buf = NULL;
    sk.cap = 0;
    sk.count = 0;
    sk.fp = stream ? stream : stdout;
    return _wasi_rt_vformat(&sk, format, ap);
}

int fprintf(FILE *stream, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int r = vfprintf(stream, format, ap);
    va_end(ap);
    return r;
}

int vprintf(const char *format, va_list ap) {
    _wasi_rt_ensure_init();
    return vfprintf(stdout, format, ap);
}

int printf(const char *format, ...) {
    _wasi_rt_ensure_init();
    va_list ap;
    va_start(ap, format);
    int r = vfprintf(stdout, format, ap);
    va_end(ap);
    return r;
}

static int _wasi_rt_vsscanf(const char *str, const char *format, va_list ap_in) {
    if (!str || !format) return -1;
    const char *s = str;
    const char *f = format;
    int assigned = 0;
    va_list ap;
    va_copy(ap, ap_in);

    while (*f) {
        if (isspace((unsigned char)*f)) {
            while (isspace((unsigned char)*f)) f++;
            while (isspace((unsigned char)*s)) s++;
            continue;
        }
        if (*f != '%') {
            if (*s != *f) break;
            s++; f++;
            continue;
        }
        f++;
        if (*f == '%') {
            if (*s != '%') break;
            s++; f++;
            continue;
        }
        int suppress = 0;
        if (*f == '*') { suppress = 1; f++; }
        int width = 0;
        while (isdigit((unsigned char)*f)) {
            width = width * 10 + (*f - '0');
            f++;
        }
        int lmod = 0;
        if (*f == 'l') {
            lmod = 1; f++;
            if (*f == 'l') { lmod = 2; f++; }
        } else if (*f == 'h') {
            f++; if (*f == 'h') f++;
        }
        char spec = *f ? *f++ : '\0';
        if (spec == 'n') {
            if (!suppress) {
                int *np = va_arg(ap, int *);
                if (np) *np = (int)(s - str);
            }
            continue;
        }
        if (spec == 'c') {
            int take = (width > 0) ? width : 1;
            if (!*s) break;
            if (!suppress) {
                char *cp = va_arg(ap, char *);
                int i;
                for (i = 0; i < take && *s; i++) cp[i] = *s++;
            } else {
                while (take-- > 0 && *s) s++;
            }
            if (!suppress) assigned++;
            continue;
        }
        if (spec == '[') {
            uint8_t set[256];
            memset(set, 0, sizeof(set));
            int invert = 0;
            if (*f == '^') { invert = 1; f++; }
            if (*f == ']') { set[(unsigned char)']'] = 1; f++; }
            while (*f && *f != ']') {
                unsigned char c1 = (unsigned char)*f++;
                if (*f == '-' && f[1] && f[1] != ']') {
                    f++;
                    unsigned char c2 = (unsigned char)*f++;
                    unsigned int k;
                    if (c1 <= c2) {
                        for (k = c1; k <= c2; k++) set[k] = 1;
                    } else {
                        set[c1] = 1;
                        set[(unsigned char)'-'] = 1;
                        set[c2] = 1;
                    }
                } else {
                    set[c1] = 1;
                }
            }
            if (*f == ']') f++;
            char *dp = suppress ? NULL : va_arg(ap, char *);
            int cnt = 0;
            while (*s && (width == 0 || cnt < width)) {
                int in_set = set[(unsigned char)*s] ? 1 : 0;
                if (invert ? in_set : !in_set) break;
                if (dp) dp[cnt] = *s;
                cnt++;
                s++;
            }
            if (cnt == 0) break;
            if (dp) { dp[cnt] = '\0'; assigned++; }
            continue;
        }
        while (isspace((unsigned char)*s)) s++;
        if (spec == 's') {
            if (!*s) break;
            char *dp = suppress ? NULL : va_arg(ap, char *);
            int cnt = 0;
            while (*s && !isspace((unsigned char)*s) && (width == 0 || cnt < width)) {
                if (dp) dp[cnt] = *s;
                cnt++; s++;
            }
            if (dp) { dp[cnt] = '\0'; assigned++; }
        } else if (spec == 'd' || spec == 'i' || spec == 'u' || spec == 'x' || spec == 'X' || spec == 'o') {
            int base = (spec == 'x' || spec == 'X') ? 16 : ((spec == 'o') ? 8 : ((spec == 'i') ? 0 : 10));
            const char *start = s;
            char numbuf[128];
            int ni = 0;
            int maxw = (width > 0 && width < 127) ? width : 127;
            while (s[ni] && ni < maxw) {
                numbuf[ni] = s[ni];
                ni++;
            }
            numbuf[ni] = '\0';
            char *ep = NULL;
            if (spec == 'd' || spec == 'i') {
                long long v = strtoll(numbuf, &ep, base);
                if (ep == numbuf) break;
                s = start + (ep - numbuf);
                if (!suppress) {
                    if (lmod >= 1) *va_arg(ap, long long *) = v;
                    else *va_arg(ap, int *) = (int)v;
                    assigned++;
                }
            } else {
                unsigned long long v = strtoull(numbuf, &ep, base);
                if (ep == numbuf) break;
                s = start + (ep - numbuf);
                if (!suppress) {
                    if (lmod >= 1) *va_arg(ap, unsigned long long *) = v;
                    else *va_arg(ap, unsigned int *) = (unsigned int)v;
                    assigned++;
                }
            }
        } else if (spec == 'f' || spec == 'e' || spec == 'g') {
            char *ep = NULL;
            double dv = strtod(s, &ep);
            if (ep == s) break;
            s = ep;
            if (!suppress) {
                if (lmod >= 1) *va_arg(ap, double *) = dv;
                else *va_arg(ap, float *) = (float)dv;
                assigned++;
            }
        } else {
            break;
        }
    }
    va_end(ap);
    return assigned;
}

int vsscanf(const char *str, const char *format, va_list ap) {
    return _wasi_rt_vsscanf(str, format, ap);
}

int sscanf(const char *str, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int r = _wasi_rt_vsscanf(str, format, ap);
    va_end(ap);
    return r;
}

int fscanf(FILE *stream, const char *format, ...) {
    char linebuf[1024];
    if (!stream || !format) return -1;
    if (!fgets(linebuf, sizeof(linebuf), stream)) return -1;
    va_list ap;
    va_start(ap, format);
    int r = _wasi_rt_vsscanf(linebuf, format, ap);
    va_end(ap);
    return r;
}

/* -------------------------------------------------------------------------
 * 8. Process Lifecycle, Environment, Time & POSIX Shims
 * ------------------------------------------------------------------------- */
int atexit(void (*fn)(void)) {
    if (!fn || _wasi_rt_atexit_count >= 32) return -1;
    _wasi_rt_atexit_fns[_wasi_rt_atexit_count++] = fn;
    return 0;
}

void exit(int status) {
    while (_wasi_rt_atexit_count > 0) {
        _wasi_rt_atexit_fns[--_wasi_rt_atexit_count]();
    }
    fflush(NULL);
    __wasi_proc_exit(status);
}

void abort(void) {
    fflush(NULL);
    __wasi_proc_exit(134);
}

char *getenv(const char *name) {
    if (!name || !_wasi_rt_environ) return NULL;
    size_t nlen = strlen(name);
    int i;
    for (i = 0; i < _wasi_rt_envc; i++) {
        char *entry = _wasi_rt_environ[i];
        if (entry && strncmp(entry, name, nlen) == 0 && entry[nlen] == '=') {
            return entry + nlen + 1;
        }
    }
    return NULL;
}

int setenv(const char *name, const char *value, int overwrite) { (void)name; (void)value; (void)overwrite; return -1; }
int unsetenv(const char *name) { (void)name; return -1; }
int system(const char *cmd) { (void)cmd; return -1; }
int isatty(int fd) { (void)fd; return 0; }
char *getcwd(char *buf, size_t size) {
    if (buf && size >= 2) { buf[0] = '.'; buf[1] = '\0'; return buf; }
    return NULL;
}
int shm_open(const char *name, int oflag, int mode) { (void)name; (void)oflag; (void)mode; return -1; }
void *mmap(void *addr, size_t length, int prot, int flags, int fd, long long offset) {
    (void)addr; (void)length; (void)prot; (void)flags; (void)fd; (void)offset;
    return (void *)-1;
}

long clock(void) {
    uint64_t ns = 0;
    if (__wasi_clock_time_get(1U, 1000ULL, &ns) != 0) return 0L;
    return (long)(ns / 1000ULL);
}

int clock_gettime(int clk_id, void *tp) {
    uint64_t ns = 0;
    int rc = __wasi_clock_time_get((uint32_t)clk_id, 1000ULL, &ns);
    if (rc != 0) return -1;
    if (tp) {
        long *lp = (long *)tp;
        lp[0] = (long)(ns / 1000000000ULL);
        lp[1] = (long)(ns % 1000000000ULL);
    }
    return 0;
}

int gettimeofday(void *tv, void *tz) {
    (void)tz;
    uint64_t ns = 0;
    if (__wasi_clock_time_get(0U, 1000ULL, &ns) != 0) return -1;
    if (tv) {
        long *lp = (long *)tv;
        lp[0] = (long)(ns / 1000000000ULL);
        lp[1] = (long)((ns % 1000000000ULL) / 1000ULL);
    }
    return 0;
}

double round(double x) { return (x >= 0.0) ? (double)(long long)(x + 0.5) : (double)(long long)(x - 0.5); }
float roundf(float x) { return (float)round((double)x); }
double fabs(double x) { return (x < 0.0) ? -x : x; }
float fabsf(float x) { return (x < 0.0f) ? -x : x; }

/* -------------------------------------------------------------------------
 * 8b. Wall-Clock Calendar Time, Setjmp/Longjmp, and Freestanding IEEE Math
 * ------------------------------------------------------------------------- */
time_t time(time_t *tloc) {
    uint64_t ns = 0;
    time_t sec = 0;
    if (__wasi_clock_time_get(0U, 1000000ULL, &ns) == 0) {
        sec = (time_t)(ns / 1000000000ULL);
    }
    if (tloc) *tloc = sec;
    return sec;
}

double difftime(time_t t1, time_t t0) {
    return (double)(t1 - t0);
}

struct tm *gmtime_r_wasi(const time_t *timer, struct tm *res) {
    if (!timer || !res) return NULL;
    long long t = (long long)(*timer);
    long long days = t / 86400LL;
    long long rem = t % 86400LL;
    if (rem < 0) { rem += 86400LL; days -= 1LL; }

    long long z = days + 719468LL;
    long long era = (z >= 0 ? z : z - 146096LL) / 146097LL;
    unsigned long long doe = (unsigned long long)(z - era * 146097LL);
    unsigned long long yoe = (doe - doe / 1460ULL + doe / 36524ULL - doe / 146096ULL) / 365ULL;
    long long y = (long long)yoe + era * 400LL;
    unsigned long long doy = doe - (365ULL * yoe + yoe / 4ULL - yoe / 100ULL);
    unsigned long long mp = (5ULL * doy + 2ULL) / 153ULL;
    unsigned int d = (unsigned int)(doy - (153ULL * mp + 2ULL) / 5ULL + 1ULL);
    unsigned int m = (unsigned int)(mp < 10ULL ? mp + 3ULL : mp - 9ULL);
    y += (m <= 2U) ? 1LL : 0LL;

    static const int mdays_cum[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    int is_leap = ((y % 4LL == 0) && ((y % 100LL != 0) || (y % 400LL == 0))) ? 1 : 0;
    int yday = mdays_cum[m - 1U] + ((is_leap && m > 2U) ? 1 : 0) + (int)d - 1;
    int wday = (int)((days + 4LL) % 7LL);
    if (wday < 0) wday += 7;

    res->tm_sec = (int)(rem % 60LL);
    res->tm_min = (int)((rem % 3600LL) / 60LL);
    res->tm_hour = (int)(rem / 3600LL);
    res->tm_mday = (int)d;
    res->tm_mon = (int)(m - 1U);
    res->tm_year = (int)(y - 1900LL);
    res->tm_wday = wday;
    res->tm_yday = yday;
    res->tm_isdst = 0;
    res->tm_gmtoff = 0;
    res->tm_zone = "UTC";
    return res;
}

struct tm *localtime_r(const time_t *t, struct tm *res) {
    return gmtime_r_wasi(t, res);
}

static struct tm _wasi_rt_tm_buf;
struct tm *gmtime(const time_t *t) {
    return gmtime_r_wasi(t, &_wasi_rt_tm_buf);
}

struct tm *localtime(const time_t *t) {
    return gmtime_r_wasi(t, &_wasi_rt_tm_buf);
}

time_t mktime(struct tm *tm) {
    if (!tm) return (time_t)-1;
    long long y = (long long)tm->tm_year + 1900LL;
    unsigned int m = (unsigned int)(tm->tm_mon + 1);
    unsigned int d = (unsigned int)tm->tm_mday;
    y -= (m <= 2U) ? 1LL : 0LL;
    long long era = (y >= 0 ? y : y - 399LL) / 400LL;
    unsigned long long yoe = (unsigned long long)(y - era * 400LL);
    unsigned long long doy = (153ULL * (m > 2U ? (unsigned long long)(m - 3U) : (unsigned long long)(m + 9U)) + 2ULL) / 5ULL + d - 1ULL;
    unsigned long long doe = yoe * 365ULL + yoe / 4ULL - yoe / 100ULL + doy;
    long long days = era * 146097LL + (long long)doe - 719468LL;
    return (time_t)(days * 86400LL + (long long)tm->tm_hour * 3600LL + (long long)tm->tm_min * 60LL + (long long)tm->tm_sec);
}

int setjmp(jmp_buf env) {
    (void)env;
    return 0;
}

int _setjmp(jmp_buf env) {
    (void)env;
    return 0;
}

int sigsetjmp(sigjmp_buf env, int savesigs) {
    (void)env; (void)savesigs;
    return 0;
}

void longjmp(jmp_buf env, int val) {
    (void)env;
    fprintf(stderr, "wasi_rt: uncaught longjmp(%d)\n", val);
    exit(val ? val : 1);
}

void _longjmp(jmp_buf env, int val) {
    longjmp(env, val);
}

void siglongjmp(sigjmp_buf env, int val) {
    longjmp((long *)env, val);
}

void (*signal(int sig, void (*func)(int)))(int) {
    (void)sig; (void)func;
    return NULL;
}

static uint64_t _wasi_rt_d2u(double x) {
    uint64_t u = 0ULL;
    memcpy(&u, &x, 8);
    return u;
}

static double _wasi_rt_u2d(uint64_t u) {
    double x = 0.0;
    memcpy(&x, &u, 8);
    return x;
}

double copysign(double x, double y) {
    uint64_t ux = _wasi_rt_d2u(x) & 0x7FFFFFFFFFFFFFFFULL;
    uint64_t uy = _wasi_rt_d2u(y) & 0x8000000000000000ULL;
    return _wasi_rt_u2d(ux | uy);
}

float copysignf(float x, float y) {
    return (float)copysign((double)x, (double)y);
}

double floor(double x) {
    if (x == 0.0 || x != x || fabs(x) >= 4503599627370496.0) return x;
    long long i = (long long)x;
    double d = (double)i;
    return (d > x) ? (d - 1.0) : d;
}

float floorf(float x) { return (float)floor((double)x); }

double ceil(double x) {
    if (x == 0.0 || x != x || fabs(x) >= 4503599627370496.0) return x;
    long long i = (long long)x;
    double d = (double)i;
    return (d < x) ? (d + 1.0) : d;
}

float ceilf(float x) { return (float)ceil((double)x); }

double trunc(double x) {
    if (x == 0.0 || x != x || fabs(x) >= 4503599627370496.0) return x;
    return (double)(long long)x;
}

float truncf(float x) { return (float)trunc((double)x); }

double fmax(double x, double y) {
    if (x != x) return y;
    if (y != y) return x;
    return (x > y) ? x : y;
}

double fmin(double x, double y) {
    if (x != x) return y;
    if (y != y) return x;
    return (x < y) ? x : y;
}

float fmaxf(float x, float y) { return (float)fmax((double)x, (double)y); }
float fminf(float x, float y) { return (float)fmin((double)x, (double)y); }

double frexp(double x, int *eptr) {
    if (x == 0.0 || x != x) {
        if (eptr) *eptr = 0;
        return x;
    }
    uint64_t u = _wasi_rt_d2u(x);
    int exp_bits = (int)((u >> 52) & 0x7FFULL);
    if (exp_bits == 0x7FF) {
        if (eptr) *eptr = 0;
        return x;
    }
    int e_adj = 0;
    if (exp_bits == 0) {
        x *= 4503599627370496.0;
        u = _wasi_rt_d2u(x);
        exp_bits = (int)((u >> 52) & 0x7FFULL);
        e_adj = -52;
    }
    if (eptr) *eptr = (exp_bits - 1022) + e_adj;
    u = (u & 0x800FFFFFFFFFFFFFULL) | 0x3FE0000000000000ULL;
    return _wasi_rt_u2d(u);
}

float frexpf(float x, int *eptr) {
    return (float)frexp((double)x, eptr);
}

double ldexp(double x, int exp) {
    if (x == 0.0 || exp == 0 || x != x) return x;
    if (x > 1.7976931348623157e+308 || x < -1.7976931348623157e+308) return x;
    if (exp > 2048) exp = 2048;
    if (exp < -2048) exp = -2048;
    while (exp >= 32) { x *= 4294967296.0; exp -= 32; }
    while (exp <= -32) { x *= (1.0 / 4294967296.0); exp += 32; }
    while (exp > 0) { x *= 2.0; exp--; }
    while (exp < 0) { x *= 0.5; exp++; }
    return x;
}

float ldexpf(float x, int exp) {
    return (float)ldexp((double)x, exp);
}

double modf(double x, double *iptr) {
    double t = trunc(x);
    if (iptr) *iptr = t;
    return x - t;
}

double fmod(double x, double y) {
    if (y == 0.0 || x != x || y != y) return 0.0 / 0.0;
    double ax = fabs(x), ay = fabs(y);
    if (ax < ay) return x;
    double q = trunc(x / y);
    double r = x - q * y;
    return r;
}

float fmodf(float x, float y) {
    return (float)fmod((double)x, (double)y);
}

double sqrt(double x) {
    if (x < 0.0) return 0.0 / 0.0;
    if (x == 0.0 || x != x || x > 1.7976931348623157e+308) return x;
    int e = 0;
    double m = frexp(x, &e);
    if (e & 1) { m *= 2.0; e -= 1; }
    double y = 0.5 * (m + 1.0);
    int i;
    for (i = 0; i < 8; i++) y = 0.5 * (y + m / y);
    return ldexp(y, e / 2);
}

float sqrtf(float x) { return (float)sqrt((double)x); }

double exp(double x) {
    if (x != x) return x;
    if (x > 709.7827) return 1.0 / 0.0;
    if (x < -745.1332) return 0.0;
    const double LN2 = 0.69314718055994530942;
    const double INV_LN2 = 1.44269504088896340736;
    double kf = floor(x * INV_LN2 + 0.5);
    int k = (int)kf;
    double r = x - kf * LN2;
    double sum = 1.0, term = 1.0;
    int i;
    for (i = 1; i <= 18; i++) {
        term *= r / (double)i;
        sum += term;
    }
    return ldexp(sum, k);
}

float expf(float x) { return (float)exp((double)x); }

double log(double x) {
    if (x != x) return x;
    if (x < 0.0) return 0.0 / 0.0;
    if (x == 0.0) return -1.0 / 0.0;
    if (x > 1.7976931348623157e+308) return x;
    int k = 0;
    double m = frexp(x, &k);
    if (m < 0.70710678118654752440) { m *= 2.0; k -= 1; }
    double u = (m - 1.0) / (m + 1.0);
    double u2 = u * u;
    double term = u;
    double sum = 0.0;
    int i;
    for (i = 1; i <= 23; i += 2) {
        sum += term / (double)i;
        term *= u2;
    }
    return 2.0 * sum + (double)k * 0.69314718055994530942;
}

float logf(float x) { return (float)log((double)x); }
double log2(double x) { return log(x) * 1.44269504088896340736; }
float log2f(float x) { return (float)log2((double)x); }
double log10(double x) { return log(x) * 0.43429448190325182765; }
float log10f(float x) { return (float)log10((double)x); }

double pow(double x, double y) {
    if (y == 0.0 || x == 1.0) return 1.0;
    if (x != x || y != y) return 0.0 / 0.0;
    if (fabs(y) <= 2147483647.0 && floor(y) == y) {
        long long n = (long long)y;
        int neg_exp = 0;
        if (n < 0) { neg_exp = 1; n = -n; }
        double res = 1.0;
        double base = x;
        while (n > 0) {
            if (n & 1LL) res *= base;
            base *= base;
            n >>= 1;
        }
        return neg_exp ? (1.0 / res) : res;
    }
    if (x == 0.0) return (y < 0.0) ? (1.0 / 0.0) : 0.0;
    if (x < 0.0) return 0.0 / 0.0;
    return exp(y * log(x));
}

float powf(float x, float y) { return (float)pow((double)x, (double)y); }

static void _wasi_rt_sincos_reduce(double x, double *r_out, int *q_out) {
    const double HALF_PI = 1.57079632679489661923;
    const double INV_HALF_PI = 0.63661977236758134308;
    double qf = floor(x * INV_HALF_PI + 0.5);
    *q_out = ((long long)qf) & 3;
    *r_out = x - qf * HALF_PI;
}

static double _wasi_rt_sin_core(double r) {
    double r2 = r * r, term = r, sum = r;
    int i;
    for (i = 3; i <= 15; i += 2) {
        term *= -r2 / (double)(i * (i - 1));
        sum += term;
    }
    return sum;
}

static double _wasi_rt_cos_core(double r) {
    double r2 = r * r, term = 1.0, sum = 1.0;
    int i;
    for (i = 2; i <= 16; i += 2) {
        term *= -r2 / (double)(i * (i - 1));
        sum += term;
    }
    return sum;
}

double sin(double x) {
    if (x == 0.0) return x;
    if (x != x || fabs(x) > 1.0e15) return 0.0 / 0.0;
    double r = 0.0; int q = 0;
    _wasi_rt_sincos_reduce(x, &r, &q);
    if (q == 0) return _wasi_rt_sin_core(r);
    if (q == 1) return _wasi_rt_cos_core(r);
    if (q == 2) return -_wasi_rt_sin_core(r);
    return -_wasi_rt_cos_core(r);
}

double cos(double x) {
    if (x != x || fabs(x) > 1.0e15) return 0.0 / 0.0;
    double r = 0.0; int q = 0;
    _wasi_rt_sincos_reduce(x, &r, &q);
    if (q == 0) return _wasi_rt_cos_core(r);
    if (q == 1) return -_wasi_rt_sin_core(r);
    if (q == 2) return -_wasi_rt_cos_core(r);
    return _wasi_rt_sin_core(r);
}

double tan(double x) { return sin(x) / cos(x); }
float sinf(float x) { return (float)sin((double)x); }
float cosf(float x) { return (float)cos((double)x); }
float tanf(float x) { return (float)tan((double)x); }

double atan(double x) {
    if (x != x || x == 0.0) return x;
    int neg = 0, recip = 0;
    if (x < 0.0) { neg = 1; x = -x; }
    if (x > 1.0) { recip = 1; x = 1.0 / x; }
    /* Half-angle reduction twice so x <= 0.27 -> rapid polynomial convergence */
    x = x / (1.0 + sqrt(1.0 + x * x));
    x = x / (1.0 + sqrt(1.0 + x * x));
    double x2 = x * x, term = x, sum = 0.0;
    int i;
    for (i = 1; i <= 15; i += 2) {
        sum += ((i & 2) ? -1.0 : 1.0) * term / (double)i;
        term *= x2;
    }
    double r = 4.0 * sum;
    if (recip) r = 1.57079632679489661923 - r;
    return neg ? -r : r;
}

double atan2(double y, double x) {
    const double PI = 3.14159265358979323846;
    if (x != x || y != y) return 0.0 / 0.0;
    int yneg = (int)((_wasi_rt_d2u(y) >> 63) & 1ULL);
    int xneg = (int)((_wasi_rt_d2u(x) >> 63) & 1ULL);
    if (x > 0.0) return atan(y / x);
    if (x < 0.0) return yneg ? (atan(y / x) - PI) : (atan(y / x) + PI);
    if (y > 0.0) return 0.5 * PI;
    if (y < 0.0) return -0.5 * PI;
    if (xneg) return yneg ? -PI : PI;
    return yneg ? -0.0 : 0.0;
}

double asin(double x) {
    if (fabs(x) > 1.0) return 0.0 / 0.0;
    if (fabs(x) == 1.0) return (x > 0.0) ? 1.57079632679489661923 : -1.57079632679489661923;
    return atan2(x, sqrt(1.0 - x * x));
}

double acos(double x) {
    if (fabs(x) > 1.0) return 0.0 / 0.0;
    return atan2(sqrt(1.0 - x * x), x);
}

float atanf(float x) { return (float)atan((double)x); }
float atan2f(float y, float x) { return (float)atan2((double)y, (double)x); }
float asinf(float x) { return (float)asin((double)x); }
float acosf(float x) { return (float)acos((double)x); }

#ifndef FE_TONEAREST
#define FE_TONEAREST  0
#define FE_DOWNWARD   1
#define FE_UPWARD     2
#define FE_TOWARDZERO 3
#endif

static int fegetround(void) { return FE_TONEAREST; }
static int fesetround(int mode) { (void)mode; return 0; }

static int _wasi_signbit(double x) {
    return (int)((_wasi_rt_d2u(x) >> 63) & 1ULL);
}
#ifdef signbit
#undef signbit
#endif
#define signbit(x) _wasi_signbit((double)(x))

long lrint(double x) {
    if (x != x || fabs(x) >= 2147483648.0) return (long)x;
    if (x >= 0.0) {
        double f = floor(x);
        double d = x - f;
        if (d > 0.5) return (long)(f + 1.0);
        if (d < 0.5) return (long)f;
        long iv = (long)f;
        return (iv & 1L) ? (iv + 1L) : iv;
    } else {
        double c = ceil(x);
        double d = c - x;
        if (d > 0.5) return (long)(c - 1.0);
        if (d < 0.5) return (long)c;
        long iv = (long)c;
        return (iv & 1L) ? (iv - 1L) : iv;
    }
}

double hypot(double x, double y) {
    double ax = fabs(x), ay = fabs(y);
    double inf = 1.0 / 0.0;
    if (ax == inf || ay == inf) return inf;
    if (ax != ax || ay != ay) return 0.0 / 0.0;
    if (ax < ay) { double t = ax; ax = ay; ay = t; }
    if (ax == 0.0) return 0.0;
    double r = ay / ax;
    return ax * sqrt(1.0 + r * r);
}

double cbrt(double x) {
    if (x == 0.0 || x != x || fabs(x) > 1.7976931348623157e+308) return x;
    int neg = 0;
    if (x < 0.0) { neg = 1; x = -x; }
    int e = 0;
    double m = frexp(x, &e);
    int r = e % 3;
    if (r < 0) r += 3;
    if (r == 1) { m *= 2.0; e -= 1; }
    else if (r == 2) { m *= 4.0; e -= 2; }
    double y = 0.5 * (m + 1.0);
    int i;
    for (i = 0; i < 10; i++) y = (2.0 * y + m / (y * y)) / 3.0;
    double out = ldexp(y, e / 3);
    return neg ? -out : out;
}

double expm1(double x) {
    if (x != x || x == 0.0) return x;
    if (x > 709.7827) return 1.0 / 0.0;
    if (x < -38.0) return -1.0;
    if (fabs(x) < 1e-4) {
        double t = x, s = 0.0;
        int k;
        for (k = 1; k <= 8; k++) {
            s += t;
            t *= x / (double)(k + 1);
        }
        return s;
    }
    return exp(x) - 1.0;
}

double log1p(double x) {
    if (x != x || x == 0.0) return x;
    if (x < -1.0) return 0.0 / 0.0;
    if (x == -1.0) return -1.0 / 0.0;
    if (x > 1.7976931348623157e+308) return x;
    if (fabs(x) < 1e-4) {
        double t = x, s = 0.0;
        int k;
        for (k = 1; k <= 10; k++) {
            s += (k & 1) ? (t / (double)k) : (-t / (double)k);
            t *= x;
        }
        return s;
    }
    return log(1.0 + x);
}

double sinh(double x) {
    if (x != x || x == 0.0) return x;
    if (fabs(x) > 710.0) return (x > 0.0) ? (1.0 / 0.0) : (-1.0 / 0.0);
    if (fabs(x) < 0.25) {
        double s = expm1(x);
        return 0.5 * (s + s / (1.0 + s));
    }
    double e = exp(fabs(x));
    double res = 0.5 * (e - 1.0 / e);
    return (x < 0.0) ? -res : res;
}

double cosh(double x) {
    if (x != x) return x;
    double ax = fabs(x);
    if (ax > 710.0) return 1.0 / 0.0;
    double e = exp(ax);
    return 0.5 * (e + 1.0 / e);
}

double tanh(double x) {
    if (x != x || x == 0.0) return x;
    double ax = fabs(x);
    if (ax > 22.0) return (x > 0.0) ? 1.0 : -1.0;
    double res = sinh(ax) / cosh(ax);
    return (x < 0.0) ? -res : res;
}

double asinh(double x) {
    if (x != x || x == 0.0) return x;
    double ax = fabs(x);
    if (ax > 1.7976931348623157e+308) return x;
    double res;
    if (ax > 1e8) res = log(ax) + 0.69314718055994530942;
    else if (ax < 1e-4) res = ax - (ax * ax * ax) / 6.0;
    else res = log1p(ax + (ax * ax) / (1.0 + sqrt(1.0 + ax * ax)));
    return (x < 0.0) ? -res : res;
}

double acosh(double x) {
    if (x != x) return x;
    if (x < 1.0) return 0.0 / 0.0;
    if (x > 1.7976931348623157e+308) return x;
    if (x > 1e8) return log(x) + 0.69314718055994530942;
    double xm1 = x - 1.0;
    return log1p(xm1 + sqrt(2.0 * xm1 + xm1 * xm1));
}

double atanh(double x) {
    if (x != x || x == 0.0) return x;
    double ax = fabs(x);
    if (ax > 1.0) return 0.0 / 0.0;
    if (ax == 1.0) return (x > 0.0) ? (1.0 / 0.0) : (-1.0 / 0.0);
    double res = 0.5 * log1p(2.0 * ax / (1.0 - ax));
    return (x < 0.0) ? -res : res;
}

void perror(const char *s) {
    if (s && s[0]) {
        fprintf(stderr, "%s: error %d\n", s, _wasi_rt_errno_val);
    } else {
        fprintf(stderr, "error %d\n", _wasi_rt_errno_val);
    }
}

int unlink(const char *path) {
    return remove(path);
}

int access(const char *path, int mode) {
    (void)mode;
    _wasi_rt_ensure_init();
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    fclose(f);
    return 0;
}

#ifndef ZCC_WASI_STAT_DEFINED
#define ZCC_WASI_STAT_DEFINED
struct stat {
    int st_size;
    int st_mode;
};
#endif

#ifndef O_RDONLY
#define O_RDONLY 0
#endif
#ifndef O_WRONLY
#define O_WRONLY 1
#endif
#ifndef O_RDWR
#define O_RDWR 2
#endif
#ifndef O_CREAT
#define O_CREAT 0x40
#endif
#ifndef O_TRUNC
#define O_TRUNC 0x200
#endif
#ifndef O_APPEND
#define O_APPEND 0x400
#endif
#ifndef O_BINARY
#define O_BINARY 0
#endif
#ifndef R_OK
#define R_OK 4
#endif
#ifndef W_OK
#define W_OK 2
#endif
#ifndef F_OK
#define F_OK 0
#endif

int open(const char *path, int flags, ...) {
    _wasi_rt_ensure_init();
    if (!path) return -1;
    const char *rel = _wasi_rt_relpath(path);
    uint32_t oflags = 0U;
    uint32_t fdflags = 0U;
    if (flags & O_CREAT) oflags |= _WASI_RT_O_CREAT;
    if (flags & O_TRUNC) oflags |= _WASI_RT_O_TRUNC;
    if (flags & O_APPEND) fdflags |= _WASI_RT_FDFLAG_APP;
    int need_w = ((flags & 3) == O_WRONLY) || ((flags & 3) == O_RDWR) || (flags & (O_CREAT | O_TRUNC | O_APPEND));
    uint64_t want_rights = need_w ? _WASI_RT_RIGHTS_WRITE : _WASI_RT_RIGHTS_READ;
    uint64_t base_rights = want_rights & _wasi_rt_preopen_rights_inh;
    if (base_rights == 0ULL) base_rights = want_rights;
    int new_fd = -1;
    int rc = __wasi_path_open(_wasi_rt_preopen_fd, 0U, rel, (uint32_t)strlen(rel),
                              oflags, base_rights, base_rights, fdflags, &new_fd);
    if (rc != 0 || new_fd < 0) {
        _wasi_rt_errno_val = rc;
        return -1;
    }
    return new_fd;
}

long lseek(int fd, long offset, int whence) {
    if (fd < 0) return -1L;
    uint64_t newoff = 0;
    int rc = __wasi_fd_seek(fd, (int64_t)offset, (uint32_t)whence, &newoff);
    if (rc != 0) {
        _wasi_rt_errno_val = rc;
        return -1L;
    }
    return (long)newoff;
}

int fstat(int fd, struct stat *buf) {
    if (fd < 0 || !buf) return -1;
    uint64_t cur = 0, end = 0;
    if (__wasi_fd_seek(fd, 0LL, 1U, &cur) != 0) return -1;
    if (__wasi_fd_seek(fd, 0LL, 2U, &end) != 0) return -1;
    __wasi_fd_seek(fd, (int64_t)cur, 0U, &cur);
    buf->st_size = (int)end;
    buf->st_mode = 0100644;
    return 0;
}

int mkdir(const char *path, unsigned int mode) {
    (void)path;
    (void)mode;
    return 0;
}

/* -------------------------------------------------------------------------
 * 9. WASI Entry Point (`_start`) with LP64 8-Byte Pointer Unpacking
 * ------------------------------------------------------------------------- */
void _start(void) {
    _wasi_rt_ensure_init();

    uint32_t raw_argc = 0, raw_argv_sz = 0;
    int argc = 1;
    static char *_wasi_rt_default_argv[2] = { "zcc.wasm", NULL };
    char **argv = _wasi_rt_default_argv;

    if (__wasi_args_sizes_get(&raw_argc, &raw_argv_sz) == 0 && raw_argc > 0) {
        uint32_t *a32 = (uint32_t *)malloc((size_t)(raw_argc + 1U) * 4U);
        uint8_t *abuf = (uint8_t *)malloc((size_t)(raw_argv_sz + 1U));
        if (a32 && abuf && __wasi_args_get(a32, abuf) == 0) {
            char **a64 = (char **)malloc((size_t)(raw_argc + 1U) * sizeof(char *));
            if (a64) {
                uint32_t ai;
                for (ai = 0; ai < raw_argc; ai++) {
                    a64[ai] = (char *)(uintptr_t)a32[ai];
                }
                a64[raw_argc] = NULL;
                argc = (int)raw_argc;
                argv = a64;
            }
        }
        free(a32);
    }

    int rc = main(argc, argv);
    exit(rc);
}
