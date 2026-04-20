#define MINIZ_NO_ARCHIVE_APIS
#include "miniz.c"

#ifdef MINIZ_UNALIGNED_USE_MEMCPY
static int unaligned_memcpy_works = 1;
#else
static int unaligned_memcpy_fails = 1;
#endif

#if MINIZ_USE_UNALIGNED_LOADS_AND_STORES
static int miniz_unaligned_loads_works = 1;
#else
static int miniz_unaligned_loads_fails = 1;
#endif
