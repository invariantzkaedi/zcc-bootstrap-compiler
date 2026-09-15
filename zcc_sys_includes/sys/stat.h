/*
 * zcc_sys_includes/sys/stat.h - POSIX File Characteristics & Permissions for ZCC
 */
#ifndef _SYS_STAT_H
#define _SYS_STAT_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _MODE_T_DEFINED
#define _MODE_T_DEFINED
typedef unsigned int mode_t;
#endif

/* File mode permission bits */
#define S_IRWXU 00700   /* Read, write, execute by owner */
#define S_IRUSR 00400   /* Read by owner */
#define S_IWUSR 00200   /* Write by owner */
#define S_IXUSR 00100   /* Execute by owner */

#define S_IRWXG 00070   /* Read, write, execute by group */
#define S_IRGRP 00040   /* Read by group */
#define S_IWGRP 00020   /* Write by group */
#define S_IXGRP 00010   /* Execute by group */

#define S_IRWXO 00007   /* Read, write, execute by others */
#define S_IROTH 00004   /* Read by others */
#define S_IWOTH 00002   /* Write by others */
#define S_IXOTH 00001   /* Execute by others */

/* File Type Bits */
#define S_IFMT   0170000 /* Bitmask for file type */
#define S_IFREG  0100000 /* Regular file */
#define S_IFDIR  0040000 /* Directory */
#define S_IFCHR  0020000 /* Character device */
#define S_IFBLK  0060000 /* Block device */
#define S_IFIFO  0010000 /* FIFO / Pipe */
#define S_IFLNK  0120000 /* Symbolic link */
#define S_IFSOCK 0140000 /* Socket */

/* File Type Test Macros */
#define S_ISREG(m)  (((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m)  (((m) & S_IFMT) == S_IFDIR)
#define S_ISCHR(m)  (((m) & S_IFMT) == S_IFCHR)
#define S_ISBLK(m)  (((m) & S_IFMT) == S_IFBLK)
#define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)
#define S_ISLNK(m)  (((m) & S_IFMT) == S_IFLNK)
#define S_ISSOCK(m) (((m) & S_IFMT) == S_IFSOCK)

int chmod(const char *pathname, mode_t mode);
int mkdir(const char *pathname, mode_t mode);

#ifdef __cplusplus
}
#endif

#endif /* _SYS_STAT_H */
