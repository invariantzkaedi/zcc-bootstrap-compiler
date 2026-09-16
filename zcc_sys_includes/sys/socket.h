/*
 * zcc_sys_includes/sys/socket.h - Standard POSIX Berkeley Socket API for ZCC
 */
#ifndef _SYS_SOCKET_H
#define _SYS_SOCKET_H

#include <sys/types.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _SA_FAMILY_T_DEFINED
#define _SA_FAMILY_T_DEFINED
typedef unsigned short sa_family_t;
#endif

#ifndef _SOCKLEN_T_DEFINED
#define _SOCKLEN_T_DEFINED
typedef unsigned int socklen_t;
#endif

/* Address families / Protocol families */
#define AF_UNSPEC   0
#define AF_UNIX     1
#define AF_LOCAL    AF_UNIX
#define AF_INET     2
#define AF_INET6    10

#define PF_UNSPEC   AF_UNSPEC
#define PF_UNIX     AF_UNIX
#define PF_LOCAL    AF_LOCAL
#define PF_INET     AF_INET
#define PF_INET6    AF_INET6

/* Socket communication types */
#define SOCK_STREAM     1
#define SOCK_DGRAM      2
#define SOCK_RAW        3
#define SOCK_RDM        4
#define SOCK_SEQPACKET  5

/* Socket options level */
#define SOL_SOCKET      1

#define SO_DEBUG        1
#define SO_REUSEADDR    2
#define SO_TYPE         3
#define SO_ERROR        4
#define SO_DONTROUTE    5
#define SO_BROADCAST    6
#define SO_SNDBUF       7
#define SO_RCVBUF       8
#define SO_KEEPALIVE    9
#define SO_OOBINLINE    10

/* How to shutdown socket */
#define SHUT_RD         0
#define SHUT_WR         1
#define SHUT_RDWR       2

/* Generic socket address structure */
struct sockaddr {
    sa_family_t sa_family;
    char        sa_data[14];
};

/* Core POSIX Berkeley Socket function prototypes */
int     socket(int domain, int type, int protocol);
int     bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int     listen(int sockfd, int backlog);
int     accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
int     connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
ssize_t send(int sockfd, const void *buf, size_t len, int flags);
ssize_t recv(int sockfd, void *buf, size_t len, int flags);
ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
               const struct sockaddr *dest_addr, socklen_t addrlen);
ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
                 struct sockaddr *src_addr, socklen_t *addrlen);
int     shutdown(int sockfd, int how);
int     getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen);
int     setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen);
int     getsockname(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
int     getpeername(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
int     socketpair(int domain, int type, int protocol, int sv[2]);

#ifdef __cplusplus
}
#endif

#endif /* _SYS_SOCKET_H */
