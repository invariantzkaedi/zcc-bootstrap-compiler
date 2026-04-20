#if defined(__x86_64__)
int cpu_x86_64 = 1;
#define MINIZ_X86_OR_X64_CPU 1
#endif
#if !defined(MINIZ_USE_UNALIGNED_LOADS_AND_STORES)
int unaligned_undefined = 1;
#if MINIZ_X86_OR_X64_CPU
int cpu_x86_64_inner = 1;
#define MINIZ_USE_UNALIGNED_LOADS_AND_STORES 1
#define MINIZ_UNALIGNED_USE_MEMCPY
#endif
#endif
#if MINIZ_USE_UNALIGNED_LOADS_AND_STORES
int unaligned_is_1 = 1;
#ifdef MINIZ_UNALIGNED_USE_MEMCPY
static int tdefl_mem = 1;
#else
static int tdefl_mem = 2;
#endif
#else
static int tdefl_mem = 0;
#endif
