#define MINIZ_USE_UNALIGNED_LOADS_AND_STORES 1
#define MINIZ_UNALIGNED_USE_MEMCPY

#if MINIZ_USE_UNALIGNED_LOADS_AND_STORES
#ifdef MINIZ_UNALIGNED_USE_MEMCPY
static int tdefl_mem = 1;
#else
static int tdefl_mem = 2;
#endif
#endif
