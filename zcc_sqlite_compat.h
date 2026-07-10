#ifndef ZCC_SQLITE_COMPAT_H
#define ZCC_SQLITE_COMPAT_H

#include <sys/types.h>
#include <time.h>

/* Missing type definitions */
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

/* Missing POSIX constants */
#define O_EXCL 02000
#define S_ISREG(m) (((m) & 0170000) == 0100000)
#define S_ISLNK(m) (((m) & 0170000) == 0120000)
#define S_ISDIR(m) (((m) & 0170000) == 0040000)
#define W_OK 2
#define R_OK 4
#define F_OK 0
#define F_RDLCK 0
#define F_WRLCK 1
#define F_UNLCK 2
#define F_GETLK 5
#define F_SETLK 6
#define F_SETLKW 7
#define MREMAP_MAYMOVE 1
#define ETIMEDOUT 110
#define EBUSY 16
#define ENOLCK 37
#define EPERM 1
#define EIO 5
#define _SC_PAGESIZE 30

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
int lstat(const char *pathname, struct stat *statbuf);
int getpid(void);
int nanosleep(const struct timespec *req, struct timespec *rem);
int utimes(const char *filename, const struct timeval times[2]);
int fsync(int fd);
long sysconf(int name);

/* Builtins mapping */
#define __builtin_bswap16(x) ((((x)&0xff)<<8)|(((x)>>8)&0xff))
#define __builtin_bswap32(x) ((((x)&0xff)<<24)|(((x)&0xff00)<<8)|(((x)&0xff0000)>>8)|(((x)>>24)&0xff))

#define __builtin_add_overflow(a, b, res) (*(res) = (a) + (b), (*(res) < (a)))
#define __builtin_sub_overflow(a, b, res) (*(res) = (a) - (b), ((a) < (b)))
#define __builtin_mul_overflow(a, b, res) (*(res) = (a) * (b), ((a) != 0 && *(res) / (a) != (b)))

/* Atomic ops mapping for thread-safe-disabled SQLite */
#define __atomic_load_n(ptr,mo) (*(ptr))
#define __atomic_store_n(ptr,val,mo) (*(ptr)=(val))

#endif /* ZCC_SQLITE_COMPAT_H */
