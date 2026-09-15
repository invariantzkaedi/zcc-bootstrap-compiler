/*
 * zcc_sys_includes/sys/wait.h - POSIX Process Waiting and Termination Status for ZCC
 */
#ifndef _SYS_WAIT_H
#define _SYS_WAIT_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Wait Options */
#define WNOHANG     1   /* Don't block waiting */
#define WUNTRACED   2   /* Report status of stopped children */
#define WCONTINUED  8   /* Report status of continued children */

/* Macros for interpreting wait status */
#define WIFEXITED(status)    (((status) & 0x7f) == 0)
#define WEXITSTATUS(status)  (((status) & 0xff00) >> 8)
#define WIFSIGNALED(status)  (((signed char) (((status) & 0x7f) + 1) >> 1) > 0)
#define WTERMSIG(status)     ((status) & 0x7f)
#define WIFSTOPPED(status)   (((status) & 0xff) == 0x7f)
#define WSTOPSIG(status)     WEXITSTATUS(status)

/* Process Wait Declarations */
pid_t wait(int *wstatus);
pid_t waitpid(pid_t pid, int *wstatus, int options);

#ifdef __cplusplus
}
#endif

#endif /* _SYS_WAIT_H */
