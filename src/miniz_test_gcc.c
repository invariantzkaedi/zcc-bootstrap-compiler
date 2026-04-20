




       

typedef long int ptrdiff_t;
typedef long unsigned int size_t;
typedef int wchar_t;
typedef struct {
  long long __max_align_ll __attribute__((__aligned__(__alignof__(long long))));
  long double __max_align_ld __attribute__((__aligned__(__alignof__(long double))));
} max_align_t;
    
   typedef unsigned long mz_ulong;


    void mz_free(void *p);



    mz_ulong mz_adler32(mz_ulong adler, const unsigned char *ptr, size_t buf_len);



    mz_ulong mz_crc32(mz_ulong crc, const unsigned char *ptr, size_t buf_len);


    enum
    {
        MZ_DEFAULT_STRATEGY = 0,
        MZ_FILTERED = 1,
        MZ_HUFFMAN_ONLY = 2,
        MZ_RLE = 3,
        MZ_FIXED = 4
    };






    typedef void *(*mz_alloc_func)(void *opaque, size_t items, size_t size);
    typedef void (*mz_free_func)(void *opaque, void *address);
    typedef void *(*mz_realloc_func)(void *opaque, void *address, size_t items, size_t size);


    enum
    {
        MZ_NO_COMPRESSION = 0,
        MZ_BEST_SPEED = 1,
        MZ_BEST_COMPRESSION = 9,
        MZ_UBER_COMPRESSION = 10,
        MZ_DEFAULT_LEVEL = 6,
        MZ_DEFAULT_COMPRESSION = -1
    };
    enum
    {
        MZ_NO_FLUSH = 0,
        MZ_PARTIAL_FLUSH = 1,
        MZ_SYNC_FLUSH = 2,
        MZ_FULL_FLUSH = 3,
        MZ_FINISH = 4,
        MZ_BLOCK = 5
    };


    enum
    {
        MZ_OK = 0,
        MZ_STREAM_END = 1,
        MZ_NEED_DICT = 2,
        MZ_ERRNO = -1,
        MZ_STREAM_ERROR = -2,
        MZ_DATA_ERROR = -3,
        MZ_MEM_ERROR = -4,
        MZ_BUF_ERROR = -5,
        MZ_VERSION_ERROR = -6,
        MZ_PARAM_ERROR = -10000
    };




    struct mz_internal_state;


    typedef struct mz_stream_s
    {
        const unsigned char *next_in;
        unsigned int avail_in;
        mz_ulong total_in;

        unsigned char *next_out;
        unsigned int avail_out;
        mz_ulong total_out;

        char *msg;
        struct mz_internal_state *state;

        mz_alloc_func zalloc;
        mz_free_func zfree;
        void *opaque;

        int data_type;
        mz_ulong adler;
        mz_ulong reserved;
    } mz_stream;

    typedef mz_stream *mz_streamp;


    const char *mz_version(void);
    int mz_deflateInit(mz_streamp pStream, int level);






    int mz_deflateInit2(mz_streamp pStream, int level, int method, int window_bits, int mem_level, int strategy);


    int mz_deflateReset(mz_streamp pStream);
    int mz_deflate(mz_streamp pStream, int flush);





    int mz_deflateEnd(mz_streamp pStream);


    mz_ulong mz_deflateBound(mz_streamp pStream, mz_ulong source_len);



    int mz_compress(unsigned char *pDest, mz_ulong *pDest_len, const unsigned char *pSource, mz_ulong source_len);
    int mz_compress2(unsigned char *pDest, mz_ulong *pDest_len, const unsigned char *pSource, mz_ulong source_len, int level);


    mz_ulong mz_compressBound(mz_ulong source_len);






    int mz_inflateInit(mz_streamp pStream);



    int mz_inflateInit2(mz_streamp pStream, int window_bits);


    int mz_inflateReset(mz_streamp pStream);
    int mz_inflate(mz_streamp pStream, int flush);


    int mz_inflateEnd(mz_streamp pStream);



    int mz_uncompress(unsigned char *pDest, mz_ulong *pDest_len, const unsigned char *pSource, mz_ulong source_len);
    int mz_uncompress2(unsigned char *pDest, mz_ulong *pDest_len, const unsigned char *pSource, mz_ulong *pSource_len);



    const char *mz_error(int err);




    typedef unsigned char Byte;
    typedef unsigned int uInt;
    typedef mz_ulong uLong;
    typedef Byte Bytef;
    typedef uInt uIntf;
    typedef char charf;
    typedef int intf;
    typedef void *voidpf;
    typedef uLong uLongf;
    typedef void *voidp;
    typedef void *const voidpc;
    typedef void *(*alloc_func)(void *opaque, size_t items, size_t size);

    typedef void (*free_func)(void *opaque, void *address);






    static __inline__ __attribute__((__always_inline__)) int deflateInit(mz_streamp pStream, int level)
    {
        return mz_deflateInit(pStream, level);
    }
    static __inline__ __attribute__((__always_inline__)) int deflateInit2(mz_streamp pStream, int level, int method, int window_bits, int mem_level, int strategy)
    {
        return mz_deflateInit2(pStream, level, method, window_bits, mem_level, strategy);
    }
    static __inline__ __attribute__((__always_inline__)) int deflateReset(mz_streamp pStream)
    {
        return mz_deflateReset(pStream);
    }
    static __inline__ __attribute__((__always_inline__)) int deflate(mz_streamp pStream, int flush)
    {
        return mz_deflate(pStream, flush);
    }
    static __inline__ __attribute__((__always_inline__)) int deflateEnd(mz_streamp pStream)
    {
        return mz_deflateEnd(pStream);
    }
    static __inline__ __attribute__((__always_inline__)) mz_ulong deflateBound(mz_streamp pStream, mz_ulong source_len)
    {
        return mz_deflateBound(pStream, source_len);
    }
    static __inline__ __attribute__((__always_inline__)) int compress(unsigned char *pDest, mz_ulong *pDest_len, const unsigned char *pSource, mz_ulong source_len)
    {
        return mz_compress(pDest, pDest_len, pSource, source_len);
    }
    static __inline__ __attribute__((__always_inline__)) int compress2(unsigned char *pDest, mz_ulong *pDest_len, const unsigned char *pSource, mz_ulong source_len, int level)
    {
        return mz_compress2(pDest, pDest_len, pSource, source_len, level);
    }
    static __inline__ __attribute__((__always_inline__)) mz_ulong compressBound(mz_ulong source_len)
    {
        return mz_compressBound(source_len);
    }




    static __inline__ __attribute__((__always_inline__)) int inflateInit(mz_streamp pStream)
    {
        return mz_inflateInit(pStream);
    }

    static __inline__ __attribute__((__always_inline__)) int inflateInit2(mz_streamp pStream, int window_bits)
    {
        return mz_inflateInit2(pStream, window_bits);
    }

    static __inline__ __attribute__((__always_inline__)) int inflateReset(mz_streamp pStream)
    {
        return mz_inflateReset(pStream);
    }

    static __inline__ __attribute__((__always_inline__)) int inflate(mz_streamp pStream, int flush)
    {
        return mz_inflate(pStream, flush);
    }

    static __inline__ __attribute__((__always_inline__)) int inflateEnd(mz_streamp pStream)
    {
        return mz_inflateEnd(pStream);
    }

    static __inline__ __attribute__((__always_inline__)) int uncompress(unsigned char* pDest, mz_ulong* pDest_len, const unsigned char* pSource, mz_ulong source_len)
    {
        return mz_uncompress(pDest, pDest_len, pSource, source_len);
    }

    static __inline__ __attribute__((__always_inline__)) int uncompress2(unsigned char* pDest, mz_ulong* pDest_len, const unsigned char* pSource, mz_ulong* pSource_len)
    {
        return mz_uncompress2(pDest, pDest_len, pSource, pSource_len);
    }


    static __inline__ __attribute__((__always_inline__)) mz_ulong crc32(mz_ulong crc, const unsigned char *ptr, size_t buf_len)
    {
        return mz_crc32(crc, ptr, buf_len);
    }

    static __inline__ __attribute__((__always_inline__)) mz_ulong adler32(mz_ulong adler, const unsigned char *ptr, size_t buf_len)
    {
        return mz_adler32(adler, ptr, buf_len);
    }




    static __inline__ __attribute__((__always_inline__)) const char* zError(int err)
    {
        return mz_error(err);
    }
       




extern void __assert_fail (const char *__assertion, const char *__file,
      unsigned int __line, const char *__function)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__noreturn__));


extern void __assert_perror_fail (int __errnum, const char *__file,
      unsigned int __line, const char *__function)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__noreturn__));




extern void __assert (const char *__assertion, const char *__file, int __line)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__noreturn__));





typedef unsigned char __u_char;
typedef unsigned short int __u_short;
typedef unsigned int __u_int;
typedef unsigned long int __u_long;


typedef signed char __int8_t;
typedef unsigned char __uint8_t;
typedef signed short int __int16_t;
typedef unsigned short int __uint16_t;
typedef signed int __int32_t;
typedef unsigned int __uint32_t;

typedef signed long int __int64_t;
typedef unsigned long int __uint64_t;






typedef __int8_t __int_least8_t;
typedef __uint8_t __uint_least8_t;
typedef __int16_t __int_least16_t;
typedef __uint16_t __uint_least16_t;
typedef __int32_t __int_least32_t;
typedef __uint32_t __uint_least32_t;
typedef __int64_t __int_least64_t;
typedef __uint64_t __uint_least64_t;



typedef long int __quad_t;
typedef unsigned long int __u_quad_t;







typedef long int __intmax_t;
typedef unsigned long int __uintmax_t;


typedef unsigned long int __dev_t;
typedef unsigned int __uid_t;
typedef unsigned int __gid_t;
typedef unsigned long int __ino_t;
typedef unsigned long int __ino64_t;
typedef unsigned int __mode_t;
typedef unsigned long int __nlink_t;
typedef long int __off_t;
typedef long int __off64_t;
typedef int __pid_t;
typedef struct { int __val[2]; } __fsid_t;
typedef long int __clock_t;
typedef unsigned long int __rlim_t;
typedef unsigned long int __rlim64_t;
typedef unsigned int __id_t;
typedef long int __time_t;
typedef unsigned int __useconds_t;
typedef long int __suseconds_t;
typedef long int __suseconds64_t;

typedef int __daddr_t;
typedef int __key_t;


typedef int __clockid_t;


typedef void * __timer_t;


typedef long int __blksize_t;




typedef long int __blkcnt_t;
typedef long int __blkcnt64_t;


typedef unsigned long int __fsblkcnt_t;
typedef unsigned long int __fsblkcnt64_t;


typedef unsigned long int __fsfilcnt_t;
typedef unsigned long int __fsfilcnt64_t;


typedef long int __fsword_t;

typedef long int __ssize_t;


typedef long int __syscall_slong_t;

typedef unsigned long int __syscall_ulong_t;



typedef __off64_t __loff_t;
typedef char *__caddr_t;


typedef long int __intptr_t;


typedef unsigned int __socklen_t;




typedef int __sig_atomic_t;




typedef __int8_t int8_t;
typedef __int16_t int16_t;
typedef __int32_t int32_t;
typedef __int64_t int64_t;


typedef __uint8_t uint8_t;
typedef __uint16_t uint16_t;
typedef __uint32_t uint32_t;
typedef __uint64_t uint64_t;



typedef __int_least8_t int_least8_t;
typedef __int_least16_t int_least16_t;
typedef __int_least32_t int_least32_t;
typedef __int_least64_t int_least64_t;


typedef __uint_least8_t uint_least8_t;
typedef __uint_least16_t uint_least16_t;
typedef __uint_least32_t uint_least32_t;
typedef __uint_least64_t uint_least64_t;





typedef signed char int_fast8_t;

typedef long int int_fast16_t;
typedef long int int_fast32_t;
typedef long int int_fast64_t;
typedef unsigned char uint_fast8_t;

typedef unsigned long int uint_fast16_t;
typedef unsigned long int uint_fast32_t;
typedef unsigned long int uint_fast64_t;
typedef long int intptr_t;


typedef unsigned long int uintptr_t;
typedef __intmax_t intmax_t;
typedef __uintmax_t uintmax_t;














typedef struct
  {
    int quot;
    int rem;
  } div_t;



typedef struct
  {
    long int quot;
    long int rem;
  } ldiv_t;





__extension__ typedef struct
  {
    long long int quot;
    long long int rem;
  } lldiv_t;
extern size_t __ctype_get_mb_cur_max (void) __attribute__ ((__nothrow__ , __leaf__)) ;



extern double atof (const char *__nptr)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__pure__)) __attribute__ ((__nonnull__ (1))) ;

extern int atoi (const char *__nptr)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__pure__)) __attribute__ ((__nonnull__ (1))) ;

extern long int atol (const char *__nptr)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__pure__)) __attribute__ ((__nonnull__ (1))) ;



__extension__ extern long long int atoll (const char *__nptr)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__pure__)) __attribute__ ((__nonnull__ (1))) ;



extern double strtod (const char *__restrict __nptr,
        char **__restrict __endptr)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1)));



extern float strtof (const char *__restrict __nptr,
       char **__restrict __endptr) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1)));

extern long double strtold (const char *__restrict __nptr,
       char **__restrict __endptr)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1)));
extern long int strtol (const char *__restrict __nptr,
   char **__restrict __endptr, int __base)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1)));

extern unsigned long int strtoul (const char *__restrict __nptr,
      char **__restrict __endptr, int __base)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1)));



__extension__
extern long long int strtoq (const char *__restrict __nptr,
        char **__restrict __endptr, int __base)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1)));

__extension__
extern unsigned long long int strtouq (const char *__restrict __nptr,
           char **__restrict __endptr, int __base)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1)));




__extension__
extern long long int strtoll (const char *__restrict __nptr,
         char **__restrict __endptr, int __base)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1)));

__extension__
extern unsigned long long int strtoull (const char *__restrict __nptr,
     char **__restrict __endptr, int __base)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1)));
extern char *l64a (long int __n) __attribute__ ((__nothrow__ , __leaf__)) ;


extern long int a64l (const char *__s)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__pure__)) __attribute__ ((__nonnull__ (1))) ;










typedef __u_char u_char;
typedef __u_short u_short;
typedef __u_int u_int;
typedef __u_long u_long;
typedef __quad_t quad_t;
typedef __u_quad_t u_quad_t;
typedef __fsid_t fsid_t;


typedef __loff_t loff_t;




typedef __ino_t ino_t;
typedef __dev_t dev_t;




typedef __gid_t gid_t;




typedef __mode_t mode_t;




typedef __nlink_t nlink_t;




typedef __uid_t uid_t;





typedef __off_t off_t;
typedef __pid_t pid_t;





typedef __id_t id_t;




typedef __ssize_t ssize_t;





typedef __daddr_t daddr_t;
typedef __caddr_t caddr_t;





typedef __key_t key_t;










typedef __clock_t clock_t;







typedef __clockid_t clockid_t;
typedef __time_t time_t;






typedef __timer_t timer_t;



typedef unsigned long int ulong;
typedef unsigned short int ushort;
typedef unsigned int uint;







typedef __uint8_t u_int8_t;
typedef __uint16_t u_int16_t;
typedef __uint32_t u_int32_t;
typedef __uint64_t u_int64_t;


typedef int register_t __attribute__ ((__mode__ (__word__)));
static __inline __uint16_t
__bswap_16 (__uint16_t __bsx)
{

  return __builtin_bswap16 (__bsx);



}






static __inline __uint32_t
__bswap_32 (__uint32_t __bsx)
{

  return __builtin_bswap32 (__bsx);



}
__extension__ static __inline __uint64_t
__bswap_64 (__uint64_t __bsx)
{

  return __builtin_bswap64 (__bsx);



}
static __inline __uint16_t
__uint16_identity (__uint16_t __x)
{
  return __x;
}

static __inline __uint32_t
__uint32_identity (__uint32_t __x)
{
  return __x;
}

static __inline __uint64_t
__uint64_identity (__uint64_t __x)
{
  return __x;
}











typedef struct
{
  unsigned long int __val[(1024 / (8 * sizeof (unsigned long int)))];
} __sigset_t;


typedef __sigset_t sigset_t;










struct timeval
{




  __time_t tv_sec;
  __suseconds_t tv_usec;

};

struct timespec
{



  __time_t tv_sec;




  __syscall_slong_t tv_nsec;
};



typedef __suseconds_t suseconds_t;





typedef long int __fd_mask;
typedef struct
  {






    __fd_mask __fds_bits[1024 / (8 * (int) sizeof (__fd_mask))];


  } fd_set;






typedef __fd_mask fd_mask;

extern int select (int __nfds, fd_set *__restrict __readfds,
     fd_set *__restrict __writefds,
     fd_set *__restrict __exceptfds,
     struct timeval *__restrict __timeout);
extern int pselect (int __nfds, fd_set *__restrict __readfds,
      fd_set *__restrict __writefds,
      fd_set *__restrict __exceptfds,
      const struct timespec *__restrict __timeout,
      const __sigset_t *__restrict __sigmask);






typedef __blksize_t blksize_t;






typedef __blkcnt_t blkcnt_t;



typedef __fsblkcnt_t fsblkcnt_t;



typedef __fsfilcnt_t fsfilcnt_t;

typedef union
{
  __extension__ unsigned long long int __value64;
  struct
  {
    unsigned int __low;
    unsigned int __high;
  } __value32;
} __atomic_wide_counter;




typedef struct __pthread_internal_list
{
  struct __pthread_internal_list *__prev;
  struct __pthread_internal_list *__next;
} __pthread_list_t;

typedef struct __pthread_internal_slist
{
  struct __pthread_internal_slist *__next;
} __pthread_slist_t;
struct __pthread_mutex_s
{
  int __lock;
  unsigned int __count;
  int __owner;

  unsigned int __nusers;



  int __kind;

  short __spins;
  short __elision;
  __pthread_list_t __list;
};
struct __pthread_rwlock_arch_t
{
  unsigned int __readers;
  unsigned int __writers;
  unsigned int __wrphase_futex;
  unsigned int __writers_futex;
  unsigned int __pad3;
  unsigned int __pad4;

  int __cur_writer;
  int __shared;
  signed char __rwelision;




  unsigned char __pad1[7];


  unsigned long int __pad2;


  unsigned int __flags;
};




struct __pthread_cond_s
{
  __atomic_wide_counter __wseq;
  __atomic_wide_counter __g1_start;
  unsigned int __g_refs[2] ;
  unsigned int __g_size[2];
  unsigned int __g1_orig_size;
  unsigned int __wrefs;
  unsigned int __g_signals[2];
};

typedef unsigned int __tss_t;
typedef unsigned long int __thrd_t;

typedef struct
{
  int __data ;
} __once_flag;



typedef unsigned long int pthread_t;




typedef union
{
  char __size[4];
  int __align;
} pthread_mutexattr_t;




typedef union
{
  char __size[4];
  int __align;
} pthread_condattr_t;



typedef unsigned int pthread_key_t;



typedef int pthread_once_t;


union pthread_attr_t
{
  char __size[56];
  long int __align;
};

typedef union pthread_attr_t pthread_attr_t;




typedef union
{
  struct __pthread_mutex_s __data;
  char __size[40];
  long int __align;
} pthread_mutex_t;


typedef union
{
  struct __pthread_cond_s __data;
  char __size[48];
  __extension__ long long int __align;
} pthread_cond_t;





typedef union
{
  struct __pthread_rwlock_arch_t __data;
  char __size[56];
  long int __align;
} pthread_rwlock_t;

typedef union
{
  char __size[8];
  long int __align;
} pthread_rwlockattr_t;





typedef volatile int pthread_spinlock_t;




typedef union
{
  char __size[32];
  long int __align;
} pthread_barrier_t;

typedef union
{
  char __size[4];
  int __align;
} pthread_barrierattr_t;









extern long int random (void) __attribute__ ((__nothrow__ , __leaf__));


extern void srandom (unsigned int __seed) __attribute__ ((__nothrow__ , __leaf__));





extern char *initstate (unsigned int __seed, char *__statebuf,
   size_t __statelen) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (2)));



extern char *setstate (char *__statebuf) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1)));







struct random_data
  {
    int32_t *fptr;
    int32_t *rptr;
    int32_t *state;
    int rand_type;
    int rand_deg;
    int rand_sep;
    int32_t *end_ptr;
  };

extern int random_r (struct random_data *__restrict __buf,
       int32_t *__restrict __result) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1, 2)));

extern int srandom_r (unsigned int __seed, struct random_data *__buf)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (2)));

extern int initstate_r (unsigned int __seed, char *__restrict __statebuf,
   size_t __statelen,
   struct random_data *__restrict __buf)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (2, 4)));

extern int setstate_r (char *__restrict __statebuf,
         struct random_data *__restrict __buf)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1, 2)));





extern int rand (void) __attribute__ ((__nothrow__ , __leaf__));

extern void srand (unsigned int __seed) __attribute__ ((__nothrow__ , __leaf__));



extern int rand_r (unsigned int *__seed) __attribute__ ((__nothrow__ , __leaf__));







extern double drand48 (void) __attribute__ ((__nothrow__ , __leaf__));
extern double erand48 (unsigned short int __xsubi[3]) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1)));


extern long int lrand48 (void) __attribute__ ((__nothrow__ , __leaf__));
extern long int nrand48 (unsigned short int __xsubi[3])
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1)));


extern long int mrand48 (void) __attribute__ ((__nothrow__ , __leaf__));
extern long int jrand48 (unsigned short int __xsubi[3])
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1)));


extern void srand48 (long int __seedval) __attribute__ ((__nothrow__ , __leaf__));
extern unsigned short int *seed48 (unsigned short int __seed16v[3])
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1)));
extern void lcong48 (unsigned short int __param[7]) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1)));





struct drand48_data
  {
    unsigned short int __x[3];
    unsigned short int __old_x[3];
    unsigned short int __c;
    unsigned short int __init;
    __extension__ unsigned long long int __a;

  };


extern int drand48_r (struct drand48_data *__restrict __buffer,
        double *__restrict __result) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1, 2)));
extern int erand48_r (unsigned short int __xsubi[3],
        struct drand48_data *__restrict __buffer,
        double *__restrict __result) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1, 2)));


extern int lrand48_r (struct drand48_data *__restrict __buffer,
        long int *__restrict __result)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1, 2)));
extern int nrand48_r (unsigned short int __xsubi[3],
        struct drand48_data *__restrict __buffer,
        long int *__restrict __result)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1, 2)));


extern int mrand48_r (struct drand48_data *__restrict __buffer,
        long int *__restrict __result)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1, 2)));
extern int jrand48_r (unsigned short int __xsubi[3],
        struct drand48_data *__restrict __buffer,
        long int *__restrict __result)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1, 2)));


extern int srand48_r (long int __seedval, struct drand48_data *__buffer)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (2)));

extern int seed48_r (unsigned short int __seed16v[3],
       struct drand48_data *__buffer) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1, 2)));

extern int lcong48_r (unsigned short int __param[7],
        struct drand48_data *__buffer)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1, 2)));


extern __uint32_t arc4random (void)
     __attribute__ ((__nothrow__ , __leaf__)) ;


extern void arc4random_buf (void *__buf, size_t __size)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1)));



extern __uint32_t arc4random_uniform (__uint32_t __upper_bound)
     __attribute__ ((__nothrow__ , __leaf__)) ;




extern void *malloc (size_t __size) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__malloc__))
     __attribute__ ((__alloc_size__ (1))) ;

extern void *calloc (size_t __nmemb, size_t __size)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__malloc__)) __attribute__ ((__alloc_size__ (1, 2))) ;






extern void *realloc (void *__ptr, size_t __size)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__warn_unused_result__)) __attribute__ ((__alloc_size__ (2)));


extern void free (void *__ptr) __attribute__ ((__nothrow__ , __leaf__));







extern void *reallocarray (void *__ptr, size_t __nmemb, size_t __size)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__warn_unused_result__))
     __attribute__ ((__alloc_size__ (2, 3)))
    __attribute__ ((__malloc__ (__builtin_free, 1)));


extern void *reallocarray (void *__ptr, size_t __nmemb, size_t __size)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__malloc__ (reallocarray, 1)));










extern void *alloca (size_t __size) __attribute__ ((__nothrow__ , __leaf__));











extern void *valloc (size_t __size) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__malloc__))
     __attribute__ ((__alloc_size__ (1))) ;




extern int posix_memalign (void **__memptr, size_t __alignment, size_t __size)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1))) ;




extern void *aligned_alloc (size_t __alignment, size_t __size)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__malloc__)) __attribute__ ((__alloc_align__ (1)))
     __attribute__ ((__alloc_size__ (2))) ;



extern void abort (void) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__noreturn__));



extern int atexit (void (*__func) (void)) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1)));







extern int at_quick_exit (void (*__func) (void)) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1)));






extern int on_exit (void (*__func) (int __status, void *__arg), void *__arg)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1)));





extern void exit (int __status) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__noreturn__));





extern void quick_exit (int __status) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__noreturn__));





extern void _Exit (int __status) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__noreturn__));




extern char *getenv (const char *__name) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1))) ;
extern int putenv (char *__string) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1)));





extern int setenv (const char *__name, const char *__value, int __replace)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (2)));


extern int unsetenv (const char *__name) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1)));






extern int clearenv (void) __attribute__ ((__nothrow__ , __leaf__));
extern char *mktemp (char *__template) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1)));
extern int mkstemp (char *__template) __attribute__ ((__nonnull__ (1))) ;
extern int mkstemps (char *__template, int __suffixlen) __attribute__ ((__nonnull__ (1))) ;
extern char *mkdtemp (char *__template) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1))) ;
extern int system (const char *__command) ;
extern char *realpath (const char *__restrict __name,
         char *__restrict __resolved) __attribute__ ((__nothrow__ , __leaf__)) ;






typedef int (*__compar_fn_t) (const void *, const void *);
extern void *bsearch (const void *__key, const void *__base,
        size_t __nmemb, size_t __size, __compar_fn_t __compar)
     __attribute__ ((__nonnull__ (1, 2, 5))) ;







extern void qsort (void *__base, size_t __nmemb, size_t __size,
     __compar_fn_t __compar) __attribute__ ((__nonnull__ (1, 4)));
extern int abs (int __x) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__const__)) ;
extern long int labs (long int __x) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__const__)) ;


__extension__ extern long long int llabs (long long int __x)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__const__)) ;






extern div_t div (int __numer, int __denom)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__const__)) ;
extern ldiv_t ldiv (long int __numer, long int __denom)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__const__)) ;


__extension__ extern lldiv_t lldiv (long long int __numer,
        long long int __denom)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__const__)) ;
extern char *ecvt (double __value, int __ndigit, int *__restrict __decpt,
     int *__restrict __sign) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (3, 4))) ;




extern char *fcvt (double __value, int __ndigit, int *__restrict __decpt,
     int *__restrict __sign) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (3, 4))) ;




extern char *gcvt (double __value, int __ndigit, char *__buf)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (3))) ;




extern char *qecvt (long double __value, int __ndigit,
      int *__restrict __decpt, int *__restrict __sign)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (3, 4))) ;
extern char *qfcvt (long double __value, int __ndigit,
      int *__restrict __decpt, int *__restrict __sign)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (3, 4))) ;
extern char *qgcvt (long double __value, int __ndigit, char *__buf)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (3))) ;




extern int ecvt_r (double __value, int __ndigit, int *__restrict __decpt,
     int *__restrict __sign, char *__restrict __buf,
     size_t __len) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (3, 4, 5)));
extern int fcvt_r (double __value, int __ndigit, int *__restrict __decpt,
     int *__restrict __sign, char *__restrict __buf,
     size_t __len) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (3, 4, 5)));

extern int qecvt_r (long double __value, int __ndigit,
      int *__restrict __decpt, int *__restrict __sign,
      char *__restrict __buf, size_t __len)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (3, 4, 5)));
extern int qfcvt_r (long double __value, int __ndigit,
      int *__restrict __decpt, int *__restrict __sign,
      char *__restrict __buf, size_t __len)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (3, 4, 5)));





extern int mblen (const char *__s, size_t __n) __attribute__ ((__nothrow__ , __leaf__));


extern int mbtowc (wchar_t *__restrict __pwc,
     const char *__restrict __s, size_t __n) __attribute__ ((__nothrow__ , __leaf__));


extern int wctomb (char *__s, wchar_t __wchar) __attribute__ ((__nothrow__ , __leaf__));



extern size_t mbstowcs (wchar_t *__restrict __pwcs,
   const char *__restrict __s, size_t __n) __attribute__ ((__nothrow__ , __leaf__))
    __attribute__ ((__access__ (__read_only__, 2)));

extern size_t wcstombs (char *__restrict __s,
   const wchar_t *__restrict __pwcs, size_t __n)
     __attribute__ ((__nothrow__ , __leaf__))
  __attribute__ ((__access__ (__write_only__, 1, 3)))
  __attribute__ ((__access__ (__read_only__, 2)));






extern int rpmatch (const char *__response) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1))) ;
extern int getsubopt (char **__restrict __optionp,
        char *const *__restrict __tokens,
        char **__restrict __valuep)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1, 2, 3))) ;
extern int getloadavg (double __loadavg[], int __nelem)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1)));







extern void *memcpy (void *__restrict __dest, const void *__restrict __src,
       size_t __n) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1, 2)));


extern void *memmove (void *__dest, const void *__src, size_t __n)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1, 2)));





extern void *memccpy (void *__restrict __dest, const void *__restrict __src,
        int __c, size_t __n)
    __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1, 2))) __attribute__ ((__access__ (__write_only__, 1, 4)));




extern void *memset (void *__s, int __c, size_t __n) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1)));


extern int memcmp (const void *__s1, const void *__s2, size_t __n)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__pure__)) __attribute__ ((__nonnull__ (1, 2)));
extern int __memcmpeq (const void *__s1, const void *__s2, size_t __n)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__pure__)) __attribute__ ((__nonnull__ (1, 2)));
extern void *memchr (const void *__s, int __c, size_t __n)
      __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__pure__)) __attribute__ ((__nonnull__ (1)));
extern char *strcpy (char *__restrict __dest, const char *__restrict __src)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1, 2)));

extern char *strncpy (char *__restrict __dest,
        const char *__restrict __src, size_t __n)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1, 2)));


extern char *strcat (char *__restrict __dest, const char *__restrict __src)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1, 2)));

extern char *strncat (char *__restrict __dest, const char *__restrict __src,
        size_t __n) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1, 2)));


extern int strcmp (const char *__s1, const char *__s2)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__pure__)) __attribute__ ((__nonnull__ (1, 2)));

extern int strncmp (const char *__s1, const char *__s2, size_t __n)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__pure__)) __attribute__ ((__nonnull__ (1, 2)));


extern int strcoll (const char *__s1, const char *__s2)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__pure__)) __attribute__ ((__nonnull__ (1, 2)));

extern size_t strxfrm (char *__restrict __dest,
         const char *__restrict __src, size_t __n)
    __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (2))) __attribute__ ((__access__ (__write_only__, 1, 3)));



struct __locale_struct
{

  struct __locale_data *__locales[13];


  const unsigned short int *__ctype_b;
  const int *__ctype_tolower;
  const int *__ctype_toupper;


  const char *__names[13];
};

typedef struct __locale_struct *__locale_t;

typedef __locale_t locale_t;


extern int strcoll_l (const char *__s1, const char *__s2, locale_t __l)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__pure__)) __attribute__ ((__nonnull__ (1, 2, 3)));


extern size_t strxfrm_l (char *__dest, const char *__src, size_t __n,
    locale_t __l) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (2, 4)))
     __attribute__ ((__access__ (__write_only__, 1, 3)));





extern char *strdup (const char *__s)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__malloc__)) __attribute__ ((__nonnull__ (1)));






extern char *strndup (const char *__string, size_t __n)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__malloc__)) __attribute__ ((__nonnull__ (1)));
extern char *strchr (const char *__s, int __c)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__pure__)) __attribute__ ((__nonnull__ (1)));
extern char *strrchr (const char *__s, int __c)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__pure__)) __attribute__ ((__nonnull__ (1)));
extern char *strchrnul (const char *__s, int __c)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__pure__)) __attribute__ ((__nonnull__ (1)));





extern size_t strcspn (const char *__s, const char *__reject)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__pure__)) __attribute__ ((__nonnull__ (1, 2)));


extern size_t strspn (const char *__s, const char *__accept)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__pure__)) __attribute__ ((__nonnull__ (1, 2)));
extern char *strpbrk (const char *__s, const char *__accept)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__pure__)) __attribute__ ((__nonnull__ (1, 2)));
extern char *strstr (const char *__haystack, const char *__needle)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__pure__)) __attribute__ ((__nonnull__ (1, 2)));




extern char *strtok (char *__restrict __s, const char *__restrict __delim)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (2)));



extern char *__strtok_r (char *__restrict __s,
    const char *__restrict __delim,
    char **__restrict __save_ptr)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (2, 3)));

extern char *strtok_r (char *__restrict __s, const char *__restrict __delim,
         char **__restrict __save_ptr)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (2, 3)));
extern char *strcasestr (const char *__haystack, const char *__needle)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__pure__)) __attribute__ ((__nonnull__ (1, 2)));







extern void *memmem (const void *__haystack, size_t __haystacklen,
       const void *__needle, size_t __needlelen)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__pure__)) __attribute__ ((__nonnull__ (1, 3)))
    __attribute__ ((__access__ (__read_only__, 1, 2)))
    __attribute__ ((__access__ (__read_only__, 3, 4)));



extern void *__mempcpy (void *__restrict __dest,
   const void *__restrict __src, size_t __n)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1, 2)));
extern void *mempcpy (void *__restrict __dest,
        const void *__restrict __src, size_t __n)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1, 2)));




extern size_t strlen (const char *__s)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__pure__)) __attribute__ ((__nonnull__ (1)));




extern size_t strnlen (const char *__string, size_t __maxlen)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__pure__)) __attribute__ ((__nonnull__ (1)));




extern char *strerror (int __errnum) __attribute__ ((__nothrow__ , __leaf__));
extern int strerror_r (int __errnum, char *__buf, size_t __buflen) __asm__ ("" "__xpg_strerror_r") __attribute__ ((__nothrow__ , __leaf__))

                        __attribute__ ((__nonnull__ (2)))
    __attribute__ ((__access__ (__write_only__, 2, 3)));
extern char *strerror_l (int __errnum, locale_t __l) __attribute__ ((__nothrow__ , __leaf__));













extern int bcmp (const void *__s1, const void *__s2, size_t __n)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__pure__)) __attribute__ ((__nonnull__ (1, 2)));


extern void bcopy (const void *__src, void *__dest, size_t __n)
  __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1, 2)));


extern void bzero (void *__s, size_t __n) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1)));
extern char *index (const char *__s, int __c)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__pure__)) __attribute__ ((__nonnull__ (1)));
extern char *rindex (const char *__s, int __c)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__pure__)) __attribute__ ((__nonnull__ (1)));






extern int ffs (int __i) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__const__));





extern int ffsl (long int __l) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__const__));
__extension__ extern int ffsll (long long int __ll)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__const__));



extern int strcasecmp (const char *__s1, const char *__s2)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__pure__)) __attribute__ ((__nonnull__ (1, 2)));


extern int strncasecmp (const char *__s1, const char *__s2, size_t __n)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__pure__)) __attribute__ ((__nonnull__ (1, 2)));






extern int strcasecmp_l (const char *__s1, const char *__s2, locale_t __loc)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__pure__)) __attribute__ ((__nonnull__ (1, 2, 3)));



extern int strncasecmp_l (const char *__s1, const char *__s2,
     size_t __n, locale_t __loc)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__pure__)) __attribute__ ((__nonnull__ (1, 2, 4)));






extern void explicit_bzero (void *__s, size_t __n) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1)))
    __attribute__ ((__access__ (__write_only__, 1, 2)));



extern char *strsep (char **__restrict __stringp,
       const char *__restrict __delim)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1, 2)));




extern char *strsignal (int __sig) __attribute__ ((__nothrow__ , __leaf__));
extern char *__stpcpy (char *__restrict __dest, const char *__restrict __src)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1, 2)));
extern char *stpcpy (char *__restrict __dest, const char *__restrict __src)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1, 2)));



extern char *__stpncpy (char *__restrict __dest,
   const char *__restrict __src, size_t __n)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1, 2)));
extern char *stpncpy (char *__restrict __dest,
        const char *__restrict __src, size_t __n)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1, 2)));




extern size_t strlcpy (char *__restrict __dest,
         const char *__restrict __src, size_t __n)
  __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1, 2))) __attribute__ ((__access__ (__write_only__, 1, 3)));



extern size_t strlcat (char *__restrict __dest,
         const char *__restrict __src, size_t __n)
  __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1, 2))) __attribute__ ((__access__ (__read_write__, 1, 3)));






typedef unsigned char mz_uint8;
typedef int16_t mz_int16;
typedef uint16_t mz_uint16;
typedef uint32_t mz_uint32;
typedef uint32_t mz_uint;
typedef int64_t mz_int64;
typedef uint64_t mz_uint64;
typedef int mz_bool;








typedef __builtin_va_list __gnuc_va_list;






typedef struct
{
  int __count;
  union
  {
    unsigned int __wch;
    char __wchb[4];
  } __value;
} __mbstate_t;




typedef struct _G_fpos_t
{
  __off_t __pos;
  __mbstate_t __state;
} __fpos_t;
typedef struct _G_fpos64_t
{
  __off64_t __pos;
  __mbstate_t __state;
} __fpos64_t;



struct _IO_FILE;
typedef struct _IO_FILE __FILE;



struct _IO_FILE;


typedef struct _IO_FILE FILE;
struct _IO_FILE;
struct _IO_marker;
struct _IO_codecvt;
struct _IO_wide_data;




typedef void _IO_lock_t;





struct _IO_FILE
{
  int _flags;


  char *_IO_read_ptr;
  char *_IO_read_end;
  char *_IO_read_base;
  char *_IO_write_base;
  char *_IO_write_ptr;
  char *_IO_write_end;
  char *_IO_buf_base;
  char *_IO_buf_end;


  char *_IO_save_base;
  char *_IO_backup_base;
  char *_IO_save_end;

  struct _IO_marker *_markers;

  struct _IO_FILE *_chain;

  int _fileno;
  int _flags2;
  __off_t _old_offset;


  unsigned short _cur_column;
  signed char _vtable_offset;
  char _shortbuf[1];

  _IO_lock_t *_lock;







  __off64_t _offset;

  struct _IO_codecvt *_codecvt;
  struct _IO_wide_data *_wide_data;
  struct _IO_FILE *_freeres_list;
  void *_freeres_buf;
  size_t __pad5;
  int _mode;

  char _unused2[15 * sizeof (int) - 4 * sizeof (void *) - sizeof (size_t)];
};


typedef __ssize_t cookie_read_function_t (void *__cookie, char *__buf,
                                          size_t __nbytes);







typedef __ssize_t cookie_write_function_t (void *__cookie, const char *__buf,
                                           size_t __nbytes);







typedef int cookie_seek_function_t (void *__cookie, __off64_t *__pos, int __w);


typedef int cookie_close_function_t (void *__cookie);






typedef struct _IO_cookie_io_functions_t
{
  cookie_read_function_t *read;
  cookie_write_function_t *write;
  cookie_seek_function_t *seek;
  cookie_close_function_t *close;
} cookie_io_functions_t;





typedef __gnuc_va_list va_list;
typedef __fpos_t fpos_t;
extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;






extern int remove (const char *__filename) __attribute__ ((__nothrow__ , __leaf__));

extern int rename (const char *__old, const char *__new) __attribute__ ((__nothrow__ , __leaf__));



extern int renameat (int __oldfd, const char *__old, int __newfd,
       const char *__new) __attribute__ ((__nothrow__ , __leaf__));
extern int fclose (FILE *__stream) __attribute__ ((__nonnull__ (1)));
extern FILE *tmpfile (void)
  __attribute__ ((__malloc__)) __attribute__ ((__malloc__ (fclose, 1))) ;
extern char *tmpnam (char[20]) __attribute__ ((__nothrow__ , __leaf__)) ;




extern char *tmpnam_r (char __s[20]) __attribute__ ((__nothrow__ , __leaf__)) ;
extern char *tempnam (const char *__dir, const char *__pfx)
   __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__malloc__)) __attribute__ ((__malloc__ (__builtin_free, 1)));






extern int fflush (FILE *__stream);
extern int fflush_unlocked (FILE *__stream);
extern FILE *fopen (const char *__restrict __filename,
      const char *__restrict __modes)
  __attribute__ ((__malloc__)) __attribute__ ((__malloc__ (fclose, 1))) ;




extern FILE *freopen (const char *__restrict __filename,
        const char *__restrict __modes,
        FILE *__restrict __stream) __attribute__ ((__nonnull__ (3)));
extern FILE *fdopen (int __fd, const char *__modes) __attribute__ ((__nothrow__ , __leaf__))
  __attribute__ ((__malloc__)) __attribute__ ((__malloc__ (fclose, 1))) ;





extern FILE *fopencookie (void *__restrict __magic_cookie,
     const char *__restrict __modes,
     cookie_io_functions_t __io_funcs) __attribute__ ((__nothrow__ , __leaf__))
  __attribute__ ((__malloc__)) __attribute__ ((__malloc__ (fclose, 1))) ;




extern FILE *fmemopen (void *__s, size_t __len, const char *__modes)
  __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__malloc__)) __attribute__ ((__malloc__ (fclose, 1))) ;




extern FILE *open_memstream (char **__bufloc, size_t *__sizeloc) __attribute__ ((__nothrow__ , __leaf__))
  __attribute__ ((__malloc__)) __attribute__ ((__malloc__ (fclose, 1))) ;
extern void setbuf (FILE *__restrict __stream, char *__restrict __buf) __attribute__ ((__nothrow__ , __leaf__))
  __attribute__ ((__nonnull__ (1)));



extern int setvbuf (FILE *__restrict __stream, char *__restrict __buf,
      int __modes, size_t __n) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1)));




extern void setbuffer (FILE *__restrict __stream, char *__restrict __buf,
         size_t __size) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1)));


extern void setlinebuf (FILE *__stream) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1)));







extern int fprintf (FILE *__restrict __stream,
      const char *__restrict __format, ...) __attribute__ ((__nonnull__ (1)));




extern int printf (const char *__restrict __format, ...);

extern int sprintf (char *__restrict __s,
      const char *__restrict __format, ...) __attribute__ ((__nothrow__));





extern int vfprintf (FILE *__restrict __s, const char *__restrict __format,
       __gnuc_va_list __arg) __attribute__ ((__nonnull__ (1)));




extern int vprintf (const char *__restrict __format, __gnuc_va_list __arg);

extern int vsprintf (char *__restrict __s, const char *__restrict __format,
       __gnuc_va_list __arg) __attribute__ ((__nothrow__));



extern int snprintf (char *__restrict __s, size_t __maxlen,
       const char *__restrict __format, ...)
     __attribute__ ((__nothrow__)) __attribute__ ((__format__ (__printf__, 3, 4)));

extern int vsnprintf (char *__restrict __s, size_t __maxlen,
        const char *__restrict __format, __gnuc_va_list __arg)
     __attribute__ ((__nothrow__)) __attribute__ ((__format__ (__printf__, 3, 0)));





extern int vasprintf (char **__restrict __ptr, const char *__restrict __f,
        __gnuc_va_list __arg)
     __attribute__ ((__nothrow__)) __attribute__ ((__format__ (__printf__, 2, 0))) ;
extern int __asprintf (char **__restrict __ptr,
         const char *__restrict __fmt, ...)
     __attribute__ ((__nothrow__)) __attribute__ ((__format__ (__printf__, 2, 3))) ;
extern int asprintf (char **__restrict __ptr,
       const char *__restrict __fmt, ...)
     __attribute__ ((__nothrow__)) __attribute__ ((__format__ (__printf__, 2, 3))) ;




extern int vdprintf (int __fd, const char *__restrict __fmt,
       __gnuc_va_list __arg)
     __attribute__ ((__format__ (__printf__, 2, 0)));
extern int dprintf (int __fd, const char *__restrict __fmt, ...)
     __attribute__ ((__format__ (__printf__, 2, 3)));







extern int fscanf (FILE *__restrict __stream,
     const char *__restrict __format, ...) __attribute__ ((__nonnull__ (1)));




extern int scanf (const char *__restrict __format, ...) ;

extern int sscanf (const char *__restrict __s,
     const char *__restrict __format, ...) __attribute__ ((__nothrow__ , __leaf__));
extern int fscanf (FILE *__restrict __stream, const char *__restrict __format, ...) __asm__ ("" "__isoc99_fscanf")

                                __attribute__ ((__nonnull__ (1)));
extern int scanf (const char *__restrict __format, ...) __asm__ ("" "__isoc99_scanf")
                              ;
extern int sscanf (const char *__restrict __s, const char *__restrict __format, ...) __asm__ ("" "__isoc99_sscanf") __attribute__ ((__nothrow__ , __leaf__))

                      ;
extern int vfscanf (FILE *__restrict __s, const char *__restrict __format,
      __gnuc_va_list __arg)
     __attribute__ ((__format__ (__scanf__, 2, 0))) __attribute__ ((__nonnull__ (1)));





extern int vscanf (const char *__restrict __format, __gnuc_va_list __arg)
     __attribute__ ((__format__ (__scanf__, 1, 0))) ;


extern int vsscanf (const char *__restrict __s,
      const char *__restrict __format, __gnuc_va_list __arg)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__format__ (__scanf__, 2, 0)));
extern int vfscanf (FILE *__restrict __s, const char *__restrict __format, __gnuc_va_list __arg) __asm__ ("" "__isoc99_vfscanf")



     __attribute__ ((__format__ (__scanf__, 2, 0))) __attribute__ ((__nonnull__ (1)));
extern int vscanf (const char *__restrict __format, __gnuc_va_list __arg) __asm__ ("" "__isoc99_vscanf")

     __attribute__ ((__format__ (__scanf__, 1, 0))) ;
extern int vsscanf (const char *__restrict __s, const char *__restrict __format, __gnuc_va_list __arg) __asm__ ("" "__isoc99_vsscanf") __attribute__ ((__nothrow__ , __leaf__))



     __attribute__ ((__format__ (__scanf__, 2, 0)));
extern int fgetc (FILE *__stream) __attribute__ ((__nonnull__ (1)));
extern int getc (FILE *__stream) __attribute__ ((__nonnull__ (1)));





extern int getchar (void);






extern int getc_unlocked (FILE *__stream) __attribute__ ((__nonnull__ (1)));
extern int getchar_unlocked (void);
extern int fgetc_unlocked (FILE *__stream) __attribute__ ((__nonnull__ (1)));
extern int fputc (int __c, FILE *__stream) __attribute__ ((__nonnull__ (2)));
extern int putc (int __c, FILE *__stream) __attribute__ ((__nonnull__ (2)));





extern int putchar (int __c);
extern int fputc_unlocked (int __c, FILE *__stream) __attribute__ ((__nonnull__ (2)));







extern int putc_unlocked (int __c, FILE *__stream) __attribute__ ((__nonnull__ (2)));
extern int putchar_unlocked (int __c);






extern int getw (FILE *__stream) __attribute__ ((__nonnull__ (1)));


extern int putw (int __w, FILE *__stream) __attribute__ ((__nonnull__ (2)));







extern char *fgets (char *__restrict __s, int __n, FILE *__restrict __stream)
     __attribute__ ((__access__ (__write_only__, 1, 2))) __attribute__ ((__nonnull__ (3)));
extern __ssize_t __getdelim (char **__restrict __lineptr,
                             size_t *__restrict __n, int __delimiter,
                             FILE *__restrict __stream) __attribute__ ((__nonnull__ (4)));
extern __ssize_t getdelim (char **__restrict __lineptr,
                           size_t *__restrict __n, int __delimiter,
                           FILE *__restrict __stream) __attribute__ ((__nonnull__ (4)));







extern __ssize_t getline (char **__restrict __lineptr,
                          size_t *__restrict __n,
                          FILE *__restrict __stream) __attribute__ ((__nonnull__ (3)));







extern int fputs (const char *__restrict __s, FILE *__restrict __stream)
  __attribute__ ((__nonnull__ (2)));





extern int puts (const char *__s);






extern int ungetc (int __c, FILE *__stream) __attribute__ ((__nonnull__ (2)));






extern size_t fread (void *__restrict __ptr, size_t __size,
       size_t __n, FILE *__restrict __stream)
  __attribute__ ((__nonnull__ (4)));




extern size_t fwrite (const void *__restrict __ptr, size_t __size,
        size_t __n, FILE *__restrict __s) __attribute__ ((__nonnull__ (4)));
extern size_t fread_unlocked (void *__restrict __ptr, size_t __size,
         size_t __n, FILE *__restrict __stream)
  __attribute__ ((__nonnull__ (4)));
extern size_t fwrite_unlocked (const void *__restrict __ptr, size_t __size,
          size_t __n, FILE *__restrict __stream)
  __attribute__ ((__nonnull__ (4)));







extern int fseek (FILE *__stream, long int __off, int __whence)
  __attribute__ ((__nonnull__ (1)));




extern long int ftell (FILE *__stream) __attribute__ ((__nonnull__ (1)));




extern void rewind (FILE *__stream) __attribute__ ((__nonnull__ (1)));
extern int fseeko (FILE *__stream, __off_t __off, int __whence)
  __attribute__ ((__nonnull__ (1)));




extern __off_t ftello (FILE *__stream) __attribute__ ((__nonnull__ (1)));
extern int fgetpos (FILE *__restrict __stream, fpos_t *__restrict __pos)
  __attribute__ ((__nonnull__ (1)));




extern int fsetpos (FILE *__stream, const fpos_t *__pos) __attribute__ ((__nonnull__ (1)));
extern void clearerr (FILE *__stream) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1)));

extern int feof (FILE *__stream) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1)));

extern int ferror (FILE *__stream) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1)));



extern void clearerr_unlocked (FILE *__stream) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1)));
extern int feof_unlocked (FILE *__stream) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1)));
extern int ferror_unlocked (FILE *__stream) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1)));







extern void perror (const char *__s) __attribute__ ((__cold__));




extern int fileno (FILE *__stream) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1)));




extern int fileno_unlocked (FILE *__stream) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1)));
extern int pclose (FILE *__stream) __attribute__ ((__nonnull__ (1)));





extern FILE *popen (const char *__command, const char *__modes)
  __attribute__ ((__malloc__)) __attribute__ ((__malloc__ (pclose, 1))) ;






extern char *ctermid (char *__s) __attribute__ ((__nothrow__ , __leaf__))
  __attribute__ ((__access__ (__write_only__, 1)));
extern void flockfile (FILE *__stream) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1)));



extern int ftrylockfile (FILE *__stream) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1)));


extern void funlockfile (FILE *__stream) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1)));
extern int __uflow (FILE *);
extern int __overflow (FILE *, int);

    
   extern void *miniz_def_alloc_func(void *opaque, size_t items, size_t size);
    extern void miniz_def_free_func(void *opaque, void *address);
    extern void *miniz_def_realloc_func(void *opaque, void *address, size_t items, size_t size);







        
    enum
    {
        TDEFL_HUFFMAN_ONLY = 0,
        TDEFL_DEFAULT_MAX_PROBES = 128,
        TDEFL_MAX_PROBES_MASK = 0xFFF
    };
    enum
    {
        TDEFL_WRITE_ZLIB_HEADER = 0x01000,
        TDEFL_COMPUTE_ADLER32 = 0x02000,
        TDEFL_GREEDY_PARSING_FLAG = 0x04000,
        TDEFL_NONDETERMINISTIC_PARSING_FLAG = 0x08000,
        TDEFL_RLE_MATCHES = 0x10000,
        TDEFL_FILTER_MATCHES = 0x20000,
        TDEFL_FORCE_ALL_STATIC_BLOCKS = 0x40000,
        TDEFL_FORCE_ALL_RAW_BLOCKS = 0x80000
    };
    void *tdefl_compress_mem_to_heap(const void *pSrc_buf, size_t src_buf_len, size_t *pOut_len, int flags);



    size_t tdefl_compress_mem_to_mem(void *pOut_buf, size_t out_buf_len, const void *pSrc_buf, size_t src_buf_len, int flags);
    void *tdefl_write_image_to_png_file_in_memory_ex(const void *pImage, int w, int h, int num_chans, size_t *pLen_out, mz_uint level, mz_bool flip);
    void *tdefl_write_image_to_png_file_in_memory(const void *pImage, int w, int h, int num_chans, size_t *pLen_out);


    typedef mz_bool (*tdefl_put_buf_func_ptr)(const void *pBuf, int len, void *pUser);


    mz_bool tdefl_compress_mem_to_output(const void *pBuf, size_t buf_len, tdefl_put_buf_func_ptr pPut_buf_func, void *pPut_buf_user, int flags);

    enum
    {
        TDEFL_MAX_HUFF_TABLES = 3,
        TDEFL_MAX_HUFF_SYMBOLS_0 = 288,
        TDEFL_MAX_HUFF_SYMBOLS_1 = 32,
        TDEFL_MAX_HUFF_SYMBOLS_2 = 19,
        TDEFL_LZ_DICT_SIZE = 32768,
        TDEFL_LZ_DICT_SIZE_MASK = TDEFL_LZ_DICT_SIZE - 1,
        TDEFL_MIN_MATCH_LEN = 3,
        TDEFL_MAX_MATCH_LEN = 258
    };
enum
{
    TDEFL_LZ_CODE_BUF_SIZE = 64 * 1024,
    TDEFL_OUT_BUF_SIZE = (mz_uint)((TDEFL_LZ_CODE_BUF_SIZE * 13) / 10),
    TDEFL_MAX_HUFF_SYMBOLS = 288,
    TDEFL_LZ_HASH_BITS = 15,
    TDEFL_LEVEL1_HASH_SIZE_MASK = 4095,
    TDEFL_LZ_HASH_SHIFT = (TDEFL_LZ_HASH_BITS + 2) / 3,
    TDEFL_LZ_HASH_SIZE = 1 << TDEFL_LZ_HASH_BITS
};



    typedef enum
    {
        TDEFL_STATUS_BAD_PARAM = -2,
        TDEFL_STATUS_PUT_BUF_FAILED = -1,
        TDEFL_STATUS_OKAY = 0,
        TDEFL_STATUS_DONE = 1
    } tdefl_status;


    typedef enum
    {
        TDEFL_NO_FLUSH = 0,
        TDEFL_SYNC_FLUSH = 2,
        TDEFL_FULL_FLUSH = 3,
        TDEFL_FINISH = 4
    } tdefl_flush;


    typedef struct
    {
        tdefl_put_buf_func_ptr m_pPut_buf_func;
        void *m_pPut_buf_user;
        mz_uint m_flags, m_max_probes[2];
        int m_greedy_parsing;
        mz_uint m_adler32, m_lookahead_pos, m_lookahead_size, m_dict_size;
        mz_uint8 *m_pLZ_code_buf, *m_pLZ_flags, *m_pOutput_buf, *m_pOutput_buf_end;
        mz_uint m_num_flags_left, m_total_lz_bytes, m_lz_code_buf_dict_pos, m_bits_in, m_bit_buffer;
        mz_uint m_saved_match_dist, m_saved_match_len, m_saved_lit, m_output_flush_ofs, m_output_flush_remaining, m_finished, m_block_index, m_wants_to_finish;
        tdefl_status m_prev_return_status;
        const void *m_pIn_buf;
        void *m_pOut_buf;
        size_t *m_pIn_buf_size, *m_pOut_buf_size;
        tdefl_flush m_flush;
        const mz_uint8 *m_pSrc;
        size_t m_src_buf_left, m_out_buf_ofs;
        mz_uint8 m_dict[TDEFL_LZ_DICT_SIZE + TDEFL_MAX_MATCH_LEN - 1];
        mz_uint16 m_huff_count[TDEFL_MAX_HUFF_TABLES][TDEFL_MAX_HUFF_SYMBOLS];
        mz_uint16 m_huff_codes[TDEFL_MAX_HUFF_TABLES][TDEFL_MAX_HUFF_SYMBOLS];
        mz_uint8 m_huff_code_sizes[TDEFL_MAX_HUFF_TABLES][TDEFL_MAX_HUFF_SYMBOLS];
        mz_uint8 m_lz_code_buf[TDEFL_LZ_CODE_BUF_SIZE];
        mz_uint16 m_next[TDEFL_LZ_DICT_SIZE];
        mz_uint16 m_hash[TDEFL_LZ_HASH_SIZE];
        mz_uint8 m_output_buf[TDEFL_OUT_BUF_SIZE];
    } tdefl_compressor;






    tdefl_status tdefl_init(tdefl_compressor *d, tdefl_put_buf_func_ptr pPut_buf_func, void *pPut_buf_user, int flags);


    tdefl_status tdefl_compress(tdefl_compressor *d, const void *pIn_buf, size_t *pIn_buf_size, void *pOut_buf, size_t *pOut_buf_size, tdefl_flush flush);



    tdefl_status tdefl_compress_buffer(tdefl_compressor *d, const void *pIn_buf, size_t in_buf_size, tdefl_flush flush);

    tdefl_status tdefl_get_prev_return_status(tdefl_compressor *d);
    mz_uint32 tdefl_get_adler32(tdefl_compressor *d);





    mz_uint tdefl_create_comp_flags_from_zip_params(int level, int window_bits, int strategy);





    tdefl_compressor *tdefl_compressor_alloc(void);
    void tdefl_compressor_free(tdefl_compressor *pComp);







        
    enum
    {
        TINFL_FLAG_PARSE_ZLIB_HEADER = 1,
        TINFL_FLAG_HAS_MORE_INPUT = 2,
        TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF = 4,
        TINFL_FLAG_COMPUTE_ADLER32 = 8
    };
    void *tinfl_decompress_mem_to_heap(const void *pSrc_buf, size_t src_buf_len, size_t *pOut_len, int flags);




    size_t tinfl_decompress_mem_to_mem(void *pOut_buf, size_t out_buf_len, const void *pSrc_buf, size_t src_buf_len, int flags);



    typedef int (*tinfl_put_buf_func_ptr)(const void *pBuf, int len, void *pUser);
    int tinfl_decompress_mem_to_callback(const void *pIn_buf, size_t *pIn_buf_size, tinfl_put_buf_func_ptr pPut_buf_func, void *pPut_buf_user, int flags);

    struct tinfl_decompressor_tag;
    typedef struct tinfl_decompressor_tag tinfl_decompressor;





    tinfl_decompressor *tinfl_decompressor_alloc(void);
    void tinfl_decompressor_free(tinfl_decompressor *pDecomp);






    typedef enum
    {



        TINFL_STATUS_FAILED_CANNOT_MAKE_PROGRESS = -4,


        TINFL_STATUS_BAD_PARAM = -3,


        TINFL_STATUS_ADLER32_MISMATCH = -2,


        TINFL_STATUS_FAILED = -1,





        TINFL_STATUS_DONE = 0,




        TINFL_STATUS_NEEDS_MORE_INPUT = 1,





        TINFL_STATUS_HAS_MORE_OUTPUT = 2
    } tinfl_status;
    tinfl_status tinfl_decompress(tinfl_decompressor *r, const mz_uint8 *pIn_buf_next, size_t *pIn_buf_size, mz_uint8 *pOut_buf_start, mz_uint8 *pOut_buf_next, size_t *pOut_buf_size, const mz_uint32 decomp_flags);


    enum
    {
        TINFL_MAX_HUFF_TABLES = 3,
        TINFL_MAX_HUFF_SYMBOLS_0 = 288,
        TINFL_MAX_HUFF_SYMBOLS_1 = 32,
        TINFL_MAX_HUFF_SYMBOLS_2 = 19,
        TINFL_FAST_LOOKUP_BITS = 10,
        TINFL_FAST_LOOKUP_SIZE = 1 << TINFL_FAST_LOOKUP_BITS
    };
    typedef mz_uint64 tinfl_bit_buf_t;






    struct tinfl_decompressor_tag
    {
        mz_uint32 m_state, m_num_bits, m_zhdr0, m_zhdr1, m_z_adler32, m_final, m_type, m_check_adler32, m_dist, m_counter, m_num_extra, m_table_sizes[TINFL_MAX_HUFF_TABLES];
        tinfl_bit_buf_t m_bit_buf;
        size_t m_dist_from_out_buf_start;
        mz_int16 m_look_up[TINFL_MAX_HUFF_TABLES][TINFL_FAST_LOOKUP_SIZE];
        mz_int16 m_tree_0[TINFL_MAX_HUFF_SYMBOLS_0 * 2];
        mz_int16 m_tree_1[TINFL_MAX_HUFF_SYMBOLS_1 * 2];
        mz_int16 m_tree_2[TINFL_MAX_HUFF_SYMBOLS_2 * 2];
        mz_uint8 m_code_size_0[TINFL_MAX_HUFF_SYMBOLS_0];
        mz_uint8 m_code_size_1[TINFL_MAX_HUFF_SYMBOLS_1];
        mz_uint8 m_code_size_2[TINFL_MAX_HUFF_SYMBOLS_2];
        mz_uint8 m_raw_header[4], m_len_codes[TINFL_MAX_HUFF_SYMBOLS_0 + TINFL_MAX_HUFF_SYMBOLS_1 + 137];
    };







       
typedef unsigned char mz_validate_uint16[sizeof(mz_uint16) == 2 ? 1 : -1];
typedef unsigned char mz_validate_uint32[sizeof(mz_uint32) == 4 ? 1 : -1];
typedef unsigned char mz_validate_uint64[sizeof(mz_uint64) == 8 ? 1 : -1];
    mz_ulong mz_adler32(mz_ulong adler, const unsigned char *ptr, size_t buf_len)
    {
        mz_uint32 i, s1 = (mz_uint32)(adler & 0xffff), s2 = (mz_uint32)(adler >> 16);
        size_t block_len = buf_len % 5552;
        if (!ptr)
            return (1);
        while (buf_len)
        {
            for (i = 0; i + 7 < block_len; i += 8, ptr += 8)
            {
                s1 += ptr[0], s2 += s1;
                s1 += ptr[1], s2 += s1;
                s1 += ptr[2], s2 += s1;
                s1 += ptr[3], s2 += s1;
                s1 += ptr[4], s2 += s1;
                s1 += ptr[5], s2 += s1;
                s1 += ptr[6], s2 += s1;
                s1 += ptr[7], s2 += s1;
            }
            for (; i < block_len; ++i)
                s1 += *ptr++, s2 += s1;
            s1 %= 65521U, s2 %= 65521U;
            buf_len -= block_len;
            block_len = 5552;
        }
        return (s2 << 16) + s1;
    }
mz_ulong mz_crc32(mz_ulong crc, const mz_uint8 *ptr, size_t buf_len)
{
    static const mz_uint32 s_crc_table[256] = {
        0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F, 0xE963A535,
        0x9E6495A3, 0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988, 0x09B64C2B, 0x7EB17CBD,
        0xE7B82D07, 0x90BF1D91, 0x1DB71064, 0x6AB020F2, 0xF3B97148, 0x84BE41DE, 0x1ADAD47D,
        0x6DDDE4EB, 0xF4D4B551, 0x83D385C7, 0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC,
        0x14015C4F, 0x63066CD9, 0xFA0F3D63, 0x8D080DF5, 0x3B6E20C8, 0x4C69105E, 0xD56041E4,
        0xA2677172, 0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B, 0x35B5A8FA, 0x42B2986C,
        0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59, 0x26D930AC,
        0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423, 0xCFBA9599, 0xB8BDA50F,
        0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924, 0x2F6F7C87, 0x58684C11, 0xC1611DAB,
        0xB6662D3D, 0x76DC4190, 0x01DB7106, 0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F,
        0x9FBFE4A5, 0xE8B8D433, 0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB,
        0x086D3D2D, 0x91646C97, 0xE6635C01, 0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
        0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457, 0x65B0D9C6, 0x12B7E950, 0x8BBEB8EA,
        0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65, 0x4DB26158, 0x3AB551CE,
        0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7, 0xA4D1C46D, 0xD3D6F4FB, 0x4369E96A,
        0x346ED9FC, 0xAD678846, 0xDA60B8D0, 0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9,
        0x5005713C, 0x270241AA, 0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409,
        0xCE61E49F, 0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81,
        0xB7BD5C3B, 0xC0BA6CAD, 0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A, 0xEAD54739,
        0x9DD277AF, 0x04DB2615, 0x73DC1683, 0xE3630B12, 0x94643B84, 0x0D6D6A3E, 0x7A6A5AA8,
        0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1, 0xF00F9344, 0x8708A3D2, 0x1E01F268,
        0x6906C2FE, 0xF762575D, 0x806567CB, 0x196C3671, 0x6E6B06E7, 0xFED41B76, 0x89D32BE0,
        0x10DA7A5A, 0x67DD4ACC, 0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5, 0xD6D6A3E8,
        0xA1D1937E, 0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
        0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55, 0x316E8EEF,
        0x4669BE79, 0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236, 0xCC0C7795, 0xBB0B4703,
        0x220216B9, 0x5505262F, 0xC5BA3BBE, 0xB2BD0B28, 0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7,
        0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D, 0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A,
        0x9C0906A9, 0xEB0E363F, 0x72076785, 0x05005713, 0x95BF4A82, 0xE2B87A14, 0x7BB12BAE,
        0x0CB61B38, 0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21, 0x86D3D2D4, 0xF1D4E242,
        0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777, 0x88085AE6,
        0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69, 0x616BFFD3, 0x166CCF45,
        0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2, 0xA7672661, 0xD06016F7, 0x4969474D,
        0x3E6E77DB, 0xAED16A4A, 0xD9D65ADC, 0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5,
        0x47B2CF7F, 0x30B5FFE9, 0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605,
        0xCDD70693, 0x54DE5729, 0x23D967BF, 0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
        0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
    };

    mz_uint32 crc32 = (mz_uint32)crc ^ 0xFFFFFFFF;
    const mz_uint8 *pByte_buf = (const mz_uint8 *)ptr;

    while (buf_len >= 4)
    {
        crc32 = (crc32 >> 8) ^ s_crc_table[(crc32 ^ pByte_buf[0]) & 0xFF];
        crc32 = (crc32 >> 8) ^ s_crc_table[(crc32 ^ pByte_buf[1]) & 0xFF];
        crc32 = (crc32 >> 8) ^ s_crc_table[(crc32 ^ pByte_buf[2]) & 0xFF];
        crc32 = (crc32 >> 8) ^ s_crc_table[(crc32 ^ pByte_buf[3]) & 0xFF];
        pByte_buf += 4;
        buf_len -= 4;
    }

    while (buf_len)
    {
        crc32 = (crc32 >> 8) ^ s_crc_table[(crc32 ^ pByte_buf[0]) & 0xFF];
        ++pByte_buf;
        --buf_len;
    }

    return ~crc32;
}


    void mz_free(void *p)
    {
        free(p);
    }

    void *miniz_def_alloc_func(void *opaque, size_t items, size_t size)
    {
        (void)opaque, (void)items, (void)size;
        return malloc(items * size);
    }
    void miniz_def_free_func(void *opaque, void *address)
    {
        (void)opaque, (void)address;
        free(address);
    }
    void *miniz_def_realloc_func(void *opaque, void *address, size_t items, size_t size)
    {
        (void)opaque, (void)address, (void)items, (void)size;
        return realloc(address, items * size);
    }

    const char *mz_version(void)
    {
        return "11.3.1";
    }





    int mz_deflateInit(mz_streamp pStream, int level)
    {
        return mz_deflateInit2(pStream, level, 8, 15, 9, MZ_DEFAULT_STRATEGY);
    }

    int mz_deflateInit2(mz_streamp pStream, int level, int method, int window_bits, int mem_level, int strategy)
    {
        tdefl_compressor *pComp;
        mz_uint comp_flags = TDEFL_COMPUTE_ADLER32 | tdefl_create_comp_flags_from_zip_params(level, window_bits, strategy);

        if (!pStream)
            return MZ_STREAM_ERROR;
        if ((method != 8) || ((mem_level < 1) || (mem_level > 9)) || ((window_bits != 15) && (-window_bits != 15)))
            return MZ_PARAM_ERROR;

        pStream->data_type = 0;
        pStream->adler = (1);
        pStream->msg = 
                      ((void *)0)
                          ;
        pStream->reserved = 0;
        pStream->total_in = 0;
        pStream->total_out = 0;
        if (!pStream->zalloc)
            pStream->zalloc = miniz_def_alloc_func;
        if (!pStream->zfree)
            pStream->zfree = miniz_def_free_func;

        pComp = (tdefl_compressor *)pStream->zalloc(pStream->opaque, 1, sizeof(tdefl_compressor));
        if (!pComp)
            return MZ_MEM_ERROR;

        pStream->state = (struct mz_internal_state *)pComp;

        if (tdefl_init(pComp, 
                             ((void *)0)
                                 , 
                                   ((void *)0)
                                       , comp_flags) != TDEFL_STATUS_OKAY)
        {
            mz_deflateEnd(pStream);
            return MZ_PARAM_ERROR;
        }

        return MZ_OK;
    }

    int mz_deflateReset(mz_streamp pStream)
    {
        if ((!pStream) || (!pStream->state) || (!pStream->zalloc) || (!pStream->zfree))
            return MZ_STREAM_ERROR;
        pStream->total_in = pStream->total_out = 0;
        tdefl_init((tdefl_compressor *)pStream->state, 
                                                      ((void *)0)
                                                          , 
                                                            ((void *)0)
                                                                , ((tdefl_compressor *)pStream->state)->m_flags);
        return MZ_OK;
    }

    int mz_deflate(mz_streamp pStream, int flush)
    {
        size_t in_bytes, out_bytes;
        mz_ulong orig_total_in, orig_total_out;
        int mz_status = MZ_OK;

        if ((!pStream) || (!pStream->state) || (flush < 0) || (flush > MZ_FINISH) || (!pStream->next_out))
            return MZ_STREAM_ERROR;
        if (!pStream->avail_out)
            return MZ_BUF_ERROR;

        if (flush == MZ_PARTIAL_FLUSH)
            flush = MZ_SYNC_FLUSH;

        if (((tdefl_compressor *)pStream->state)->m_prev_return_status == TDEFL_STATUS_DONE)
            return (flush == MZ_FINISH) ? MZ_STREAM_END : MZ_BUF_ERROR;

        orig_total_in = pStream->total_in;
        orig_total_out = pStream->total_out;
        for (;;)
        {
            tdefl_status defl_status;
            in_bytes = pStream->avail_in;
            out_bytes = pStream->avail_out;

            defl_status = tdefl_compress((tdefl_compressor *)pStream->state, pStream->next_in, &in_bytes, pStream->next_out, &out_bytes, (tdefl_flush)flush);
            pStream->next_in += (mz_uint)in_bytes;
            pStream->avail_in -= (mz_uint)in_bytes;
            pStream->total_in += (mz_uint)in_bytes;
            pStream->adler = tdefl_get_adler32((tdefl_compressor *)pStream->state);

            pStream->next_out += (mz_uint)out_bytes;
            pStream->avail_out -= (mz_uint)out_bytes;
            pStream->total_out += (mz_uint)out_bytes;

            if (defl_status < 0)
            {
                mz_status = MZ_STREAM_ERROR;
                break;
            }
            else if (defl_status == TDEFL_STATUS_DONE)
            {
                mz_status = MZ_STREAM_END;
                break;
            }
            else if (!pStream->avail_out)
                break;
            else if ((!pStream->avail_in) && (flush != MZ_FINISH))
            {
                if ((flush) || (pStream->total_in != orig_total_in) || (pStream->total_out != orig_total_out))
                    break;
                return MZ_BUF_ERROR;

            }
        }
        return mz_status;
    }

    int mz_deflateEnd(mz_streamp pStream)
    {
        if (!pStream)
            return MZ_STREAM_ERROR;
        if (pStream->state)
        {
            pStream->zfree(pStream->opaque, pStream->state);
            pStream->state = 
                            ((void *)0)
                                ;
        }
        return MZ_OK;
    }

    mz_ulong mz_deflateBound(mz_streamp pStream, mz_ulong source_len)
    {
        (void)pStream;

        return (((128 + (source_len * 110) / 100) > (128 + source_len + ((source_len / (31 * 1024)) + 1) * 5)) ? (128 + (source_len * 110) / 100) : (128 + source_len + ((source_len / (31 * 1024)) + 1) * 5));
    }

    int mz_compress2(unsigned char *pDest, mz_ulong *pDest_len, const unsigned char *pSource, mz_ulong source_len, int level)
    {
        int status;
        mz_stream stream;
        memset(&stream, 0, sizeof(stream));


        if ((mz_uint64)(source_len | *pDest_len) > 0xFFFFFFFFU)
            return MZ_PARAM_ERROR;

        stream.next_in = pSource;
        stream.avail_in = (mz_uint32)source_len;
        stream.next_out = pDest;
        stream.avail_out = (mz_uint32)*pDest_len;

        status = mz_deflateInit(&stream, level);
        if (status != MZ_OK)
            return status;

        status = mz_deflate(&stream, MZ_FINISH);
        if (status != MZ_STREAM_END)
        {
            mz_deflateEnd(&stream);
            return (status == MZ_OK) ? MZ_BUF_ERROR : status;
        }

        *pDest_len = stream.total_out;
        return mz_deflateEnd(&stream);
    }

    int mz_compress(unsigned char *pDest, mz_ulong *pDest_len, const unsigned char *pSource, mz_ulong source_len)
    {
        return mz_compress2(pDest, pDest_len, pSource, source_len, MZ_DEFAULT_COMPRESSION);
    }

    mz_ulong mz_compressBound(mz_ulong source_len)
    {
        return mz_deflateBound(
                              ((void *)0)
                                  , source_len);
    }





    typedef struct
    {
        tinfl_decompressor m_decomp;
        mz_uint m_dict_ofs, m_dict_avail, m_first_call, m_has_flushed;
        int m_window_bits;
        mz_uint8 m_dict[32768];
        tinfl_status m_last_status;
    } inflate_state;

    int mz_inflateInit2(mz_streamp pStream, int window_bits)
    {
        inflate_state *pDecomp;
        if (!pStream)
            return MZ_STREAM_ERROR;
        if ((window_bits != 15) && (-window_bits != 15))
            return MZ_PARAM_ERROR;

        pStream->data_type = 0;
        pStream->adler = 0;
        pStream->msg = 
                      ((void *)0)
                          ;
        pStream->total_in = 0;
        pStream->total_out = 0;
        pStream->reserved = 0;
        if (!pStream->zalloc)
            pStream->zalloc = miniz_def_alloc_func;
        if (!pStream->zfree)
            pStream->zfree = miniz_def_free_func;

        pDecomp = (inflate_state *)pStream->zalloc(pStream->opaque, 1, sizeof(inflate_state));
        if (!pDecomp)
            return MZ_MEM_ERROR;

        pStream->state = (struct mz_internal_state *)pDecomp;

        do { (&pDecomp->m_decomp)->m_state = 0; } while (0);
        pDecomp->m_dict_ofs = 0;
        pDecomp->m_dict_avail = 0;
        pDecomp->m_last_status = TINFL_STATUS_NEEDS_MORE_INPUT;
        pDecomp->m_first_call = 1;
        pDecomp->m_has_flushed = 0;
        pDecomp->m_window_bits = window_bits;

        return MZ_OK;
    }

    int mz_inflateInit(mz_streamp pStream)
    {
        return mz_inflateInit2(pStream, 15);
    }

    int mz_inflateReset(mz_streamp pStream)
    {
        inflate_state *pDecomp;
        if (!pStream)
            return MZ_STREAM_ERROR;

        pStream->data_type = 0;
        pStream->adler = 0;
        pStream->msg = 
                      ((void *)0)
                          ;
        pStream->total_in = 0;
        pStream->total_out = 0;
        pStream->reserved = 0;

        pDecomp = (inflate_state *)pStream->state;

        do { (&pDecomp->m_decomp)->m_state = 0; } while (0);
        pDecomp->m_dict_ofs = 0;
        pDecomp->m_dict_avail = 0;
        pDecomp->m_last_status = TINFL_STATUS_NEEDS_MORE_INPUT;
        pDecomp->m_first_call = 1;
        pDecomp->m_has_flushed = 0;
                                                  ;

        return MZ_OK;
    }

    int mz_inflate(mz_streamp pStream, int flush)
    {
        inflate_state *pState;
        mz_uint n, first_call, decomp_flags = TINFL_FLAG_COMPUTE_ADLER32;
        size_t in_bytes, out_bytes, orig_avail_in;
        tinfl_status status;

        if ((!pStream) || (!pStream->state))
            return MZ_STREAM_ERROR;
        if (flush == MZ_PARTIAL_FLUSH)
            flush = MZ_SYNC_FLUSH;
        if ((flush) && (flush != MZ_SYNC_FLUSH) && (flush != MZ_FINISH))
            return MZ_STREAM_ERROR;

        pState = (inflate_state *)pStream->state;
        if (pState->m_window_bits > 0)
            decomp_flags |= TINFL_FLAG_PARSE_ZLIB_HEADER;
        orig_avail_in = pStream->avail_in;

        first_call = pState->m_first_call;
        pState->m_first_call = 0;
        if (pState->m_last_status < 0)
            return MZ_DATA_ERROR;

        if (pState->m_has_flushed && (flush != MZ_FINISH))
            return MZ_STREAM_ERROR;
        pState->m_has_flushed |= (flush == MZ_FINISH);

        if ((flush == MZ_FINISH) && (first_call))
        {

            decomp_flags |= TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF;
            in_bytes = pStream->avail_in;
            out_bytes = pStream->avail_out;
            status = tinfl_decompress(&pState->m_decomp, pStream->next_in, &in_bytes, pStream->next_out, pStream->next_out, &out_bytes, decomp_flags);
            pState->m_last_status = status;
            pStream->next_in += (mz_uint)in_bytes;
            pStream->avail_in -= (mz_uint)in_bytes;
            pStream->total_in += (mz_uint)in_bytes;
            pStream->adler = (&pState->m_decomp)->m_check_adler32;
            pStream->next_out += (mz_uint)out_bytes;
            pStream->avail_out -= (mz_uint)out_bytes;
            pStream->total_out += (mz_uint)out_bytes;

            if (status < 0)
                return MZ_DATA_ERROR;
            else if (status != TINFL_STATUS_DONE)
            {
                pState->m_last_status = TINFL_STATUS_FAILED;
                return MZ_BUF_ERROR;
            }
            return MZ_STREAM_END;
        }

        if (flush != MZ_FINISH)
            decomp_flags |= TINFL_FLAG_HAS_MORE_INPUT;

        if (pState->m_dict_avail)
        {
            n = (((pState->m_dict_avail) < (pStream->avail_out)) ? (pState->m_dict_avail) : (pStream->avail_out));
            memcpy(pStream->next_out, pState->m_dict + pState->m_dict_ofs, n);
            pStream->next_out += n;
            pStream->avail_out -= n;
            pStream->total_out += n;
            pState->m_dict_avail -= n;
            pState->m_dict_ofs = (pState->m_dict_ofs + n) & (32768 - 1);
            return ((pState->m_last_status == TINFL_STATUS_DONE) && (!pState->m_dict_avail)) ? MZ_STREAM_END : MZ_OK;
        }

        for (;;)
        {
            in_bytes = pStream->avail_in;
            out_bytes = 32768 - pState->m_dict_ofs;

            status = tinfl_decompress(&pState->m_decomp, pStream->next_in, &in_bytes, pState->m_dict, pState->m_dict + pState->m_dict_ofs, &out_bytes, decomp_flags);
            pState->m_last_status = status;

            pStream->next_in += (mz_uint)in_bytes;
            pStream->avail_in -= (mz_uint)in_bytes;
            pStream->total_in += (mz_uint)in_bytes;
            pStream->adler = (&pState->m_decomp)->m_check_adler32;

            pState->m_dict_avail = (mz_uint)out_bytes;

            n = (((pState->m_dict_avail) < (pStream->avail_out)) ? (pState->m_dict_avail) : (pStream->avail_out));
            memcpy(pStream->next_out, pState->m_dict + pState->m_dict_ofs, n);
            pStream->next_out += n;
            pStream->avail_out -= n;
            pStream->total_out += n;
            pState->m_dict_avail -= n;
            pState->m_dict_ofs = (pState->m_dict_ofs + n) & (32768 - 1);

            if (status < 0)
                return MZ_DATA_ERROR;
            else if ((status == TINFL_STATUS_NEEDS_MORE_INPUT) && (!orig_avail_in))
                return MZ_BUF_ERROR;
            else if (flush == MZ_FINISH)
            {

                if (status == TINFL_STATUS_DONE)
                    return pState->m_dict_avail ? MZ_BUF_ERROR : MZ_STREAM_END;

                else if (!pStream->avail_out)
                    return MZ_BUF_ERROR;
            }
            else if ((status == TINFL_STATUS_DONE) || (!pStream->avail_in) || (!pStream->avail_out) || (pState->m_dict_avail))
                break;
        }

        return ((status == TINFL_STATUS_DONE) && (!pState->m_dict_avail)) ? MZ_STREAM_END : MZ_OK;
    }

    int mz_inflateEnd(mz_streamp pStream)
    {
        if (!pStream)
            return MZ_STREAM_ERROR;
        if (pStream->state)
        {
            pStream->zfree(pStream->opaque, pStream->state);
            pStream->state = 
                            ((void *)0)
                                ;
        }
        return MZ_OK;
    }
    int mz_uncompress2(unsigned char *pDest, mz_ulong *pDest_len, const unsigned char *pSource, mz_ulong *pSource_len)
    {
        mz_stream stream;
        int status;
        memset(&stream, 0, sizeof(stream));


        if ((mz_uint64)(*pSource_len | *pDest_len) > 0xFFFFFFFFU)
            return MZ_PARAM_ERROR;

        stream.next_in = pSource;
        stream.avail_in = (mz_uint32)*pSource_len;
        stream.next_out = pDest;
        stream.avail_out = (mz_uint32)*pDest_len;

        status = mz_inflateInit(&stream);
        if (status != MZ_OK)
            return status;

        status = mz_inflate(&stream, MZ_FINISH);
        *pSource_len = *pSource_len - stream.avail_in;
        if (status != MZ_STREAM_END)
        {
            mz_inflateEnd(&stream);
            return ((status == MZ_BUF_ERROR) && (!stream.avail_in)) ? MZ_DATA_ERROR : status;
        }
        *pDest_len = stream.total_out;

        return mz_inflateEnd(&stream);
    }

    int mz_uncompress(unsigned char *pDest, mz_ulong *pDest_len, const unsigned char *pSource, mz_ulong source_len)
    {
        return mz_uncompress2(pDest, pDest_len, pSource, &source_len);
    }



    const char *mz_error(int err)
    {
        static struct
        {
            int m_err;
            const char *m_pDesc;
        } s_error_descs[] = {
            { MZ_OK, "" }, { MZ_STREAM_END, "stream end" }, { MZ_NEED_DICT, "need dictionary" }, { MZ_ERRNO, "file error" }, { MZ_STREAM_ERROR, "stream error" }, { MZ_DATA_ERROR, "data error" }, { MZ_MEM_ERROR, "out of memory" }, { MZ_BUF_ERROR, "buf error" }, { MZ_VERSION_ERROR, "version error" }, { MZ_PARAM_ERROR, "parameter error" }
        };
        mz_uint i;
        for (i = 0; i < sizeof(s_error_descs) / sizeof(s_error_descs[0]); ++i)
            if (s_error_descs[i].m_err == err)
                return s_error_descs[i].m_pDesc;
        return 
              ((void *)0)
                  ;
    }
    static const mz_uint16 s_tdefl_len_sym[256] = {
        257, 258, 259, 260, 261, 262, 263, 264, 265, 265, 266, 266, 267, 267, 268, 268, 269, 269, 269, 269, 270, 270, 270, 270, 271, 271, 271, 271, 272, 272, 272, 272,
        273, 273, 273, 273, 273, 273, 273, 273, 274, 274, 274, 274, 274, 274, 274, 274, 275, 275, 275, 275, 275, 275, 275, 275, 276, 276, 276, 276, 276, 276, 276, 276,
        277, 277, 277, 277, 277, 277, 277, 277, 277, 277, 277, 277, 277, 277, 277, 277, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278, 278,
        279, 279, 279, 279, 279, 279, 279, 279, 279, 279, 279, 279, 279, 279, 279, 279, 280, 280, 280, 280, 280, 280, 280, 280, 280, 280, 280, 280, 280, 280, 280, 280,
        281, 281, 281, 281, 281, 281, 281, 281, 281, 281, 281, 281, 281, 281, 281, 281, 281, 281, 281, 281, 281, 281, 281, 281, 281, 281, 281, 281, 281, 281, 281, 281,
        282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282,
        283, 283, 283, 283, 283, 283, 283, 283, 283, 283, 283, 283, 283, 283, 283, 283, 283, 283, 283, 283, 283, 283, 283, 283, 283, 283, 283, 283, 283, 283, 283, 283,
        284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 284, 285
    };

    static const mz_uint8 s_tdefl_len_extra[256] = {
        0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
        4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0
    };

    static const mz_uint8 s_tdefl_small_dist_sym[512] = {
        0, 1, 2, 3, 4, 4, 5, 5, 6, 6, 6, 6, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9, 9, 9, 9, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 11, 11, 11, 11, 11, 11,
        11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 13,
        13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
        14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
        14, 14, 14, 14, 14, 14, 14, 14, 14, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
        15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
        16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
        16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
        16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17,
        17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17,
        17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17,
        17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17
    };

    static const mz_uint8 s_tdefl_small_dist_extra[512] = {
        0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
        6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
        6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7
    };

    static const mz_uint8 s_tdefl_large_dist_sym[128] = {
        0, 0, 18, 19, 20, 20, 21, 21, 22, 22, 22, 22, 23, 23, 23, 23, 24, 24, 24, 24, 24, 24, 24, 24, 25, 25, 25, 25, 25, 25, 25, 25, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26,
        26, 26, 26, 26, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
        28, 28, 28, 28, 28, 28, 28, 28, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29
    };

    static const mz_uint8 s_tdefl_large_dist_extra[128] = {
        0, 0, 8, 8, 9, 9, 9, 9, 10, 10, 10, 10, 10, 10, 10, 10, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
        12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
        13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13
    };


    typedef struct
    {
        mz_uint16 m_key, m_sym_index;
    } tdefl_sym_freq;
    static tdefl_sym_freq *tdefl_radix_sort_syms(mz_uint num_syms, tdefl_sym_freq *pSyms0, tdefl_sym_freq *pSyms1)
    {
        mz_uint32 total_passes = 2, pass_shift, pass, i, hist[256 * 2];
        tdefl_sym_freq *pCur_syms = pSyms0, *pNew_syms = pSyms1;
        memset((hist), 0, sizeof(hist));
        for (i = 0; i < num_syms; i++)
        {
            mz_uint freq = pSyms0[i].m_key;
            hist[freq & 0xFF]++;
            hist[256 + ((freq >> 8) & 0xFF)]++;
        }
        while ((total_passes > 1) && (num_syms == hist[(total_passes - 1) * 256]))
            total_passes--;
        for (pass_shift = 0, pass = 0; pass < total_passes; pass++, pass_shift += 8)
        {
            const mz_uint32 *pHist = &hist[pass << 8];
            mz_uint offsets[256], cur_ofs = 0;
            for (i = 0; i < 256; i++)
            {
                offsets[i] = cur_ofs;
                cur_ofs += pHist[i];
            }
            for (i = 0; i < num_syms; i++)
                pNew_syms[offsets[(pCur_syms[i].m_key >> pass_shift) & 0xFF]++] = pCur_syms[i];
            {
                tdefl_sym_freq *t = pCur_syms;
                pCur_syms = pNew_syms;
                pNew_syms = t;
            }
        }
        return pCur_syms;
    }


    static void tdefl_calculate_minimum_redundancy(tdefl_sym_freq *A, int n)
    {
        int root, leaf, next, avbl, used, dpth;
        if (n == 0)
            return;
        else if (n == 1)
        {
            A[0].m_key = 1;
            return;
        }
        A[0].m_key += A[1].m_key;
        root = 0;
        leaf = 2;
        for (next = 1; next < n - 1; next++)
        {
            if (leaf >= n || A[root].m_key < A[leaf].m_key)
            {
                A[next].m_key = A[root].m_key;
                A[root++].m_key = (mz_uint16)next;
            }
            else
                A[next].m_key = A[leaf++].m_key;
            if (leaf >= n || (root < next && A[root].m_key < A[leaf].m_key))
            {
                A[next].m_key = (mz_uint16)(A[next].m_key + A[root].m_key);
                A[root++].m_key = (mz_uint16)next;
            }
            else
                A[next].m_key = (mz_uint16)(A[next].m_key + A[leaf++].m_key);
        }
        A[n - 2].m_key = 0;
        for (next = n - 3; next >= 0; next--)
            A[next].m_key = A[A[next].m_key].m_key + 1;
        avbl = 1;
        used = dpth = 0;
        root = n - 2;
        next = n - 1;
        while (avbl > 0)
        {
            while (root >= 0 && (int)A[root].m_key == dpth)
            {
                used++;
                root--;
            }
            while (avbl > used)
            {
                A[next--].m_key = (mz_uint16)(dpth);
                avbl--;
            }
            avbl = 2 * used;
            dpth++;
            used = 0;
        }
    }


    enum
    {
        TDEFL_MAX_SUPPORTED_HUFF_CODESIZE = 32
    };
    static void tdefl_huffman_enforce_max_code_size(int *pNum_codes, int code_list_len, int max_code_size)
    {
        int i;
        mz_uint32 total = 0;
        if (code_list_len <= 1)
            return;
        for (i = max_code_size + 1; i <= TDEFL_MAX_SUPPORTED_HUFF_CODESIZE; i++)
            pNum_codes[max_code_size] += pNum_codes[i];
        for (i = max_code_size; i > 0; i--)
            total += (((mz_uint32)pNum_codes[i]) << (max_code_size - i));
        while (total != (1UL << max_code_size))
        {
            pNum_codes[max_code_size]--;
            for (i = max_code_size - 1; i > 0; i--)
                if (pNum_codes[i])
                {
                    pNum_codes[i]--;
                    pNum_codes[i + 1] += 2;
                    break;
                }
            total--;
        }
    }

    static void tdefl_optimize_huffman_table(tdefl_compressor *d, int table_num, int table_len, int code_size_limit, int static_table)
    {
        int i, j, l, num_codes[1 + TDEFL_MAX_SUPPORTED_HUFF_CODESIZE];
        mz_uint next_code[TDEFL_MAX_SUPPORTED_HUFF_CODESIZE + 1];
        memset((num_codes), 0, sizeof(num_codes));
        if (static_table)
        {
            for (i = 0; i < table_len; i++)
                num_codes[d->m_huff_code_sizes[table_num][i]]++;
        }
        else
        {
            tdefl_sym_freq syms0[TDEFL_MAX_HUFF_SYMBOLS], syms1[TDEFL_MAX_HUFF_SYMBOLS], *pSyms;
            int num_used_syms = 0;
            const mz_uint16 *pSym_count = &d->m_huff_count[table_num][0];
            for (i = 0; i < table_len; i++)
                if (pSym_count[i])
                {
                    syms0[num_used_syms].m_key = (mz_uint16)pSym_count[i];
                    syms0[num_used_syms++].m_sym_index = (mz_uint16)i;
                }

            pSyms = tdefl_radix_sort_syms(num_used_syms, syms0, syms1);
            tdefl_calculate_minimum_redundancy(pSyms, num_used_syms);

            for (i = 0; i < num_used_syms; i++)
                num_codes[pSyms[i].m_key]++;

            tdefl_huffman_enforce_max_code_size(num_codes, num_used_syms, code_size_limit);

            memset((d->m_huff_code_sizes[table_num]), 0, sizeof(d->m_huff_code_sizes[table_num]));
            memset((d->m_huff_codes[table_num]), 0, sizeof(d->m_huff_codes[table_num]));
            for (i = 1, j = num_used_syms; i <= code_size_limit; i++)
                for (l = num_codes[i]; l > 0; l--)
                    d->m_huff_code_sizes[table_num][pSyms[--j].m_sym_index] = (mz_uint8)(i);
        }

        next_code[1] = 0;
        for (j = 0, i = 2; i <= code_size_limit; i++)
            next_code[i] = j = ((j + num_codes[i - 1]) << 1);

        for (i = 0; i < table_len; i++)
        {
            mz_uint rev_code = 0, code, code_size;
            if ((code_size = d->m_huff_code_sizes[table_num][i]) == 0)
                continue;
            code = next_code[code_size]++;
            for (l = code_size; l > 0; l--, code >>= 1)
                rev_code = (rev_code << 1) | (code & 1);
            d->m_huff_codes[table_num][i] = (mz_uint16)rev_code;
        }
    }
    static const mz_uint8 s_tdefl_packed_code_size_syms_swizzle[] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };

    static void tdefl_start_dynamic_block(tdefl_compressor *d)
    {
        int num_lit_codes, num_dist_codes, num_bit_lengths;
        mz_uint i, total_code_sizes_to_pack, num_packed_code_sizes, rle_z_count, rle_repeat_count, packed_code_sizes_index;
        mz_uint8 code_sizes_to_pack[TDEFL_MAX_HUFF_SYMBOLS_0 + TDEFL_MAX_HUFF_SYMBOLS_1], packed_code_sizes[TDEFL_MAX_HUFF_SYMBOLS_0 + TDEFL_MAX_HUFF_SYMBOLS_1], prev_code_size = 0xFF;

        d->m_huff_count[0][256] = 1;

        tdefl_optimize_huffman_table(d, 0, TDEFL_MAX_HUFF_SYMBOLS_0, 15, (0));
        tdefl_optimize_huffman_table(d, 1, TDEFL_MAX_HUFF_SYMBOLS_1, 15, (0));

        for (num_lit_codes = 286; num_lit_codes > 257; num_lit_codes--)
            if (d->m_huff_code_sizes[0][num_lit_codes - 1])
                break;
        for (num_dist_codes = 30; num_dist_codes > 1; num_dist_codes--)
            if (d->m_huff_code_sizes[1][num_dist_codes - 1])
                break;

        memcpy(code_sizes_to_pack, &d->m_huff_code_sizes[0][0], num_lit_codes);
        memcpy(code_sizes_to_pack + num_lit_codes, &d->m_huff_code_sizes[1][0], num_dist_codes);
        total_code_sizes_to_pack = num_lit_codes + num_dist_codes;
        num_packed_code_sizes = 0;
        rle_z_count = 0;
        rle_repeat_count = 0;

        memset(&d->m_huff_count[2][0], 0, sizeof(d->m_huff_count[2][0]) * TDEFL_MAX_HUFF_SYMBOLS_2);
        for (i = 0; i < total_code_sizes_to_pack; i++)
        {
            mz_uint8 code_size = code_sizes_to_pack[i];
            if (!code_size)
            {
                { if (rle_repeat_count) { if (rle_repeat_count < 3) { d->m_huff_count[2][prev_code_size] = (mz_uint16)(d->m_huff_count[2][prev_code_size] + rle_repeat_count); while (rle_repeat_count--) packed_code_sizes[num_packed_code_sizes++] = prev_code_size; } else { d->m_huff_count[2][16] = (mz_uint16)(d->m_huff_count[2][16] + 1); packed_code_sizes[num_packed_code_sizes++] = 16; packed_code_sizes[num_packed_code_sizes++] = (mz_uint8)(rle_repeat_count - 3); } rle_repeat_count = 0; } };
                if (++rle_z_count == 138)
                {
                    { if (rle_z_count) { if (rle_z_count < 3) { d->m_huff_count[2][0] = (mz_uint16)(d->m_huff_count[2][0] + rle_z_count); while (rle_z_count--) packed_code_sizes[num_packed_code_sizes++] = 0; } else if (rle_z_count <= 10) { d->m_huff_count[2][17] = (mz_uint16)(d->m_huff_count[2][17] + 1); packed_code_sizes[num_packed_code_sizes++] = 17; packed_code_sizes[num_packed_code_sizes++] = (mz_uint8)(rle_z_count - 3); } else { d->m_huff_count[2][18] = (mz_uint16)(d->m_huff_count[2][18] + 1); packed_code_sizes[num_packed_code_sizes++] = 18; packed_code_sizes[num_packed_code_sizes++] = (mz_uint8)(rle_z_count - 11); } rle_z_count = 0; } };
                }
            }
            else
            {
                { if (rle_z_count) { if (rle_z_count < 3) { d->m_huff_count[2][0] = (mz_uint16)(d->m_huff_count[2][0] + rle_z_count); while (rle_z_count--) packed_code_sizes[num_packed_code_sizes++] = 0; } else if (rle_z_count <= 10) { d->m_huff_count[2][17] = (mz_uint16)(d->m_huff_count[2][17] + 1); packed_code_sizes[num_packed_code_sizes++] = 17; packed_code_sizes[num_packed_code_sizes++] = (mz_uint8)(rle_z_count - 3); } else { d->m_huff_count[2][18] = (mz_uint16)(d->m_huff_count[2][18] + 1); packed_code_sizes[num_packed_code_sizes++] = 18; packed_code_sizes[num_packed_code_sizes++] = (mz_uint8)(rle_z_count - 11); } rle_z_count = 0; } };
                if (code_size != prev_code_size)
                {
                    { if (rle_repeat_count) { if (rle_repeat_count < 3) { d->m_huff_count[2][prev_code_size] = (mz_uint16)(d->m_huff_count[2][prev_code_size] + rle_repeat_count); while (rle_repeat_count--) packed_code_sizes[num_packed_code_sizes++] = prev_code_size; } else { d->m_huff_count[2][16] = (mz_uint16)(d->m_huff_count[2][16] + 1); packed_code_sizes[num_packed_code_sizes++] = 16; packed_code_sizes[num_packed_code_sizes++] = (mz_uint8)(rle_repeat_count - 3); } rle_repeat_count = 0; } };
                    d->m_huff_count[2][code_size] = (mz_uint16)(d->m_huff_count[2][code_size] + 1);
                    packed_code_sizes[num_packed_code_sizes++] = code_size;
                }
                else if (++rle_repeat_count == 6)
                {
                    { if (rle_repeat_count) { if (rle_repeat_count < 3) { d->m_huff_count[2][prev_code_size] = (mz_uint16)(d->m_huff_count[2][prev_code_size] + rle_repeat_count); while (rle_repeat_count--) packed_code_sizes[num_packed_code_sizes++] = prev_code_size; } else { d->m_huff_count[2][16] = (mz_uint16)(d->m_huff_count[2][16] + 1); packed_code_sizes[num_packed_code_sizes++] = 16; packed_code_sizes[num_packed_code_sizes++] = (mz_uint8)(rle_repeat_count - 3); } rle_repeat_count = 0; } };
                }
            }
            prev_code_size = code_size;
        }
        if (rle_repeat_count)
        {
            { if (rle_repeat_count) { if (rle_repeat_count < 3) { d->m_huff_count[2][prev_code_size] = (mz_uint16)(d->m_huff_count[2][prev_code_size] + rle_repeat_count); while (rle_repeat_count--) packed_code_sizes[num_packed_code_sizes++] = prev_code_size; } else { d->m_huff_count[2][16] = (mz_uint16)(d->m_huff_count[2][16] + 1); packed_code_sizes[num_packed_code_sizes++] = 16; packed_code_sizes[num_packed_code_sizes++] = (mz_uint8)(rle_repeat_count - 3); } rle_repeat_count = 0; } };
        }
        else
        {
            { if (rle_z_count) { if (rle_z_count < 3) { d->m_huff_count[2][0] = (mz_uint16)(d->m_huff_count[2][0] + rle_z_count); while (rle_z_count--) packed_code_sizes[num_packed_code_sizes++] = 0; } else if (rle_z_count <= 10) { d->m_huff_count[2][17] = (mz_uint16)(d->m_huff_count[2][17] + 1); packed_code_sizes[num_packed_code_sizes++] = 17; packed_code_sizes[num_packed_code_sizes++] = (mz_uint8)(rle_z_count - 3); } else { d->m_huff_count[2][18] = (mz_uint16)(d->m_huff_count[2][18] + 1); packed_code_sizes[num_packed_code_sizes++] = 18; packed_code_sizes[num_packed_code_sizes++] = (mz_uint8)(rle_z_count - 11); } rle_z_count = 0; } };
        }

        tdefl_optimize_huffman_table(d, 2, TDEFL_MAX_HUFF_SYMBOLS_2, 7, (0));

        do { mz_uint bits = 2; mz_uint len = 2; 
       ((void) sizeof ((
       bits <= ((1U << len) - 1U)
       ) ? 1 : 0), __extension__ ({ if (
       bits <= ((1U << len) - 1U)
       ) ; else __assert_fail (
       "bits <= ((1U << len) - 1U)"
       , "miniz.c", 1048, __extension__ __PRETTY_FUNCTION__); }))
       ; d->m_bit_buffer |= (bits << d->m_bits_in); d->m_bits_in += len; while (d->m_bits_in >= 8) { if (d->m_pOutput_buf < d->m_pOutput_buf_end) *d->m_pOutput_buf++ = (mz_uint8)(d->m_bit_buffer); d->m_bit_buffer >>= 8; d->m_bits_in -= 8; } } while (0);

        do { mz_uint bits = num_lit_codes - 257; mz_uint len = 5; 
       ((void) sizeof ((
       bits <= ((1U << len) - 1U)
       ) ? 1 : 0), __extension__ ({ if (
       bits <= ((1U << len) - 1U)
       ) ; else __assert_fail (
       "bits <= ((1U << len) - 1U)"
       , "miniz.c", 1050, __extension__ __PRETTY_FUNCTION__); }))
       ; d->m_bit_buffer |= (bits << d->m_bits_in); d->m_bits_in += len; while (d->m_bits_in >= 8) { if (d->m_pOutput_buf < d->m_pOutput_buf_end) *d->m_pOutput_buf++ = (mz_uint8)(d->m_bit_buffer); d->m_bit_buffer >>= 8; d->m_bits_in -= 8; } } while (0);
        do { mz_uint bits = num_dist_codes - 1; mz_uint len = 5; 
       ((void) sizeof ((
       bits <= ((1U << len) - 1U)
       ) ? 1 : 0), __extension__ ({ if (
       bits <= ((1U << len) - 1U)
       ) ; else __assert_fail (
       "bits <= ((1U << len) - 1U)"
       , "miniz.c", 1051, __extension__ __PRETTY_FUNCTION__); }))
       ; d->m_bit_buffer |= (bits << d->m_bits_in); d->m_bits_in += len; while (d->m_bits_in >= 8) { if (d->m_pOutput_buf < d->m_pOutput_buf_end) *d->m_pOutput_buf++ = (mz_uint8)(d->m_bit_buffer); d->m_bit_buffer >>= 8; d->m_bits_in -= 8; } } while (0);

        for (num_bit_lengths = 18; num_bit_lengths >= 0; num_bit_lengths--)
            if (d->m_huff_code_sizes[2][s_tdefl_packed_code_size_syms_swizzle[num_bit_lengths]])
                break;
        num_bit_lengths = (((4) > ((num_bit_lengths + 1))) ? (4) : ((num_bit_lengths + 1)));
        do { mz_uint bits = num_bit_lengths - 4; mz_uint len = 4; 
       ((void) sizeof ((
       bits <= ((1U << len) - 1U)
       ) ? 1 : 0), __extension__ ({ if (
       bits <= ((1U << len) - 1U)
       ) ; else __assert_fail (
       "bits <= ((1U << len) - 1U)"
       , "miniz.c", 1057, __extension__ __PRETTY_FUNCTION__); }))
       ; d->m_bit_buffer |= (bits << d->m_bits_in); d->m_bits_in += len; while (d->m_bits_in >= 8) { if (d->m_pOutput_buf < d->m_pOutput_buf_end) *d->m_pOutput_buf++ = (mz_uint8)(d->m_bit_buffer); d->m_bit_buffer >>= 8; d->m_bits_in -= 8; } } while (0);
        for (i = 0; (int)i < num_bit_lengths; i++)
            do { mz_uint bits = d->m_huff_code_sizes[2][s_tdefl_packed_code_size_syms_swizzle[i]]; mz_uint len = 3; 
           ((void) sizeof ((
           bits <= ((1U << len) - 1U)
           ) ? 1 : 0), __extension__ ({ if (
           bits <= ((1U << len) - 1U)
           ) ; else __assert_fail (
           "bits <= ((1U << len) - 1U)"
           , "miniz.c", 1059, __extension__ __PRETTY_FUNCTION__); }))
           ; d->m_bit_buffer |= (bits << d->m_bits_in); d->m_bits_in += len; while (d->m_bits_in >= 8) { if (d->m_pOutput_buf < d->m_pOutput_buf_end) *d->m_pOutput_buf++ = (mz_uint8)(d->m_bit_buffer); d->m_bit_buffer >>= 8; d->m_bits_in -= 8; } } while (0);

        for (packed_code_sizes_index = 0; packed_code_sizes_index < num_packed_code_sizes;)
        {
            mz_uint code = packed_code_sizes[packed_code_sizes_index++];
            
           ((void) sizeof ((
           code < TDEFL_MAX_HUFF_SYMBOLS_2
           ) ? 1 : 0), __extension__ ({ if (
           code < TDEFL_MAX_HUFF_SYMBOLS_2
           ) ; else __assert_fail (
           "code < TDEFL_MAX_HUFF_SYMBOLS_2"
           , "miniz.c", 1064, __extension__ __PRETTY_FUNCTION__); }))
                                                     ;
            do { mz_uint bits = d->m_huff_codes[2][code]; mz_uint len = d->m_huff_code_sizes[2][code]; 
           ((void) sizeof ((
           bits <= ((1U << len) - 1U)
           ) ? 1 : 0), __extension__ ({ if (
           bits <= ((1U << len) - 1U)
           ) ; else __assert_fail (
           "bits <= ((1U << len) - 1U)"
           , "miniz.c", 1065, __extension__ __PRETTY_FUNCTION__); }))
           ; d->m_bit_buffer |= (bits << d->m_bits_in); d->m_bits_in += len; while (d->m_bits_in >= 8) { if (d->m_pOutput_buf < d->m_pOutput_buf_end) *d->m_pOutput_buf++ = (mz_uint8)(d->m_bit_buffer); d->m_bit_buffer >>= 8; d->m_bits_in -= 8; } } while (0);
            if (code >= 16)
                do { mz_uint bits = packed_code_sizes[packed_code_sizes_index++]; mz_uint len = "\02\03\07"[code - 16]; 
               ((void) sizeof ((
               bits <= ((1U << len) - 1U)
               ) ? 1 : 0), __extension__ ({ if (
               bits <= ((1U << len) - 1U)
               ) ; else __assert_fail (
               "bits <= ((1U << len) - 1U)"
               , "miniz.c", 1067, __extension__ __PRETTY_FUNCTION__); }))
               ; d->m_bit_buffer |= (bits << d->m_bits_in); d->m_bits_in += len; while (d->m_bits_in >= 8) { if (d->m_pOutput_buf < d->m_pOutput_buf_end) *d->m_pOutput_buf++ = (mz_uint8)(d->m_bit_buffer); d->m_bit_buffer >>= 8; d->m_bits_in -= 8; } } while (0);
        }
    }

    static void tdefl_start_static_block(tdefl_compressor *d)
    {
        mz_uint i;
        mz_uint8 *p = &d->m_huff_code_sizes[0][0];

        for (i = 0; i <= 143; ++i)
            *p++ = 8;
        for (; i <= 255; ++i)
            *p++ = 9;
        for (; i <= 279; ++i)
            *p++ = 7;
        for (; i <= 287; ++i)
            *p++ = 8;

        memset(d->m_huff_code_sizes[1], 5, 32);

        tdefl_optimize_huffman_table(d, 0, 288, 15, (1));
        tdefl_optimize_huffman_table(d, 1, 32, 15, (1));

        do { mz_uint bits = 1; mz_uint len = 2; 
       ((void) sizeof ((
       bits <= ((1U << len) - 1U)
       ) ? 1 : 0), __extension__ ({ if (
       bits <= ((1U << len) - 1U)
       ) ; else __assert_fail (
       "bits <= ((1U << len) - 1U)"
       , "miniz.c", 1090, __extension__ __PRETTY_FUNCTION__); }))
       ; d->m_bit_buffer |= (bits << d->m_bits_in); d->m_bits_in += len; while (d->m_bits_in >= 8) { if (d->m_pOutput_buf < d->m_pOutput_buf_end) *d->m_pOutput_buf++ = (mz_uint8)(d->m_bit_buffer); d->m_bit_buffer >>= 8; d->m_bits_in -= 8; } } while (0);
    }

    static const mz_uint mz_bitmasks[17] = { 0x0000, 0x0001, 0x0003, 0x0007, 0x000F, 0x001F, 0x003F, 0x007F, 0x00FF, 0x01FF, 0x03FF, 0x07FF, 0x0FFF, 0x1FFF, 0x3FFF, 0x7FFF, 0xFFFF };
static mz_bool tdefl_compress_lz_codes(tdefl_compressor *d)
{
    mz_uint flags;
    mz_uint8 *pLZ_codes;

    flags = 1;
    for (pLZ_codes = d->m_lz_code_buf; pLZ_codes < d->m_pLZ_code_buf; flags >>= 1)
    {
        if (flags == 1)
            flags = *pLZ_codes++ | 0x100;
        if (flags & 1)
        {
            mz_uint sym, num_extra_bits;
            mz_uint match_len = pLZ_codes[0], match_dist = (pLZ_codes[1] | (pLZ_codes[2] << 8));
            pLZ_codes += 3;

            
           ((void) sizeof ((
           d->m_huff_code_sizes[0][s_tdefl_len_sym[match_len]]
           ) ? 1 : 0), __extension__ ({ if (
           d->m_huff_code_sizes[0][s_tdefl_len_sym[match_len]]
           ) ; else __assert_fail (
           "d->m_huff_code_sizes[0][s_tdefl_len_sym[match_len]]"
           , "miniz.c", 1207, __extension__ __PRETTY_FUNCTION__); }))
                                                                         ;
            do { mz_uint bits = d->m_huff_codes[0][s_tdefl_len_sym[match_len]]; mz_uint len = d->m_huff_code_sizes[0][s_tdefl_len_sym[match_len]]; 
           ((void) sizeof ((
           bits <= ((1U << len) - 1U)
           ) ? 1 : 0), __extension__ ({ if (
           bits <= ((1U << len) - 1U)
           ) ; else __assert_fail (
           "bits <= ((1U << len) - 1U)"
           , "miniz.c", 1208, __extension__ __PRETTY_FUNCTION__); }))
           ; d->m_bit_buffer |= (bits << d->m_bits_in); d->m_bits_in += len; while (d->m_bits_in >= 8) { if (d->m_pOutput_buf < d->m_pOutput_buf_end) *d->m_pOutput_buf++ = (mz_uint8)(d->m_bit_buffer); d->m_bit_buffer >>= 8; d->m_bits_in -= 8; } } while (0);
            do { mz_uint bits = match_len & mz_bitmasks[s_tdefl_len_extra[match_len]]; mz_uint len = s_tdefl_len_extra[match_len]; 
           ((void) sizeof ((
           bits <= ((1U << len) - 1U)
           ) ? 1 : 0), __extension__ ({ if (
           bits <= ((1U << len) - 1U)
           ) ; else __assert_fail (
           "bits <= ((1U << len) - 1U)"
           , "miniz.c", 1209, __extension__ __PRETTY_FUNCTION__); }))
           ; d->m_bit_buffer |= (bits << d->m_bits_in); d->m_bits_in += len; while (d->m_bits_in >= 8) { if (d->m_pOutput_buf < d->m_pOutput_buf_end) *d->m_pOutput_buf++ = (mz_uint8)(d->m_bit_buffer); d->m_bit_buffer >>= 8; d->m_bits_in -= 8; } } while (0);

            if (match_dist < 512)
            {
                sym = s_tdefl_small_dist_sym[match_dist];
                num_extra_bits = s_tdefl_small_dist_extra[match_dist];
            }
            else
            {
                sym = s_tdefl_large_dist_sym[match_dist >> 8];
                num_extra_bits = s_tdefl_large_dist_extra[match_dist >> 8];
            }
            
           ((void) sizeof ((
           d->m_huff_code_sizes[1][sym]
           ) ? 1 : 0), __extension__ ({ if (
           d->m_huff_code_sizes[1][sym]
           ) ; else __assert_fail (
           "d->m_huff_code_sizes[1][sym]"
           , "miniz.c", 1221, __extension__ __PRETTY_FUNCTION__); }))
                                                  ;
            do { mz_uint bits = d->m_huff_codes[1][sym]; mz_uint len = d->m_huff_code_sizes[1][sym]; 
           ((void) sizeof ((
           bits <= ((1U << len) - 1U)
           ) ? 1 : 0), __extension__ ({ if (
           bits <= ((1U << len) - 1U)
           ) ; else __assert_fail (
           "bits <= ((1U << len) - 1U)"
           , "miniz.c", 1222, __extension__ __PRETTY_FUNCTION__); }))
           ; d->m_bit_buffer |= (bits << d->m_bits_in); d->m_bits_in += len; while (d->m_bits_in >= 8) { if (d->m_pOutput_buf < d->m_pOutput_buf_end) *d->m_pOutput_buf++ = (mz_uint8)(d->m_bit_buffer); d->m_bit_buffer >>= 8; d->m_bits_in -= 8; } } while (0);
            do { mz_uint bits = match_dist & mz_bitmasks[num_extra_bits]; mz_uint len = num_extra_bits; 
           ((void) sizeof ((
           bits <= ((1U << len) - 1U)
           ) ? 1 : 0), __extension__ ({ if (
           bits <= ((1U << len) - 1U)
           ) ; else __assert_fail (
           "bits <= ((1U << len) - 1U)"
           , "miniz.c", 1223, __extension__ __PRETTY_FUNCTION__); }))
           ; d->m_bit_buffer |= (bits << d->m_bits_in); d->m_bits_in += len; while (d->m_bits_in >= 8) { if (d->m_pOutput_buf < d->m_pOutput_buf_end) *d->m_pOutput_buf++ = (mz_uint8)(d->m_bit_buffer); d->m_bit_buffer >>= 8; d->m_bits_in -= 8; } } while (0);
        }
        else
        {
            mz_uint lit = *pLZ_codes++;
            
           ((void) sizeof ((
           d->m_huff_code_sizes[0][lit]
           ) ? 1 : 0), __extension__ ({ if (
           d->m_huff_code_sizes[0][lit]
           ) ; else __assert_fail (
           "d->m_huff_code_sizes[0][lit]"
           , "miniz.c", 1228, __extension__ __PRETTY_FUNCTION__); }))
                                                  ;
            do { mz_uint bits = d->m_huff_codes[0][lit]; mz_uint len = d->m_huff_code_sizes[0][lit]; 
           ((void) sizeof ((
           bits <= ((1U << len) - 1U)
           ) ? 1 : 0), __extension__ ({ if (
           bits <= ((1U << len) - 1U)
           ) ; else __assert_fail (
           "bits <= ((1U << len) - 1U)"
           , "miniz.c", 1229, __extension__ __PRETTY_FUNCTION__); }))
           ; d->m_bit_buffer |= (bits << d->m_bits_in); d->m_bits_in += len; while (d->m_bits_in >= 8) { if (d->m_pOutput_buf < d->m_pOutput_buf_end) *d->m_pOutput_buf++ = (mz_uint8)(d->m_bit_buffer); d->m_bit_buffer >>= 8; d->m_bits_in -= 8; } } while (0);
        }
    }

    do { mz_uint bits = d->m_huff_codes[0][256]; mz_uint len = d->m_huff_code_sizes[0][256]; 
   ((void) sizeof ((
   bits <= ((1U << len) - 1U)
   ) ? 1 : 0), __extension__ ({ if (
   bits <= ((1U << len) - 1U)
   ) ; else __assert_fail (
   "bits <= ((1U << len) - 1U)"
   , "miniz.c", 1233, __extension__ __PRETTY_FUNCTION__); }))
   ; d->m_bit_buffer |= (bits << d->m_bits_in); d->m_bits_in += len; while (d->m_bits_in >= 8) { if (d->m_pOutput_buf < d->m_pOutput_buf_end) *d->m_pOutput_buf++ = (mz_uint8)(d->m_bit_buffer); d->m_bit_buffer >>= 8; d->m_bits_in -= 8; } } while (0);

    return (d->m_pOutput_buf < d->m_pOutput_buf_end);
}


    static mz_bool tdefl_compress_block(tdefl_compressor *d, mz_bool static_block)
    {
        if (static_block)
            tdefl_start_static_block(d);
        else
            tdefl_start_dynamic_block(d);
        return tdefl_compress_lz_codes(d);
    }

    static const mz_uint s_tdefl_num_probes[11] = { 0, 1, 6, 32, 16, 32, 128, 256, 512, 768, 1500 };

    static int tdefl_flush_block(tdefl_compressor *d, int flush)
    {
        mz_uint saved_bit_buf, saved_bits_in;
        mz_uint8 *pSaved_output_buf;
        mz_bool comp_block_succeeded = (0);
        int n, use_raw_block = ((d->m_flags & TDEFL_FORCE_ALL_RAW_BLOCKS) != 0) && (d->m_lookahead_pos - d->m_lz_code_buf_dict_pos) <= d->m_dict_size;
        mz_uint8 *pOutput_buf_start = ((d->m_pPut_buf_func == 
                                                             ((void *)0)
                                                                 ) && ((*d->m_pOut_buf_size - d->m_out_buf_ofs) >= TDEFL_OUT_BUF_SIZE)) ? ((mz_uint8 *)d->m_pOut_buf + d->m_out_buf_ofs) : d->m_output_buf;

        d->m_pOutput_buf = pOutput_buf_start;
        d->m_pOutput_buf_end = d->m_pOutput_buf + TDEFL_OUT_BUF_SIZE - 16;

        
       ((void) sizeof ((
       !d->m_output_flush_remaining
       ) ? 1 : 0), __extension__ ({ if (
       !d->m_output_flush_remaining
       ) ; else __assert_fail (
       "!d->m_output_flush_remaining"
       , "miniz.c", 1261, __extension__ __PRETTY_FUNCTION__); }))
                                              ;
        d->m_output_flush_ofs = 0;
        d->m_output_flush_remaining = 0;

        *d->m_pLZ_flags = (mz_uint8)(*d->m_pLZ_flags >> d->m_num_flags_left);
        d->m_pLZ_code_buf -= (d->m_num_flags_left == 8);

        if ((d->m_flags & TDEFL_WRITE_ZLIB_HEADER) && (!d->m_block_index))
        {
            const mz_uint8 cmf = 0x78;
            mz_uint8 flg, flevel = 3;
            mz_uint header, i, mz_un = sizeof(s_tdefl_num_probes) / sizeof(mz_uint);


            for (i = 0; i < mz_un; i++)
                if (s_tdefl_num_probes[i] == (d->m_flags & 0xFFF))
                    break;

            if (i < 2)
                flevel = 0;
            else if (i < 6)
                flevel = 1;
            else if (i == 6)
                flevel = 2;

            header = cmf << 8 | (flevel << 6);
            header += 31 - (header % 31);
            flg = header & 0xFF;

            do { mz_uint bits = cmf; mz_uint len = 8; 
           ((void) sizeof ((
           bits <= ((1U << len) - 1U)
           ) ? 1 : 0), __extension__ ({ if (
           bits <= ((1U << len) - 1U)
           ) ; else __assert_fail (
           "bits <= ((1U << len) - 1U)"
           , "miniz.c", 1290, __extension__ __PRETTY_FUNCTION__); }))
           ; d->m_bit_buffer |= (bits << d->m_bits_in); d->m_bits_in += len; while (d->m_bits_in >= 8) { if (d->m_pOutput_buf < d->m_pOutput_buf_end) *d->m_pOutput_buf++ = (mz_uint8)(d->m_bit_buffer); d->m_bit_buffer >>= 8; d->m_bits_in -= 8; } } while (0);
            do { mz_uint bits = flg; mz_uint len = 8; 
           ((void) sizeof ((
           bits <= ((1U << len) - 1U)
           ) ? 1 : 0), __extension__ ({ if (
           bits <= ((1U << len) - 1U)
           ) ; else __assert_fail (
           "bits <= ((1U << len) - 1U)"
           , "miniz.c", 1291, __extension__ __PRETTY_FUNCTION__); }))
           ; d->m_bit_buffer |= (bits << d->m_bits_in); d->m_bits_in += len; while (d->m_bits_in >= 8) { if (d->m_pOutput_buf < d->m_pOutput_buf_end) *d->m_pOutput_buf++ = (mz_uint8)(d->m_bit_buffer); d->m_bit_buffer >>= 8; d->m_bits_in -= 8; } } while (0);
        }

        do { mz_uint bits = flush == TDEFL_FINISH; mz_uint len = 1; 
       ((void) sizeof ((
       bits <= ((1U << len) - 1U)
       ) ? 1 : 0), __extension__ ({ if (
       bits <= ((1U << len) - 1U)
       ) ; else __assert_fail (
       "bits <= ((1U << len) - 1U)"
       , "miniz.c", 1294, __extension__ __PRETTY_FUNCTION__); }))
       ; d->m_bit_buffer |= (bits << d->m_bits_in); d->m_bits_in += len; while (d->m_bits_in >= 8) { if (d->m_pOutput_buf < d->m_pOutput_buf_end) *d->m_pOutput_buf++ = (mz_uint8)(d->m_bit_buffer); d->m_bit_buffer >>= 8; d->m_bits_in -= 8; } } while (0);

        pSaved_output_buf = d->m_pOutput_buf;
        saved_bit_buf = d->m_bit_buffer;
        saved_bits_in = d->m_bits_in;

        if (!use_raw_block)
            comp_block_succeeded = tdefl_compress_block(d, (d->m_flags & TDEFL_FORCE_ALL_STATIC_BLOCKS) || (d->m_total_lz_bytes < 48));


        if (((use_raw_block) || ((d->m_total_lz_bytes) && ((d->m_pOutput_buf - pSaved_output_buf + 1U) >= d->m_total_lz_bytes))) &&
            ((d->m_lookahead_pos - d->m_lz_code_buf_dict_pos) <= d->m_dict_size))
        {
            mz_uint i;
            d->m_pOutput_buf = pSaved_output_buf;
            d->m_bit_buffer = saved_bit_buf, d->m_bits_in = saved_bits_in;
            do { mz_uint bits = 0; mz_uint len = 2; 
           ((void) sizeof ((
           bits <= ((1U << len) - 1U)
           ) ? 1 : 0), __extension__ ({ if (
           bits <= ((1U << len) - 1U)
           ) ; else __assert_fail (
           "bits <= ((1U << len) - 1U)"
           , "miniz.c", 1310, __extension__ __PRETTY_FUNCTION__); }))
           ; d->m_bit_buffer |= (bits << d->m_bits_in); d->m_bits_in += len; while (d->m_bits_in >= 8) { if (d->m_pOutput_buf < d->m_pOutput_buf_end) *d->m_pOutput_buf++ = (mz_uint8)(d->m_bit_buffer); d->m_bit_buffer >>= 8; d->m_bits_in -= 8; } } while (0);
            if (d->m_bits_in)
            {
                do { mz_uint bits = 0; mz_uint len = 8 - d->m_bits_in; 
               ((void) sizeof ((
               bits <= ((1U << len) - 1U)
               ) ? 1 : 0), __extension__ ({ if (
               bits <= ((1U << len) - 1U)
               ) ; else __assert_fail (
               "bits <= ((1U << len) - 1U)"
               , "miniz.c", 1313, __extension__ __PRETTY_FUNCTION__); }))
               ; d->m_bit_buffer |= (bits << d->m_bits_in); d->m_bits_in += len; while (d->m_bits_in >= 8) { if (d->m_pOutput_buf < d->m_pOutput_buf_end) *d->m_pOutput_buf++ = (mz_uint8)(d->m_bit_buffer); d->m_bit_buffer >>= 8; d->m_bits_in -= 8; } } while (0);
            }
            for (i = 2; i; --i, d->m_total_lz_bytes ^= 0xFFFF)
            {
                do { mz_uint bits = d->m_total_lz_bytes & 0xFFFF; mz_uint len = 16; 
               ((void) sizeof ((
               bits <= ((1U << len) - 1U)
               ) ? 1 : 0), __extension__ ({ if (
               bits <= ((1U << len) - 1U)
               ) ; else __assert_fail (
               "bits <= ((1U << len) - 1U)"
               , "miniz.c", 1317, __extension__ __PRETTY_FUNCTION__); }))
               ; d->m_bit_buffer |= (bits << d->m_bits_in); d->m_bits_in += len; while (d->m_bits_in >= 8) { if (d->m_pOutput_buf < d->m_pOutput_buf_end) *d->m_pOutput_buf++ = (mz_uint8)(d->m_bit_buffer); d->m_bit_buffer >>= 8; d->m_bits_in -= 8; } } while (0);
            }
            for (i = 0; i < d->m_total_lz_bytes; ++i)
            {
                do { mz_uint bits = d->m_dict[(d->m_lz_code_buf_dict_pos + i) & TDEFL_LZ_DICT_SIZE_MASK]; mz_uint len = 8; 
               ((void) sizeof ((
               bits <= ((1U << len) - 1U)
               ) ? 1 : 0), __extension__ ({ if (
               bits <= ((1U << len) - 1U)
               ) ; else __assert_fail (
               "bits <= ((1U << len) - 1U)"
               , "miniz.c", 1321, __extension__ __PRETTY_FUNCTION__); }))
               ; d->m_bit_buffer |= (bits << d->m_bits_in); d->m_bits_in += len; while (d->m_bits_in >= 8) { if (d->m_pOutput_buf < d->m_pOutput_buf_end) *d->m_pOutput_buf++ = (mz_uint8)(d->m_bit_buffer); d->m_bit_buffer >>= 8; d->m_bits_in -= 8; } } while (0);
            }
        }

        else if (!comp_block_succeeded)
        {
            d->m_pOutput_buf = pSaved_output_buf;
            d->m_bit_buffer = saved_bit_buf, d->m_bits_in = saved_bits_in;
            tdefl_compress_block(d, (1));
        }

        if (flush)
        {
            if (flush == TDEFL_FINISH)
            {
                if (d->m_bits_in)
                {
                    do { mz_uint bits = 0; mz_uint len = 8 - d->m_bits_in; 
                   ((void) sizeof ((
                   bits <= ((1U << len) - 1U)
                   ) ? 1 : 0), __extension__ ({ if (
                   bits <= ((1U << len) - 1U)
                   ) ; else __assert_fail (
                   "bits <= ((1U << len) - 1U)"
                   , "miniz.c", 1338, __extension__ __PRETTY_FUNCTION__); }))
                   ; d->m_bit_buffer |= (bits << d->m_bits_in); d->m_bits_in += len; while (d->m_bits_in >= 8) { if (d->m_pOutput_buf < d->m_pOutput_buf_end) *d->m_pOutput_buf++ = (mz_uint8)(d->m_bit_buffer); d->m_bit_buffer >>= 8; d->m_bits_in -= 8; } } while (0);
                }
                if (d->m_flags & TDEFL_WRITE_ZLIB_HEADER)
                {
                    mz_uint i, a = d->m_adler32;
                    for (i = 0; i < 4; i++)
                    {
                        do { mz_uint bits = (a >> 24) & 0xFF; mz_uint len = 8; 
                       ((void) sizeof ((
                       bits <= ((1U << len) - 1U)
                       ) ? 1 : 0), __extension__ ({ if (
                       bits <= ((1U << len) - 1U)
                       ) ; else __assert_fail (
                       "bits <= ((1U << len) - 1U)"
                       , "miniz.c", 1345, __extension__ __PRETTY_FUNCTION__); }))
                       ; d->m_bit_buffer |= (bits << d->m_bits_in); d->m_bits_in += len; while (d->m_bits_in >= 8) { if (d->m_pOutput_buf < d->m_pOutput_buf_end) *d->m_pOutput_buf++ = (mz_uint8)(d->m_bit_buffer); d->m_bit_buffer >>= 8; d->m_bits_in -= 8; } } while (0);
                        a <<= 8;
                    }
                }
            }
            else
            {
                mz_uint i, z = 0;
                do { mz_uint bits = 0; mz_uint len = 3; 
               ((void) sizeof ((
               bits <= ((1U << len) - 1U)
               ) ? 1 : 0), __extension__ ({ if (
               bits <= ((1U << len) - 1U)
               ) ; else __assert_fail (
               "bits <= ((1U << len) - 1U)"
               , "miniz.c", 1353, __extension__ __PRETTY_FUNCTION__); }))
               ; d->m_bit_buffer |= (bits << d->m_bits_in); d->m_bits_in += len; while (d->m_bits_in >= 8) { if (d->m_pOutput_buf < d->m_pOutput_buf_end) *d->m_pOutput_buf++ = (mz_uint8)(d->m_bit_buffer); d->m_bit_buffer >>= 8; d->m_bits_in -= 8; } } while (0);
                if (d->m_bits_in)
                {
                    do { mz_uint bits = 0; mz_uint len = 8 - d->m_bits_in; 
                   ((void) sizeof ((
                   bits <= ((1U << len) - 1U)
                   ) ? 1 : 0), __extension__ ({ if (
                   bits <= ((1U << len) - 1U)
                   ) ; else __assert_fail (
                   "bits <= ((1U << len) - 1U)"
                   , "miniz.c", 1356, __extension__ __PRETTY_FUNCTION__); }))
                   ; d->m_bit_buffer |= (bits << d->m_bits_in); d->m_bits_in += len; while (d->m_bits_in >= 8) { if (d->m_pOutput_buf < d->m_pOutput_buf_end) *d->m_pOutput_buf++ = (mz_uint8)(d->m_bit_buffer); d->m_bit_buffer >>= 8; d->m_bits_in -= 8; } } while (0);
                }
                for (i = 2; i; --i, z ^= 0xFFFF)
                {
                    do { mz_uint bits = z & 0xFFFF; mz_uint len = 16; 
                   ((void) sizeof ((
                   bits <= ((1U << len) - 1U)
                   ) ? 1 : 0), __extension__ ({ if (
                   bits <= ((1U << len) - 1U)
                   ) ; else __assert_fail (
                   "bits <= ((1U << len) - 1U)"
                   , "miniz.c", 1360, __extension__ __PRETTY_FUNCTION__); }))
                   ; d->m_bit_buffer |= (bits << d->m_bits_in); d->m_bits_in += len; while (d->m_bits_in >= 8) { if (d->m_pOutput_buf < d->m_pOutput_buf_end) *d->m_pOutput_buf++ = (mz_uint8)(d->m_bit_buffer); d->m_bit_buffer >>= 8; d->m_bits_in -= 8; } } while (0);
                }
            }
        }

        
       ((void) sizeof ((
       d->m_pOutput_buf < d->m_pOutput_buf_end
       ) ? 1 : 0), __extension__ ({ if (
       d->m_pOutput_buf < d->m_pOutput_buf_end
       ) ; else __assert_fail (
       "d->m_pOutput_buf < d->m_pOutput_buf_end"
       , "miniz.c", 1365, __extension__ __PRETTY_FUNCTION__); }))
                                                         ;

        memset(&d->m_huff_count[0][0], 0, sizeof(d->m_huff_count[0][0]) * TDEFL_MAX_HUFF_SYMBOLS_0);
        memset(&d->m_huff_count[1][0], 0, sizeof(d->m_huff_count[1][0]) * TDEFL_MAX_HUFF_SYMBOLS_1);

        d->m_pLZ_code_buf = d->m_lz_code_buf + 1;
        d->m_pLZ_flags = d->m_lz_code_buf;
        d->m_num_flags_left = 8;
        d->m_lz_code_buf_dict_pos += d->m_total_lz_bytes;
        d->m_total_lz_bytes = 0;
        d->m_block_index++;

        if ((n = (int)(d->m_pOutput_buf - pOutput_buf_start)) != 0)
        {
            if (d->m_pPut_buf_func)
            {
                *d->m_pIn_buf_size = d->m_pSrc - (const mz_uint8 *)d->m_pIn_buf;
                if (!(*d->m_pPut_buf_func)(d->m_output_buf, n, d->m_pPut_buf_user))
                    return (d->m_prev_return_status = TDEFL_STATUS_PUT_BUF_FAILED);
            }
            else if (pOutput_buf_start == d->m_output_buf)
            {
                int bytes_to_copy = (int)((((size_t)n) < ((size_t)(*d->m_pOut_buf_size - d->m_out_buf_ofs))) ? ((size_t)n) : ((size_t)(*d->m_pOut_buf_size - d->m_out_buf_ofs)));
                memcpy((mz_uint8 *)d->m_pOut_buf + d->m_out_buf_ofs, d->m_output_buf, bytes_to_copy);
                d->m_out_buf_ofs += bytes_to_copy;
                if ((n -= bytes_to_copy) != 0)
                {
                    d->m_output_flush_ofs = bytes_to_copy;
                    d->m_output_flush_remaining = n;
                }
            }
            else
            {
                d->m_out_buf_ofs += n;
            }
        }

        return d->m_output_flush_remaining;
    }
static __inline__ __attribute__((__always_inline__)) void tdefl_find_match(tdefl_compressor *d, mz_uint lookahead_pos, mz_uint max_dist, mz_uint max_match_len, mz_uint *pMatch_dist, mz_uint *pMatch_len)
{
    mz_uint dist, pos = lookahead_pos & TDEFL_LZ_DICT_SIZE_MASK, match_len = *pMatch_len, probe_pos = pos, next_probe_pos, probe_len;
    mz_uint num_probes_left = d->m_max_probes[match_len >= 32];
    const mz_uint8 *s = d->m_dict + pos, *p, *q;
    mz_uint8 c0 = d->m_dict[pos + match_len], c1 = d->m_dict[pos + match_len - 1];
    
   ((void) sizeof ((
   max_match_len <= TDEFL_MAX_MATCH_LEN
   ) ? 1 : 0), __extension__ ({ if (
   max_match_len <= TDEFL_MAX_MATCH_LEN
   ) ; else __assert_fail (
   "max_match_len <= TDEFL_MAX_MATCH_LEN"
   , "miniz.c", 1482, __extension__ __PRETTY_FUNCTION__); }))
                                                  ;
    if (max_match_len <= match_len)
        return;
    for (;;)
    {
        for (;;)
        {
            if (--num_probes_left == 0)
                return;







            next_probe_pos = d->m_next[probe_pos]; if ((!next_probe_pos) || ((dist = (mz_uint16)(lookahead_pos - next_probe_pos)) > max_dist)) return; probe_pos = next_probe_pos & TDEFL_LZ_DICT_SIZE_MASK; if ((d->m_dict[probe_pos + match_len] == c0) && (d->m_dict[probe_pos + match_len - 1] == c1)) break;;
            next_probe_pos = d->m_next[probe_pos]; if ((!next_probe_pos) || ((dist = (mz_uint16)(lookahead_pos - next_probe_pos)) > max_dist)) return; probe_pos = next_probe_pos & TDEFL_LZ_DICT_SIZE_MASK; if ((d->m_dict[probe_pos + match_len] == c0) && (d->m_dict[probe_pos + match_len - 1] == c1)) break;;
            next_probe_pos = d->m_next[probe_pos]; if ((!next_probe_pos) || ((dist = (mz_uint16)(lookahead_pos - next_probe_pos)) > max_dist)) return; probe_pos = next_probe_pos & TDEFL_LZ_DICT_SIZE_MASK; if ((d->m_dict[probe_pos + match_len] == c0) && (d->m_dict[probe_pos + match_len - 1] == c1)) break;;
        }
        if (!dist)
            break;
        p = s;
        q = d->m_dict + probe_pos;
        for (probe_len = 0; probe_len < max_match_len; probe_len++)
            if (*p++ != *q++)
                break;
        if (probe_len > match_len)
        {
            *pMatch_dist = dist;
            if ((*pMatch_len = match_len = probe_len) == max_match_len)
                return;
            c0 = d->m_dict[pos + match_len];
            c1 = d->m_dict[pos + match_len - 1];
        }
    }
}
    static __inline__ __attribute__((__always_inline__)) void tdefl_record_literal(tdefl_compressor *d, mz_uint8 lit)
    {
        d->m_total_lz_bytes++;
        *d->m_pLZ_code_buf++ = lit;
        *d->m_pLZ_flags = (mz_uint8)(*d->m_pLZ_flags >> 1);
        if (--d->m_num_flags_left == 0)
        {
            d->m_num_flags_left = 8;
            d->m_pLZ_flags = d->m_pLZ_code_buf++;
        }
        d->m_huff_count[0][lit]++;
    }

    static __inline__ __attribute__((__always_inline__)) void tdefl_record_match(tdefl_compressor *d, mz_uint match_len, mz_uint match_dist)
    {
        mz_uint32 s0, s1;

        
       ((void) sizeof ((
       (match_len >= TDEFL_MIN_MATCH_LEN) && (match_dist >= 1) && (match_dist <= TDEFL_LZ_DICT_SIZE)
       ) ? 1 : 0), __extension__ ({ if (
       (match_len >= TDEFL_MIN_MATCH_LEN) && (match_dist >= 1) && (match_dist <= TDEFL_LZ_DICT_SIZE)
       ) ; else __assert_fail (
       "(match_len >= TDEFL_MIN_MATCH_LEN) && (match_dist >= 1) && (match_dist <= TDEFL_LZ_DICT_SIZE)"
       , "miniz.c", 1723, __extension__ __PRETTY_FUNCTION__); }))
                                                                                                               ;

        d->m_total_lz_bytes += match_len;

        d->m_pLZ_code_buf[0] = (mz_uint8)(match_len - TDEFL_MIN_MATCH_LEN);

        match_dist -= 1;
        d->m_pLZ_code_buf[1] = (mz_uint8)(match_dist & 0xFF);
        d->m_pLZ_code_buf[2] = (mz_uint8)(match_dist >> 8);
        d->m_pLZ_code_buf += 3;

        *d->m_pLZ_flags = (mz_uint8)((*d->m_pLZ_flags >> 1) | 0x80);
        if (--d->m_num_flags_left == 0)
        {
            d->m_num_flags_left = 8;
            d->m_pLZ_flags = d->m_pLZ_code_buf++;
        }

        s0 = s_tdefl_small_dist_sym[match_dist & 511];
        s1 = s_tdefl_large_dist_sym[(match_dist >> 8) & 127];
        d->m_huff_count[1][(match_dist < 512) ? s0 : s1]++;
        d->m_huff_count[0][s_tdefl_len_sym[match_len - TDEFL_MIN_MATCH_LEN]]++;
    }

    static mz_bool tdefl_compress_normal(tdefl_compressor *d)
    {
        const mz_uint8 *pSrc = d->m_pSrc;
        size_t src_buf_left = d->m_src_buf_left;
        tdefl_flush flush = d->m_flush;

        while ((src_buf_left) || ((flush) && (d->m_lookahead_size)))
        {
            mz_uint len_to_move, cur_match_dist, cur_match_len, cur_pos;

            if ((d->m_lookahead_size + d->m_dict_size) >= (TDEFL_MIN_MATCH_LEN - 1))
            {
                mz_uint dst_pos = (d->m_lookahead_pos + d->m_lookahead_size) & TDEFL_LZ_DICT_SIZE_MASK, ins_pos = d->m_lookahead_pos + d->m_lookahead_size - 2;
                mz_uint hash = (d->m_dict[ins_pos & TDEFL_LZ_DICT_SIZE_MASK] << TDEFL_LZ_HASH_SHIFT) ^ d->m_dict[(ins_pos + 1) & TDEFL_LZ_DICT_SIZE_MASK];
                mz_uint num_bytes_to_process = (mz_uint)(((src_buf_left) < (TDEFL_MAX_MATCH_LEN - d->m_lookahead_size)) ? (src_buf_left) : (TDEFL_MAX_MATCH_LEN - d->m_lookahead_size));
                const mz_uint8 *pSrc_end = pSrc ? pSrc + num_bytes_to_process : 
                                                                               ((void *)0)
                                                                                   ;
                src_buf_left -= num_bytes_to_process;
                d->m_lookahead_size += num_bytes_to_process;
                while (pSrc != pSrc_end)
                {
                    mz_uint8 c = *pSrc++;
                    d->m_dict[dst_pos] = c;
                    if (dst_pos < (TDEFL_MAX_MATCH_LEN - 1))
                        d->m_dict[TDEFL_LZ_DICT_SIZE + dst_pos] = c;
                    hash = ((hash << TDEFL_LZ_HASH_SHIFT) ^ c) & (TDEFL_LZ_HASH_SIZE - 1);
                    d->m_next[ins_pos & TDEFL_LZ_DICT_SIZE_MASK] = d->m_hash[hash];
                    d->m_hash[hash] = (mz_uint16)(ins_pos);
                    dst_pos = (dst_pos + 1) & TDEFL_LZ_DICT_SIZE_MASK;
                    ins_pos++;
                }
            }
            else
            {
                while ((src_buf_left) && (d->m_lookahead_size < TDEFL_MAX_MATCH_LEN))
                {
                    mz_uint8 c = *pSrc++;
                    mz_uint dst_pos = (d->m_lookahead_pos + d->m_lookahead_size) & TDEFL_LZ_DICT_SIZE_MASK;
                    src_buf_left--;
                    d->m_dict[dst_pos] = c;
                    if (dst_pos < (TDEFL_MAX_MATCH_LEN - 1))
                        d->m_dict[TDEFL_LZ_DICT_SIZE + dst_pos] = c;
                    if ((++d->m_lookahead_size + d->m_dict_size) >= TDEFL_MIN_MATCH_LEN)
                    {
                        mz_uint ins_pos = d->m_lookahead_pos + (d->m_lookahead_size - 1) - 2;
                        mz_uint hash = ((d->m_dict[ins_pos & TDEFL_LZ_DICT_SIZE_MASK] << (TDEFL_LZ_HASH_SHIFT * 2)) ^ (d->m_dict[(ins_pos + 1) & TDEFL_LZ_DICT_SIZE_MASK] << TDEFL_LZ_HASH_SHIFT) ^ c) & (TDEFL_LZ_HASH_SIZE - 1);
                        d->m_next[ins_pos & TDEFL_LZ_DICT_SIZE_MASK] = d->m_hash[hash];
                        d->m_hash[hash] = (mz_uint16)(ins_pos);
                    }
                }
            }
            d->m_dict_size = (((TDEFL_LZ_DICT_SIZE - d->m_lookahead_size) < (d->m_dict_size)) ? (TDEFL_LZ_DICT_SIZE - d->m_lookahead_size) : (d->m_dict_size));
            if ((!flush) && (d->m_lookahead_size < TDEFL_MAX_MATCH_LEN))
                break;


            len_to_move = 1;
            cur_match_dist = 0;
            cur_match_len = d->m_saved_match_len ? d->m_saved_match_len : (TDEFL_MIN_MATCH_LEN - 1);
            cur_pos = d->m_lookahead_pos & TDEFL_LZ_DICT_SIZE_MASK;
            if (d->m_flags & (TDEFL_RLE_MATCHES | TDEFL_FORCE_ALL_RAW_BLOCKS))
            {
                if ((d->m_dict_size) && (!(d->m_flags & TDEFL_FORCE_ALL_RAW_BLOCKS)))
                {
                    mz_uint8 c = d->m_dict[(cur_pos - 1) & TDEFL_LZ_DICT_SIZE_MASK];
                    cur_match_len = 0;
                    while (cur_match_len < d->m_lookahead_size)
                    {
                        if (d->m_dict[cur_pos + cur_match_len] != c)
                            break;
                        cur_match_len++;
                    }
                    if (cur_match_len < TDEFL_MIN_MATCH_LEN)
                        cur_match_len = 0;
                    else
                        cur_match_dist = 1;
                }
            }
            else
            {
                tdefl_find_match(d, d->m_lookahead_pos, d->m_dict_size, d->m_lookahead_size, &cur_match_dist, &cur_match_len);
            }
            if (((cur_match_len == TDEFL_MIN_MATCH_LEN) && (cur_match_dist >= 8U * 1024U)) || (cur_pos == cur_match_dist) || ((d->m_flags & TDEFL_FILTER_MATCHES) && (cur_match_len <= 5)))
            {
                cur_match_dist = cur_match_len = 0;
            }
            if (d->m_saved_match_len)
            {
                if (cur_match_len > d->m_saved_match_len)
                {
                    tdefl_record_literal(d, (mz_uint8)d->m_saved_lit);
                    if (cur_match_len >= 128)
                    {
                        tdefl_record_match(d, cur_match_len, cur_match_dist);
                        d->m_saved_match_len = 0;
                        len_to_move = cur_match_len;
                    }
                    else
                    {
                        d->m_saved_lit = d->m_dict[cur_pos];
                        d->m_saved_match_dist = cur_match_dist;
                        d->m_saved_match_len = cur_match_len;
                    }
                }
                else
                {
                    tdefl_record_match(d, d->m_saved_match_len, d->m_saved_match_dist);
                    len_to_move = d->m_saved_match_len - 1;
                    d->m_saved_match_len = 0;
                }
            }
            else if (!cur_match_dist)
                tdefl_record_literal(d, d->m_dict[(((cur_pos) < (sizeof(d->m_dict) - 1)) ? (cur_pos) : (sizeof(d->m_dict) - 1))]);
            else if ((d->m_greedy_parsing) || (d->m_flags & TDEFL_RLE_MATCHES) || (cur_match_len >= 128))
            {
                tdefl_record_match(d, cur_match_len, cur_match_dist);
                len_to_move = cur_match_len;
            }
            else
            {
                d->m_saved_lit = d->m_dict[(((cur_pos) < (sizeof(d->m_dict) - 1)) ? (cur_pos) : (sizeof(d->m_dict) - 1))];
                d->m_saved_match_dist = cur_match_dist;
                d->m_saved_match_len = cur_match_len;
            }

            d->m_lookahead_pos += len_to_move;
            
           ((void) sizeof ((
           d->m_lookahead_size >= len_to_move
           ) ? 1 : 0), __extension__ ({ if (
           d->m_lookahead_size >= len_to_move
           ) ; else __assert_fail (
           "d->m_lookahead_size >= len_to_move"
           , "miniz.c", 1872, __extension__ __PRETTY_FUNCTION__); }))
                                                        ;
            d->m_lookahead_size -= len_to_move;
            d->m_dict_size = (((d->m_dict_size + len_to_move) < ((mz_uint)TDEFL_LZ_DICT_SIZE)) ? (d->m_dict_size + len_to_move) : ((mz_uint)TDEFL_LZ_DICT_SIZE));

            if ((d->m_pLZ_code_buf > &d->m_lz_code_buf[TDEFL_LZ_CODE_BUF_SIZE - 8]) ||
                ((d->m_total_lz_bytes > 31 * 1024) && (((((mz_uint)(d->m_pLZ_code_buf - d->m_lz_code_buf) * 115) >> 7) >= d->m_total_lz_bytes) || (d->m_flags & TDEFL_FORCE_ALL_RAW_BLOCKS))))
            {
                int n;
                d->m_pSrc = pSrc;
                d->m_src_buf_left = src_buf_left;
                if ((n = tdefl_flush_block(d, 0)) != 0)
                    return (n < 0) ? (0) : (1);
            }
        }

        d->m_pSrc = pSrc;
        d->m_src_buf_left = src_buf_left;
        return (1);
    }

    static tdefl_status tdefl_flush_output_buffer(tdefl_compressor *d)
    {
        if (d->m_pIn_buf_size)
        {
            *d->m_pIn_buf_size = d->m_pSrc - (const mz_uint8 *)d->m_pIn_buf;
        }

        if (d->m_pOut_buf_size)
        {
            size_t n = (((*d->m_pOut_buf_size - d->m_out_buf_ofs) < (d->m_output_flush_remaining)) ? (*d->m_pOut_buf_size - d->m_out_buf_ofs) : (d->m_output_flush_remaining));
            memcpy((mz_uint8 *)d->m_pOut_buf + d->m_out_buf_ofs, d->m_output_buf + d->m_output_flush_ofs, n);
            d->m_output_flush_ofs += (mz_uint)n;
            d->m_output_flush_remaining -= (mz_uint)n;
            d->m_out_buf_ofs += n;

            *d->m_pOut_buf_size = d->m_out_buf_ofs;
        }

        return (d->m_finished && !d->m_output_flush_remaining) ? TDEFL_STATUS_DONE : TDEFL_STATUS_OKAY;
    }

    tdefl_status tdefl_compress(tdefl_compressor *d, const void *pIn_buf, size_t *pIn_buf_size, void *pOut_buf, size_t *pOut_buf_size, tdefl_flush flush)
    {
        if (!d)
        {
            if (pIn_buf_size)
                *pIn_buf_size = 0;
            if (pOut_buf_size)
                *pOut_buf_size = 0;
            return TDEFL_STATUS_BAD_PARAM;
        }

        d->m_pIn_buf = pIn_buf;
        d->m_pIn_buf_size = pIn_buf_size;
        d->m_pOut_buf = pOut_buf;
        d->m_pOut_buf_size = pOut_buf_size;
        d->m_pSrc = (const mz_uint8 *)(pIn_buf);
        d->m_src_buf_left = pIn_buf_size ? *pIn_buf_size : 0;
        d->m_out_buf_ofs = 0;
        d->m_flush = flush;

        if (((d->m_pPut_buf_func != 
                                   ((void *)0)
                                       ) == ((pOut_buf != 
                                                          ((void *)0)
                                                              ) || (pOut_buf_size != 
                                                                                     ((void *)0)
                                                                                         ))) || (d->m_prev_return_status != TDEFL_STATUS_OKAY) ||
            (d->m_wants_to_finish && (flush != TDEFL_FINISH)) || (pIn_buf_size && *pIn_buf_size && !pIn_buf) || (pOut_buf_size && *pOut_buf_size && !pOut_buf))
        {
            if (pIn_buf_size)
                *pIn_buf_size = 0;
            if (pOut_buf_size)
                *pOut_buf_size = 0;
            return (d->m_prev_return_status = TDEFL_STATUS_BAD_PARAM);
        }
        d->m_wants_to_finish |= (flush == TDEFL_FINISH);

        if ((d->m_output_flush_remaining) || (d->m_finished))
            return (d->m_prev_return_status = tdefl_flush_output_buffer(d));
        {
            if (!tdefl_compress_normal(d))
                return d->m_prev_return_status;
        }

        if ((d->m_flags & (TDEFL_WRITE_ZLIB_HEADER | TDEFL_COMPUTE_ADLER32)) && (pIn_buf))
            d->m_adler32 = (mz_uint32)mz_adler32(d->m_adler32, (const mz_uint8 *)pIn_buf, d->m_pSrc - (const mz_uint8 *)pIn_buf);

        if ((flush) && (!d->m_lookahead_size) && (!d->m_src_buf_left) && (!d->m_output_flush_remaining))
        {
            if (tdefl_flush_block(d, flush) < 0)
                return d->m_prev_return_status;
            d->m_finished = (flush == TDEFL_FINISH);
            if (flush == TDEFL_FULL_FLUSH)
            {
                memset((d->m_hash), 0, sizeof(d->m_hash));
                memset((d->m_next), 0, sizeof(d->m_next));
                d->m_dict_size = 0;
            }
        }

        return (d->m_prev_return_status = tdefl_flush_output_buffer(d));
    }

    tdefl_status tdefl_compress_buffer(tdefl_compressor *d, const void *pIn_buf, size_t in_buf_size, tdefl_flush flush)
    {
        
       ((void) sizeof ((
       d->m_pPut_buf_func
       ) ? 1 : 0), __extension__ ({ if (
       d->m_pPut_buf_func
       ) ; else __assert_fail (
       "d->m_pPut_buf_func"
       , "miniz.c", 1983, __extension__ __PRETTY_FUNCTION__); }))
                                    ;
        return tdefl_compress(d, pIn_buf, &in_buf_size, 
                                                       ((void *)0)
                                                           , 
                                                             ((void *)0)
                                                                 , flush);
    }

    tdefl_status tdefl_init(tdefl_compressor *d, tdefl_put_buf_func_ptr pPut_buf_func, void *pPut_buf_user, int flags)
    {
        d->m_pPut_buf_func = pPut_buf_func;
        d->m_pPut_buf_user = pPut_buf_user;
        d->m_flags = (mz_uint)(flags);
        d->m_max_probes[0] = 1 + ((flags & 0xFFF) + 2) / 3;
        d->m_greedy_parsing = (flags & TDEFL_GREEDY_PARSING_FLAG) != 0;
        d->m_max_probes[1] = 1 + (((flags & 0xFFF) >> 2) + 2) / 3;
        if (!(flags & TDEFL_NONDETERMINISTIC_PARSING_FLAG))
            memset((d->m_hash), 0, sizeof(d->m_hash));
        d->m_lookahead_pos = d->m_lookahead_size = d->m_dict_size = d->m_total_lz_bytes = d->m_lz_code_buf_dict_pos = d->m_bits_in = 0;
        d->m_output_flush_ofs = d->m_output_flush_remaining = d->m_finished = d->m_block_index = d->m_bit_buffer = d->m_wants_to_finish = 0;
        d->m_pLZ_code_buf = d->m_lz_code_buf + 1;
        d->m_pLZ_flags = d->m_lz_code_buf;
        *d->m_pLZ_flags = 0;
        d->m_num_flags_left = 8;
        d->m_pOutput_buf = d->m_output_buf;
        d->m_pOutput_buf_end = d->m_output_buf;
        d->m_prev_return_status = TDEFL_STATUS_OKAY;
        d->m_saved_match_dist = d->m_saved_match_len = d->m_saved_lit = 0;
        d->m_adler32 = 1;
        d->m_pIn_buf = 
                      ((void *)0)
                          ;
        d->m_pOut_buf = 
                       ((void *)0)
                           ;
        d->m_pIn_buf_size = 
                           ((void *)0)
                               ;
        d->m_pOut_buf_size = 
                            ((void *)0)
                                ;
        d->m_flush = TDEFL_NO_FLUSH;
        d->m_pSrc = 
                   ((void *)0)
                       ;
        d->m_src_buf_left = 0;
        d->m_out_buf_ofs = 0;
        if (!(flags & TDEFL_NONDETERMINISTIC_PARSING_FLAG))
            memset((d->m_dict), 0, sizeof(d->m_dict));
        memset(&d->m_huff_count[0][0], 0, sizeof(d->m_huff_count[0][0]) * TDEFL_MAX_HUFF_SYMBOLS_0);
        memset(&d->m_huff_count[1][0], 0, sizeof(d->m_huff_count[1][0]) * TDEFL_MAX_HUFF_SYMBOLS_1);
        return TDEFL_STATUS_OKAY;
    }

    tdefl_status tdefl_get_prev_return_status(tdefl_compressor *d)
    {
        return d->m_prev_return_status;
    }

    mz_uint32 tdefl_get_adler32(tdefl_compressor *d)
    {
        return d->m_adler32;
    }

    mz_bool tdefl_compress_mem_to_output(const void *pBuf, size_t buf_len, tdefl_put_buf_func_ptr pPut_buf_func, void *pPut_buf_user, int flags)
    {
        tdefl_compressor *pComp;
        mz_bool succeeded;
        if (((buf_len) && (!pBuf)) || (!pPut_buf_func))
            return (0);
        pComp = (tdefl_compressor *)malloc(sizeof(tdefl_compressor));
        if (!pComp)
            return (0);
        succeeded = (tdefl_init(pComp, pPut_buf_func, pPut_buf_user, flags) == TDEFL_STATUS_OKAY);
        succeeded = succeeded && (tdefl_compress_buffer(pComp, pBuf, buf_len, TDEFL_FINISH) == TDEFL_STATUS_DONE);
        free(pComp);
        return succeeded;
    }

    typedef struct
    {
        size_t m_size, m_capacity;
        mz_uint8 *m_pBuf;
        mz_bool m_expandable;
    } tdefl_output_buffer;

    static mz_bool tdefl_output_buffer_putter(const void *pBuf, int len, void *pUser)
    {
        tdefl_output_buffer *p = (tdefl_output_buffer *)pUser;
        size_t new_size = p->m_size + len;
        if (new_size > p->m_capacity)
        {
            size_t new_capacity = p->m_capacity;
            mz_uint8 *pNew_buf;
            if (!p->m_expandable)
                return (0);
            do
            {
                new_capacity = (((128U) > (new_capacity << 1U)) ? (128U) : (new_capacity << 1U));
            } while (new_size > new_capacity);
            pNew_buf = (mz_uint8 *)realloc(p->m_pBuf, new_capacity);
            if (!pNew_buf)
                return (0);
            p->m_pBuf = pNew_buf;
            p->m_capacity = new_capacity;
        }
        memcpy((mz_uint8 *)p->m_pBuf + p->m_size, pBuf, len);
        p->m_size = new_size;
        return (1);
    }

    void *tdefl_compress_mem_to_heap(const void *pSrc_buf, size_t src_buf_len, size_t *pOut_len, int flags)
    {
        tdefl_output_buffer out_buf;
        memset(&(out_buf), 0, sizeof(out_buf));
        if (!pOut_len)
            return (0);
        else
            *pOut_len = 0;
        out_buf.m_expandable = (1);
        if (!tdefl_compress_mem_to_output(pSrc_buf, src_buf_len, tdefl_output_buffer_putter, &out_buf, flags))
            return 
                  ((void *)0)
                      ;
        *pOut_len = out_buf.m_size;
        return out_buf.m_pBuf;
    }

    size_t tdefl_compress_mem_to_mem(void *pOut_buf, size_t out_buf_len, const void *pSrc_buf, size_t src_buf_len, int flags)
    {
        tdefl_output_buffer out_buf;
        memset(&(out_buf), 0, sizeof(out_buf));
        if (!pOut_buf)
            return 0;
        out_buf.m_pBuf = (mz_uint8 *)pOut_buf;
        out_buf.m_capacity = out_buf_len;
        if (!tdefl_compress_mem_to_output(pSrc_buf, src_buf_len, tdefl_output_buffer_putter, &out_buf, flags))
            return 0;
        return out_buf.m_size;
    }


    mz_uint tdefl_create_comp_flags_from_zip_params(int level, int window_bits, int strategy)
    {
        mz_uint comp_flags = s_tdefl_num_probes[(level >= 0) ? (((10) < (level)) ? (10) : (level)) : MZ_DEFAULT_LEVEL] | ((level <= 3) ? TDEFL_GREEDY_PARSING_FLAG : 0);
        if (window_bits > 0)
            comp_flags |= TDEFL_WRITE_ZLIB_HEADER;

        if (!level)
            comp_flags |= TDEFL_FORCE_ALL_RAW_BLOCKS;
        else if (strategy == MZ_FILTERED)
            comp_flags |= TDEFL_FILTER_MATCHES;
        else if (strategy == MZ_HUFFMAN_ONLY)
            comp_flags &= ~TDEFL_MAX_PROBES_MASK;
        else if (strategy == MZ_FIXED)
            comp_flags |= TDEFL_FORCE_ALL_STATIC_BLOCKS;
        else if (strategy == MZ_RLE)
            comp_flags |= TDEFL_RLE_MATCHES;

        return comp_flags;
    }
    void *tdefl_write_image_to_png_file_in_memory_ex(const void *pImage, int w, int h, int num_chans, size_t *pLen_out, mz_uint level, mz_bool flip)
    {

        static const mz_uint s_tdefl_png_num_probes[11] = { 0, 1, 6, 32, 16, 32, 128, 256, 512, 768, 1500 };
        tdefl_compressor *pComp = (tdefl_compressor *)malloc(sizeof(tdefl_compressor));
        tdefl_output_buffer out_buf;
        int i, bpl = w * num_chans, y, z;
        mz_uint32 c;
        *pLen_out = 0;
        if (!pComp)
            return 
                  ((void *)0)
                      ;
        memset(&(out_buf), 0, sizeof(out_buf));
        out_buf.m_expandable = (1);
        out_buf.m_capacity = 57 + (((64) > ((1 + bpl) * h)) ? (64) : ((1 + bpl) * h));
        if (
           ((void *)0) 
                == (out_buf.m_pBuf = (mz_uint8 *)malloc(out_buf.m_capacity)))
        {
            free(pComp);
            return 
                  ((void *)0)
                      ;
        }

        for (z = 41; z; --z)
            tdefl_output_buffer_putter(&z, 1, &out_buf);

        tdefl_init(pComp, tdefl_output_buffer_putter, &out_buf, s_tdefl_png_num_probes[(((10) < (level)) ? (10) : (level))] | TDEFL_WRITE_ZLIB_HEADER);
        for (y = 0; y < h; ++y)
        {
            tdefl_compress_buffer(pComp, &z, 1, TDEFL_NO_FLUSH);
            tdefl_compress_buffer(pComp, (mz_uint8 *)pImage + (flip ? (h - 1 - y) : y) * bpl, bpl, TDEFL_NO_FLUSH);
        }
        if (tdefl_compress_buffer(pComp, 
                                        ((void *)0)
                                            , 0, TDEFL_FINISH) != TDEFL_STATUS_DONE)
        {
            free(pComp);
            free(out_buf.m_pBuf);
            return 
                  ((void *)0)
                      ;
        }

        *pLen_out = out_buf.m_size - 41;
        {
            static const mz_uint8 chans[] = { 0x00, 0x00, 0x04, 0x02, 0x06 };
            mz_uint8 pnghdr[41] = { 0x89, 0x50, 0x4e, 0x47, 0x0d,
                                    0x0a, 0x1a, 0x0a, 0x00, 0x00,
                                    0x00, 0x0d, 0x49, 0x48, 0x44,
                                    0x52, 0x00, 0x00, 0x00, 0x00,
                                    0x00, 0x00, 0x00, 0x00, 0x08,
                                    0x00, 0x00, 0x00, 0x00, 0x00,
                                    0x00, 0x00, 0x00, 0x00, 0x00,
                                    0x00, 0x00, 0x49, 0x44, 0x41,
                                    0x54 };
            pnghdr[18] = (mz_uint8)(w >> 8);
            pnghdr[19] = (mz_uint8)w;
            pnghdr[22] = (mz_uint8)(h >> 8);
            pnghdr[23] = (mz_uint8)h;
            pnghdr[25] = chans[num_chans];
            pnghdr[33] = (mz_uint8)(*pLen_out >> 24);
            pnghdr[34] = (mz_uint8)(*pLen_out >> 16);
            pnghdr[35] = (mz_uint8)(*pLen_out >> 8);
            pnghdr[36] = (mz_uint8)*pLen_out;
            c = (mz_uint32)mz_crc32((0), pnghdr + 12, 17);
            for (i = 0; i < 4; ++i, c <<= 8)
                ((mz_uint8 *)(pnghdr + 29))[i] = (mz_uint8)(c >> 24);
            memcpy(out_buf.m_pBuf, pnghdr, 41);
        }

        if (!tdefl_output_buffer_putter("\0\0\0\0\0\0\0\0\x49\x45\x4e\x44\xae\x42\x60\x82", 16, &out_buf))
        {
            *pLen_out = 0;
            free(pComp);
            free(out_buf.m_pBuf);
            return 
                  ((void *)0)
                      ;
        }
        c = (mz_uint32)mz_crc32((0), out_buf.m_pBuf + 41 - 4, *pLen_out + 4);
        for (i = 0; i < 4; ++i, c <<= 8)
            (out_buf.m_pBuf + out_buf.m_size - 16)[i] = (mz_uint8)(c >> 24);

        *pLen_out += 57;
        free(pComp);
        return out_buf.m_pBuf;
    }
    void *tdefl_write_image_to_png_file_in_memory(const void *pImage, int w, int h, int num_chans, size_t *pLen_out)
    {

        return tdefl_write_image_to_png_file_in_memory_ex(pImage, w, h, num_chans, pLen_out, 6, (0));
    }





    tdefl_compressor *tdefl_compressor_alloc(void)
    {
        return (tdefl_compressor *)malloc(sizeof(tdefl_compressor));
    }

    void tdefl_compressor_free(tdefl_compressor *pComp)
    {
        free(pComp);
    }
    static void tinfl_clear_tree(tinfl_decompressor *r)
    {
        if (r->m_type == 0)
            memset((r->m_tree_0), 0, sizeof(r->m_tree_0));
        else if (r->m_type == 1)
            memset((r->m_tree_1), 0, sizeof(r->m_tree_1));
        else
            memset((r->m_tree_2), 0, sizeof(r->m_tree_2));
    }

    tinfl_status tinfl_decompress(tinfl_decompressor *r, const mz_uint8 *pIn_buf_next, size_t *pIn_buf_size, mz_uint8 *pOut_buf_start, mz_uint8 *pOut_buf_next, size_t *pOut_buf_size, const mz_uint32 decomp_flags)
    {
        static const mz_uint16 s_length_base[31] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258, 0, 0 };
        static const mz_uint8 s_length_extra[31] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0, 0, 0 };
        static const mz_uint16 s_dist_base[32] = { 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577, 0, 0 };
        static const mz_uint8 s_dist_extra[32] = { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };
        static const mz_uint8 s_length_dezigzag[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };
        static const mz_uint16 s_min_table_sizes[3] = { 257, 1, 4 };

        mz_int16 *pTrees[3];
        mz_uint8 *pCode_sizes[3];

        tinfl_status status = TINFL_STATUS_FAILED;
        mz_uint32 num_bits, dist, counter, num_extra;
        tinfl_bit_buf_t bit_buf;
        const mz_uint8 *pIn_buf_cur = pIn_buf_next, *const pIn_buf_end = pIn_buf_next + *pIn_buf_size;
        mz_uint8 *pOut_buf_cur = pOut_buf_next, *const pOut_buf_end = pOut_buf_next ? pOut_buf_next + *pOut_buf_size : 
                                                                                                                      ((void *)0)
                                                                                                                          ;
        size_t out_buf_size_mask = (decomp_flags & TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF) ? (size_t)-1 : ((pOut_buf_next - pOut_buf_start) + *pOut_buf_size) - 1, dist_from_out_buf_start;


        if (((out_buf_size_mask + 1) & out_buf_size_mask) || (pOut_buf_next < pOut_buf_start))
        {
            *pIn_buf_size = *pOut_buf_size = 0;
            return TINFL_STATUS_BAD_PARAM;
        }

        pTrees[0] = r->m_tree_0;
        pTrees[1] = r->m_tree_1;
        pTrees[2] = r->m_tree_2;
        pCode_sizes[0] = r->m_code_size_0;
        pCode_sizes[1] = r->m_code_size_1;
        pCode_sizes[2] = r->m_code_size_2;

        num_bits = r->m_num_bits;
        bit_buf = r->m_bit_buf;
        dist = r->m_dist;
        counter = r->m_counter;
        num_extra = r->m_num_extra;
        dist_from_out_buf_start = r->m_dist_from_out_buf_start;
        switch (r->m_state) { case 0:

        bit_buf = num_bits = dist = counter = num_extra = r->m_zhdr0 = r->m_zhdr1 = 0;
        r->m_z_adler32 = r->m_check_adler32 = 1;
        if (decomp_flags & TINFL_FLAG_PARSE_ZLIB_HEADER)
        {
            do { while (pIn_buf_cur >= pIn_buf_end) { do { status = (decomp_flags & TINFL_FLAG_HAS_MORE_INPUT) ? TINFL_STATUS_NEEDS_MORE_INPUT : TINFL_STATUS_FAILED_CANNOT_MAKE_PROGRESS; r->m_state = 1; goto common_exit; case 1:; } while (0); } r->m_zhdr0 = *pIn_buf_cur++; } while (0);
            do { while (pIn_buf_cur >= pIn_buf_end) { do { status = (decomp_flags & TINFL_FLAG_HAS_MORE_INPUT) ? TINFL_STATUS_NEEDS_MORE_INPUT : TINFL_STATUS_FAILED_CANNOT_MAKE_PROGRESS; r->m_state = 2; goto common_exit; case 2:; } while (0); } r->m_zhdr1 = *pIn_buf_cur++; } while (0);
            counter = (((r->m_zhdr0 * 256 + r->m_zhdr1) % 31 != 0) || (r->m_zhdr1 & 32) || ((r->m_zhdr0 & 15) != 8));
            if (!(decomp_flags & TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF))
                counter |= (((1U << (8U + (r->m_zhdr0 >> 4))) > 32768U) || ((out_buf_size_mask + 1) < (size_t)((size_t)1 << (8U + (r->m_zhdr0 >> 4)))));
            if (counter)
            {
                do { for (;;) { do { status = TINFL_STATUS_FAILED; r->m_state = 36; goto common_exit; case 36:; } while (0); } } while (0);
            }
        }

        do
        {
            do { if (num_bits < (mz_uint)(3)) { do { mz_uint c; do { while (pIn_buf_cur >= pIn_buf_end) { do { status = (decomp_flags & TINFL_FLAG_HAS_MORE_INPUT) ? TINFL_STATUS_NEEDS_MORE_INPUT : TINFL_STATUS_FAILED_CANNOT_MAKE_PROGRESS; r->m_state = 3; goto common_exit; case 3:; } while (0); } c = *pIn_buf_cur++; } while (0); bit_buf |= (((tinfl_bit_buf_t)c) << num_bits); num_bits += 8; } while (num_bits < (mz_uint)(3)); } r->m_final = bit_buf & ((1 << (3)) - 1); bit_buf >>= (3); num_bits -= (3); } while (0);
            r->m_type = r->m_final >> 1;
            if (r->m_type == 0)
            {
                do { if (num_bits < (mz_uint)(num_bits & 7)) { do { mz_uint c; do { while (pIn_buf_cur >= pIn_buf_end) { do { status = (decomp_flags & TINFL_FLAG_HAS_MORE_INPUT) ? TINFL_STATUS_NEEDS_MORE_INPUT : TINFL_STATUS_FAILED_CANNOT_MAKE_PROGRESS; r->m_state = 5; goto common_exit; case 5:; } while (0); } c = *pIn_buf_cur++; } while (0); bit_buf |= (((tinfl_bit_buf_t)c) << num_bits); num_bits += 8; } while (num_bits < (mz_uint)(num_bits & 7)); } bit_buf >>= (num_bits & 7); num_bits -= (num_bits & 7); } while (0);
                for (counter = 0; counter < 4; ++counter)
                {
                    if (num_bits)
                        do { if (num_bits < (mz_uint)(8)) { do { mz_uint c; do { while (pIn_buf_cur >= pIn_buf_end) { do { status = (decomp_flags & TINFL_FLAG_HAS_MORE_INPUT) ? TINFL_STATUS_NEEDS_MORE_INPUT : TINFL_STATUS_FAILED_CANNOT_MAKE_PROGRESS; r->m_state = 6; goto common_exit; case 6:; } while (0); } c = *pIn_buf_cur++; } while (0); bit_buf |= (((tinfl_bit_buf_t)c) << num_bits); num_bits += 8; } while (num_bits < (mz_uint)(8)); } r->m_raw_header[counter] = bit_buf & ((1 << (8)) - 1); bit_buf >>= (8); num_bits -= (8); } while (0);
                    else
                        do { while (pIn_buf_cur >= pIn_buf_end) { do { status = (decomp_flags & TINFL_FLAG_HAS_MORE_INPUT) ? TINFL_STATUS_NEEDS_MORE_INPUT : TINFL_STATUS_FAILED_CANNOT_MAKE_PROGRESS; r->m_state = 7; goto common_exit; case 7:; } while (0); } r->m_raw_header[counter] = *pIn_buf_cur++; } while (0);
                }
                if ((counter = (r->m_raw_header[0] | (r->m_raw_header[1] << 8))) != (mz_uint)(0xFFFF ^ (r->m_raw_header[2] | (r->m_raw_header[3] << 8))))
                {
                    do { for (;;) { do { status = TINFL_STATUS_FAILED; r->m_state = 39; goto common_exit; case 39:; } while (0); } } while (0);
                }
                while ((counter) && (num_bits))
                {
                    do { if (num_bits < (mz_uint)(8)) { do { mz_uint c; do { while (pIn_buf_cur >= pIn_buf_end) { do { status = (decomp_flags & TINFL_FLAG_HAS_MORE_INPUT) ? TINFL_STATUS_NEEDS_MORE_INPUT : TINFL_STATUS_FAILED_CANNOT_MAKE_PROGRESS; r->m_state = 51; goto common_exit; case 51:; } while (0); } c = *pIn_buf_cur++; } while (0); bit_buf |= (((tinfl_bit_buf_t)c) << num_bits); num_bits += 8; } while (num_bits < (mz_uint)(8)); } dist = bit_buf & ((1 << (8)) - 1); bit_buf >>= (8); num_bits -= (8); } while (0);
                    while (pOut_buf_cur >= pOut_buf_end)
                    {
                        do { status = TINFL_STATUS_HAS_MORE_OUTPUT; r->m_state = 52; goto common_exit; case 52:; } while (0);
                    }
                    *pOut_buf_cur++ = (mz_uint8)dist;
                    counter--;
                }
                while (counter)
                {
                    size_t n;
                    while (pOut_buf_cur >= pOut_buf_end)
                    {
                        do { status = TINFL_STATUS_HAS_MORE_OUTPUT; r->m_state = 9; goto common_exit; case 9:; } while (0);
                    }
                    while (pIn_buf_cur >= pIn_buf_end)
                    {
                        do { status = (decomp_flags & TINFL_FLAG_HAS_MORE_INPUT) ? TINFL_STATUS_NEEDS_MORE_INPUT : TINFL_STATUS_FAILED_CANNOT_MAKE_PROGRESS; r->m_state = 38; goto common_exit; case 38:; } while (0);
                    }
                    n = (((((((size_t)(pOut_buf_end - pOut_buf_cur)) < ((size_t)(pIn_buf_end - pIn_buf_cur))) ? ((size_t)(pOut_buf_end - pOut_buf_cur)) : ((size_t)(pIn_buf_end - pIn_buf_cur)))) < (counter)) ? (((((size_t)(pOut_buf_end - pOut_buf_cur)) < ((size_t)(pIn_buf_end - pIn_buf_cur))) ? ((size_t)(pOut_buf_end - pOut_buf_cur)) : ((size_t)(pIn_buf_end - pIn_buf_cur)))) : (counter));
                    memcpy(pOut_buf_cur, pIn_buf_cur, n);
                    pIn_buf_cur += n;
                    pOut_buf_cur += n;
                    counter -= (mz_uint)n;
                }
            }
            else if (r->m_type == 3)
            {
                do { for (;;) { do { status = TINFL_STATUS_FAILED; r->m_state = 10; goto common_exit; case 10:; } while (0); } } while (0);
            }
            else
            {
                if (r->m_type == 1)
                {
                    mz_uint8 *p = r->m_code_size_0;
                    mz_uint i;
                    r->m_table_sizes[0] = 288;
                    r->m_table_sizes[1] = 32;
                    memset(r->m_code_size_1, 5, 32);
                    for (i = 0; i <= 143; ++i)
                        *p++ = 8;
                    for (; i <= 255; ++i)
                        *p++ = 9;
                    for (; i <= 279; ++i)
                        *p++ = 7;
                    for (; i <= 287; ++i)
                        *p++ = 8;
                }
                else
                {
                    for (counter = 0; counter < 3; counter++)
                    {
                        do { if (num_bits < (mz_uint)("\05\05\04"[counter])) { do { mz_uint c; do { while (pIn_buf_cur >= pIn_buf_end) { do { status = (decomp_flags & TINFL_FLAG_HAS_MORE_INPUT) ? TINFL_STATUS_NEEDS_MORE_INPUT : TINFL_STATUS_FAILED_CANNOT_MAKE_PROGRESS; r->m_state = 11; goto common_exit; case 11:; } while (0); } c = *pIn_buf_cur++; } while (0); bit_buf |= (((tinfl_bit_buf_t)c) << num_bits); num_bits += 8; } while (num_bits < (mz_uint)("\05\05\04"[counter])); } r->m_table_sizes[counter] = bit_buf & ((1 << ("\05\05\04"[counter])) - 1); bit_buf >>= ("\05\05\04"[counter]); num_bits -= ("\05\05\04"[counter]); } while (0);
                        r->m_table_sizes[counter] += s_min_table_sizes[counter];
                    }
                    memset((r->m_code_size_2), 0, sizeof(r->m_code_size_2));
                    for (counter = 0; counter < r->m_table_sizes[2]; counter++)
                    {
                        mz_uint s;
                        do { if (num_bits < (mz_uint)(3)) { do { mz_uint c; do { while (pIn_buf_cur >= pIn_buf_end) { do { status = (decomp_flags & TINFL_FLAG_HAS_MORE_INPUT) ? TINFL_STATUS_NEEDS_MORE_INPUT : TINFL_STATUS_FAILED_CANNOT_MAKE_PROGRESS; r->m_state = 14; goto common_exit; case 14:; } while (0); } c = *pIn_buf_cur++; } while (0); bit_buf |= (((tinfl_bit_buf_t)c) << num_bits); num_bits += 8; } while (num_bits < (mz_uint)(3)); } s = bit_buf & ((1 << (3)) - 1); bit_buf >>= (3); num_bits -= (3); } while (0);
                        r->m_code_size_2[s_length_dezigzag[counter]] = (mz_uint8)s;
                    }
                    r->m_table_sizes[2] = 19;
                }
                for (; (int)r->m_type >= 0; r->m_type--)
                {
                    int tree_next, tree_cur;
                    mz_int16 *pLookUp;
                    mz_int16 *pTree;
                    mz_uint8 *pCode_size;
                    mz_uint i, j, used_syms, total, sym_index, next_code[17], total_syms[16];
                    pLookUp = r->m_look_up[r->m_type];
                    pTree = pTrees[r->m_type];
                    pCode_size = pCode_sizes[r->m_type];
                    memset((total_syms), 0, sizeof(total_syms));
                    memset(pLookUp, 0, sizeof(r->m_look_up[0]));
                    tinfl_clear_tree(r);
                    for (i = 0; i < r->m_table_sizes[r->m_type]; ++i)
                        total_syms[pCode_size[i]]++;
                    used_syms = 0, total = 0;
                    next_code[0] = next_code[1] = 0;
                    for (i = 1; i <= 15; ++i)
                    {
                        used_syms += total_syms[i];
                        next_code[i + 1] = (total = ((total + total_syms[i]) << 1));
                    }
                    if ((65536 != total) && (used_syms > 1))
                    {
                        do { for (;;) { do { status = TINFL_STATUS_FAILED; r->m_state = 35; goto common_exit; case 35:; } while (0); } } while (0);
                    }
                    for (tree_next = -1, sym_index = 0; sym_index < r->m_table_sizes[r->m_type]; ++sym_index)
                    {
                        mz_uint rev_code = 0, l, cur_code, code_size = pCode_size[sym_index];
                        if (!code_size)
                            continue;
                        cur_code = next_code[code_size]++;
                        for (l = code_size; l > 0; l--, cur_code >>= 1)
                            rev_code = (rev_code << 1) | (cur_code & 1);
                        if (code_size <= TINFL_FAST_LOOKUP_BITS)
                        {
                            mz_int16 k = (mz_int16)((code_size << 9) | sym_index);
                            while (rev_code < TINFL_FAST_LOOKUP_SIZE)
                            {
                                pLookUp[rev_code] = k;
                                rev_code += (1 << code_size);
                            }
                            continue;
                        }
                        if (0 == (tree_cur = pLookUp[rev_code & (TINFL_FAST_LOOKUP_SIZE - 1)]))
                        {
                            pLookUp[rev_code & (TINFL_FAST_LOOKUP_SIZE - 1)] = (mz_int16)tree_next;
                            tree_cur = tree_next;
                            tree_next -= 2;
                        }
                        rev_code >>= (TINFL_FAST_LOOKUP_BITS - 1);
                        for (j = code_size; j > (TINFL_FAST_LOOKUP_BITS + 1); j--)
                        {
                            tree_cur -= ((rev_code >>= 1) & 1);
                            if (!pTree[-tree_cur - 1])
                            {
                                pTree[-tree_cur - 1] = (mz_int16)tree_next;
                                tree_cur = tree_next;
                                tree_next -= 2;
                            }
                            else
                                tree_cur = pTree[-tree_cur - 1];
                        }
                        tree_cur -= ((rev_code >>= 1) & 1);
                        pTree[-tree_cur - 1] = (mz_int16)sym_index;
                    }
                    if (r->m_type == 2)
                    {
                        for (counter = 0; counter < (r->m_table_sizes[0] + r->m_table_sizes[1]);)
                        {
                            mz_uint s;
                            do { int temp; mz_uint code_len, c; if (num_bits < 15) { if ((pIn_buf_end - pIn_buf_cur) < 2) { do { temp = r->m_look_up[2][bit_buf & (TINFL_FAST_LOOKUP_SIZE - 1)]; if (temp >= 0) { code_len = temp >> 9; if ((code_len) && (num_bits >= code_len)) break; } else if (num_bits > TINFL_FAST_LOOKUP_BITS) { code_len = TINFL_FAST_LOOKUP_BITS; do { temp = r->m_tree_2[~temp + ((bit_buf >> code_len++) & 1)]; } while ((temp < 0) && (num_bits >= (code_len + 1))); if (temp >= 0) break; } do { while (pIn_buf_cur >= pIn_buf_end) { do { status = (decomp_flags & TINFL_FLAG_HAS_MORE_INPUT) ? TINFL_STATUS_NEEDS_MORE_INPUT : TINFL_STATUS_FAILED_CANNOT_MAKE_PROGRESS; r->m_state = 16; goto common_exit; case 16:; } while (0); } c = *pIn_buf_cur++; } while (0); bit_buf |= (((tinfl_bit_buf_t)c) << num_bits); num_bits += 8; } while (num_bits < 15);; } else { bit_buf |= (((tinfl_bit_buf_t)pIn_buf_cur[0]) << num_bits) | (((tinfl_bit_buf_t)pIn_buf_cur[1]) << (num_bits + 8)); pIn_buf_cur += 2; num_bits += 16; } } if ((temp = r->m_look_up[2][bit_buf & (TINFL_FAST_LOOKUP_SIZE - 1)]) >= 0) code_len = temp >> 9, temp &= 511; else { code_len = TINFL_FAST_LOOKUP_BITS; do { temp = r->m_tree_2[~temp + ((bit_buf >> code_len++) & 1)]; } while (temp < 0); } dist = temp; bit_buf >>= code_len; num_bits -= code_len; } while (0);
                            if (dist < 16)
                            {
                                r->m_len_codes[counter++] = (mz_uint8)dist;
                                continue;
                            }
                            if ((dist == 16) && (!counter))
                            {
                                do { for (;;) { do { status = TINFL_STATUS_FAILED; r->m_state = 17; goto common_exit; case 17:; } while (0); } } while (0);
                            }
                            num_extra = "\02\03\07"[dist - 16];
                            do { if (num_bits < (mz_uint)(num_extra)) { do { mz_uint c; do { while (pIn_buf_cur >= pIn_buf_end) { do { status = (decomp_flags & TINFL_FLAG_HAS_MORE_INPUT) ? TINFL_STATUS_NEEDS_MORE_INPUT : TINFL_STATUS_FAILED_CANNOT_MAKE_PROGRESS; r->m_state = 18; goto common_exit; case 18:; } while (0); } c = *pIn_buf_cur++; } while (0); bit_buf |= (((tinfl_bit_buf_t)c) << num_bits); num_bits += 8; } while (num_bits < (mz_uint)(num_extra)); } s = bit_buf & ((1 << (num_extra)) - 1); bit_buf >>= (num_extra); num_bits -= (num_extra); } while (0);
                            s += "\03\03\013"[dist - 16];
                            memset(r->m_len_codes + counter, (dist == 16) ? r->m_len_codes[counter - 1] : 0, s);
                            counter += s;
                        }
                        if ((r->m_table_sizes[0] + r->m_table_sizes[1]) != counter)
                        {
                            do { for (;;) { do { status = TINFL_STATUS_FAILED; r->m_state = 21; goto common_exit; case 21:; } while (0); } } while (0);
                        }
                        memcpy(r->m_code_size_0, r->m_len_codes, r->m_table_sizes[0]);
                        memcpy(r->m_code_size_1, r->m_len_codes + r->m_table_sizes[0], r->m_table_sizes[1]);
                    }
                }
                for (;;)
                {
                    mz_uint8 *pSrc;
                    for (;;)
                    {
                        if (((pIn_buf_end - pIn_buf_cur) < 4) || ((pOut_buf_end - pOut_buf_cur) < 2))
                        {
                            do { int temp; mz_uint code_len, c; if (num_bits < 15) { if ((pIn_buf_end - pIn_buf_cur) < 2) { do { temp = r->m_look_up[0][bit_buf & (TINFL_FAST_LOOKUP_SIZE - 1)]; if (temp >= 0) { code_len = temp >> 9; if ((code_len) && (num_bits >= code_len)) break; } else if (num_bits > TINFL_FAST_LOOKUP_BITS) { code_len = TINFL_FAST_LOOKUP_BITS; do { temp = r->m_tree_0[~temp + ((bit_buf >> code_len++) & 1)]; } while ((temp < 0) && (num_bits >= (code_len + 1))); if (temp >= 0) break; } do { while (pIn_buf_cur >= pIn_buf_end) { do { status = (decomp_flags & TINFL_FLAG_HAS_MORE_INPUT) ? TINFL_STATUS_NEEDS_MORE_INPUT : TINFL_STATUS_FAILED_CANNOT_MAKE_PROGRESS; r->m_state = 23; goto common_exit; case 23:; } while (0); } c = *pIn_buf_cur++; } while (0); bit_buf |= (((tinfl_bit_buf_t)c) << num_bits); num_bits += 8; } while (num_bits < 15);; } else { bit_buf |= (((tinfl_bit_buf_t)pIn_buf_cur[0]) << num_bits) | (((tinfl_bit_buf_t)pIn_buf_cur[1]) << (num_bits + 8)); pIn_buf_cur += 2; num_bits += 16; } } if ((temp = r->m_look_up[0][bit_buf & (TINFL_FAST_LOOKUP_SIZE - 1)]) >= 0) code_len = temp >> 9, temp &= 511; else { code_len = TINFL_FAST_LOOKUP_BITS; do { temp = r->m_tree_0[~temp + ((bit_buf >> code_len++) & 1)]; } while (temp < 0); } counter = temp; bit_buf >>= code_len; num_bits -= code_len; } while (0);
                            if (counter >= 256)
                                break;
                            while (pOut_buf_cur >= pOut_buf_end)
                            {
                                do { status = TINFL_STATUS_HAS_MORE_OUTPUT; r->m_state = 24; goto common_exit; case 24:; } while (0);
                            }
                            *pOut_buf_cur++ = (mz_uint8)counter;
                        }
                        else
                        {
                            int sym2;
                            mz_uint code_len;

                            if (num_bits < 30)
                            {
                                bit_buf |= (((tinfl_bit_buf_t)((mz_uint32)(((const mz_uint8 *)(pIn_buf_cur))[0]) | ((mz_uint32)(((const mz_uint8 *)(pIn_buf_cur))[1]) << 8U) | ((mz_uint32)(((const mz_uint8 *)(pIn_buf_cur))[2]) << 16U) | ((mz_uint32)(((const mz_uint8 *)(pIn_buf_cur))[3]) << 24U))) << num_bits);
                                pIn_buf_cur += 4;
                                num_bits += 32;
                            }
                            if ((sym2 = r->m_look_up[0][bit_buf & (TINFL_FAST_LOOKUP_SIZE - 1)]) >= 0)
                                code_len = sym2 >> 9;
                            else
                            {
                                code_len = TINFL_FAST_LOOKUP_BITS;
                                do
                                {
                                    sym2 = r->m_tree_0[~sym2 + ((bit_buf >> code_len++) & 1)];
                                } while (sym2 < 0);
                            }
                            counter = sym2;
                            bit_buf >>= code_len;
                            num_bits -= code_len;
                            if (counter & 256)
                                break;
                            if ((sym2 = r->m_look_up[0][bit_buf & (TINFL_FAST_LOOKUP_SIZE - 1)]) >= 0)
                                code_len = sym2 >> 9;
                            else
                            {
                                code_len = TINFL_FAST_LOOKUP_BITS;
                                do
                                {
                                    sym2 = r->m_tree_0[~sym2 + ((bit_buf >> code_len++) & 1)];
                                } while (sym2 < 0);
                            }
                            bit_buf >>= code_len;
                            num_bits -= code_len;

                            pOut_buf_cur[0] = (mz_uint8)counter;
                            if (sym2 & 256)
                            {
                                pOut_buf_cur++;
                                counter = sym2;
                                break;
                            }
                            pOut_buf_cur[1] = (mz_uint8)sym2;
                            pOut_buf_cur += 2;
                        }
                    }
                    if ((counter &= 511) == 256)
                        break;

                    num_extra = s_length_extra[counter - 257];
                    counter = s_length_base[counter - 257];
                    if (num_extra)
                    {
                        mz_uint extra_bits;
                        do { if (num_bits < (mz_uint)(num_extra)) { do { mz_uint c; do { while (pIn_buf_cur >= pIn_buf_end) { do { status = (decomp_flags & TINFL_FLAG_HAS_MORE_INPUT) ? TINFL_STATUS_NEEDS_MORE_INPUT : TINFL_STATUS_FAILED_CANNOT_MAKE_PROGRESS; r->m_state = 25; goto common_exit; case 25:; } while (0); } c = *pIn_buf_cur++; } while (0); bit_buf |= (((tinfl_bit_buf_t)c) << num_bits); num_bits += 8; } while (num_bits < (mz_uint)(num_extra)); } extra_bits = bit_buf & ((1 << (num_extra)) - 1); bit_buf >>= (num_extra); num_bits -= (num_extra); } while (0);
                        counter += extra_bits;
                    }

                    do { int temp; mz_uint code_len, c; if (num_bits < 15) { if ((pIn_buf_end - pIn_buf_cur) < 2) { do { temp = r->m_look_up[1][bit_buf & (TINFL_FAST_LOOKUP_SIZE - 1)]; if (temp >= 0) { code_len = temp >> 9; if ((code_len) && (num_bits >= code_len)) break; } else if (num_bits > TINFL_FAST_LOOKUP_BITS) { code_len = TINFL_FAST_LOOKUP_BITS; do { temp = r->m_tree_1[~temp + ((bit_buf >> code_len++) & 1)]; } while ((temp < 0) && (num_bits >= (code_len + 1))); if (temp >= 0) break; } do { while (pIn_buf_cur >= pIn_buf_end) { do { status = (decomp_flags & TINFL_FLAG_HAS_MORE_INPUT) ? TINFL_STATUS_NEEDS_MORE_INPUT : TINFL_STATUS_FAILED_CANNOT_MAKE_PROGRESS; r->m_state = 26; goto common_exit; case 26:; } while (0); } c = *pIn_buf_cur++; } while (0); bit_buf |= (((tinfl_bit_buf_t)c) << num_bits); num_bits += 8; } while (num_bits < 15);; } else { bit_buf |= (((tinfl_bit_buf_t)pIn_buf_cur[0]) << num_bits) | (((tinfl_bit_buf_t)pIn_buf_cur[1]) << (num_bits + 8)); pIn_buf_cur += 2; num_bits += 16; } } if ((temp = r->m_look_up[1][bit_buf & (TINFL_FAST_LOOKUP_SIZE - 1)]) >= 0) code_len = temp >> 9, temp &= 511; else { code_len = TINFL_FAST_LOOKUP_BITS; do { temp = r->m_tree_1[~temp + ((bit_buf >> code_len++) & 1)]; } while (temp < 0); } dist = temp; bit_buf >>= code_len; num_bits -= code_len; } while (0);
                    num_extra = s_dist_extra[dist];
                    dist = s_dist_base[dist];
                    if (num_extra)
                    {
                        mz_uint extra_bits;
                        do { if (num_bits < (mz_uint)(num_extra)) { do { mz_uint c; do { while (pIn_buf_cur >= pIn_buf_end) { do { status = (decomp_flags & TINFL_FLAG_HAS_MORE_INPUT) ? TINFL_STATUS_NEEDS_MORE_INPUT : TINFL_STATUS_FAILED_CANNOT_MAKE_PROGRESS; r->m_state = 27; goto common_exit; case 27:; } while (0); } c = *pIn_buf_cur++; } while (0); bit_buf |= (((tinfl_bit_buf_t)c) << num_bits); num_bits += 8; } while (num_bits < (mz_uint)(num_extra)); } extra_bits = bit_buf & ((1 << (num_extra)) - 1); bit_buf >>= (num_extra); num_bits -= (num_extra); } while (0);
                        dist += extra_bits;
                    }

                    dist_from_out_buf_start = pOut_buf_cur - pOut_buf_start;
                    if ((dist == 0 || dist > dist_from_out_buf_start || dist_from_out_buf_start == 0) && (decomp_flags & TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF))
                    {
                        do { for (;;) { do { status = TINFL_STATUS_FAILED; r->m_state = 37; goto common_exit; case 37:; } while (0); } } while (0);
                    }

                    pSrc = pOut_buf_start + ((dist_from_out_buf_start - dist) & out_buf_size_mask);

                    if (((((pOut_buf_cur) > (pSrc)) ? (pOut_buf_cur) : (pSrc)) + counter) > pOut_buf_end)
                    {
                        while (counter--)
                        {
                            while (pOut_buf_cur >= pOut_buf_end)
                            {
                                do { status = TINFL_STATUS_HAS_MORE_OUTPUT; r->m_state = 53; goto common_exit; case 53:; } while (0);
                            }
                            *pOut_buf_cur++ = pOut_buf_start[(dist_from_out_buf_start++ - dist) & out_buf_size_mask];
                        }
                        continue;
                    }
                    while (counter > 2)
                    {
                        pOut_buf_cur[0] = pSrc[0];
                        pOut_buf_cur[1] = pSrc[1];
                        pOut_buf_cur[2] = pSrc[2];
                        pOut_buf_cur += 3;
                        pSrc += 3;
                        counter -= 3;
                    }
                    if (counter > 0)
                    {
                        pOut_buf_cur[0] = pSrc[0];
                        if (counter > 1)
                            pOut_buf_cur[1] = pSrc[1];
                        pOut_buf_cur += counter;
                    }
                }
            }
        } while (!(r->m_final & 1));



        do { if (num_bits < (mz_uint)(num_bits & 7)) { do { mz_uint c; do { while (pIn_buf_cur >= pIn_buf_end) { do { status = (decomp_flags & TINFL_FLAG_HAS_MORE_INPUT) ? TINFL_STATUS_NEEDS_MORE_INPUT : TINFL_STATUS_FAILED_CANNOT_MAKE_PROGRESS; r->m_state = 32; goto common_exit; case 32:; } while (0); } c = *pIn_buf_cur++; } while (0); bit_buf |= (((tinfl_bit_buf_t)c) << num_bits); num_bits += 8; } while (num_bits < (mz_uint)(num_bits & 7)); } bit_buf >>= (num_bits & 7); num_bits -= (num_bits & 7); } while (0);
        while ((pIn_buf_cur > pIn_buf_next) && (num_bits >= 8))
        {
            --pIn_buf_cur;
            num_bits -= 8;
        }
        bit_buf &= ~(~(tinfl_bit_buf_t)0 << num_bits);
        
       ((void) sizeof ((
       !num_bits
       ) ? 1 : 0), __extension__ ({ if (
       !num_bits
       ) ; else __assert_fail (
       "!num_bits"
       , "miniz.c", 2847, __extension__ __PRETTY_FUNCTION__); }))
                           ;

        if (decomp_flags & TINFL_FLAG_PARSE_ZLIB_HEADER)
        {
            for (counter = 0; counter < 4; ++counter)
            {
                mz_uint s;
                if (num_bits)
                    do { if (num_bits < (mz_uint)(8)) { do { mz_uint c; do { while (pIn_buf_cur >= pIn_buf_end) { do { status = (decomp_flags & TINFL_FLAG_HAS_MORE_INPUT) ? TINFL_STATUS_NEEDS_MORE_INPUT : TINFL_STATUS_FAILED_CANNOT_MAKE_PROGRESS; r->m_state = 41; goto common_exit; case 41:; } while (0); } c = *pIn_buf_cur++; } while (0); bit_buf |= (((tinfl_bit_buf_t)c) << num_bits); num_bits += 8; } while (num_bits < (mz_uint)(8)); } s = bit_buf & ((1 << (8)) - 1); bit_buf >>= (8); num_bits -= (8); } while (0);
                else
                    do { while (pIn_buf_cur >= pIn_buf_end) { do { status = (decomp_flags & TINFL_FLAG_HAS_MORE_INPUT) ? TINFL_STATUS_NEEDS_MORE_INPUT : TINFL_STATUS_FAILED_CANNOT_MAKE_PROGRESS; r->m_state = 42; goto common_exit; case 42:; } while (0); } s = *pIn_buf_cur++; } while (0);
                r->m_z_adler32 = (r->m_z_adler32 << 8) | s;
            }
        }
        do { for (;;) { do { status = TINFL_STATUS_DONE; r->m_state = 34; goto common_exit; case 34:; } while (0); } } while (0);

        }

    common_exit:



        if ((status != TINFL_STATUS_NEEDS_MORE_INPUT) && (status != TINFL_STATUS_FAILED_CANNOT_MAKE_PROGRESS))
        {
            while ((pIn_buf_cur > pIn_buf_next) && (num_bits >= 8))
            {
                --pIn_buf_cur;
                num_bits -= 8;
            }
        }
        r->m_num_bits = num_bits;
        r->m_bit_buf = bit_buf & ~(~(tinfl_bit_buf_t)0 << num_bits);
        r->m_dist = dist;
        r->m_counter = counter;
        r->m_num_extra = num_extra;
        r->m_dist_from_out_buf_start = dist_from_out_buf_start;
        *pIn_buf_size = pIn_buf_cur - pIn_buf_next;
        *pOut_buf_size = pOut_buf_cur - pOut_buf_next;
        if ((decomp_flags & (TINFL_FLAG_PARSE_ZLIB_HEADER | TINFL_FLAG_COMPUTE_ADLER32)) && (status >= 0))
        {
            const mz_uint8 *ptr = pOut_buf_next;
            size_t buf_len = *pOut_buf_size;
            mz_uint32 i, s1 = r->m_check_adler32 & 0xffff, s2 = r->m_check_adler32 >> 16;
            size_t block_len = buf_len % 5552;
            while (buf_len)
            {
                for (i = 0; i + 7 < block_len; i += 8, ptr += 8)
                {
                    s1 += ptr[0], s2 += s1;
                    s1 += ptr[1], s2 += s1;
                    s1 += ptr[2], s2 += s1;
                    s1 += ptr[3], s2 += s1;
                    s1 += ptr[4], s2 += s1;
                    s1 += ptr[5], s2 += s1;
                    s1 += ptr[6], s2 += s1;
                    s1 += ptr[7], s2 += s1;
                }
                for (; i < block_len; ++i)
                    s1 += *ptr++, s2 += s1;
                s1 %= 65521U, s2 %= 65521U;
                buf_len -= block_len;
                block_len = 5552;
            }
            r->m_check_adler32 = (s2 << 16) + s1;
            if ((status == TINFL_STATUS_DONE) && (decomp_flags & TINFL_FLAG_PARSE_ZLIB_HEADER) && (r->m_check_adler32 != r->m_z_adler32))
                status = TINFL_STATUS_ADLER32_MISMATCH;
        }
        return status;
    }


    void *tinfl_decompress_mem_to_heap(const void *pSrc_buf, size_t src_buf_len, size_t *pOut_len, int flags)
    {
        tinfl_decompressor decomp;
        void *pBuf = 
                    ((void *)0)
                        , *pNew_buf;
        size_t src_buf_ofs = 0, out_buf_capacity = 0;
        *pOut_len = 0;
        do { (&decomp)->m_state = 0; } while (0);
        for (;;)
        {
            size_t src_buf_size = src_buf_len - src_buf_ofs, dst_buf_size = out_buf_capacity - *pOut_len, new_out_buf_capacity;
            tinfl_status status = tinfl_decompress(&decomp, (const mz_uint8 *)pSrc_buf + src_buf_ofs, &src_buf_size, (mz_uint8 *)pBuf, pBuf ? (mz_uint8 *)pBuf + *pOut_len : 
                                                                                                                                                                            ((void *)0)
                                                                                                                                                                                , &dst_buf_size,
                                                   (flags & ~TINFL_FLAG_HAS_MORE_INPUT) | TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
            if ((status < 0) || (status == TINFL_STATUS_NEEDS_MORE_INPUT))
            {
                free(pBuf);
                *pOut_len = 0;
                return 
                      ((void *)0)
                          ;
            }
            src_buf_ofs += src_buf_size;
            *pOut_len += dst_buf_size;
            if (status == TINFL_STATUS_DONE)
                break;
            new_out_buf_capacity = out_buf_capacity * 2;
            if (new_out_buf_capacity < 128)
                new_out_buf_capacity = 128;
            pNew_buf = realloc(pBuf, new_out_buf_capacity);
            if (!pNew_buf)
            {
                free(pBuf);
                *pOut_len = 0;
                return 
                      ((void *)0)
                          ;
            }
            pBuf = pNew_buf;
            out_buf_capacity = new_out_buf_capacity;
        }
        return pBuf;
    }

    size_t tinfl_decompress_mem_to_mem(void *pOut_buf, size_t out_buf_len, const void *pSrc_buf, size_t src_buf_len, int flags)
    {
        tinfl_decompressor decomp;
        tinfl_status status;
        do { (&decomp)->m_state = 0; } while (0);
        status = tinfl_decompress(&decomp, (const mz_uint8 *)pSrc_buf, &src_buf_len, (mz_uint8 *)pOut_buf, (mz_uint8 *)pOut_buf, &out_buf_len, (flags & ~TINFL_FLAG_HAS_MORE_INPUT) | TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
        return (status != TINFL_STATUS_DONE) ? ((size_t)(-1)) : out_buf_len;
    }

    int tinfl_decompress_mem_to_callback(const void *pIn_buf, size_t *pIn_buf_size, tinfl_put_buf_func_ptr pPut_buf_func, void *pPut_buf_user, int flags)
    {
        int result = 0;
        tinfl_decompressor decomp;
        mz_uint8 *pDict = (mz_uint8 *)malloc(32768);
        size_t in_buf_ofs = 0, dict_ofs = 0;
        if (!pDict)
            return TINFL_STATUS_FAILED;
        memset(pDict, 0, 32768);
        do { (&decomp)->m_state = 0; } while (0);
        for (;;)
        {
            size_t in_buf_size = *pIn_buf_size - in_buf_ofs, dst_buf_size = 32768 - dict_ofs;
            tinfl_status status = tinfl_decompress(&decomp, (const mz_uint8 *)pIn_buf + in_buf_ofs, &in_buf_size, pDict, pDict + dict_ofs, &dst_buf_size,
                                                   (flags & ~(TINFL_FLAG_HAS_MORE_INPUT | TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF)));
            in_buf_ofs += in_buf_size;
            if ((dst_buf_size) && (!(*pPut_buf_func)(pDict + dict_ofs, (int)dst_buf_size, pPut_buf_user)))
                break;
            if (status != TINFL_STATUS_HAS_MORE_OUTPUT)
            {
                result = (status == TINFL_STATUS_DONE);
                break;
            }
            dict_ofs = (dict_ofs + dst_buf_size) & (32768 - 1);
        }
        free(pDict);
        *pIn_buf_size = in_buf_ofs;
        return result;
    }


    tinfl_decompressor *tinfl_decompressor_alloc(void)
    {
        tinfl_decompressor *pDecomp = (tinfl_decompressor *)malloc(sizeof(tinfl_decompressor));
        if (pDecomp)
            do { (pDecomp)->m_state = 0; } while (0);
        return pDecomp;
    }

    void tinfl_decompressor_free(tinfl_decompressor *pDecomp)
    {
        free(pDecomp);
    }



int main() {
    unsigned char *in_buf = (unsigned char *)malloc((64 * 1024));
    unsigned char *comp_buf = (unsigned char *)malloc((64 * 1024) * 2);
    unsigned char *decomp_buf = (unsigned char *)malloc((64 * 1024));

    if (!in_buf || !comp_buf || !decomp_buf) {
        printf("FAIL: out of memory\n");
        return 1;
    }


    size_t i;
    for (i = 0; i < (64 * 1024); i++) {
        in_buf[i] = (unsigned char)(i % 251);
    }

    unsigned long comp_len = (64 * 1024) * 2;
    int status = mz_compress(comp_buf, &comp_len, in_buf, (64 * 1024));
    if (status != MZ_OK) {
        printf("FAIL: compression error %d\n", status);
        return 1;
    }

    unsigned long decomp_len = (64 * 1024);
    status = mz_uncompress(decomp_buf, &decomp_len, comp_buf, comp_len);
    if (status != MZ_OK) {
        printf("FAIL: decompression error %d\n", status);
        return 1;
    }

    if (decomp_len != (64 * 1024)) {
        printf("FAIL: size mismatch %lu vs %d\n", decomp_len, (64 * 1024));
        return 1;
    }


    for (i = 0; i < (64 * 1024); i++) {
        if (in_buf[i] != decomp_buf[i]) {
            printf("FAIL: data corruption at byte %lu\n", (unsigned long)i);
            return 1;
        }
    }


    unsigned long long checksum = 0;
    for (i = 0; i < comp_len; i++) {
        checksum = (checksum + comp_buf[i]) * 31;
    }

    printf("PASS (compressed %d bytes down to %lu bytes, checksum %llu)\n", (64 * 1024), comp_len, checksum);

    free(in_buf);
    free(comp_buf);
    free(decomp_buf);

    return 0;
}
