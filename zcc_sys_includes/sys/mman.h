/*
 * zcc_sys_includes/sys/mman.h - POSIX Memory Management Declarations for ZCC
 */
#ifndef _SYS_MMAN_H
#define _SYS_MMAN_H

#include <stddef.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Memory Protection Flags */
#define PROT_NONE       0x0     /* Page can not be accessed */
#define PROT_READ       0x1     /* Page can be read */
#define PROT_WRITE      0x2     /* Page can be written */
#define PROT_EXEC       0x4     /* Page can be executed */

/* Sharing / Mapping Types */
#define MAP_SHARED      0x01    /* Share changes */
#define MAP_PRIVATE     0x02    /* Changes are private */
#define MAP_TYPE        0x0f    /* Mask for type of mapping */

/* Mapping Flags */
#define MAP_FIXED       0x10    /* Interpret addr exactly */
#define MAP_ANONYMOUS   0x20    /* Don't use a file */
#define MAP_ANON        MAP_ANONYMOUS

/* Error return value */
#define MAP_FAILED      ((void *) -1)

/* Core POSIX Virtual Memory Management Functions */
void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
int   munmap(void *addr, size_t length);
int   mprotect(void *addr, size_t length, int prot);
int   msync(void *addr, size_t length, int flags);

#ifdef __cplusplus
}
#endif

#endif /* _SYS_MMAN_H */
