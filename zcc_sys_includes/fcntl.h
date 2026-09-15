/*
 * zcc_sys_includes/fcntl.h - POSIX File Control Options for ZCC
 */
#ifndef _FCNTL_H
#define _FCNTL_H

#include <sys/stat.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

/* File Access Modes (Linux / POSIX standard) */
#define O_ACCMODE   00000003
#define O_RDONLY    00000000
#define O_WRONLY    00000001
#define O_RDWR      00000002

/* File Creation and Status Flags */
#define O_CREAT     00000100
#define O_EXCL      00000200
#define O_NOCTTY    00000400
#define O_TRUNC     00001000
#define O_APPEND    00002000
#define O_NONBLOCK  00004000
#define O_NDELAY    O_NONBLOCK
#define O_SYNC      00010000
#define O_FSYNC     O_SYNC
#define O_ASYNC     00020000

/* POSIX File Declarations */
int open(const char *pathname, int flags, ...);
int creat(const char *pathname, mode_t mode);
int fcntl(int fd, int cmd, ...);

#ifdef __cplusplus
}
#endif

#endif /* _FCNTL_H */
