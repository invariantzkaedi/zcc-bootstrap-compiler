#ifndef _SYS_UN_H
#define _SYS_UN_H

typedef unsigned short sa_family_t;

struct sockaddr_un {
    sa_family_t sun_family;
    char sun_path[108];
};

#endif /* _SYS_UN_H */
