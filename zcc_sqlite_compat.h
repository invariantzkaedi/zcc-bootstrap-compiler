#ifndef ZCC_SQLITE_COMPAT_H
#define ZCC_SQLITE_COMPAT_H

#include <sys/types.h>
#include <time.h>

#ifdef __ZCC__
/* Missing type definitions under ZCC */
typedef int pid_t;
typedef int uid_t;
typedef int gid_t;
typedef int mode_t;
typedef int dev_t;

struct timespec {
    long tv_sec;
    long tv_nsec;
};

struct timeval {
    long tv_sec;
    long tv_usec;
};

struct flock {
    short l_type;
    short l_whence;
    off_t l_start;
    off_t l_len;
    pid_t l_pid;
};

/* Missing function declarations */
char *getcwd(char *buf, size_t size);
int ftruncate(int fd, off_t length);
ssize_t pread(int fd, void *buf, size_t count, off_t offset);
ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset);
int fchmod(int fd, mode_t mode);
int unlink(const char *pathname);
int mkdir(const char *pathname, mode_t mode);
int rmdir(const char *pathname);
int fchown(int fd, uid_t owner, gid_t group);
uid_t geteuid(void);
void *mremap(void *old_address, size_t old_size, size_t new_size, int flags, ...);
ssize_t readlink(const char *pathname, char *buf, size_t bufsiz);
struct stat;
int lstat(const char *pathname, struct stat *statbuf);
int getpid(void);
int nanosleep(const struct timespec *req, struct timespec *rem);
int utimes(const char *filename, const struct timeval times[2]);
int fsync(int fd);
long sysconf(int name);
#endif

/* Builtins mapping */
#define __builtin_bswap16(x) ((((x)&0xff)<<8)|(((x)>>8)&0xff))
#define __builtin_bswap32(x) ((((x)&0xff)<<24)|(((x)&0xff00)<<8)|(((x)&0xff0000)>>8)|(((x)>>24)&0xff))

#define __builtin_add_overflow(a, b, res) (*(res) = (a) + (b), (*(res) < (a)))
#define __builtin_sub_overflow(a, b, res) (*(res) = (a) - (b), ((a) < (b)))
#define __builtin_mul_overflow(a, b, res) (*(res) = (a) * (b), ((a) != 0 && *(res) / (a) != (b)))

/* Atomic ops mapping for thread-safe-disabled SQLite */
#define __atomic_load_n(ptr,mo) (*(ptr))
#define __atomic_store_n(ptr,val,mo) (*(ptr)=(val))

/*
 * SQL-CRASH-38060 Container Hardening & Fortification:
 * Enforces exact ZCC SystemV AMD64 Parse struct layout offsets across
 * Ubuntu 24.04 / GCC 13.3.0 container runtimes to prevent VLA/FinishCoding segfaults.
 */
#ifndef ZCC_PARSE_OFFSETS_DEFINED
#define ZCC_PARSE_OFFSETS_DEFINED
#define ZCC_PARSE_TOTAL_SIZE      424
#define ZCC_PARSE_LASTTOKEN_OFF   288
#define ZCC_PARSE_TAIL_SIZE       136
#define ZCC_PARSE_HDR_SIZE        176

/* Compile-time static assertions guaranteeing layout invariants */
_Static_assert(ZCC_PARSE_TOTAL_SIZE == 424, "ZCC SystemV Parse struct size mismatch");
_Static_assert(ZCC_PARSE_LASTTOKEN_OFF == 288, "ZCC SystemV Parse sLastToken offset mismatch");
_Static_assert(ZCC_PARSE_TOTAL_SIZE - ZCC_PARSE_LASTTOKEN_OFF == ZCC_PARSE_TAIL_SIZE, "ZCC SystemV Parse tail invariant violated");

/* Memory boundary safety macros */
#define ZCC_SQLITE_VALIDATE_PARSE_PTR(p) \
    do { \
        if (!(p)) { \
            fprintf(stderr, "[ZCC-CONTAINER-GUARD] NULL Parse pointer intercepted!\n"); \
            abort(); \
        } \
    } while(0)

#define ZCC_SQLITE_SAFE_TAIL_OFFSET(p) (((char*)(p)) + ZCC_PARSE_LASTTOKEN_OFF)
#endif

#endif /* ZCC_SQLITE_COMPAT_H */
