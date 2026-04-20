#define __x86_64__ 1

#if defined(_M_IX86) || defined(_M_X64) || defined(__i386__) || defined(__i386) || defined(__i486__) || defined(__i486) || defined(i386) || defined(__ia64__) || defined(__x86_64__)
#define MINIZ_X86_OR_X64_CPU 1
#endif

#if !defined(MINIZ_USE_UNALIGNED_LOADS_AND_STORES)
#if MINIZ_X86_OR_X64_CPU
#define MINIZ_USE_UNALIGNED_LOADS_AND_STORES 1
#define MINIZ_UNALIGNED_USE_MEMCPY
#endif
#endif

#if MINIZ_USE_UNALIGNED_LOADS_AND_STORES
static int block_a = 1;
#else
static int block_b = 2;
#endif

#if MINIZ_USE_UNALIGNED_LOADS_AND_STORES && MINIZ_LITTLE_ENDIAN
static int block_c = 3;
#endif
