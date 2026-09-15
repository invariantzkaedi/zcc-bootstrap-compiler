/*
 * zcc_sys_includes/sys/types.h - Standard POSIX System Data Types for ZCC
 */
#ifndef _SYS_TYPES_H
#define _SYS_TYPES_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

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

#ifndef _MODE_T_DEFINED
#define _MODE_T_DEFINED
typedef unsigned int mode_t;
#endif

#ifndef _SSIZE_T_DEFINED
#define _SSIZE_T_DEFINED
typedef long ssize_t;
#endif

#ifndef _DEV_T_DEFINED
#define _DEV_T_DEFINED
typedef uint64_t dev_t;
#endif

#ifndef _INO_T_DEFINED
#define _INO_T_DEFINED
typedef uint64_t ino_t;
#endif

#ifndef _NLINK_T_DEFINED
#define _NLINK_T_DEFINED
typedef uint64_t nlink_t;
#endif

#ifndef _TIME_T_DEFINED
#define _TIME_T_DEFINED
typedef int64_t time_t;
#endif

#ifdef __cplusplus
}
#endif

#endif /* _SYS_TYPES_H */
