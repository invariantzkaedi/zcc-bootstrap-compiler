#ifndef __attribute__
#define __attribute__(x)
#endif
typedef unsigned long size_t;
#define NULL 0
#ifndef offsetof
#define offsetof(type, member) __builtin_offsetof(type, member)
#endif
