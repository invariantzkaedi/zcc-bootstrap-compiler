#ifndef ZCC_CURL_COMPAT_H
#define ZCC_CURL_COMPAT_H

#include <stdarg.h>
#include <sys/types.h>
#include <time.h>

typedef unsigned int socklen_t;
typedef int pid_t;
typedef int uid_t;
typedef int gid_t;
typedef int mode_t;
typedef long time_t;
typedef long suseconds_t;
typedef unsigned short sa_family_t;

typedef struct {
    unsigned long fds_bits[16];
} fd_set;

struct in_addr {
    unsigned int s_addr;
};

struct sockaddr_in {
    sa_family_t sin_family;
    unsigned short sin_port;
    struct in_addr sin_addr;
    char sin_zero[8];
};

struct in6_addr {
    unsigned char s6_addr[16];
};

struct sockaddr_in6 {
    sa_family_t sin6_family;
    unsigned short sin6_port;
    unsigned int sin6_flowinfo;
    struct in6_addr sin6_addr;
    unsigned int sin6_scope_id;
};

#define AF_UNSPEC 0
#define AF_INET 2
#define AF_INET6 10

#define PF_UNSPEC 0
#define PF_INET 2
#define PF_INET6 10

#define SOCK_STREAM 1
#define SOCK_DGRAM 2
#define IPPROTO_IP 0
#define IPPROTO_TCP 6
#define IPPROTO_UDP 17

const char *inet_ntop(int af, const void *src, char *dst, socklen_t size);
int inet_pton(int af, const char *src, void *dst);
int strerror_r(int errnum, char *buf, size_t buflen);

#define __builtin_expect(exp, c) (exp)
#define __builtin_unreachable() do {} while(0)
#define __builtin_clz(x) __builtin_clz(x)

#endif /* ZCC_CURL_COMPAT_H */
