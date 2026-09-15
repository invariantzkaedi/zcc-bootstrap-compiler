#ifndef _NET_IF_H
#define _NET_IF_H

unsigned int if_nametoindex(const char *ifname);
char *if_indextoname(unsigned int ifindex, char *ifname);

#endif /* _NET_IF_H */
