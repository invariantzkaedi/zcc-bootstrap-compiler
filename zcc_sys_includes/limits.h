#ifndef _LIMITS_H
#define _LIMITS_H
#define CHAR_BIT     8
#define SCHAR_MIN    (-128)
#define SCHAR_MAX    127
#define UCHAR_MAX    255
#define CHAR_MIN     SCHAR_MIN
#define CHAR_MAX     SCHAR_MAX
#define SHRT_MIN     (-32768)
#define SHRT_MAX     32767
#define USHRT_MAX    65535
#define INT_MIN      (-2147483647-1)
#define INT_MAX      2147483647
#define UINT_MAX     4294967295U
#define LONG_MIN     (-9223372036854775807L-1)
#define LONG_MAX     9223372036854775807L
#define ULONG_MAX    18446744073709551615UL
#define LLONG_MIN    (-9223372036854775807LL-1)
#define LLONG_MAX    9223372036854775807LL
#define ULLONG_MAX   18446744073709551615ULL

/* Fixed-width type limits — required by <stdint.h> consumers and csmith safe_math.h */
#define INT8_MIN     (-128)
#define INT8_MAX     127
#define UINT8_MAX    255U
#define INT16_MIN    (-32768)
#define INT16_MAX    32767
#define UINT16_MAX   65535U
#define INT32_MIN    (-2147483647-1)
#define INT32_MAX    2147483647
#define UINT32_MAX   4294967295U
#define INT64_MIN    (-9223372036854775807LL-1)
#define INT64_MAX    9223372036854775807LL
#define UINT64_MAX   18446744073709551615ULL
#endif
