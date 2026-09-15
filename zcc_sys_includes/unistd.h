/*
 * zcc_sys_includes/unistd.h - POSIX Standard Symbolic Constants and Types for ZCC
 */
#ifndef _UNISTD_H
#define _UNISTD_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Standard POSIX Types */
#ifndef _PID_T_DEFINED
#define _PID_T_DEFINED
typedef int pid_t;
#endif

#ifndef _UID_T_DEFINED
#define _UID_T_DEFINED
typedef unsigned int uid_t;
#endif

#ifndef _GID_T_DEFINED
#define _GID_T_DEFINED
typedef unsigned int gid_t;
#endif

#ifndef _OFF_T_DEFINED
#define _OFF_T_DEFINED
typedef long off_t;
#endif

#ifndef _SSIZE_T_DEFINED
#define _SSIZE_T_DEFINED
typedef long ssize_t;
#endif

#ifndef _USECONDS_T_DEFINED
#define _USECONDS_T_DEFINED
typedef unsigned int useconds_t;
#endif

/* Standard File Descriptors */
#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

/* Access Mode Bitmasks for access() */
#define F_OK 0
#define X_OK 1
#define W_OK 2
#define R_OK 4

/* Whence Values for lseek() */
#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif

/* Core POSIX System I/O Declarations */
ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
int     close(int fd);
off_t   lseek(int fd, off_t offset, int whence);
int     unlink(const char *pathname);
int     access(const char *pathname, int mode);

/* Process Identification and Control */
pid_t   getpid(void);
pid_t   getppid(void);
uid_t   getuid(void);
gid_t   getgid(void);
pid_t   fork(void);
int     pipe(int pipefd[2]);
int     execve(const char *pathname, char *const argv[], char *const envp[]);
void    _exit(int status);

/* Timing and Sleep */
unsigned int sleep(unsigned int seconds);
int          usleep(useconds_t usec);

/* System Call Invocation Helper */
long syscall(long number, ...);

#ifdef __cplusplus
}
#endif

#endif /* _UNISTD_H */
