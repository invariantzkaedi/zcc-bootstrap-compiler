#ifndef _STDINT_H
#define _STDINT_H

/* ZCC stdint.h stub — fixed-width integer types for x86-64 LP64 ABI */

typedef signed char        int8_t;
typedef short              int16_t;
typedef int                int32_t;
typedef long long          int64_t;
typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;

typedef long               intptr_t;
typedef unsigned long      uintptr_t;
typedef long long          intmax_t;
typedef unsigned long long uintmax_t;

/* Least-width types */
typedef signed char        int_least8_t;
typedef short              int_least16_t;
typedef int                int_least32_t;
typedef long long          int_least64_t;
typedef unsigned char      uint_least8_t;
typedef unsigned short     uint_least16_t;
typedef unsigned int       uint_least32_t;
typedef unsigned long long uint_least64_t;

/* Fast types */
typedef signed char        int_fast8_t;
typedef long               int_fast16_t;
typedef long               int_fast32_t;
typedef long long          int_fast64_t;
typedef unsigned char      uint_fast8_t;
typedef unsigned long      uint_fast16_t;
typedef unsigned long      uint_fast32_t;
typedef unsigned long long uint_fast64_t;

/* Limits — pull from limits.h to avoid duplication */
#include <limits.h>

/* INT8 / INT16 / INT32 / INT64 min/max (may be defined by limits.h already) */
#ifndef INT8_MIN
#define INT8_MIN     (-128)
#define INT8_MAX     127
#define UINT8_MAX    255U
#endif
#ifndef INT16_MIN
#define INT16_MIN    (-32768)
#define INT16_MAX    32767
#define UINT16_MAX   65535U
#endif
#ifndef INT32_MIN
#define INT32_MIN    (-2147483647-1)
#define INT32_MAX    2147483647
#define UINT32_MAX   4294967295U
#endif
#ifndef INT64_MIN
#define INT64_MIN    (-9223372036854775807LL-1)
#define INT64_MAX    9223372036854775807LL
#define UINT64_MAX   18446744073709551615ULL
#endif

#define INTPTR_MIN   (-9223372036854775807L-1)
#define INTPTR_MAX   9223372036854775807L
#define UINTPTR_MAX  18446744073709551615UL
#define INTMAX_MIN   (-9223372036854775807LL-1)
#define INTMAX_MAX   9223372036854775807LL
#define UINTMAX_MAX  18446744073709551615ULL

#define INT8_C(x)    (x)
#define INT16_C(x)   (x)
#define INT32_C(x)   (x)
#define INT64_C(x)   (x ## LL)
#define UINT8_C(x)   (x ## U)
#define UINT16_C(x)  (x ## U)
#define UINT32_C(x)  (x ## U)
#define UINT64_C(x)  (x ## ULL)
#define INTMAX_C(x)  (x ## LL)
#define UINTMAX_C(x) (x ## ULL)

#endif /* _STDINT_H */
