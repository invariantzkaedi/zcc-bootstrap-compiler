/*
 * ZCC Standalone Win64 C99 Math & POSIX Compatibility Runtime (`src/win64_c99_rt.c`)
 * ====================================================================================
 * Supplies IEEE-754 C99 math functions missing from Windows System32\msvcrt.dll plus
 * binary-safe POSIX file/time shims for zero-toolchain PE32+ executables (QuickJS,
 * MicroPython, DOOM 1.10). Kept separate from `src/win64_app_crt.c` so minimal
 * console binaries opt in cleanly when compiled as an explicit Win64 unit.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

extern double _hypot(double x, double y);
extern int _stricmp(const char *s1, const char *s2);
extern int _strnicmp(const char *s1, const char *s2, size_t count);
extern int _open(const char *path, int oflag, ...);
extern int _close(int fd);
extern int _read(int fd, void *buf, unsigned int count);
extern int _write(int fd, const void *buf, unsigned int count);
extern long _lseek(int fd, long offset, int origin);
extern int _mkdir(const char *dirname);
extern int _access(const char *path, int mode);

typedef struct {
    long long time;
    unsigned short millitm;
    short timezone;
    short dstflag;
} _w64_timeb64_t;

extern void _ftime64(_w64_timeb64_t *tp);

typedef struct {
    long long tv_sec;
    long long tv_usec;
} _w64_timeval_t;

static uint64_t _w64_d2u(double x) {
    uint64_t u = 0ULL;
    memcpy(&u, &x, 8);
    return u;
}

static double _w64_u2d(uint64_t u) {
    double x = 0.0;
    memcpy(&x, &u, 8);
    return x;
}

int _w64_isnan(double x) {
    uint64_t u = _w64_d2u(x) & 0x7FFFFFFFFFFFFFFFULL;
    return u > 0x7FF0000000000000ULL;
}

int _w64_isinf(double x) {
    uint64_t u = _w64_d2u(x) & 0x7FFFFFFFFFFFFFFFULL;
    return u == 0x7FF0000000000000ULL;
}

int _w64_isfinite(double x) {
    uint64_t u = _w64_d2u(x) & 0x7FFFFFFFFFFFFFFFULL;
    return u < 0x7FF0000000000000ULL;
}

int _w64_signbit(double x) {
    return (int)((_w64_d2u(x) >> 63) & 1ULL);
}

int _w64_fegetround(void) {
    return 0; /* FE_TONEAREST */
}

int _w64_fesetround(int mode) {
    (void)mode;
    return 0;
}

int fegetround(void) { return 0; }
int fesetround(int round) { (void)round; return 0; }
#undef isinf
#undef isnan
#undef isfinite
int isinf(double x) { return _w64_isinf(x); }
int isnan(double x) { return _w64_isnan(x); }
int isfinite(double x) { return _w64_isfinite(x); }

double copysign(double x, double y) {
    uint64_t ux = _w64_d2u(x) & 0x7FFFFFFFFFFFFFFFULL;
    uint64_t uy = _w64_d2u(y) & 0x8000000000000000ULL;
    return _w64_u2d(ux | uy);
}

float copysignf(float x, float y) {
    return (float)copysign((double)x, (double)y);
}

double trunc(double x) {
    if (x == 0.0 || _w64_isnan(x) || fabs(x) >= 4503599627370496.0) return x;
    return (double)(long long)x;
}

float truncf(float x) {
    return (float)trunc((double)x);
}

double round(double x) {
    if (x == 0.0 || _w64_isnan(x) || fabs(x) >= 4503599627370496.0) return x;
    if (x > 0.0) {
        double f = floor(x);
        return ((x - f) >= 0.5) ? (f + 1.0) : f;
    } else {
        double c = ceil(x);
        return ((c - x) >= 0.5) ? (c - 1.0) : c;
    }
}

float roundf(float x) {
    return (float)round((double)x);
}

double nearbyint(double x) {
    if (x == 0.0 || _w64_isnan(x) || fabs(x) >= 4503599627370496.0) return x;
    double f = floor(x);
    double d = x - f;
    double res;
    if (d > 0.5) {
        res = f + 1.0;
    } else if (d < 0.5) {
        res = f;
    } else {
        res = (fmod(f, 2.0) == 0.0) ? f : (f + 1.0);
    }
    return copysign(res, x);
}

float nearbyintf(float x) {
    return (float)nearbyint((double)x);
}

double rint(double x) {
    return nearbyint(x);
}

float rintf(float x) {
    return (float)rint((double)x);
}

long lrint(double x) {
    double r = nearbyint(x);
    if (_w64_isnan(r) || fabs(r) >= 2147483648.0) return (long)r;
    return (long)r;
}

long long llrint(double x) {
    double r = nearbyint(x);
    if (_w64_isnan(r) || fabs(r) >= 9223372036854775808.0) return (long long)r;
    return (long long)r;
}

double fmax(double x, double y) {
    if (_w64_isnan(x)) return y;
    if (_w64_isnan(y)) return x;
    if (x == 0.0 && y == 0.0) return _w64_signbit(x) ? y : x;
    return (x > y) ? x : y;
}

double fmin(double x, double y) {
    if (_w64_isnan(x)) return y;
    if (_w64_isnan(y)) return x;
    if (x == 0.0 && y == 0.0) return _w64_signbit(x) ? x : y;
    return (x < y) ? x : y;
}

float fmaxf(float x, float y) { return (float)fmax((double)x, (double)y); }
float fminf(float x, float y) { return (float)fmin((double)x, (double)y); }

double log2(double x) {
    return log(x) * 1.44269504088896340736;
}

float log2f(float x) {
    return (float)log2((double)x);
}

double exp2(double x) {
    return pow(2.0, x);
}

float exp2f(float x) {
    return (float)exp2((double)x);
}

double fma(double x, double y, double z) {
    return (x * y) + z;
}

double nan(const char *tagp) {
    (void)tagp;
    return _w64_u2d(0x7FF8000000000000ULL);
}

double hypot(double x, double y) {
    double ax = fabs(x), ay = fabs(y);
    double inf = 1.0 / 0.0;
    if (ax == inf || ay == inf) return inf;
    if (_w64_isnan(ax) || _w64_isnan(ay)) return _w64_u2d(0x7FF8000000000000ULL);
    if (ax < ay) { double t = ax; ax = ay; ay = t; }
    if (ax == 0.0) return 0.0;
    double r = ay / ax;
    return ax * sqrt(1.0 + r * r);
}

double cbrt(double x) {
    if (x == 0.0 || _w64_isnan(x) || _w64_isinf(x)) return x;
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
    if (_w64_isnan(x) || x == 0.0) return x;
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
    if (_w64_isnan(x) || x == 0.0) return x;
    if (x < -1.0) return _w64_u2d(0x7FF8000000000000ULL);
    if (x == -1.0) return -1.0 / 0.0;
    if (_w64_isinf(x)) return x;
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

double asinh(double x) {
    if (_w64_isnan(x) || x == 0.0 || _w64_isinf(x)) return x;
    double ax = fabs(x);
    double res;
    if (ax > 1e8) res = log(ax) + 0.69314718055994530942;
    else if (ax < 1e-4) res = ax - (ax * ax * ax) / 6.0;
    else res = log1p(ax + (ax * ax) / (1.0 + sqrt(1.0 + ax * ax)));
    return (x < 0.0) ? -res : res;
}

double acosh(double x) {
    if (_w64_isnan(x)) return x;
    if (x < 1.0) return _w64_u2d(0x7FF8000000000000ULL);
    if (_w64_isinf(x)) return x;
    if (x > 1e8) return log(x) + 0.69314718055994530942;
    double xm1 = x - 1.0;
    return log1p(xm1 + sqrt(2.0 * xm1 + xm1 * xm1));
}

double atanh(double x) {
    if (_w64_isnan(x) || x == 0.0) return x;
    double ax = fabs(x);
    if (ax > 1.0) return _w64_u2d(0x7FF8000000000000ULL);
    if (ax == 1.0) return (x > 0.0) ? (1.0 / 0.0) : (-1.0 / 0.0);
    double res = 0.5 * log1p(2.0 * ax / (1.0 - ax));
    return (x < 0.0) ? -res : res;
}

float strtof(const char *nptr, char **endptr) {
    return (float)strtod(nptr, endptr);
}

float floorf(float x) { return (float)floor((double)x); }
float ceilf(float x)  { return (float)ceil((double)x); }
float fabsf(float x)  { return (x < 0.0f) ? -x : x; }

/* POSIX & time compatibility shims backed cleanly by System32\msvcrt.dll */
int gettimeofday(void *tv_ptr, void *tz_ptr) {
    (void)tz_ptr;
    if (tv_ptr) {
        _w64_timeb64_t tb;
        memset(&tb, 0, sizeof(tb));
        _ftime64(&tb);
        _w64_timeval_t *tv = (_w64_timeval_t *)tv_ptr;
        tv->tv_sec = tb.time;
        tv->tv_usec = (long long)tb.millitm * 1000LL;
    }
    return 0;
}

int strcasecmp(const char *s1, const char *s2) {
    return _stricmp(s1, s2);
}

int strncasecmp(const char *s1, const char *s2, size_t n) {
    return _strnicmp(s1, s2, n);
}

/* Translate Linux/WASI O_* flags into MSVCRT binary-safe flags (ALWAYS set _O_BINARY=0x8000) */
int _win64_posix_open(const char *path, int flags, ...) {
    int acc = flags & 3;
    int mflags = 0x8000; /* _O_BINARY */
    if (acc == 1) mflags |= 0x0001;      /* _O_WRONLY */
    else if (acc == 2) mflags |= 0x0002; /* _O_RDWR */
    if (flags & 0x0040) mflags |= 0x0100; /* Linux O_CREAT -> _O_CREAT */
    if (flags & 0x0200) mflags |= 0x0200; /* O_TRUNC */
    if (flags & 0x0400) mflags |= 0x0008; /* Linux O_APPEND -> _O_APPEND */
    return _open(path, mflags, 0x0180);   /* _S_IREAD | _S_IWRITE */
}

int _win64_posix_close(int fd) {
    return _close(fd);
}

int _win64_posix_read(int fd, void *buf, unsigned int count) {
    return _read(fd, buf, count);
}

int _win64_posix_write(int fd, const void *buf, unsigned int count) {
    return _write(fd, buf, count);
}

long _win64_posix_lseek(int fd, long offset, int whence) {
    return _lseek(fd, offset, whence);
}

int _win64_posix_fstat_size(int fd, long long *out_size) {
    if (fd < 0) return -1;
    long cur = _lseek(fd, 0L, 1 /* SEEK_CUR */);
    long end = _lseek(fd, 0L, 2 /* SEEK_END */);
    if (cur >= 0L) _lseek(fd, cur, 0 /* SEEK_SET */);
    if (end < 0L) return -1;
    if (out_size) *out_size = (long long)end;
    return 0;
}

int _win64_posix_mkdir(const char *path, int mode) {
    (void)mode;
    return _mkdir(path);
}

int _win64_posix_access(const char *path, int mode) {
    return _access(path, mode & 6);
}
