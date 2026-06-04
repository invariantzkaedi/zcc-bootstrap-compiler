#define CTIMEOPT_VAL_(opt) #opt
#define CTIMEOPT_VAL(opt) CTIMEOPT_VAL_(opt)

/* Simulate the SQLite pattern */
#ifndef SQLITE_DEFAULT_CACHE_SIZE
# define SQLITE_DEFAULT_CACHE_SIZE -2000
#endif

static const char * const azCompileOpt[] = {
#ifdef SQLITE_DEFAULT_CACHE_SIZE
  "DEFAULT_CACHE_SIZE=" CTIMEOPT_VAL(SQLITE_DEFAULT_CACHE_SIZE),
#endif
  0
};
