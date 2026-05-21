# 0 "g_game.c"
# 0 "<built-in>"
# 0 "<command-line>"
# 1 "/usr/include/stdc-predef.h" 1 3 4
# 0 "<command-line>" 2
# 1 "g_game.c"
# 24 "g_game.c"
static const char
rcsid[] = "$Id: g_game.c,v 1.8 1997/02/03 22:45:09 b1 Exp $";

# 1 "/usr/include/string.h" 1 3 4
# 26 "/usr/include/string.h" 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/libc-header-start.h" 1 3 4
# 33 "/usr/include/x86_64-linux-gnu/bits/libc-header-start.h" 3 4
# 1 "/usr/include/features.h" 1 3 4
# 394 "/usr/include/features.h" 3 4
# 1 "/usr/include/features-time64.h" 1 3 4
# 20 "/usr/include/features-time64.h" 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/wordsize.h" 1 3 4
# 21 "/usr/include/features-time64.h" 2 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/timesize.h" 1 3 4
# 19 "/usr/include/x86_64-linux-gnu/bits/timesize.h" 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/wordsize.h" 1 3 4
# 20 "/usr/include/x86_64-linux-gnu/bits/timesize.h" 2 3 4
# 22 "/usr/include/features-time64.h" 2 3 4
# 395 "/usr/include/features.h" 2 3 4
# 502 "/usr/include/features.h" 3 4
# 1 "/usr/include/x86_64-linux-gnu/sys/cdefs.h" 1 3 4
# 576 "/usr/include/x86_64-linux-gnu/sys/cdefs.h" 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/wordsize.h" 1 3 4
# 577 "/usr/include/x86_64-linux-gnu/sys/cdefs.h" 2 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/long-double.h" 1 3 4
# 578 "/usr/include/x86_64-linux-gnu/sys/cdefs.h" 2 3 4
# 503 "/usr/include/features.h" 2 3 4
# 526 "/usr/include/features.h" 3 4
# 1 "/usr/include/x86_64-linux-gnu/gnu/stubs.h" 1 3 4
# 10 "/usr/include/x86_64-linux-gnu/gnu/stubs.h" 3 4
# 1 "/usr/include/x86_64-linux-gnu/gnu/stubs-64.h" 1 3 4
# 11 "/usr/include/x86_64-linux-gnu/gnu/stubs.h" 2 3 4
# 527 "/usr/include/features.h" 2 3 4
# 34 "/usr/include/x86_64-linux-gnu/bits/libc-header-start.h" 2 3 4
# 27 "/usr/include/string.h" 2 3 4






# 1 "/usr/lib/gcc/x86_64-linux-gnu/13/include/stddef.h" 1 3 4
# 214 "/usr/lib/gcc/x86_64-linux-gnu/13/include/stddef.h" 3 4

# 214 "/usr/lib/gcc/x86_64-linux-gnu/13/include/stddef.h" 3 4
typedef long unsigned int size_t;
# 34 "/usr/include/string.h" 2 3 4
# 43 "/usr/include/string.h" 3 4
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
# 80 "/usr/include/string.h" 3 4
extern int __memcmpeq (const void *__s1, const void *__s2, size_t __n)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__pure__)) __attribute__ ((__nonnull__ (1, 2)));
# 107 "/usr/include/string.h" 3 4
extern void *memchr (const void *__s, int __c, size_t __n)
      __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__pure__)) __attribute__ ((__nonnull__ (1)));
# 141 "/usr/include/string.h" 3 4
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



# 1 "/usr/include/x86_64-linux-gnu/bits/types/locale_t.h" 1 3 4
# 22 "/usr/include/x86_64-linux-gnu/bits/types/locale_t.h" 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/types/__locale_t.h" 1 3 4
# 27 "/usr/include/x86_64-linux-gnu/bits/types/__locale_t.h" 3 4
struct __locale_struct
{

  struct __locale_data *__locales[13];


  const unsigned short int *__ctype_b;
  const int *__ctype_tolower;
  const int *__ctype_toupper;


  const char *__names[13];
};

typedef struct __locale_struct *__locale_t;
# 23 "/usr/include/x86_64-linux-gnu/bits/types/locale_t.h" 2 3 4

typedef __locale_t locale_t;
# 173 "/usr/include/string.h" 2 3 4


extern int strcoll_l (const char *__s1, const char *__s2, locale_t __l)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__pure__)) __attribute__ ((__nonnull__ (1, 2, 3)));


extern size_t strxfrm_l (char *__dest, const char *__src, size_t __n,
    locale_t __l) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (2, 4)))
     __attribute__ ((__access__ (__write_only__, 1, 3)));





extern char *strdup (const char *__s)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__malloc__)) __attribute__ ((__nonnull__ (1)));






extern char *strndup (const char *__string, size_t __n)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__malloc__)) __attribute__ ((__nonnull__ (1)));
# 246 "/usr/include/string.h" 3 4
extern char *strchr (const char *__s, int __c)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__pure__)) __attribute__ ((__nonnull__ (1)));
# 273 "/usr/include/string.h" 3 4
extern char *strrchr (const char *__s, int __c)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__pure__)) __attribute__ ((__nonnull__ (1)));
# 286 "/usr/include/string.h" 3 4
extern char *strchrnul (const char *__s, int __c)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__pure__)) __attribute__ ((__nonnull__ (1)));





extern size_t strcspn (const char *__s, const char *__reject)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__pure__)) __attribute__ ((__nonnull__ (1, 2)));


extern size_t strspn (const char *__s, const char *__accept)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__pure__)) __attribute__ ((__nonnull__ (1, 2)));
# 323 "/usr/include/string.h" 3 4
extern char *strpbrk (const char *__s, const char *__accept)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__pure__)) __attribute__ ((__nonnull__ (1, 2)));
# 350 "/usr/include/string.h" 3 4
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
# 380 "/usr/include/string.h" 3 4
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
# 432 "/usr/include/string.h" 3 4
extern int strerror_r (int __errnum, char *__buf, size_t __buflen) __asm__ ("" "__xpg_strerror_r") __attribute__ ((__nothrow__ , __leaf__))

                        __attribute__ ((__nonnull__ (2)))
    __attribute__ ((__access__ (__write_only__, 2, 3)));
# 458 "/usr/include/string.h" 3 4
extern char *strerror_l (int __errnum, locale_t __l) __attribute__ ((__nothrow__ , __leaf__));



# 1 "/usr/include/strings.h" 1 3 4
# 23 "/usr/include/strings.h" 3 4
# 1 "/usr/lib/gcc/x86_64-linux-gnu/13/include/stddef.h" 1 3 4
# 24 "/usr/include/strings.h" 2 3 4










extern int bcmp (const void *__s1, const void *__s2, size_t __n)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__pure__)) __attribute__ ((__nonnull__ (1, 2)));


extern void bcopy (const void *__src, void *__dest, size_t __n)
  __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1, 2)));


extern void bzero (void *__s, size_t __n) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1)));
# 68 "/usr/include/strings.h" 3 4
extern char *index (const char *__s, int __c)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__pure__)) __attribute__ ((__nonnull__ (1)));
# 96 "/usr/include/strings.h" 3 4
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



# 463 "/usr/include/string.h" 2 3 4



extern void explicit_bzero (void *__s, size_t __n) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1)))
    __attribute__ ((__access__ (__write_only__, 1, 2)));



extern char *strsep (char **__restrict __stringp,
       const char *__restrict __delim)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1, 2)));




extern char *strsignal (int __sig) __attribute__ ((__nothrow__ , __leaf__));
# 489 "/usr/include/string.h" 3 4
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
# 552 "/usr/include/string.h" 3 4

# 28 "g_game.c" 2
# 1 "/usr/include/stdlib.h" 1 3 4
# 26 "/usr/include/stdlib.h" 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/libc-header-start.h" 1 3 4
# 27 "/usr/include/stdlib.h" 2 3 4





# 1 "/usr/lib/gcc/x86_64-linux-gnu/13/include/stddef.h" 1 3 4
# 329 "/usr/lib/gcc/x86_64-linux-gnu/13/include/stddef.h" 3 4
typedef int wchar_t;
# 33 "/usr/include/stdlib.h" 2 3 4







# 1 "/usr/include/x86_64-linux-gnu/bits/waitflags.h" 1 3 4
# 41 "/usr/include/stdlib.h" 2 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/waitstatus.h" 1 3 4
# 42 "/usr/include/stdlib.h" 2 3 4
# 56 "/usr/include/stdlib.h" 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/floatn.h" 1 3 4
# 119 "/usr/include/x86_64-linux-gnu/bits/floatn.h" 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/floatn-common.h" 1 3 4
# 24 "/usr/include/x86_64-linux-gnu/bits/floatn-common.h" 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/long-double.h" 1 3 4
# 25 "/usr/include/x86_64-linux-gnu/bits/floatn-common.h" 2 3 4
# 120 "/usr/include/x86_64-linux-gnu/bits/floatn.h" 2 3 4
# 57 "/usr/include/stdlib.h" 2 3 4


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
# 98 "/usr/include/stdlib.h" 3 4
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
# 177 "/usr/include/stdlib.h" 3 4
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
# 505 "/usr/include/stdlib.h" 3 4
extern char *l64a (long int __n) __attribute__ ((__nothrow__ , __leaf__)) ;


extern long int a64l (const char *__s)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__pure__)) __attribute__ ((__nonnull__ (1))) ;




# 1 "/usr/include/x86_64-linux-gnu/sys/types.h" 1 3 4
# 27 "/usr/include/x86_64-linux-gnu/sys/types.h" 3 4


# 1 "/usr/include/x86_64-linux-gnu/bits/types.h" 1 3 4
# 27 "/usr/include/x86_64-linux-gnu/bits/types.h" 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/wordsize.h" 1 3 4
# 28 "/usr/include/x86_64-linux-gnu/bits/types.h" 2 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/timesize.h" 1 3 4
# 19 "/usr/include/x86_64-linux-gnu/bits/timesize.h" 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/wordsize.h" 1 3 4
# 20 "/usr/include/x86_64-linux-gnu/bits/timesize.h" 2 3 4
# 29 "/usr/include/x86_64-linux-gnu/bits/types.h" 2 3 4


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
# 141 "/usr/include/x86_64-linux-gnu/bits/types.h" 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/typesizes.h" 1 3 4
# 142 "/usr/include/x86_64-linux-gnu/bits/types.h" 2 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/time64.h" 1 3 4
# 143 "/usr/include/x86_64-linux-gnu/bits/types.h" 2 3 4


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
# 30 "/usr/include/x86_64-linux-gnu/sys/types.h" 2 3 4



typedef __u_char u_char;
typedef __u_short u_short;
typedef __u_int u_int;
typedef __u_long u_long;
typedef __quad_t quad_t;
typedef __u_quad_t u_quad_t;
typedef __fsid_t fsid_t;


typedef __loff_t loff_t;




typedef __ino_t ino_t;
# 59 "/usr/include/x86_64-linux-gnu/sys/types.h" 3 4
typedef __dev_t dev_t;




typedef __gid_t gid_t;




typedef __mode_t mode_t;




typedef __nlink_t nlink_t;




typedef __uid_t uid_t;





typedef __off_t off_t;
# 97 "/usr/include/x86_64-linux-gnu/sys/types.h" 3 4
typedef __pid_t pid_t;





typedef __id_t id_t;




typedef __ssize_t ssize_t;





typedef __daddr_t daddr_t;
typedef __caddr_t caddr_t;





typedef __key_t key_t;




# 1 "/usr/include/x86_64-linux-gnu/bits/types/clock_t.h" 1 3 4






typedef __clock_t clock_t;
# 127 "/usr/include/x86_64-linux-gnu/sys/types.h" 2 3 4

# 1 "/usr/include/x86_64-linux-gnu/bits/types/clockid_t.h" 1 3 4






typedef __clockid_t clockid_t;
# 129 "/usr/include/x86_64-linux-gnu/sys/types.h" 2 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/types/time_t.h" 1 3 4
# 10 "/usr/include/x86_64-linux-gnu/bits/types/time_t.h" 3 4
typedef __time_t time_t;
# 130 "/usr/include/x86_64-linux-gnu/sys/types.h" 2 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/types/timer_t.h" 1 3 4






typedef __timer_t timer_t;
# 131 "/usr/include/x86_64-linux-gnu/sys/types.h" 2 3 4
# 144 "/usr/include/x86_64-linux-gnu/sys/types.h" 3 4
# 1 "/usr/lib/gcc/x86_64-linux-gnu/13/include/stddef.h" 1 3 4
# 145 "/usr/include/x86_64-linux-gnu/sys/types.h" 2 3 4



typedef unsigned long int ulong;
typedef unsigned short int ushort;
typedef unsigned int uint;




# 1 "/usr/include/x86_64-linux-gnu/bits/stdint-intn.h" 1 3 4
# 24 "/usr/include/x86_64-linux-gnu/bits/stdint-intn.h" 3 4
typedef __int8_t int8_t;
typedef __int16_t int16_t;
typedef __int32_t int32_t;
typedef __int64_t int64_t;
# 156 "/usr/include/x86_64-linux-gnu/sys/types.h" 2 3 4


typedef __uint8_t u_int8_t;
typedef __uint16_t u_int16_t;
typedef __uint32_t u_int32_t;
typedef __uint64_t u_int64_t;


typedef int register_t __attribute__ ((__mode__ (__word__)));
# 176 "/usr/include/x86_64-linux-gnu/sys/types.h" 3 4
# 1 "/usr/include/endian.h" 1 3 4
# 24 "/usr/include/endian.h" 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/endian.h" 1 3 4
# 35 "/usr/include/x86_64-linux-gnu/bits/endian.h" 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/endianness.h" 1 3 4
# 36 "/usr/include/x86_64-linux-gnu/bits/endian.h" 2 3 4
# 25 "/usr/include/endian.h" 2 3 4
# 35 "/usr/include/endian.h" 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/byteswap.h" 1 3 4
# 33 "/usr/include/x86_64-linux-gnu/bits/byteswap.h" 3 4
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
# 69 "/usr/include/x86_64-linux-gnu/bits/byteswap.h" 3 4
__extension__ static __inline __uint64_t
__bswap_64 (__uint64_t __bsx)
{

  return __builtin_bswap64 (__bsx);



}
# 36 "/usr/include/endian.h" 2 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/uintn-identity.h" 1 3 4
# 32 "/usr/include/x86_64-linux-gnu/bits/uintn-identity.h" 3 4
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
# 37 "/usr/include/endian.h" 2 3 4
# 177 "/usr/include/x86_64-linux-gnu/sys/types.h" 2 3 4


# 1 "/usr/include/x86_64-linux-gnu/sys/select.h" 1 3 4
# 30 "/usr/include/x86_64-linux-gnu/sys/select.h" 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/select.h" 1 3 4
# 31 "/usr/include/x86_64-linux-gnu/sys/select.h" 2 3 4


# 1 "/usr/include/x86_64-linux-gnu/bits/types/sigset_t.h" 1 3 4



# 1 "/usr/include/x86_64-linux-gnu/bits/types/__sigset_t.h" 1 3 4




typedef struct
{
  unsigned long int __val[(1024 / (8 * sizeof (unsigned long int)))];
} __sigset_t;
# 5 "/usr/include/x86_64-linux-gnu/bits/types/sigset_t.h" 2 3 4


typedef __sigset_t sigset_t;
# 34 "/usr/include/x86_64-linux-gnu/sys/select.h" 2 3 4



# 1 "/usr/include/x86_64-linux-gnu/bits/types/struct_timeval.h" 1 3 4







struct timeval
{




  __time_t tv_sec;
  __suseconds_t tv_usec;

};
# 38 "/usr/include/x86_64-linux-gnu/sys/select.h" 2 3 4

# 1 "/usr/include/x86_64-linux-gnu/bits/types/struct_timespec.h" 1 3 4
# 11 "/usr/include/x86_64-linux-gnu/bits/types/struct_timespec.h" 3 4
struct timespec
{



  __time_t tv_sec;




  __syscall_slong_t tv_nsec;
# 31 "/usr/include/x86_64-linux-gnu/bits/types/struct_timespec.h" 3 4
};
# 40 "/usr/include/x86_64-linux-gnu/sys/select.h" 2 3 4



typedef __suseconds_t suseconds_t;





typedef long int __fd_mask;
# 59 "/usr/include/x86_64-linux-gnu/sys/select.h" 3 4
typedef struct
  {






    __fd_mask __fds_bits[1024 / (8 * (int) sizeof (__fd_mask))];


  } fd_set;






typedef __fd_mask fd_mask;
# 91 "/usr/include/x86_64-linux-gnu/sys/select.h" 3 4

# 102 "/usr/include/x86_64-linux-gnu/sys/select.h" 3 4
extern int select (int __nfds, fd_set *__restrict __readfds,
     fd_set *__restrict __writefds,
     fd_set *__restrict __exceptfds,
     struct timeval *__restrict __timeout);
# 127 "/usr/include/x86_64-linux-gnu/sys/select.h" 3 4
extern int pselect (int __nfds, fd_set *__restrict __readfds,
      fd_set *__restrict __writefds,
      fd_set *__restrict __exceptfds,
      const struct timespec *__restrict __timeout,
      const __sigset_t *__restrict __sigmask);
# 153 "/usr/include/x86_64-linux-gnu/sys/select.h" 3 4

# 180 "/usr/include/x86_64-linux-gnu/sys/types.h" 2 3 4





typedef __blksize_t blksize_t;






typedef __blkcnt_t blkcnt_t;



typedef __fsblkcnt_t fsblkcnt_t;



typedef __fsfilcnt_t fsfilcnt_t;
# 227 "/usr/include/x86_64-linux-gnu/sys/types.h" 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/pthreadtypes.h" 1 3 4
# 23 "/usr/include/x86_64-linux-gnu/bits/pthreadtypes.h" 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/thread-shared-types.h" 1 3 4
# 44 "/usr/include/x86_64-linux-gnu/bits/thread-shared-types.h" 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/pthreadtypes-arch.h" 1 3 4
# 21 "/usr/include/x86_64-linux-gnu/bits/pthreadtypes-arch.h" 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/wordsize.h" 1 3 4
# 22 "/usr/include/x86_64-linux-gnu/bits/pthreadtypes-arch.h" 2 3 4
# 45 "/usr/include/x86_64-linux-gnu/bits/thread-shared-types.h" 2 3 4

# 1 "/usr/include/x86_64-linux-gnu/bits/atomic_wide_counter.h" 1 3 4
# 25 "/usr/include/x86_64-linux-gnu/bits/atomic_wide_counter.h" 3 4
typedef union
{
  __extension__ unsigned long long int __value64;
  struct
  {
    unsigned int __low;
    unsigned int __high;
  } __value32;
} __atomic_wide_counter;
# 47 "/usr/include/x86_64-linux-gnu/bits/thread-shared-types.h" 2 3 4




typedef struct __pthread_internal_list
{
  struct __pthread_internal_list *__prev;
  struct __pthread_internal_list *__next;
} __pthread_list_t;

typedef struct __pthread_internal_slist
{
  struct __pthread_internal_slist *__next;
} __pthread_slist_t;
# 76 "/usr/include/x86_64-linux-gnu/bits/thread-shared-types.h" 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/struct_mutex.h" 1 3 4
# 22 "/usr/include/x86_64-linux-gnu/bits/struct_mutex.h" 3 4
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
# 53 "/usr/include/x86_64-linux-gnu/bits/struct_mutex.h" 3 4
};
# 77 "/usr/include/x86_64-linux-gnu/bits/thread-shared-types.h" 2 3 4
# 89 "/usr/include/x86_64-linux-gnu/bits/thread-shared-types.h" 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/struct_rwlock.h" 1 3 4
# 23 "/usr/include/x86_64-linux-gnu/bits/struct_rwlock.h" 3 4
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
# 55 "/usr/include/x86_64-linux-gnu/bits/struct_rwlock.h" 3 4
};
# 90 "/usr/include/x86_64-linux-gnu/bits/thread-shared-types.h" 2 3 4




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
# 24 "/usr/include/x86_64-linux-gnu/bits/pthreadtypes.h" 2 3 4



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
# 228 "/usr/include/x86_64-linux-gnu/sys/types.h" 2 3 4



# 515 "/usr/include/stdlib.h" 2 3 4






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



# 1 "/usr/include/alloca.h" 1 3 4
# 24 "/usr/include/alloca.h" 3 4
# 1 "/usr/lib/gcc/x86_64-linux-gnu/13/include/stddef.h" 1 3 4
# 25 "/usr/include/alloca.h" 2 3 4







extern void *alloca (size_t __size) __attribute__ ((__nothrow__ , __leaf__));






# 707 "/usr/include/stdlib.h" 2 3 4





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
# 786 "/usr/include/stdlib.h" 3 4
extern int putenv (char *__string) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1)));





extern int setenv (const char *__name, const char *__value, int __replace)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (2)));


extern int unsetenv (const char *__name) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1)));






extern int clearenv (void) __attribute__ ((__nothrow__ , __leaf__));
# 814 "/usr/include/stdlib.h" 3 4
extern char *mktemp (char *__template) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1)));
# 827 "/usr/include/stdlib.h" 3 4
extern int mkstemp (char *__template) __attribute__ ((__nonnull__ (1))) ;
# 849 "/usr/include/stdlib.h" 3 4
extern int mkstemps (char *__template, int __suffixlen) __attribute__ ((__nonnull__ (1))) ;
# 870 "/usr/include/stdlib.h" 3 4
extern char *mkdtemp (char *__template) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1))) ;
# 923 "/usr/include/stdlib.h" 3 4
extern int system (const char *__command) ;
# 940 "/usr/include/stdlib.h" 3 4
extern char *realpath (const char *__restrict __name,
         char *__restrict __resolved) __attribute__ ((__nothrow__ , __leaf__)) ;






typedef int (*__compar_fn_t) (const void *, const void *);
# 960 "/usr/include/stdlib.h" 3 4
extern void *bsearch (const void *__key, const void *__base,
        size_t __nmemb, size_t __size, __compar_fn_t __compar)
     __attribute__ ((__nonnull__ (1, 2, 5))) ;







extern void qsort (void *__base, size_t __nmemb, size_t __size,
     __compar_fn_t __compar) __attribute__ ((__nonnull__ (1, 4)));
# 980 "/usr/include/stdlib.h" 3 4
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
# 1012 "/usr/include/stdlib.h" 3 4
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
# 1099 "/usr/include/stdlib.h" 3 4
extern int getsubopt (char **__restrict __optionp,
        char *const *__restrict __tokens,
        char **__restrict __valuep)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1, 2, 3))) ;
# 1145 "/usr/include/stdlib.h" 3 4
extern int getloadavg (double __loadavg[], int __nelem)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1)));
# 1155 "/usr/include/stdlib.h" 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/stdlib-float.h" 1 3 4
# 1156 "/usr/include/stdlib.h" 2 3 4
# 1167 "/usr/include/stdlib.h" 3 4

# 29 "g_game.c" 2

# 1 "doomdef.h" 1
# 26 "doomdef.h"
# 1 "/usr/include/stdio.h" 1 3 4
# 28 "/usr/include/stdio.h" 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/libc-header-start.h" 1 3 4
# 29 "/usr/include/stdio.h" 2 3 4





# 1 "/usr/lib/gcc/x86_64-linux-gnu/13/include/stddef.h" 1 3 4
# 35 "/usr/include/stdio.h" 2 3 4


# 1 "/usr/lib/gcc/x86_64-linux-gnu/13/include/stdarg.h" 1 3 4
# 40 "/usr/lib/gcc/x86_64-linux-gnu/13/include/stdarg.h" 3 4
typedef __builtin_va_list __gnuc_va_list;
# 38 "/usr/include/stdio.h" 2 3 4


# 1 "/usr/include/x86_64-linux-gnu/bits/types/__fpos_t.h" 1 3 4




# 1 "/usr/include/x86_64-linux-gnu/bits/types/__mbstate_t.h" 1 3 4
# 13 "/usr/include/x86_64-linux-gnu/bits/types/__mbstate_t.h" 3 4
typedef struct
{
  int __count;
  union
  {
    unsigned int __wch;
    char __wchb[4];
  } __value;
} __mbstate_t;
# 6 "/usr/include/x86_64-linux-gnu/bits/types/__fpos_t.h" 2 3 4




typedef struct _G_fpos_t
{
  __off_t __pos;
  __mbstate_t __state;
} __fpos_t;
# 41 "/usr/include/stdio.h" 2 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/types/__fpos64_t.h" 1 3 4
# 10 "/usr/include/x86_64-linux-gnu/bits/types/__fpos64_t.h" 3 4
typedef struct _G_fpos64_t
{
  __off64_t __pos;
  __mbstate_t __state;
} __fpos64_t;
# 42 "/usr/include/stdio.h" 2 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/types/__FILE.h" 1 3 4



struct _IO_FILE;
typedef struct _IO_FILE __FILE;
# 43 "/usr/include/stdio.h" 2 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/types/FILE.h" 1 3 4



struct _IO_FILE;


typedef struct _IO_FILE FILE;
# 44 "/usr/include/stdio.h" 2 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/types/struct_FILE.h" 1 3 4
# 35 "/usr/include/x86_64-linux-gnu/bits/types/struct_FILE.h" 3 4
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
# 45 "/usr/include/stdio.h" 2 3 4


# 1 "/usr/include/x86_64-linux-gnu/bits/types/cookie_io_functions_t.h" 1 3 4
# 27 "/usr/include/x86_64-linux-gnu/bits/types/cookie_io_functions_t.h" 3 4
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
# 48 "/usr/include/stdio.h" 2 3 4





typedef __gnuc_va_list va_list;
# 85 "/usr/include/stdio.h" 3 4
typedef __fpos_t fpos_t;
# 129 "/usr/include/stdio.h" 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/stdio_lim.h" 1 3 4
# 130 "/usr/include/stdio.h" 2 3 4
# 149 "/usr/include/stdio.h" 3 4
extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;






extern int remove (const char *__filename) __attribute__ ((__nothrow__ , __leaf__));

extern int rename (const char *__old, const char *__new) __attribute__ ((__nothrow__ , __leaf__));



extern int renameat (int __oldfd, const char *__old, int __newfd,
       const char *__new) __attribute__ ((__nothrow__ , __leaf__));
# 184 "/usr/include/stdio.h" 3 4
extern int fclose (FILE *__stream) __attribute__ ((__nonnull__ (1)));
# 194 "/usr/include/stdio.h" 3 4
extern FILE *tmpfile (void)
  __attribute__ ((__malloc__)) __attribute__ ((__malloc__ (fclose, 1))) ;
# 211 "/usr/include/stdio.h" 3 4
extern char *tmpnam (char[20]) __attribute__ ((__nothrow__ , __leaf__)) ;




extern char *tmpnam_r (char __s[20]) __attribute__ ((__nothrow__ , __leaf__)) ;
# 228 "/usr/include/stdio.h" 3 4
extern char *tempnam (const char *__dir, const char *__pfx)
   __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__malloc__)) __attribute__ ((__malloc__ (__builtin_free, 1)));






extern int fflush (FILE *__stream);
# 245 "/usr/include/stdio.h" 3 4
extern int fflush_unlocked (FILE *__stream);
# 264 "/usr/include/stdio.h" 3 4
extern FILE *fopen (const char *__restrict __filename,
      const char *__restrict __modes)
  __attribute__ ((__malloc__)) __attribute__ ((__malloc__ (fclose, 1))) ;




extern FILE *freopen (const char *__restrict __filename,
        const char *__restrict __modes,
        FILE *__restrict __stream) __attribute__ ((__nonnull__ (3)));
# 299 "/usr/include/stdio.h" 3 4
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
# 334 "/usr/include/stdio.h" 3 4
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
# 463 "/usr/include/stdio.h" 3 4
extern int fscanf (FILE *__restrict __stream, const char *__restrict __format, ...) __asm__ ("" "__isoc99_fscanf")

                                __attribute__ ((__nonnull__ (1)));
extern int scanf (const char *__restrict __format, ...) __asm__ ("" "__isoc99_scanf")
                              ;
extern int sscanf (const char *__restrict __s, const char *__restrict __format, ...) __asm__ ("" "__isoc99_sscanf") __attribute__ ((__nothrow__ , __leaf__))

                      ;
# 490 "/usr/include/stdio.h" 3 4
extern int vfscanf (FILE *__restrict __s, const char *__restrict __format,
      __gnuc_va_list __arg)
     __attribute__ ((__format__ (__scanf__, 2, 0))) __attribute__ ((__nonnull__ (1)));





extern int vscanf (const char *__restrict __format, __gnuc_va_list __arg)
     __attribute__ ((__format__ (__scanf__, 1, 0))) ;


extern int vsscanf (const char *__restrict __s,
      const char *__restrict __format, __gnuc_va_list __arg)
     __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__format__ (__scanf__, 2, 0)));
# 540 "/usr/include/stdio.h" 3 4
extern int vfscanf (FILE *__restrict __s, const char *__restrict __format, __gnuc_va_list __arg) __asm__ ("" "__isoc99_vfscanf")



     __attribute__ ((__format__ (__scanf__, 2, 0))) __attribute__ ((__nonnull__ (1)));
extern int vscanf (const char *__restrict __format, __gnuc_va_list __arg) __asm__ ("" "__isoc99_vscanf")

     __attribute__ ((__format__ (__scanf__, 1, 0))) ;
extern int vsscanf (const char *__restrict __s, const char *__restrict __format, __gnuc_va_list __arg) __asm__ ("" "__isoc99_vsscanf") __attribute__ ((__nothrow__ , __leaf__))



     __attribute__ ((__format__ (__scanf__, 2, 0)));
# 575 "/usr/include/stdio.h" 3 4
extern int fgetc (FILE *__stream) __attribute__ ((__nonnull__ (1)));
extern int getc (FILE *__stream) __attribute__ ((__nonnull__ (1)));





extern int getchar (void);






extern int getc_unlocked (FILE *__stream) __attribute__ ((__nonnull__ (1)));
extern int getchar_unlocked (void);
# 600 "/usr/include/stdio.h" 3 4
extern int fgetc_unlocked (FILE *__stream) __attribute__ ((__nonnull__ (1)));
# 611 "/usr/include/stdio.h" 3 4
extern int fputc (int __c, FILE *__stream) __attribute__ ((__nonnull__ (2)));
extern int putc (int __c, FILE *__stream) __attribute__ ((__nonnull__ (2)));





extern int putchar (int __c);
# 627 "/usr/include/stdio.h" 3 4
extern int fputc_unlocked (int __c, FILE *__stream) __attribute__ ((__nonnull__ (2)));







extern int putc_unlocked (int __c, FILE *__stream) __attribute__ ((__nonnull__ (2)));
extern int putchar_unlocked (int __c);






extern int getw (FILE *__stream) __attribute__ ((__nonnull__ (1)));


extern int putw (int __w, FILE *__stream) __attribute__ ((__nonnull__ (2)));







extern char *fgets (char *__restrict __s, int __n, FILE *__restrict __stream)
     __attribute__ ((__access__ (__write_only__, 1, 2))) __attribute__ ((__nonnull__ (3)));
# 694 "/usr/include/stdio.h" 3 4
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
# 766 "/usr/include/stdio.h" 3 4
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
# 803 "/usr/include/stdio.h" 3 4
extern int fseeko (FILE *__stream, __off_t __off, int __whence)
  __attribute__ ((__nonnull__ (1)));




extern __off_t ftello (FILE *__stream) __attribute__ ((__nonnull__ (1)));
# 829 "/usr/include/stdio.h" 3 4
extern int fgetpos (FILE *__restrict __stream, fpos_t *__restrict __pos)
  __attribute__ ((__nonnull__ (1)));




extern int fsetpos (FILE *__stream, const fpos_t *__pos) __attribute__ ((__nonnull__ (1)));
# 860 "/usr/include/stdio.h" 3 4
extern void clearerr (FILE *__stream) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1)));

extern int feof (FILE *__stream) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1)));

extern int ferror (FILE *__stream) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1)));



extern void clearerr_unlocked (FILE *__stream) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1)));
extern int feof_unlocked (FILE *__stream) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1)));
extern int ferror_unlocked (FILE *__stream) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1)));







extern void perror (const char *__s) __attribute__ ((__cold__));




extern int fileno (FILE *__stream) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1)));




extern int fileno_unlocked (FILE *__stream) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1)));
# 897 "/usr/include/stdio.h" 3 4
extern int pclose (FILE *__stream) __attribute__ ((__nonnull__ (1)));





extern FILE *popen (const char *__command, const char *__modes)
  __attribute__ ((__malloc__)) __attribute__ ((__malloc__ (pclose, 1))) ;






extern char *ctermid (char *__s) __attribute__ ((__nothrow__ , __leaf__))
  __attribute__ ((__access__ (__write_only__, 1)));
# 941 "/usr/include/stdio.h" 3 4
extern void flockfile (FILE *__stream) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1)));



extern int ftrylockfile (FILE *__stream) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1)));


extern void funlockfile (FILE *__stream) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (1)));
# 959 "/usr/include/stdio.h" 3 4
extern int __uflow (FILE *);
extern int __overflow (FILE *, int);
# 983 "/usr/include/stdio.h" 3 4

# 27 "doomdef.h" 2







# 33 "doomdef.h"
enum { VERSION = 110 };




typedef enum
{
  shareware,
  registered,
  commercial,

  retail,
  indetermined

} GameMode_t;



typedef enum
{
  doom,
  doom2,
  pack_tnt,
  pack_plut,
  none

} GameMission_t;



typedef enum
{
  english,
  french,
  german,
  unknown

} Language_t;
# 127 "doomdef.h"
typedef enum
{
    GS_LEVEL,
    GS_INTERMISSION,
    GS_FINALE,
    GS_DEMOSCREEN
} gamestate_t;
# 147 "doomdef.h"
typedef enum
{
    sk_baby,
    sk_easy,
    sk_medium,
    sk_hard,
    sk_nightmare
} skill_t;







typedef enum
{
    it_bluecard,
    it_yellowcard,
    it_redcard,
    it_blueskull,
    it_yellowskull,
    it_redskull,

    NUMCARDS

} card_t;






typedef enum
{
    wp_fist,
    wp_pistol,
    wp_shotgun,
    wp_chaingun,
    wp_missile,
    wp_plasma,
    wp_bfg,
    wp_chainsaw,
    wp_supershotgun,

    NUMWEAPONS,


    wp_nochange

} weapontype_t;



typedef enum
{
    am_clip,
    am_shell,
    am_cell,
    am_misl,
    NUMAMMO,
    am_noammo

} ammotype_t;



typedef enum
{
    pw_invulnerability,
    pw_strength,
    pw_invisibility,
    pw_ironfeet,
    pw_allmap,
    pw_infrared,
    NUMPOWERS

} powertype_t;
# 233 "doomdef.h"
typedef enum
{
    INVULNTICS = (30*35),
    INVISTICS = (60*35),
    INFRATICS = (120*35),
    IRONTICS = (60*35)

} powerduration_t;
# 31 "g_game.c" 2
# 1 "doomstat.h" 1
# 33 "doomstat.h"
# 1 "doomdata.h" 1
# 28 "doomdata.h"
# 1 "doomtype.h" 1
# 36 "doomtype.h"
typedef enum {false, true} boolean;

typedef unsigned char byte;
# 29 "doomdata.h" 2
# 43 "doomdata.h"
enum
{
  ML_LABEL,
  ML_THINGS,
  ML_LINEDEFS,
  ML_SIDEDEFS,
  ML_VERTEXES,
  ML_SEGS,
  ML_SSECTORS,
  ML_NODES,
  ML_SECTORS,
  ML_REJECT,
  ML_BLOCKMAP
};



typedef struct
{
  short x;
  short y;
} mapvertex_t;




typedef struct
{
  short textureoffset;
  short rowoffset;
  char toptexture[8];
  char bottomtexture[8];
  char midtexture[8];

  short sector;
} mapsidedef_t;





typedef struct
{
  short v1;
  short v2;
  short flags;
  short special;
  short tag;

  short sidenum[2];
} maplinedef_t;
# 141 "doomdata.h"
typedef struct
{
  short floorheight;
  short ceilingheight;
  char floorpic[8];
  char ceilingpic[8];
  short lightlevel;
  short special;
  short tag;
} mapsector_t;


typedef struct
{
  short numsegs;

  short firstseg;
} mapsubsector_t;




typedef struct
{
  short v1;
  short v2;
  short angle;
  short linedef;
  short side;
  short offset;
} mapseg_t;
# 180 "doomdata.h"
typedef struct
{

  short x;
  short y;
  short dx;
  short dy;



  short bbox[2][4];



  unsigned short children[2];

} mapnode_t;






typedef struct
{
    short x;
    short y;
    short angle;
    short type;
    short options;
} mapthing_t;
# 34 "doomstat.h" 2
# 1 "d_net.h" 1
# 26 "d_net.h"
# 1 "d_player.h" 1
# 31 "d_player.h"
# 1 "d_items.h" 1
# 34 "d_items.h"
typedef struct
{
    ammotype_t ammo;
    int upstate;
    int downstate;
    int readystate;
    int atkstate;
    int flashstate;

} weaponinfo_t;

extern weaponinfo_t weaponinfo[NUMWEAPONS];
# 32 "d_player.h" 2
# 1 "p_pspr.h" 1
# 28 "p_pspr.h"
# 1 "m_fixed.h" 1
# 38 "m_fixed.h"
typedef int fixed_t;

fixed_t FixedMul (fixed_t a, fixed_t b);
fixed_t FixedDiv (fixed_t a, fixed_t b);
fixed_t FixedDiv2 (fixed_t a, fixed_t b);
# 29 "p_pspr.h" 2
# 1 "tables.h" 1
# 42 "tables.h"
# 1 "/usr/include/math.h" 1 3 4
# 27 "/usr/include/math.h" 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/libc-header-start.h" 1 3 4
# 28 "/usr/include/math.h" 2 3 4












# 1 "/usr/include/x86_64-linux-gnu/bits/math-vector.h" 1 3 4
# 25 "/usr/include/x86_64-linux-gnu/bits/math-vector.h" 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/libm-simd-decl-stubs.h" 1 3 4
# 26 "/usr/include/x86_64-linux-gnu/bits/math-vector.h" 2 3 4
# 41 "/usr/include/math.h" 2 3 4
# 152 "/usr/include/math.h" 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/flt-eval-method.h" 1 3 4
# 153 "/usr/include/math.h" 2 3 4
# 163 "/usr/include/math.h" 3 4

# 163 "/usr/include/math.h" 3 4
typedef float float_t;
typedef double double_t;
# 204 "/usr/include/math.h" 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/fp-logb.h" 1 3 4
# 205 "/usr/include/math.h" 2 3 4
# 247 "/usr/include/math.h" 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/fp-fast.h" 1 3 4
# 248 "/usr/include/math.h" 2 3 4
# 312 "/usr/include/math.h" 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/mathcalls-helper-functions.h" 1 3 4
# 20 "/usr/include/x86_64-linux-gnu/bits/mathcalls-helper-functions.h" 3 4
extern int __fpclassify (double __value) __attribute__ ((__nothrow__ , __leaf__))
     __attribute__ ((__const__));


extern int __signbit (double __value) __attribute__ ((__nothrow__ , __leaf__))
     __attribute__ ((__const__));



extern int __isinf (double __value) __attribute__ ((__nothrow__ , __leaf__))
  __attribute__ ((__const__));


extern int __finite (double __value) __attribute__ ((__nothrow__ , __leaf__))
  __attribute__ ((__const__));


extern int __isnan (double __value) __attribute__ ((__nothrow__ , __leaf__))
  __attribute__ ((__const__));


extern int __iseqsig (double __x, double __y) __attribute__ ((__nothrow__ , __leaf__));


extern int __issignaling (double __value) __attribute__ ((__nothrow__ , __leaf__))
     __attribute__ ((__const__));
# 313 "/usr/include/math.h" 2 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/mathcalls.h" 1 3 4
# 53 "/usr/include/x86_64-linux-gnu/bits/mathcalls.h" 3 4
 extern double acos (double __x) __attribute__ ((__nothrow__ , __leaf__)); extern double __acos (double __x) __attribute__ ((__nothrow__ , __leaf__));

 extern double asin (double __x) __attribute__ ((__nothrow__ , __leaf__)); extern double __asin (double __x) __attribute__ ((__nothrow__ , __leaf__));

 extern double atan (double __x) __attribute__ ((__nothrow__ , __leaf__)); extern double __atan (double __x) __attribute__ ((__nothrow__ , __leaf__));

 extern double atan2 (double __y, double __x) __attribute__ ((__nothrow__ , __leaf__)); extern double __atan2 (double __y, double __x) __attribute__ ((__nothrow__ , __leaf__));


 extern double cos (double __x) __attribute__ ((__nothrow__ , __leaf__)); extern double __cos (double __x) __attribute__ ((__nothrow__ , __leaf__));

 extern double sin (double __x) __attribute__ ((__nothrow__ , __leaf__)); extern double __sin (double __x) __attribute__ ((__nothrow__ , __leaf__));

 extern double tan (double __x) __attribute__ ((__nothrow__ , __leaf__)); extern double __tan (double __x) __attribute__ ((__nothrow__ , __leaf__));




 extern double cosh (double __x) __attribute__ ((__nothrow__ , __leaf__)); extern double __cosh (double __x) __attribute__ ((__nothrow__ , __leaf__));

 extern double sinh (double __x) __attribute__ ((__nothrow__ , __leaf__)); extern double __sinh (double __x) __attribute__ ((__nothrow__ , __leaf__));

 extern double tanh (double __x) __attribute__ ((__nothrow__ , __leaf__)); extern double __tanh (double __x) __attribute__ ((__nothrow__ , __leaf__));
# 85 "/usr/include/x86_64-linux-gnu/bits/mathcalls.h" 3 4
 extern double acosh (double __x) __attribute__ ((__nothrow__ , __leaf__)); extern double __acosh (double __x) __attribute__ ((__nothrow__ , __leaf__));

 extern double asinh (double __x) __attribute__ ((__nothrow__ , __leaf__)); extern double __asinh (double __x) __attribute__ ((__nothrow__ , __leaf__));

 extern double atanh (double __x) __attribute__ ((__nothrow__ , __leaf__)); extern double __atanh (double __x) __attribute__ ((__nothrow__ , __leaf__));





 extern double exp (double __x) __attribute__ ((__nothrow__ , __leaf__)); extern double __exp (double __x) __attribute__ ((__nothrow__ , __leaf__));


extern double frexp (double __x, int *__exponent) __attribute__ ((__nothrow__ , __leaf__)); extern double __frexp (double __x, int *__exponent) __attribute__ ((__nothrow__ , __leaf__));


extern double ldexp (double __x, int __exponent) __attribute__ ((__nothrow__ , __leaf__)); extern double __ldexp (double __x, int __exponent) __attribute__ ((__nothrow__ , __leaf__));


 extern double log (double __x) __attribute__ ((__nothrow__ , __leaf__)); extern double __log (double __x) __attribute__ ((__nothrow__ , __leaf__));


 extern double log10 (double __x) __attribute__ ((__nothrow__ , __leaf__)); extern double __log10 (double __x) __attribute__ ((__nothrow__ , __leaf__));


extern double modf (double __x, double *__iptr) __attribute__ ((__nothrow__ , __leaf__)); extern double __modf (double __x, double *__iptr) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (2)));
# 119 "/usr/include/x86_64-linux-gnu/bits/mathcalls.h" 3 4
 extern double expm1 (double __x) __attribute__ ((__nothrow__ , __leaf__)); extern double __expm1 (double __x) __attribute__ ((__nothrow__ , __leaf__));


 extern double log1p (double __x) __attribute__ ((__nothrow__ , __leaf__)); extern double __log1p (double __x) __attribute__ ((__nothrow__ , __leaf__));


extern double logb (double __x) __attribute__ ((__nothrow__ , __leaf__)); extern double __logb (double __x) __attribute__ ((__nothrow__ , __leaf__));




 extern double exp2 (double __x) __attribute__ ((__nothrow__ , __leaf__)); extern double __exp2 (double __x) __attribute__ ((__nothrow__ , __leaf__));


 extern double log2 (double __x) __attribute__ ((__nothrow__ , __leaf__)); extern double __log2 (double __x) __attribute__ ((__nothrow__ , __leaf__));






 extern double pow (double __x, double __y) __attribute__ ((__nothrow__ , __leaf__)); extern double __pow (double __x, double __y) __attribute__ ((__nothrow__ , __leaf__));


extern double sqrt (double __x) __attribute__ ((__nothrow__ , __leaf__)); extern double __sqrt (double __x) __attribute__ ((__nothrow__ , __leaf__));



 extern double hypot (double __x, double __y) __attribute__ ((__nothrow__ , __leaf__)); extern double __hypot (double __x, double __y) __attribute__ ((__nothrow__ , __leaf__));




 extern double cbrt (double __x) __attribute__ ((__nothrow__ , __leaf__)); extern double __cbrt (double __x) __attribute__ ((__nothrow__ , __leaf__));






extern double ceil (double __x) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__const__)); extern double __ceil (double __x) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__const__));


extern double fabs (double __x) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__const__)); extern double __fabs (double __x) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__const__));


extern double floor (double __x) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__const__)); extern double __floor (double __x) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__const__));


extern double fmod (double __x, double __y) __attribute__ ((__nothrow__ , __leaf__)); extern double __fmod (double __x, double __y) __attribute__ ((__nothrow__ , __leaf__));
# 177 "/usr/include/x86_64-linux-gnu/bits/mathcalls.h" 3 4
extern int isinf (double __value) __attribute__ ((__nothrow__ , __leaf__))
  __attribute__ ((__const__));




extern int finite (double __value) __attribute__ ((__nothrow__ , __leaf__))
  __attribute__ ((__const__));


extern double drem (double __x, double __y) __attribute__ ((__nothrow__ , __leaf__)); extern double __drem (double __x, double __y) __attribute__ ((__nothrow__ , __leaf__));



extern double significand (double __x) __attribute__ ((__nothrow__ , __leaf__)); extern double __significand (double __x) __attribute__ ((__nothrow__ , __leaf__));






extern double copysign (double __x, double __y) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__const__)); extern double __copysign (double __x, double __y) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__const__));




extern double nan (const char *__tagb) __attribute__ ((__nothrow__ , __leaf__)); extern double __nan (const char *__tagb) __attribute__ ((__nothrow__ , __leaf__));
# 213 "/usr/include/x86_64-linux-gnu/bits/mathcalls.h" 3 4
extern int isnan (double __value) __attribute__ ((__nothrow__ , __leaf__))
  __attribute__ ((__const__));





extern double j0 (double) __attribute__ ((__nothrow__ , __leaf__)); extern double __j0 (double) __attribute__ ((__nothrow__ , __leaf__));
extern double j1 (double) __attribute__ ((__nothrow__ , __leaf__)); extern double __j1 (double) __attribute__ ((__nothrow__ , __leaf__));
extern double jn (int, double) __attribute__ ((__nothrow__ , __leaf__)); extern double __jn (int, double) __attribute__ ((__nothrow__ , __leaf__));
extern double y0 (double) __attribute__ ((__nothrow__ , __leaf__)); extern double __y0 (double) __attribute__ ((__nothrow__ , __leaf__));
extern double y1 (double) __attribute__ ((__nothrow__ , __leaf__)); extern double __y1 (double) __attribute__ ((__nothrow__ , __leaf__));
extern double yn (int, double) __attribute__ ((__nothrow__ , __leaf__)); extern double __yn (int, double) __attribute__ ((__nothrow__ , __leaf__));





 extern double erf (double) __attribute__ ((__nothrow__ , __leaf__)); extern double __erf (double) __attribute__ ((__nothrow__ , __leaf__));
 extern double erfc (double) __attribute__ ((__nothrow__ , __leaf__)); extern double __erfc (double) __attribute__ ((__nothrow__ , __leaf__));
extern double lgamma (double) __attribute__ ((__nothrow__ , __leaf__)); extern double __lgamma (double) __attribute__ ((__nothrow__ , __leaf__));




extern double tgamma (double) __attribute__ ((__nothrow__ , __leaf__)); extern double __tgamma (double) __attribute__ ((__nothrow__ , __leaf__));





extern double gamma (double) __attribute__ ((__nothrow__ , __leaf__)); extern double __gamma (double) __attribute__ ((__nothrow__ , __leaf__));







extern double lgamma_r (double, int *__signgamp) __attribute__ ((__nothrow__ , __leaf__)); extern double __lgamma_r (double, int *__signgamp) __attribute__ ((__nothrow__ , __leaf__));






extern double rint (double __x) __attribute__ ((__nothrow__ , __leaf__)); extern double __rint (double __x) __attribute__ ((__nothrow__ , __leaf__));


extern double nextafter (double __x, double __y) __attribute__ ((__nothrow__ , __leaf__)); extern double __nextafter (double __x, double __y) __attribute__ ((__nothrow__ , __leaf__));

extern double nexttoward (double __x, long double __y) __attribute__ ((__nothrow__ , __leaf__)); extern double __nexttoward (double __x, long double __y) __attribute__ ((__nothrow__ , __leaf__));
# 275 "/usr/include/x86_64-linux-gnu/bits/mathcalls.h" 3 4
extern double remainder (double __x, double __y) __attribute__ ((__nothrow__ , __leaf__)); extern double __remainder (double __x, double __y) __attribute__ ((__nothrow__ , __leaf__));



extern double scalbn (double __x, int __n) __attribute__ ((__nothrow__ , __leaf__)); extern double __scalbn (double __x, int __n) __attribute__ ((__nothrow__ , __leaf__));



extern int ilogb (double __x) __attribute__ ((__nothrow__ , __leaf__)); extern int __ilogb (double __x) __attribute__ ((__nothrow__ , __leaf__));
# 293 "/usr/include/x86_64-linux-gnu/bits/mathcalls.h" 3 4
extern double scalbln (double __x, long int __n) __attribute__ ((__nothrow__ , __leaf__)); extern double __scalbln (double __x, long int __n) __attribute__ ((__nothrow__ , __leaf__));



extern double nearbyint (double __x) __attribute__ ((__nothrow__ , __leaf__)); extern double __nearbyint (double __x) __attribute__ ((__nothrow__ , __leaf__));



extern double round (double __x) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__const__)); extern double __round (double __x) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__const__));



extern double trunc (double __x) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__const__)); extern double __trunc (double __x) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__const__));




extern double remquo (double __x, double __y, int *__quo) __attribute__ ((__nothrow__ , __leaf__)); extern double __remquo (double __x, double __y, int *__quo) __attribute__ ((__nothrow__ , __leaf__));






extern long int lrint (double __x) __attribute__ ((__nothrow__ , __leaf__)); extern long int __lrint (double __x) __attribute__ ((__nothrow__ , __leaf__));
__extension__
extern long long int llrint (double __x) __attribute__ ((__nothrow__ , __leaf__)); extern long long int __llrint (double __x) __attribute__ ((__nothrow__ , __leaf__));



extern long int lround (double __x) __attribute__ ((__nothrow__ , __leaf__)); extern long int __lround (double __x) __attribute__ ((__nothrow__ , __leaf__));
__extension__
extern long long int llround (double __x) __attribute__ ((__nothrow__ , __leaf__)); extern long long int __llround (double __x) __attribute__ ((__nothrow__ , __leaf__));



extern double fdim (double __x, double __y) __attribute__ ((__nothrow__ , __leaf__)); extern double __fdim (double __x, double __y) __attribute__ ((__nothrow__ , __leaf__));



extern double fmax (double __x, double __y) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__const__)); extern double __fmax (double __x, double __y) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__const__));


extern double fmin (double __x, double __y) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__const__)); extern double __fmin (double __x, double __y) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__const__));



extern double fma (double __x, double __y, double __z) __attribute__ ((__nothrow__ , __leaf__)); extern double __fma (double __x, double __y, double __z) __attribute__ ((__nothrow__ , __leaf__));
# 435 "/usr/include/x86_64-linux-gnu/bits/mathcalls.h" 3 4
extern double scalb (double __x, double __n) __attribute__ ((__nothrow__ , __leaf__)); extern double __scalb (double __x, double __n) __attribute__ ((__nothrow__ , __leaf__));
# 314 "/usr/include/math.h" 2 3 4
# 329 "/usr/include/math.h" 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/mathcalls-helper-functions.h" 1 3 4
# 20 "/usr/include/x86_64-linux-gnu/bits/mathcalls-helper-functions.h" 3 4
extern int __fpclassifyf (float __value) __attribute__ ((__nothrow__ , __leaf__))
     __attribute__ ((__const__));


extern int __signbitf (float __value) __attribute__ ((__nothrow__ , __leaf__))
     __attribute__ ((__const__));



extern int __isinff (float __value) __attribute__ ((__nothrow__ , __leaf__))
  __attribute__ ((__const__));


extern int __finitef (float __value) __attribute__ ((__nothrow__ , __leaf__))
  __attribute__ ((__const__));


extern int __isnanf (float __value) __attribute__ ((__nothrow__ , __leaf__))
  __attribute__ ((__const__));


extern int __iseqsigf (float __x, float __y) __attribute__ ((__nothrow__ , __leaf__));


extern int __issignalingf (float __value) __attribute__ ((__nothrow__ , __leaf__))
     __attribute__ ((__const__));
# 330 "/usr/include/math.h" 2 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/mathcalls.h" 1 3 4
# 53 "/usr/include/x86_64-linux-gnu/bits/mathcalls.h" 3 4
 extern float acosf (float __x) __attribute__ ((__nothrow__ , __leaf__)); extern float __acosf (float __x) __attribute__ ((__nothrow__ , __leaf__));

 extern float asinf (float __x) __attribute__ ((__nothrow__ , __leaf__)); extern float __asinf (float __x) __attribute__ ((__nothrow__ , __leaf__));

 extern float atanf (float __x) __attribute__ ((__nothrow__ , __leaf__)); extern float __atanf (float __x) __attribute__ ((__nothrow__ , __leaf__));

 extern float atan2f (float __y, float __x) __attribute__ ((__nothrow__ , __leaf__)); extern float __atan2f (float __y, float __x) __attribute__ ((__nothrow__ , __leaf__));


 extern float cosf (float __x) __attribute__ ((__nothrow__ , __leaf__)); extern float __cosf (float __x) __attribute__ ((__nothrow__ , __leaf__));

 extern float sinf (float __x) __attribute__ ((__nothrow__ , __leaf__)); extern float __sinf (float __x) __attribute__ ((__nothrow__ , __leaf__));

 extern float tanf (float __x) __attribute__ ((__nothrow__ , __leaf__)); extern float __tanf (float __x) __attribute__ ((__nothrow__ , __leaf__));




 extern float coshf (float __x) __attribute__ ((__nothrow__ , __leaf__)); extern float __coshf (float __x) __attribute__ ((__nothrow__ , __leaf__));

 extern float sinhf (float __x) __attribute__ ((__nothrow__ , __leaf__)); extern float __sinhf (float __x) __attribute__ ((__nothrow__ , __leaf__));

 extern float tanhf (float __x) __attribute__ ((__nothrow__ , __leaf__)); extern float __tanhf (float __x) __attribute__ ((__nothrow__ , __leaf__));
# 85 "/usr/include/x86_64-linux-gnu/bits/mathcalls.h" 3 4
 extern float acoshf (float __x) __attribute__ ((__nothrow__ , __leaf__)); extern float __acoshf (float __x) __attribute__ ((__nothrow__ , __leaf__));

 extern float asinhf (float __x) __attribute__ ((__nothrow__ , __leaf__)); extern float __asinhf (float __x) __attribute__ ((__nothrow__ , __leaf__));

 extern float atanhf (float __x) __attribute__ ((__nothrow__ , __leaf__)); extern float __atanhf (float __x) __attribute__ ((__nothrow__ , __leaf__));





 extern float expf (float __x) __attribute__ ((__nothrow__ , __leaf__)); extern float __expf (float __x) __attribute__ ((__nothrow__ , __leaf__));


extern float frexpf (float __x, int *__exponent) __attribute__ ((__nothrow__ , __leaf__)); extern float __frexpf (float __x, int *__exponent) __attribute__ ((__nothrow__ , __leaf__));


extern float ldexpf (float __x, int __exponent) __attribute__ ((__nothrow__ , __leaf__)); extern float __ldexpf (float __x, int __exponent) __attribute__ ((__nothrow__ , __leaf__));


 extern float logf (float __x) __attribute__ ((__nothrow__ , __leaf__)); extern float __logf (float __x) __attribute__ ((__nothrow__ , __leaf__));


 extern float log10f (float __x) __attribute__ ((__nothrow__ , __leaf__)); extern float __log10f (float __x) __attribute__ ((__nothrow__ , __leaf__));


extern float modff (float __x, float *__iptr) __attribute__ ((__nothrow__ , __leaf__)); extern float __modff (float __x, float *__iptr) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (2)));
# 119 "/usr/include/x86_64-linux-gnu/bits/mathcalls.h" 3 4
 extern float expm1f (float __x) __attribute__ ((__nothrow__ , __leaf__)); extern float __expm1f (float __x) __attribute__ ((__nothrow__ , __leaf__));


 extern float log1pf (float __x) __attribute__ ((__nothrow__ , __leaf__)); extern float __log1pf (float __x) __attribute__ ((__nothrow__ , __leaf__));


extern float logbf (float __x) __attribute__ ((__nothrow__ , __leaf__)); extern float __logbf (float __x) __attribute__ ((__nothrow__ , __leaf__));




 extern float exp2f (float __x) __attribute__ ((__nothrow__ , __leaf__)); extern float __exp2f (float __x) __attribute__ ((__nothrow__ , __leaf__));


 extern float log2f (float __x) __attribute__ ((__nothrow__ , __leaf__)); extern float __log2f (float __x) __attribute__ ((__nothrow__ , __leaf__));






 extern float powf (float __x, float __y) __attribute__ ((__nothrow__ , __leaf__)); extern float __powf (float __x, float __y) __attribute__ ((__nothrow__ , __leaf__));


extern float sqrtf (float __x) __attribute__ ((__nothrow__ , __leaf__)); extern float __sqrtf (float __x) __attribute__ ((__nothrow__ , __leaf__));



 extern float hypotf (float __x, float __y) __attribute__ ((__nothrow__ , __leaf__)); extern float __hypotf (float __x, float __y) __attribute__ ((__nothrow__ , __leaf__));




 extern float cbrtf (float __x) __attribute__ ((__nothrow__ , __leaf__)); extern float __cbrtf (float __x) __attribute__ ((__nothrow__ , __leaf__));






extern float ceilf (float __x) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__const__)); extern float __ceilf (float __x) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__const__));


extern float fabsf (float __x) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__const__)); extern float __fabsf (float __x) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__const__));


extern float floorf (float __x) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__const__)); extern float __floorf (float __x) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__const__));


extern float fmodf (float __x, float __y) __attribute__ ((__nothrow__ , __leaf__)); extern float __fmodf (float __x, float __y) __attribute__ ((__nothrow__ , __leaf__));
# 177 "/usr/include/x86_64-linux-gnu/bits/mathcalls.h" 3 4
extern int isinff (float __value) __attribute__ ((__nothrow__ , __leaf__))
  __attribute__ ((__const__));




extern int finitef (float __value) __attribute__ ((__nothrow__ , __leaf__))
  __attribute__ ((__const__));


extern float dremf (float __x, float __y) __attribute__ ((__nothrow__ , __leaf__)); extern float __dremf (float __x, float __y) __attribute__ ((__nothrow__ , __leaf__));



extern float significandf (float __x) __attribute__ ((__nothrow__ , __leaf__)); extern float __significandf (float __x) __attribute__ ((__nothrow__ , __leaf__));






extern float copysignf (float __x, float __y) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__const__)); extern float __copysignf (float __x, float __y) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__const__));




extern float nanf (const char *__tagb) __attribute__ ((__nothrow__ , __leaf__)); extern float __nanf (const char *__tagb) __attribute__ ((__nothrow__ , __leaf__));
# 213 "/usr/include/x86_64-linux-gnu/bits/mathcalls.h" 3 4
extern int isnanf (float __value) __attribute__ ((__nothrow__ , __leaf__))
  __attribute__ ((__const__));





extern float j0f (float) __attribute__ ((__nothrow__ , __leaf__)); extern float __j0f (float) __attribute__ ((__nothrow__ , __leaf__));
extern float j1f (float) __attribute__ ((__nothrow__ , __leaf__)); extern float __j1f (float) __attribute__ ((__nothrow__ , __leaf__));
extern float jnf (int, float) __attribute__ ((__nothrow__ , __leaf__)); extern float __jnf (int, float) __attribute__ ((__nothrow__ , __leaf__));
extern float y0f (float) __attribute__ ((__nothrow__ , __leaf__)); extern float __y0f (float) __attribute__ ((__nothrow__ , __leaf__));
extern float y1f (float) __attribute__ ((__nothrow__ , __leaf__)); extern float __y1f (float) __attribute__ ((__nothrow__ , __leaf__));
extern float ynf (int, float) __attribute__ ((__nothrow__ , __leaf__)); extern float __ynf (int, float) __attribute__ ((__nothrow__ , __leaf__));





 extern float erff (float) __attribute__ ((__nothrow__ , __leaf__)); extern float __erff (float) __attribute__ ((__nothrow__ , __leaf__));
 extern float erfcf (float) __attribute__ ((__nothrow__ , __leaf__)); extern float __erfcf (float) __attribute__ ((__nothrow__ , __leaf__));
extern float lgammaf (float) __attribute__ ((__nothrow__ , __leaf__)); extern float __lgammaf (float) __attribute__ ((__nothrow__ , __leaf__));




extern float tgammaf (float) __attribute__ ((__nothrow__ , __leaf__)); extern float __tgammaf (float) __attribute__ ((__nothrow__ , __leaf__));





extern float gammaf (float) __attribute__ ((__nothrow__ , __leaf__)); extern float __gammaf (float) __attribute__ ((__nothrow__ , __leaf__));







extern float lgammaf_r (float, int *__signgamp) __attribute__ ((__nothrow__ , __leaf__)); extern float __lgammaf_r (float, int *__signgamp) __attribute__ ((__nothrow__ , __leaf__));






extern float rintf (float __x) __attribute__ ((__nothrow__ , __leaf__)); extern float __rintf (float __x) __attribute__ ((__nothrow__ , __leaf__));


extern float nextafterf (float __x, float __y) __attribute__ ((__nothrow__ , __leaf__)); extern float __nextafterf (float __x, float __y) __attribute__ ((__nothrow__ , __leaf__));

extern float nexttowardf (float __x, long double __y) __attribute__ ((__nothrow__ , __leaf__)); extern float __nexttowardf (float __x, long double __y) __attribute__ ((__nothrow__ , __leaf__));
# 275 "/usr/include/x86_64-linux-gnu/bits/mathcalls.h" 3 4
extern float remainderf (float __x, float __y) __attribute__ ((__nothrow__ , __leaf__)); extern float __remainderf (float __x, float __y) __attribute__ ((__nothrow__ , __leaf__));



extern float scalbnf (float __x, int __n) __attribute__ ((__nothrow__ , __leaf__)); extern float __scalbnf (float __x, int __n) __attribute__ ((__nothrow__ , __leaf__));



extern int ilogbf (float __x) __attribute__ ((__nothrow__ , __leaf__)); extern int __ilogbf (float __x) __attribute__ ((__nothrow__ , __leaf__));
# 293 "/usr/include/x86_64-linux-gnu/bits/mathcalls.h" 3 4
extern float scalblnf (float __x, long int __n) __attribute__ ((__nothrow__ , __leaf__)); extern float __scalblnf (float __x, long int __n) __attribute__ ((__nothrow__ , __leaf__));



extern float nearbyintf (float __x) __attribute__ ((__nothrow__ , __leaf__)); extern float __nearbyintf (float __x) __attribute__ ((__nothrow__ , __leaf__));



extern float roundf (float __x) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__const__)); extern float __roundf (float __x) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__const__));



extern float truncf (float __x) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__const__)); extern float __truncf (float __x) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__const__));




extern float remquof (float __x, float __y, int *__quo) __attribute__ ((__nothrow__ , __leaf__)); extern float __remquof (float __x, float __y, int *__quo) __attribute__ ((__nothrow__ , __leaf__));






extern long int lrintf (float __x) __attribute__ ((__nothrow__ , __leaf__)); extern long int __lrintf (float __x) __attribute__ ((__nothrow__ , __leaf__));
__extension__
extern long long int llrintf (float __x) __attribute__ ((__nothrow__ , __leaf__)); extern long long int __llrintf (float __x) __attribute__ ((__nothrow__ , __leaf__));



extern long int lroundf (float __x) __attribute__ ((__nothrow__ , __leaf__)); extern long int __lroundf (float __x) __attribute__ ((__nothrow__ , __leaf__));
__extension__
extern long long int llroundf (float __x) __attribute__ ((__nothrow__ , __leaf__)); extern long long int __llroundf (float __x) __attribute__ ((__nothrow__ , __leaf__));



extern float fdimf (float __x, float __y) __attribute__ ((__nothrow__ , __leaf__)); extern float __fdimf (float __x, float __y) __attribute__ ((__nothrow__ , __leaf__));



extern float fmaxf (float __x, float __y) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__const__)); extern float __fmaxf (float __x, float __y) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__const__));


extern float fminf (float __x, float __y) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__const__)); extern float __fminf (float __x, float __y) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__const__));



extern float fmaf (float __x, float __y, float __z) __attribute__ ((__nothrow__ , __leaf__)); extern float __fmaf (float __x, float __y, float __z) __attribute__ ((__nothrow__ , __leaf__));
# 435 "/usr/include/x86_64-linux-gnu/bits/mathcalls.h" 3 4
extern float scalbf (float __x, float __n) __attribute__ ((__nothrow__ , __leaf__)); extern float __scalbf (float __x, float __n) __attribute__ ((__nothrow__ , __leaf__));
# 331 "/usr/include/math.h" 2 3 4
# 398 "/usr/include/math.h" 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/mathcalls-helper-functions.h" 1 3 4
# 20 "/usr/include/x86_64-linux-gnu/bits/mathcalls-helper-functions.h" 3 4
extern int __fpclassifyl (long double __value) __attribute__ ((__nothrow__ , __leaf__))
     __attribute__ ((__const__));


extern int __signbitl (long double __value) __attribute__ ((__nothrow__ , __leaf__))
     __attribute__ ((__const__));



extern int __isinfl (long double __value) __attribute__ ((__nothrow__ , __leaf__))
  __attribute__ ((__const__));


extern int __finitel (long double __value) __attribute__ ((__nothrow__ , __leaf__))
  __attribute__ ((__const__));


extern int __isnanl (long double __value) __attribute__ ((__nothrow__ , __leaf__))
  __attribute__ ((__const__));


extern int __iseqsigl (long double __x, long double __y) __attribute__ ((__nothrow__ , __leaf__));


extern int __issignalingl (long double __value) __attribute__ ((__nothrow__ , __leaf__))
     __attribute__ ((__const__));
# 399 "/usr/include/math.h" 2 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/mathcalls.h" 1 3 4
# 53 "/usr/include/x86_64-linux-gnu/bits/mathcalls.h" 3 4
 extern long double acosl (long double __x) __attribute__ ((__nothrow__ , __leaf__)); extern long double __acosl (long double __x) __attribute__ ((__nothrow__ , __leaf__));

 extern long double asinl (long double __x) __attribute__ ((__nothrow__ , __leaf__)); extern long double __asinl (long double __x) __attribute__ ((__nothrow__ , __leaf__));

 extern long double atanl (long double __x) __attribute__ ((__nothrow__ , __leaf__)); extern long double __atanl (long double __x) __attribute__ ((__nothrow__ , __leaf__));

 extern long double atan2l (long double __y, long double __x) __attribute__ ((__nothrow__ , __leaf__)); extern long double __atan2l (long double __y, long double __x) __attribute__ ((__nothrow__ , __leaf__));


 extern long double cosl (long double __x) __attribute__ ((__nothrow__ , __leaf__)); extern long double __cosl (long double __x) __attribute__ ((__nothrow__ , __leaf__));

 extern long double sinl (long double __x) __attribute__ ((__nothrow__ , __leaf__)); extern long double __sinl (long double __x) __attribute__ ((__nothrow__ , __leaf__));

 extern long double tanl (long double __x) __attribute__ ((__nothrow__ , __leaf__)); extern long double __tanl (long double __x) __attribute__ ((__nothrow__ , __leaf__));




 extern long double coshl (long double __x) __attribute__ ((__nothrow__ , __leaf__)); extern long double __coshl (long double __x) __attribute__ ((__nothrow__ , __leaf__));

 extern long double sinhl (long double __x) __attribute__ ((__nothrow__ , __leaf__)); extern long double __sinhl (long double __x) __attribute__ ((__nothrow__ , __leaf__));

 extern long double tanhl (long double __x) __attribute__ ((__nothrow__ , __leaf__)); extern long double __tanhl (long double __x) __attribute__ ((__nothrow__ , __leaf__));
# 85 "/usr/include/x86_64-linux-gnu/bits/mathcalls.h" 3 4
 extern long double acoshl (long double __x) __attribute__ ((__nothrow__ , __leaf__)); extern long double __acoshl (long double __x) __attribute__ ((__nothrow__ , __leaf__));

 extern long double asinhl (long double __x) __attribute__ ((__nothrow__ , __leaf__)); extern long double __asinhl (long double __x) __attribute__ ((__nothrow__ , __leaf__));

 extern long double atanhl (long double __x) __attribute__ ((__nothrow__ , __leaf__)); extern long double __atanhl (long double __x) __attribute__ ((__nothrow__ , __leaf__));





 extern long double expl (long double __x) __attribute__ ((__nothrow__ , __leaf__)); extern long double __expl (long double __x) __attribute__ ((__nothrow__ , __leaf__));


extern long double frexpl (long double __x, int *__exponent) __attribute__ ((__nothrow__ , __leaf__)); extern long double __frexpl (long double __x, int *__exponent) __attribute__ ((__nothrow__ , __leaf__));


extern long double ldexpl (long double __x, int __exponent) __attribute__ ((__nothrow__ , __leaf__)); extern long double __ldexpl (long double __x, int __exponent) __attribute__ ((__nothrow__ , __leaf__));


 extern long double logl (long double __x) __attribute__ ((__nothrow__ , __leaf__)); extern long double __logl (long double __x) __attribute__ ((__nothrow__ , __leaf__));


 extern long double log10l (long double __x) __attribute__ ((__nothrow__ , __leaf__)); extern long double __log10l (long double __x) __attribute__ ((__nothrow__ , __leaf__));


extern long double modfl (long double __x, long double *__iptr) __attribute__ ((__nothrow__ , __leaf__)); extern long double __modfl (long double __x, long double *__iptr) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__nonnull__ (2)));
# 119 "/usr/include/x86_64-linux-gnu/bits/mathcalls.h" 3 4
 extern long double expm1l (long double __x) __attribute__ ((__nothrow__ , __leaf__)); extern long double __expm1l (long double __x) __attribute__ ((__nothrow__ , __leaf__));


 extern long double log1pl (long double __x) __attribute__ ((__nothrow__ , __leaf__)); extern long double __log1pl (long double __x) __attribute__ ((__nothrow__ , __leaf__));


extern long double logbl (long double __x) __attribute__ ((__nothrow__ , __leaf__)); extern long double __logbl (long double __x) __attribute__ ((__nothrow__ , __leaf__));




 extern long double exp2l (long double __x) __attribute__ ((__nothrow__ , __leaf__)); extern long double __exp2l (long double __x) __attribute__ ((__nothrow__ , __leaf__));


 extern long double log2l (long double __x) __attribute__ ((__nothrow__ , __leaf__)); extern long double __log2l (long double __x) __attribute__ ((__nothrow__ , __leaf__));






 extern long double powl (long double __x, long double __y) __attribute__ ((__nothrow__ , __leaf__)); extern long double __powl (long double __x, long double __y) __attribute__ ((__nothrow__ , __leaf__));


extern long double sqrtl (long double __x) __attribute__ ((__nothrow__ , __leaf__)); extern long double __sqrtl (long double __x) __attribute__ ((__nothrow__ , __leaf__));



 extern long double hypotl (long double __x, long double __y) __attribute__ ((__nothrow__ , __leaf__)); extern long double __hypotl (long double __x, long double __y) __attribute__ ((__nothrow__ , __leaf__));




 extern long double cbrtl (long double __x) __attribute__ ((__nothrow__ , __leaf__)); extern long double __cbrtl (long double __x) __attribute__ ((__nothrow__ , __leaf__));






extern long double ceill (long double __x) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__const__)); extern long double __ceill (long double __x) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__const__));


extern long double fabsl (long double __x) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__const__)); extern long double __fabsl (long double __x) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__const__));


extern long double floorl (long double __x) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__const__)); extern long double __floorl (long double __x) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__const__));


extern long double fmodl (long double __x, long double __y) __attribute__ ((__nothrow__ , __leaf__)); extern long double __fmodl (long double __x, long double __y) __attribute__ ((__nothrow__ , __leaf__));
# 177 "/usr/include/x86_64-linux-gnu/bits/mathcalls.h" 3 4
extern int isinfl (long double __value) __attribute__ ((__nothrow__ , __leaf__))
  __attribute__ ((__const__));




extern int finitel (long double __value) __attribute__ ((__nothrow__ , __leaf__))
  __attribute__ ((__const__));


extern long double dreml (long double __x, long double __y) __attribute__ ((__nothrow__ , __leaf__)); extern long double __dreml (long double __x, long double __y) __attribute__ ((__nothrow__ , __leaf__));



extern long double significandl (long double __x) __attribute__ ((__nothrow__ , __leaf__)); extern long double __significandl (long double __x) __attribute__ ((__nothrow__ , __leaf__));






extern long double copysignl (long double __x, long double __y) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__const__)); extern long double __copysignl (long double __x, long double __y) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__const__));




extern long double nanl (const char *__tagb) __attribute__ ((__nothrow__ , __leaf__)); extern long double __nanl (const char *__tagb) __attribute__ ((__nothrow__ , __leaf__));
# 213 "/usr/include/x86_64-linux-gnu/bits/mathcalls.h" 3 4
extern int isnanl (long double __value) __attribute__ ((__nothrow__ , __leaf__))
  __attribute__ ((__const__));





extern long double j0l (long double) __attribute__ ((__nothrow__ , __leaf__)); extern long double __j0l (long double) __attribute__ ((__nothrow__ , __leaf__));
extern long double j1l (long double) __attribute__ ((__nothrow__ , __leaf__)); extern long double __j1l (long double) __attribute__ ((__nothrow__ , __leaf__));
extern long double jnl (int, long double) __attribute__ ((__nothrow__ , __leaf__)); extern long double __jnl (int, long double) __attribute__ ((__nothrow__ , __leaf__));
extern long double y0l (long double) __attribute__ ((__nothrow__ , __leaf__)); extern long double __y0l (long double) __attribute__ ((__nothrow__ , __leaf__));
extern long double y1l (long double) __attribute__ ((__nothrow__ , __leaf__)); extern long double __y1l (long double) __attribute__ ((__nothrow__ , __leaf__));
extern long double ynl (int, long double) __attribute__ ((__nothrow__ , __leaf__)); extern long double __ynl (int, long double) __attribute__ ((__nothrow__ , __leaf__));





 extern long double erfl (long double) __attribute__ ((__nothrow__ , __leaf__)); extern long double __erfl (long double) __attribute__ ((__nothrow__ , __leaf__));
 extern long double erfcl (long double) __attribute__ ((__nothrow__ , __leaf__)); extern long double __erfcl (long double) __attribute__ ((__nothrow__ , __leaf__));
extern long double lgammal (long double) __attribute__ ((__nothrow__ , __leaf__)); extern long double __lgammal (long double) __attribute__ ((__nothrow__ , __leaf__));




extern long double tgammal (long double) __attribute__ ((__nothrow__ , __leaf__)); extern long double __tgammal (long double) __attribute__ ((__nothrow__ , __leaf__));





extern long double gammal (long double) __attribute__ ((__nothrow__ , __leaf__)); extern long double __gammal (long double) __attribute__ ((__nothrow__ , __leaf__));







extern long double lgammal_r (long double, int *__signgamp) __attribute__ ((__nothrow__ , __leaf__)); extern long double __lgammal_r (long double, int *__signgamp) __attribute__ ((__nothrow__ , __leaf__));






extern long double rintl (long double __x) __attribute__ ((__nothrow__ , __leaf__)); extern long double __rintl (long double __x) __attribute__ ((__nothrow__ , __leaf__));


extern long double nextafterl (long double __x, long double __y) __attribute__ ((__nothrow__ , __leaf__)); extern long double __nextafterl (long double __x, long double __y) __attribute__ ((__nothrow__ , __leaf__));

extern long double nexttowardl (long double __x, long double __y) __attribute__ ((__nothrow__ , __leaf__)); extern long double __nexttowardl (long double __x, long double __y) __attribute__ ((__nothrow__ , __leaf__));
# 275 "/usr/include/x86_64-linux-gnu/bits/mathcalls.h" 3 4
extern long double remainderl (long double __x, long double __y) __attribute__ ((__nothrow__ , __leaf__)); extern long double __remainderl (long double __x, long double __y) __attribute__ ((__nothrow__ , __leaf__));



extern long double scalbnl (long double __x, int __n) __attribute__ ((__nothrow__ , __leaf__)); extern long double __scalbnl (long double __x, int __n) __attribute__ ((__nothrow__ , __leaf__));



extern int ilogbl (long double __x) __attribute__ ((__nothrow__ , __leaf__)); extern int __ilogbl (long double __x) __attribute__ ((__nothrow__ , __leaf__));
# 293 "/usr/include/x86_64-linux-gnu/bits/mathcalls.h" 3 4
extern long double scalblnl (long double __x, long int __n) __attribute__ ((__nothrow__ , __leaf__)); extern long double __scalblnl (long double __x, long int __n) __attribute__ ((__nothrow__ , __leaf__));



extern long double nearbyintl (long double __x) __attribute__ ((__nothrow__ , __leaf__)); extern long double __nearbyintl (long double __x) __attribute__ ((__nothrow__ , __leaf__));



extern long double roundl (long double __x) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__const__)); extern long double __roundl (long double __x) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__const__));



extern long double truncl (long double __x) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__const__)); extern long double __truncl (long double __x) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__const__));




extern long double remquol (long double __x, long double __y, int *__quo) __attribute__ ((__nothrow__ , __leaf__)); extern long double __remquol (long double __x, long double __y, int *__quo) __attribute__ ((__nothrow__ , __leaf__));






extern long int lrintl (long double __x) __attribute__ ((__nothrow__ , __leaf__)); extern long int __lrintl (long double __x) __attribute__ ((__nothrow__ , __leaf__));
__extension__
extern long long int llrintl (long double __x) __attribute__ ((__nothrow__ , __leaf__)); extern long long int __llrintl (long double __x) __attribute__ ((__nothrow__ , __leaf__));



extern long int lroundl (long double __x) __attribute__ ((__nothrow__ , __leaf__)); extern long int __lroundl (long double __x) __attribute__ ((__nothrow__ , __leaf__));
__extension__
extern long long int llroundl (long double __x) __attribute__ ((__nothrow__ , __leaf__)); extern long long int __llroundl (long double __x) __attribute__ ((__nothrow__ , __leaf__));



extern long double fdiml (long double __x, long double __y) __attribute__ ((__nothrow__ , __leaf__)); extern long double __fdiml (long double __x, long double __y) __attribute__ ((__nothrow__ , __leaf__));



extern long double fmaxl (long double __x, long double __y) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__const__)); extern long double __fmaxl (long double __x, long double __y) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__const__));


extern long double fminl (long double __x, long double __y) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__const__)); extern long double __fminl (long double __x, long double __y) __attribute__ ((__nothrow__ , __leaf__)) __attribute__ ((__const__));



extern long double fmal (long double __x, long double __y, long double __z) __attribute__ ((__nothrow__ , __leaf__)); extern long double __fmal (long double __x, long double __y, long double __z) __attribute__ ((__nothrow__ , __leaf__));
# 435 "/usr/include/x86_64-linux-gnu/bits/mathcalls.h" 3 4
extern long double scalbl (long double __x, long double __n) __attribute__ ((__nothrow__ , __leaf__)); extern long double __scalbl (long double __x, long double __n) __attribute__ ((__nothrow__ , __leaf__));
# 400 "/usr/include/math.h" 2 3 4
# 481 "/usr/include/math.h" 3 4
# 1 "/usr/include/x86_64-linux-gnu/bits/mathcalls-helper-functions.h" 1 3 4
# 20 "/usr/include/x86_64-linux-gnu/bits/mathcalls-helper-functions.h" 3 4
extern int __fpclassifyf128 (_Float128 __value) __attribute__ ((__nothrow__ , __leaf__))
     __attribute__ ((__const__));


extern int __signbitf128 (_Float128 __value) __attribute__ ((__nothrow__ , __leaf__))
     __attribute__ ((__const__));



extern int __isinff128 (_Float128 __value) __attribute__ ((__nothrow__ , __leaf__))
  __attribute__ ((__const__));


extern int __finitef128 (_Float128 __value) __attribute__ ((__nothrow__ , __leaf__))
  __attribute__ ((__const__));


extern int __isnanf128 (_Float128 __value) __attribute__ ((__nothrow__ , __leaf__))
  __attribute__ ((__const__));


extern int __iseqsigf128 (_Float128 __x, _Float128 __y) __attribute__ ((__nothrow__ , __leaf__));


extern int __issignalingf128 (_Float128 __value) __attribute__ ((__nothrow__ , __leaf__))
     __attribute__ ((__const__));
# 482 "/usr/include/math.h" 2 3 4
# 854 "/usr/include/math.h" 3 4
extern int signgam;
# 934 "/usr/include/math.h" 3 4
enum
  {
    FP_NAN =

      0,
    FP_INFINITE =

      1,
    FP_ZERO =

      2,
    FP_SUBNORMAL =

      3,
    FP_NORMAL =

      4
  };
# 1472 "/usr/include/math.h" 3 4

# 43 "tables.h" 2
# 58 "tables.h"

# 58 "tables.h"
extern fixed_t finesine[5*8192/4];


extern fixed_t* finecosine;



extern fixed_t finetangent[8192/2];
# 78 "tables.h"
typedef unsigned angle_t;





extern angle_t tantoangle[2048 +1];




int
SlopeDiv
( unsigned num,
  unsigned den);
# 30 "p_pspr.h" 2
# 39 "p_pspr.h"
# 1 "info.h" 1
# 28 "info.h"
# 1 "d_think.h" 1
# 41 "d_think.h"
typedef void (*actionf_v)();
typedef void (*actionf_p1)( void* );
typedef void (*actionf_p2)( void*, void* );

typedef union
{
  actionf_p1 acp1;
  actionf_v acv;
  actionf_p2 acp2;

} actionf_t;
# 60 "d_think.h"
typedef actionf_t think_t;



typedef struct thinker_s
{
    struct thinker_s* prev;
    struct thinker_s* next;
    think_t function;

} thinker_t;
# 29 "info.h" 2

typedef enum
{
    SPR_TROO,
    SPR_SHTG,
    SPR_PUNG,
    SPR_PISG,
    SPR_PISF,
    SPR_SHTF,
    SPR_SHT2,
    SPR_CHGG,
    SPR_CHGF,
    SPR_MISG,
    SPR_MISF,
    SPR_SAWG,
    SPR_PLSG,
    SPR_PLSF,
    SPR_BFGG,
    SPR_BFGF,
    SPR_BLUD,
    SPR_PUFF,
    SPR_BAL1,
    SPR_BAL2,
    SPR_PLSS,
    SPR_PLSE,
    SPR_MISL,
    SPR_BFS1,
    SPR_BFE1,
    SPR_BFE2,
    SPR_TFOG,
    SPR_IFOG,
    SPR_PLAY,
    SPR_POSS,
    SPR_SPOS,
    SPR_VILE,
    SPR_FIRE,
    SPR_FATB,
    SPR_FBXP,
    SPR_SKEL,
    SPR_MANF,
    SPR_FATT,
    SPR_CPOS,
    SPR_SARG,
    SPR_HEAD,
    SPR_BAL7,
    SPR_BOSS,
    SPR_BOS2,
    SPR_SKUL,
    SPR_SPID,
    SPR_BSPI,
    SPR_APLS,
    SPR_APBX,
    SPR_CYBR,
    SPR_PAIN,
    SPR_SSWV,
    SPR_KEEN,
    SPR_BBRN,
    SPR_BOSF,
    SPR_ARM1,
    SPR_ARM2,
    SPR_BAR1,
    SPR_BEXP,
    SPR_FCAN,
    SPR_BON1,
    SPR_BON2,
    SPR_BKEY,
    SPR_RKEY,
    SPR_YKEY,
    SPR_BSKU,
    SPR_RSKU,
    SPR_YSKU,
    SPR_STIM,
    SPR_MEDI,
    SPR_SOUL,
    SPR_PINV,
    SPR_PSTR,
    SPR_PINS,
    SPR_MEGA,
    SPR_SUIT,
    SPR_PMAP,
    SPR_PVIS,
    SPR_CLIP,
    SPR_AMMO,
    SPR_ROCK,
    SPR_BROK,
    SPR_CELL,
    SPR_CELP,
    SPR_SHEL,
    SPR_SBOX,
    SPR_BPAK,
    SPR_BFUG,
    SPR_MGUN,
    SPR_CSAW,
    SPR_LAUN,
    SPR_PLAS,
    SPR_SHOT,
    SPR_SGN2,
    SPR_COLU,
    SPR_SMT2,
    SPR_GOR1,
    SPR_POL2,
    SPR_POL5,
    SPR_POL4,
    SPR_POL3,
    SPR_POL1,
    SPR_POL6,
    SPR_GOR2,
    SPR_GOR3,
    SPR_GOR4,
    SPR_GOR5,
    SPR_SMIT,
    SPR_COL1,
    SPR_COL2,
    SPR_COL3,
    SPR_COL4,
    SPR_CAND,
    SPR_CBRA,
    SPR_COL6,
    SPR_TRE1,
    SPR_TRE2,
    SPR_ELEC,
    SPR_CEYE,
    SPR_FSKU,
    SPR_COL5,
    SPR_TBLU,
    SPR_TGRN,
    SPR_TRED,
    SPR_SMBT,
    SPR_SMGT,
    SPR_SMRT,
    SPR_HDB1,
    SPR_HDB2,
    SPR_HDB3,
    SPR_HDB4,
    SPR_HDB5,
    SPR_HDB6,
    SPR_POB1,
    SPR_POB2,
    SPR_BRS1,
    SPR_TLMP,
    SPR_TLP2,
    NUMSPRITES

} spritenum_t;

typedef enum
{
    S_NULL,
    S_LIGHTDONE,
    S_PUNCH,
    S_PUNCHDOWN,
    S_PUNCHUP,
    S_PUNCH1,
    S_PUNCH2,
    S_PUNCH3,
    S_PUNCH4,
    S_PUNCH5,
    S_PISTOL,
    S_PISTOLDOWN,
    S_PISTOLUP,
    S_PISTOL1,
    S_PISTOL2,
    S_PISTOL3,
    S_PISTOL4,
    S_PISTOLFLASH,
    S_SGUN,
    S_SGUNDOWN,
    S_SGUNUP,
    S_SGUN1,
    S_SGUN2,
    S_SGUN3,
    S_SGUN4,
    S_SGUN5,
    S_SGUN6,
    S_SGUN7,
    S_SGUN8,
    S_SGUN9,
    S_SGUNFLASH1,
    S_SGUNFLASH2,
    S_DSGUN,
    S_DSGUNDOWN,
    S_DSGUNUP,
    S_DSGUN1,
    S_DSGUN2,
    S_DSGUN3,
    S_DSGUN4,
    S_DSGUN5,
    S_DSGUN6,
    S_DSGUN7,
    S_DSGUN8,
    S_DSGUN9,
    S_DSGUN10,
    S_DSNR1,
    S_DSNR2,
    S_DSGUNFLASH1,
    S_DSGUNFLASH2,
    S_CHAIN,
    S_CHAINDOWN,
    S_CHAINUP,
    S_CHAIN1,
    S_CHAIN2,
    S_CHAIN3,
    S_CHAINFLASH1,
    S_CHAINFLASH2,
    S_MISSILE,
    S_MISSILEDOWN,
    S_MISSILEUP,
    S_MISSILE1,
    S_MISSILE2,
    S_MISSILE3,
    S_MISSILEFLASH1,
    S_MISSILEFLASH2,
    S_MISSILEFLASH3,
    S_MISSILEFLASH4,
    S_SAW,
    S_SAWB,
    S_SAWDOWN,
    S_SAWUP,
    S_SAW1,
    S_SAW2,
    S_SAW3,
    S_PLASMA,
    S_PLASMADOWN,
    S_PLASMAUP,
    S_PLASMA1,
    S_PLASMA2,
    S_PLASMAFLASH1,
    S_PLASMAFLASH2,
    S_BFG,
    S_BFGDOWN,
    S_BFGUP,
    S_BFG1,
    S_BFG2,
    S_BFG3,
    S_BFG4,
    S_BFGFLASH1,
    S_BFGFLASH2,
    S_BLOOD1,
    S_BLOOD2,
    S_BLOOD3,
    S_PUFF1,
    S_PUFF2,
    S_PUFF3,
    S_PUFF4,
    S_TBALL1,
    S_TBALL2,
    S_TBALLX1,
    S_TBALLX2,
    S_TBALLX3,
    S_RBALL1,
    S_RBALL2,
    S_RBALLX1,
    S_RBALLX2,
    S_RBALLX3,
    S_PLASBALL,
    S_PLASBALL2,
    S_PLASEXP,
    S_PLASEXP2,
    S_PLASEXP3,
    S_PLASEXP4,
    S_PLASEXP5,
    S_ROCKET,
    S_BFGSHOT,
    S_BFGSHOT2,
    S_BFGLAND,
    S_BFGLAND2,
    S_BFGLAND3,
    S_BFGLAND4,
    S_BFGLAND5,
    S_BFGLAND6,
    S_BFGEXP,
    S_BFGEXP2,
    S_BFGEXP3,
    S_BFGEXP4,
    S_EXPLODE1,
    S_EXPLODE2,
    S_EXPLODE3,
    S_TFOG,
    S_TFOG01,
    S_TFOG02,
    S_TFOG2,
    S_TFOG3,
    S_TFOG4,
    S_TFOG5,
    S_TFOG6,
    S_TFOG7,
    S_TFOG8,
    S_TFOG9,
    S_TFOG10,
    S_IFOG,
    S_IFOG01,
    S_IFOG02,
    S_IFOG2,
    S_IFOG3,
    S_IFOG4,
    S_IFOG5,
    S_PLAY,
    S_PLAY_RUN1,
    S_PLAY_RUN2,
    S_PLAY_RUN3,
    S_PLAY_RUN4,
    S_PLAY_ATK1,
    S_PLAY_ATK2,
    S_PLAY_PAIN,
    S_PLAY_PAIN2,
    S_PLAY_DIE1,
    S_PLAY_DIE2,
    S_PLAY_DIE3,
    S_PLAY_DIE4,
    S_PLAY_DIE5,
    S_PLAY_DIE6,
    S_PLAY_DIE7,
    S_PLAY_XDIE1,
    S_PLAY_XDIE2,
    S_PLAY_XDIE3,
    S_PLAY_XDIE4,
    S_PLAY_XDIE5,
    S_PLAY_XDIE6,
    S_PLAY_XDIE7,
    S_PLAY_XDIE8,
    S_PLAY_XDIE9,
    S_POSS_STND,
    S_POSS_STND2,
    S_POSS_RUN1,
    S_POSS_RUN2,
    S_POSS_RUN3,
    S_POSS_RUN4,
    S_POSS_RUN5,
    S_POSS_RUN6,
    S_POSS_RUN7,
    S_POSS_RUN8,
    S_POSS_ATK1,
    S_POSS_ATK2,
    S_POSS_ATK3,
    S_POSS_PAIN,
    S_POSS_PAIN2,
    S_POSS_DIE1,
    S_POSS_DIE2,
    S_POSS_DIE3,
    S_POSS_DIE4,
    S_POSS_DIE5,
    S_POSS_XDIE1,
    S_POSS_XDIE2,
    S_POSS_XDIE3,
    S_POSS_XDIE4,
    S_POSS_XDIE5,
    S_POSS_XDIE6,
    S_POSS_XDIE7,
    S_POSS_XDIE8,
    S_POSS_XDIE9,
    S_POSS_RAISE1,
    S_POSS_RAISE2,
    S_POSS_RAISE3,
    S_POSS_RAISE4,
    S_SPOS_STND,
    S_SPOS_STND2,
    S_SPOS_RUN1,
    S_SPOS_RUN2,
    S_SPOS_RUN3,
    S_SPOS_RUN4,
    S_SPOS_RUN5,
    S_SPOS_RUN6,
    S_SPOS_RUN7,
    S_SPOS_RUN8,
    S_SPOS_ATK1,
    S_SPOS_ATK2,
    S_SPOS_ATK3,
    S_SPOS_PAIN,
    S_SPOS_PAIN2,
    S_SPOS_DIE1,
    S_SPOS_DIE2,
    S_SPOS_DIE3,
    S_SPOS_DIE4,
    S_SPOS_DIE5,
    S_SPOS_XDIE1,
    S_SPOS_XDIE2,
    S_SPOS_XDIE3,
    S_SPOS_XDIE4,
    S_SPOS_XDIE5,
    S_SPOS_XDIE6,
    S_SPOS_XDIE7,
    S_SPOS_XDIE8,
    S_SPOS_XDIE9,
    S_SPOS_RAISE1,
    S_SPOS_RAISE2,
    S_SPOS_RAISE3,
    S_SPOS_RAISE4,
    S_SPOS_RAISE5,
    S_VILE_STND,
    S_VILE_STND2,
    S_VILE_RUN1,
    S_VILE_RUN2,
    S_VILE_RUN3,
    S_VILE_RUN4,
    S_VILE_RUN5,
    S_VILE_RUN6,
    S_VILE_RUN7,
    S_VILE_RUN8,
    S_VILE_RUN9,
    S_VILE_RUN10,
    S_VILE_RUN11,
    S_VILE_RUN12,
    S_VILE_ATK1,
    S_VILE_ATK2,
    S_VILE_ATK3,
    S_VILE_ATK4,
    S_VILE_ATK5,
    S_VILE_ATK6,
    S_VILE_ATK7,
    S_VILE_ATK8,
    S_VILE_ATK9,
    S_VILE_ATK10,
    S_VILE_ATK11,
    S_VILE_HEAL1,
    S_VILE_HEAL2,
    S_VILE_HEAL3,
    S_VILE_PAIN,
    S_VILE_PAIN2,
    S_VILE_DIE1,
    S_VILE_DIE2,
    S_VILE_DIE3,
    S_VILE_DIE4,
    S_VILE_DIE5,
    S_VILE_DIE6,
    S_VILE_DIE7,
    S_VILE_DIE8,
    S_VILE_DIE9,
    S_VILE_DIE10,
    S_FIRE1,
    S_FIRE2,
    S_FIRE3,
    S_FIRE4,
    S_FIRE5,
    S_FIRE6,
    S_FIRE7,
    S_FIRE8,
    S_FIRE9,
    S_FIRE10,
    S_FIRE11,
    S_FIRE12,
    S_FIRE13,
    S_FIRE14,
    S_FIRE15,
    S_FIRE16,
    S_FIRE17,
    S_FIRE18,
    S_FIRE19,
    S_FIRE20,
    S_FIRE21,
    S_FIRE22,
    S_FIRE23,
    S_FIRE24,
    S_FIRE25,
    S_FIRE26,
    S_FIRE27,
    S_FIRE28,
    S_FIRE29,
    S_FIRE30,
    S_SMOKE1,
    S_SMOKE2,
    S_SMOKE3,
    S_SMOKE4,
    S_SMOKE5,
    S_TRACER,
    S_TRACER2,
    S_TRACEEXP1,
    S_TRACEEXP2,
    S_TRACEEXP3,
    S_SKEL_STND,
    S_SKEL_STND2,
    S_SKEL_RUN1,
    S_SKEL_RUN2,
    S_SKEL_RUN3,
    S_SKEL_RUN4,
    S_SKEL_RUN5,
    S_SKEL_RUN6,
    S_SKEL_RUN7,
    S_SKEL_RUN8,
    S_SKEL_RUN9,
    S_SKEL_RUN10,
    S_SKEL_RUN11,
    S_SKEL_RUN12,
    S_SKEL_FIST1,
    S_SKEL_FIST2,
    S_SKEL_FIST3,
    S_SKEL_FIST4,
    S_SKEL_MISS1,
    S_SKEL_MISS2,
    S_SKEL_MISS3,
    S_SKEL_MISS4,
    S_SKEL_PAIN,
    S_SKEL_PAIN2,
    S_SKEL_DIE1,
    S_SKEL_DIE2,
    S_SKEL_DIE3,
    S_SKEL_DIE4,
    S_SKEL_DIE5,
    S_SKEL_DIE6,
    S_SKEL_RAISE1,
    S_SKEL_RAISE2,
    S_SKEL_RAISE3,
    S_SKEL_RAISE4,
    S_SKEL_RAISE5,
    S_SKEL_RAISE6,
    S_FATSHOT1,
    S_FATSHOT2,
    S_FATSHOTX1,
    S_FATSHOTX2,
    S_FATSHOTX3,
    S_FATT_STND,
    S_FATT_STND2,
    S_FATT_RUN1,
    S_FATT_RUN2,
    S_FATT_RUN3,
    S_FATT_RUN4,
    S_FATT_RUN5,
    S_FATT_RUN6,
    S_FATT_RUN7,
    S_FATT_RUN8,
    S_FATT_RUN9,
    S_FATT_RUN10,
    S_FATT_RUN11,
    S_FATT_RUN12,
    S_FATT_ATK1,
    S_FATT_ATK2,
    S_FATT_ATK3,
    S_FATT_ATK4,
    S_FATT_ATK5,
    S_FATT_ATK6,
    S_FATT_ATK7,
    S_FATT_ATK8,
    S_FATT_ATK9,
    S_FATT_ATK10,
    S_FATT_PAIN,
    S_FATT_PAIN2,
    S_FATT_DIE1,
    S_FATT_DIE2,
    S_FATT_DIE3,
    S_FATT_DIE4,
    S_FATT_DIE5,
    S_FATT_DIE6,
    S_FATT_DIE7,
    S_FATT_DIE8,
    S_FATT_DIE9,
    S_FATT_DIE10,
    S_FATT_RAISE1,
    S_FATT_RAISE2,
    S_FATT_RAISE3,
    S_FATT_RAISE4,
    S_FATT_RAISE5,
    S_FATT_RAISE6,
    S_FATT_RAISE7,
    S_FATT_RAISE8,
    S_CPOS_STND,
    S_CPOS_STND2,
    S_CPOS_RUN1,
    S_CPOS_RUN2,
    S_CPOS_RUN3,
    S_CPOS_RUN4,
    S_CPOS_RUN5,
    S_CPOS_RUN6,
    S_CPOS_RUN7,
    S_CPOS_RUN8,
    S_CPOS_ATK1,
    S_CPOS_ATK2,
    S_CPOS_ATK3,
    S_CPOS_ATK4,
    S_CPOS_PAIN,
    S_CPOS_PAIN2,
    S_CPOS_DIE1,
    S_CPOS_DIE2,
    S_CPOS_DIE3,
    S_CPOS_DIE4,
    S_CPOS_DIE5,
    S_CPOS_DIE6,
    S_CPOS_DIE7,
    S_CPOS_XDIE1,
    S_CPOS_XDIE2,
    S_CPOS_XDIE3,
    S_CPOS_XDIE4,
    S_CPOS_XDIE5,
    S_CPOS_XDIE6,
    S_CPOS_RAISE1,
    S_CPOS_RAISE2,
    S_CPOS_RAISE3,
    S_CPOS_RAISE4,
    S_CPOS_RAISE5,
    S_CPOS_RAISE6,
    S_CPOS_RAISE7,
    S_TROO_STND,
    S_TROO_STND2,
    S_TROO_RUN1,
    S_TROO_RUN2,
    S_TROO_RUN3,
    S_TROO_RUN4,
    S_TROO_RUN5,
    S_TROO_RUN6,
    S_TROO_RUN7,
    S_TROO_RUN8,
    S_TROO_ATK1,
    S_TROO_ATK2,
    S_TROO_ATK3,
    S_TROO_PAIN,
    S_TROO_PAIN2,
    S_TROO_DIE1,
    S_TROO_DIE2,
    S_TROO_DIE3,
    S_TROO_DIE4,
    S_TROO_DIE5,
    S_TROO_XDIE1,
    S_TROO_XDIE2,
    S_TROO_XDIE3,
    S_TROO_XDIE4,
    S_TROO_XDIE5,
    S_TROO_XDIE6,
    S_TROO_XDIE7,
    S_TROO_XDIE8,
    S_TROO_RAISE1,
    S_TROO_RAISE2,
    S_TROO_RAISE3,
    S_TROO_RAISE4,
    S_TROO_RAISE5,
    S_SARG_STND,
    S_SARG_STND2,
    S_SARG_RUN1,
    S_SARG_RUN2,
    S_SARG_RUN3,
    S_SARG_RUN4,
    S_SARG_RUN5,
    S_SARG_RUN6,
    S_SARG_RUN7,
    S_SARG_RUN8,
    S_SARG_ATK1,
    S_SARG_ATK2,
    S_SARG_ATK3,
    S_SARG_PAIN,
    S_SARG_PAIN2,
    S_SARG_DIE1,
    S_SARG_DIE2,
    S_SARG_DIE3,
    S_SARG_DIE4,
    S_SARG_DIE5,
    S_SARG_DIE6,
    S_SARG_RAISE1,
    S_SARG_RAISE2,
    S_SARG_RAISE3,
    S_SARG_RAISE4,
    S_SARG_RAISE5,
    S_SARG_RAISE6,
    S_HEAD_STND,
    S_HEAD_RUN1,
    S_HEAD_ATK1,
    S_HEAD_ATK2,
    S_HEAD_ATK3,
    S_HEAD_PAIN,
    S_HEAD_PAIN2,
    S_HEAD_PAIN3,
    S_HEAD_DIE1,
    S_HEAD_DIE2,
    S_HEAD_DIE3,
    S_HEAD_DIE4,
    S_HEAD_DIE5,
    S_HEAD_DIE6,
    S_HEAD_RAISE1,
    S_HEAD_RAISE2,
    S_HEAD_RAISE3,
    S_HEAD_RAISE4,
    S_HEAD_RAISE5,
    S_HEAD_RAISE6,
    S_BRBALL1,
    S_BRBALL2,
    S_BRBALLX1,
    S_BRBALLX2,
    S_BRBALLX3,
    S_BOSS_STND,
    S_BOSS_STND2,
    S_BOSS_RUN1,
    S_BOSS_RUN2,
    S_BOSS_RUN3,
    S_BOSS_RUN4,
    S_BOSS_RUN5,
    S_BOSS_RUN6,
    S_BOSS_RUN7,
    S_BOSS_RUN8,
    S_BOSS_ATK1,
    S_BOSS_ATK2,
    S_BOSS_ATK3,
    S_BOSS_PAIN,
    S_BOSS_PAIN2,
    S_BOSS_DIE1,
    S_BOSS_DIE2,
    S_BOSS_DIE3,
    S_BOSS_DIE4,
    S_BOSS_DIE5,
    S_BOSS_DIE6,
    S_BOSS_DIE7,
    S_BOSS_RAISE1,
    S_BOSS_RAISE2,
    S_BOSS_RAISE3,
    S_BOSS_RAISE4,
    S_BOSS_RAISE5,
    S_BOSS_RAISE6,
    S_BOSS_RAISE7,
    S_BOS2_STND,
    S_BOS2_STND2,
    S_BOS2_RUN1,
    S_BOS2_RUN2,
    S_BOS2_RUN3,
    S_BOS2_RUN4,
    S_BOS2_RUN5,
    S_BOS2_RUN6,
    S_BOS2_RUN7,
    S_BOS2_RUN8,
    S_BOS2_ATK1,
    S_BOS2_ATK2,
    S_BOS2_ATK3,
    S_BOS2_PAIN,
    S_BOS2_PAIN2,
    S_BOS2_DIE1,
    S_BOS2_DIE2,
    S_BOS2_DIE3,
    S_BOS2_DIE4,
    S_BOS2_DIE5,
    S_BOS2_DIE6,
    S_BOS2_DIE7,
    S_BOS2_RAISE1,
    S_BOS2_RAISE2,
    S_BOS2_RAISE3,
    S_BOS2_RAISE4,
    S_BOS2_RAISE5,
    S_BOS2_RAISE6,
    S_BOS2_RAISE7,
    S_SKULL_STND,
    S_SKULL_STND2,
    S_SKULL_RUN1,
    S_SKULL_RUN2,
    S_SKULL_ATK1,
    S_SKULL_ATK2,
    S_SKULL_ATK3,
    S_SKULL_ATK4,
    S_SKULL_PAIN,
    S_SKULL_PAIN2,
    S_SKULL_DIE1,
    S_SKULL_DIE2,
    S_SKULL_DIE3,
    S_SKULL_DIE4,
    S_SKULL_DIE5,
    S_SKULL_DIE6,
    S_SPID_STND,
    S_SPID_STND2,
    S_SPID_RUN1,
    S_SPID_RUN2,
    S_SPID_RUN3,
    S_SPID_RUN4,
    S_SPID_RUN5,
    S_SPID_RUN6,
    S_SPID_RUN7,
    S_SPID_RUN8,
    S_SPID_RUN9,
    S_SPID_RUN10,
    S_SPID_RUN11,
    S_SPID_RUN12,
    S_SPID_ATK1,
    S_SPID_ATK2,
    S_SPID_ATK3,
    S_SPID_ATK4,
    S_SPID_PAIN,
    S_SPID_PAIN2,
    S_SPID_DIE1,
    S_SPID_DIE2,
    S_SPID_DIE3,
    S_SPID_DIE4,
    S_SPID_DIE5,
    S_SPID_DIE6,
    S_SPID_DIE7,
    S_SPID_DIE8,
    S_SPID_DIE9,
    S_SPID_DIE10,
    S_SPID_DIE11,
    S_BSPI_STND,
    S_BSPI_STND2,
    S_BSPI_SIGHT,
    S_BSPI_RUN1,
    S_BSPI_RUN2,
    S_BSPI_RUN3,
    S_BSPI_RUN4,
    S_BSPI_RUN5,
    S_BSPI_RUN6,
    S_BSPI_RUN7,
    S_BSPI_RUN8,
    S_BSPI_RUN9,
    S_BSPI_RUN10,
    S_BSPI_RUN11,
    S_BSPI_RUN12,
    S_BSPI_ATK1,
    S_BSPI_ATK2,
    S_BSPI_ATK3,
    S_BSPI_ATK4,
    S_BSPI_PAIN,
    S_BSPI_PAIN2,
    S_BSPI_DIE1,
    S_BSPI_DIE2,
    S_BSPI_DIE3,
    S_BSPI_DIE4,
    S_BSPI_DIE5,
    S_BSPI_DIE6,
    S_BSPI_DIE7,
    S_BSPI_RAISE1,
    S_BSPI_RAISE2,
    S_BSPI_RAISE3,
    S_BSPI_RAISE4,
    S_BSPI_RAISE5,
    S_BSPI_RAISE6,
    S_BSPI_RAISE7,
    S_ARACH_PLAZ,
    S_ARACH_PLAZ2,
    S_ARACH_PLEX,
    S_ARACH_PLEX2,
    S_ARACH_PLEX3,
    S_ARACH_PLEX4,
    S_ARACH_PLEX5,
    S_CYBER_STND,
    S_CYBER_STND2,
    S_CYBER_RUN1,
    S_CYBER_RUN2,
    S_CYBER_RUN3,
    S_CYBER_RUN4,
    S_CYBER_RUN5,
    S_CYBER_RUN6,
    S_CYBER_RUN7,
    S_CYBER_RUN8,
    S_CYBER_ATK1,
    S_CYBER_ATK2,
    S_CYBER_ATK3,
    S_CYBER_ATK4,
    S_CYBER_ATK5,
    S_CYBER_ATK6,
    S_CYBER_PAIN,
    S_CYBER_DIE1,
    S_CYBER_DIE2,
    S_CYBER_DIE3,
    S_CYBER_DIE4,
    S_CYBER_DIE5,
    S_CYBER_DIE6,
    S_CYBER_DIE7,
    S_CYBER_DIE8,
    S_CYBER_DIE9,
    S_CYBER_DIE10,
    S_PAIN_STND,
    S_PAIN_RUN1,
    S_PAIN_RUN2,
    S_PAIN_RUN3,
    S_PAIN_RUN4,
    S_PAIN_RUN5,
    S_PAIN_RUN6,
    S_PAIN_ATK1,
    S_PAIN_ATK2,
    S_PAIN_ATK3,
    S_PAIN_ATK4,
    S_PAIN_PAIN,
    S_PAIN_PAIN2,
    S_PAIN_DIE1,
    S_PAIN_DIE2,
    S_PAIN_DIE3,
    S_PAIN_DIE4,
    S_PAIN_DIE5,
    S_PAIN_DIE6,
    S_PAIN_RAISE1,
    S_PAIN_RAISE2,
    S_PAIN_RAISE3,
    S_PAIN_RAISE4,
    S_PAIN_RAISE5,
    S_PAIN_RAISE6,
    S_SSWV_STND,
    S_SSWV_STND2,
    S_SSWV_RUN1,
    S_SSWV_RUN2,
    S_SSWV_RUN3,
    S_SSWV_RUN4,
    S_SSWV_RUN5,
    S_SSWV_RUN6,
    S_SSWV_RUN7,
    S_SSWV_RUN8,
    S_SSWV_ATK1,
    S_SSWV_ATK2,
    S_SSWV_ATK3,
    S_SSWV_ATK4,
    S_SSWV_ATK5,
    S_SSWV_ATK6,
    S_SSWV_PAIN,
    S_SSWV_PAIN2,
    S_SSWV_DIE1,
    S_SSWV_DIE2,
    S_SSWV_DIE3,
    S_SSWV_DIE4,
    S_SSWV_DIE5,
    S_SSWV_XDIE1,
    S_SSWV_XDIE2,
    S_SSWV_XDIE3,
    S_SSWV_XDIE4,
    S_SSWV_XDIE5,
    S_SSWV_XDIE6,
    S_SSWV_XDIE7,
    S_SSWV_XDIE8,
    S_SSWV_XDIE9,
    S_SSWV_RAISE1,
    S_SSWV_RAISE2,
    S_SSWV_RAISE3,
    S_SSWV_RAISE4,
    S_SSWV_RAISE5,
    S_KEENSTND,
    S_COMMKEEN,
    S_COMMKEEN2,
    S_COMMKEEN3,
    S_COMMKEEN4,
    S_COMMKEEN5,
    S_COMMKEEN6,
    S_COMMKEEN7,
    S_COMMKEEN8,
    S_COMMKEEN9,
    S_COMMKEEN10,
    S_COMMKEEN11,
    S_COMMKEEN12,
    S_KEENPAIN,
    S_KEENPAIN2,
    S_BRAIN,
    S_BRAIN_PAIN,
    S_BRAIN_DIE1,
    S_BRAIN_DIE2,
    S_BRAIN_DIE3,
    S_BRAIN_DIE4,
    S_BRAINEYE,
    S_BRAINEYESEE,
    S_BRAINEYE1,
    S_SPAWN1,
    S_SPAWN2,
    S_SPAWN3,
    S_SPAWN4,
    S_SPAWNFIRE1,
    S_SPAWNFIRE2,
    S_SPAWNFIRE3,
    S_SPAWNFIRE4,
    S_SPAWNFIRE5,
    S_SPAWNFIRE6,
    S_SPAWNFIRE7,
    S_SPAWNFIRE8,
    S_BRAINEXPLODE1,
    S_BRAINEXPLODE2,
    S_BRAINEXPLODE3,
    S_ARM1,
    S_ARM1A,
    S_ARM2,
    S_ARM2A,
    S_BAR1,
    S_BAR2,
    S_BEXP,
    S_BEXP2,
    S_BEXP3,
    S_BEXP4,
    S_BEXP5,
    S_BBAR1,
    S_BBAR2,
    S_BBAR3,
    S_BON1,
    S_BON1A,
    S_BON1B,
    S_BON1C,
    S_BON1D,
    S_BON1E,
    S_BON2,
    S_BON2A,
    S_BON2B,
    S_BON2C,
    S_BON2D,
    S_BON2E,
    S_BKEY,
    S_BKEY2,
    S_RKEY,
    S_RKEY2,
    S_YKEY,
    S_YKEY2,
    S_BSKULL,
    S_BSKULL2,
    S_RSKULL,
    S_RSKULL2,
    S_YSKULL,
    S_YSKULL2,
    S_STIM,
    S_MEDI,
    S_SOUL,
    S_SOUL2,
    S_SOUL3,
    S_SOUL4,
    S_SOUL5,
    S_SOUL6,
    S_PINV,
    S_PINV2,
    S_PINV3,
    S_PINV4,
    S_PSTR,
    S_PINS,
    S_PINS2,
    S_PINS3,
    S_PINS4,
    S_MEGA,
    S_MEGA2,
    S_MEGA3,
    S_MEGA4,
    S_SUIT,
    S_PMAP,
    S_PMAP2,
    S_PMAP3,
    S_PMAP4,
    S_PMAP5,
    S_PMAP6,
    S_PVIS,
    S_PVIS2,
    S_CLIP,
    S_AMMO,
    S_ROCK,
    S_BROK,
    S_CELL,
    S_CELP,
    S_SHEL,
    S_SBOX,
    S_BPAK,
    S_BFUG,
    S_MGUN,
    S_CSAW,
    S_LAUN,
    S_PLAS,
    S_SHOT,
    S_SHOT2,
    S_COLU,
    S_STALAG,
    S_BLOODYTWITCH,
    S_BLOODYTWITCH2,
    S_BLOODYTWITCH3,
    S_BLOODYTWITCH4,
    S_DEADTORSO,
    S_DEADBOTTOM,
    S_HEADSONSTICK,
    S_GIBS,
    S_HEADONASTICK,
    S_HEADCANDLES,
    S_HEADCANDLES2,
    S_DEADSTICK,
    S_LIVESTICK,
    S_LIVESTICK2,
    S_MEAT2,
    S_MEAT3,
    S_MEAT4,
    S_MEAT5,
    S_STALAGTITE,
    S_TALLGRNCOL,
    S_SHRTGRNCOL,
    S_TALLREDCOL,
    S_SHRTREDCOL,
    S_CANDLESTIK,
    S_CANDELABRA,
    S_SKULLCOL,
    S_TORCHTREE,
    S_BIGTREE,
    S_TECHPILLAR,
    S_EVILEYE,
    S_EVILEYE2,
    S_EVILEYE3,
    S_EVILEYE4,
    S_FLOATSKULL,
    S_FLOATSKULL2,
    S_FLOATSKULL3,
    S_HEARTCOL,
    S_HEARTCOL2,
    S_BLUETORCH,
    S_BLUETORCH2,
    S_BLUETORCH3,
    S_BLUETORCH4,
    S_GREENTORCH,
    S_GREENTORCH2,
    S_GREENTORCH3,
    S_GREENTORCH4,
    S_REDTORCH,
    S_REDTORCH2,
    S_REDTORCH3,
    S_REDTORCH4,
    S_BTORCHSHRT,
    S_BTORCHSHRT2,
    S_BTORCHSHRT3,
    S_BTORCHSHRT4,
    S_GTORCHSHRT,
    S_GTORCHSHRT2,
    S_GTORCHSHRT3,
    S_GTORCHSHRT4,
    S_RTORCHSHRT,
    S_RTORCHSHRT2,
    S_RTORCHSHRT3,
    S_RTORCHSHRT4,
    S_HANGNOGUTS,
    S_HANGBNOBRAIN,
    S_HANGTLOOKDN,
    S_HANGTSKULL,
    S_HANGTLOOKUP,
    S_HANGTNOBRAIN,
    S_COLONGIBS,
    S_SMALLPOOL,
    S_BRAINSTEM,
    S_TECHLAMP,
    S_TECHLAMP2,
    S_TECHLAMP3,
    S_TECHLAMP4,
    S_TECH2LAMP,
    S_TECH2LAMP2,
    S_TECH2LAMP3,
    S_TECH2LAMP4,
    NUMSTATES
} statenum_t;


typedef struct
{
  spritenum_t sprite;
  long frame;
  long tics;

  actionf_t action;
  statenum_t nextstate;
  long misc1, misc2;
} state_t;

extern state_t states[NUMSTATES];
extern char *sprnames[NUMSPRITES];



typedef enum {
    MT_PLAYER,
    MT_POSSESSED,
    MT_SHOTGUY,
    MT_VILE,
    MT_FIRE,
    MT_UNDEAD,
    MT_TRACER,
    MT_SMOKE,
    MT_FATSO,
    MT_FATSHOT,
    MT_CHAINGUY,
    MT_TROOP,
    MT_SERGEANT,
    MT_SHADOWS,
    MT_HEAD,
    MT_BRUISER,
    MT_BRUISERSHOT,
    MT_KNIGHT,
    MT_SKULL,
    MT_SPIDER,
    MT_BABY,
    MT_CYBORG,
    MT_PAIN,
    MT_WOLFSS,
    MT_KEEN,
    MT_BOSSBRAIN,
    MT_BOSSSPIT,
    MT_BOSSTARGET,
    MT_SPAWNSHOT,
    MT_SPAWNFIRE,
    MT_BARREL,
    MT_TROOPSHOT,
    MT_HEADSHOT,
    MT_ROCKET,
    MT_PLASMA,
    MT_BFG,
    MT_ARACHPLAZ,
    MT_PUFF,
    MT_BLOOD,
    MT_TFOG,
    MT_IFOG,
    MT_TELEPORTMAN,
    MT_EXTRABFG,
    MT_MISC0,
    MT_MISC1,
    MT_MISC2,
    MT_MISC3,
    MT_MISC4,
    MT_MISC5,
    MT_MISC6,
    MT_MISC7,
    MT_MISC8,
    MT_MISC9,
    MT_MISC10,
    MT_MISC11,
    MT_MISC12,
    MT_INV,
    MT_MISC13,
    MT_INS,
    MT_MISC14,
    MT_MISC15,
    MT_MISC16,
    MT_MEGA,
    MT_CLIP,
    MT_MISC17,
    MT_MISC18,
    MT_MISC19,
    MT_MISC20,
    MT_MISC21,
    MT_MISC22,
    MT_MISC23,
    MT_MISC24,
    MT_MISC25,
    MT_CHAINGUN,
    MT_MISC26,
    MT_MISC27,
    MT_MISC28,
    MT_SHOTGUN,
    MT_SUPERSHOTGUN,
    MT_MISC29,
    MT_MISC30,
    MT_MISC31,
    MT_MISC32,
    MT_MISC33,
    MT_MISC34,
    MT_MISC35,
    MT_MISC36,
    MT_MISC37,
    MT_MISC38,
    MT_MISC39,
    MT_MISC40,
    MT_MISC41,
    MT_MISC42,
    MT_MISC43,
    MT_MISC44,
    MT_MISC45,
    MT_MISC46,
    MT_MISC47,
    MT_MISC48,
    MT_MISC49,
    MT_MISC50,
    MT_MISC51,
    MT_MISC52,
    MT_MISC53,
    MT_MISC54,
    MT_MISC55,
    MT_MISC56,
    MT_MISC57,
    MT_MISC58,
    MT_MISC59,
    MT_MISC60,
    MT_MISC61,
    MT_MISC62,
    MT_MISC63,
    MT_MISC64,
    MT_MISC65,
    MT_MISC66,
    MT_MISC67,
    MT_MISC68,
    MT_MISC69,
    MT_MISC70,
    MT_MISC71,
    MT_MISC72,
    MT_MISC73,
    MT_MISC74,
    MT_MISC75,
    MT_MISC76,
    MT_MISC77,
    MT_MISC78,
    MT_MISC79,
    MT_MISC80,
    MT_MISC81,
    MT_MISC82,
    MT_MISC83,
    MT_MISC84,
    MT_MISC85,
    MT_MISC86,
    NUMMOBJTYPES

} mobjtype_t;

typedef struct
{
    int doomednum;
    int spawnstate;
    int spawnhealth;
    int seestate;
    int seesound;
    int reactiontime;
    int attacksound;
    int painstate;
    int painchance;
    int painsound;
    int meleestate;
    int missilestate;
    int deathstate;
    int xdeathstate;
    int deathsound;
    int speed;
    int radius;
    int height;
    int mass;
    int damage;
    int activesound;
    int flags;
    int raisestate;

} mobjinfo_t;

extern mobjinfo_t mobjinfo[NUMMOBJTYPES];
# 40 "p_pspr.h" 2
# 60 "p_pspr.h"
typedef enum
{
    ps_weapon,
    ps_flash,
    NUMPSPRITES

} psprnum_t;

typedef struct
{
    state_t* state;
    int tics;
    fixed_t sx;
    fixed_t sy;

} pspdef_t;
# 33 "d_player.h" 2



# 1 "p_mobj.h" 1
# 117 "p_mobj.h"
typedef enum
{

    MF_SPECIAL = 1,

    MF_SOLID = 2,

    MF_SHOOTABLE = 4,

    MF_NOSECTOR = 8,

    MF_NOBLOCKMAP = 16,


    MF_AMBUSH = 32,

    MF_JUSTHIT = 64,

    MF_JUSTATTACKED = 128,


    MF_SPAWNCEILING = 256,



    MF_NOGRAVITY = 512,



    MF_DROPOFF = 0x400,

    MF_PICKUP = 0x800,

    MF_NOCLIP = 0x1000,

    MF_SLIDE = 0x2000,


    MF_FLOAT = 0x4000,


    MF_TELEPORT = 0x8000,


    MF_MISSILE = 0x10000,


    MF_DROPPED = 0x20000,


    MF_SHADOW = 0x40000,


    MF_NOBLOOD = 0x80000,


    MF_CORPSE = 0x100000,


    MF_INFLOAT = 0x200000,




    MF_COUNTKILL = 0x400000,



    MF_COUNTITEM = 0x800000,



    MF_SKULLFLY = 0x1000000,



    MF_NOTDMATCH = 0x2000000,





    MF_TRANSLATION = 0xc000000,

    MF_TRANSSHIFT = 26

} mobjflag_t;



typedef struct mobj_s
{

    thinker_t thinker;


    fixed_t x;
    fixed_t y;
    fixed_t z;


    struct mobj_s* snext;
    struct mobj_s* sprev;


    angle_t angle;
    spritenum_t sprite;
    int frame;



    struct mobj_s* bnext;
    struct mobj_s* bprev;

    struct subsector_s* subsector;


    fixed_t floorz;
    fixed_t ceilingz;


    fixed_t radius;
    fixed_t height;


    fixed_t momx;
    fixed_t momy;
    fixed_t momz;


    int validcount;

    mobjtype_t type;
    mobjinfo_t* info;

    int tics;
    state_t* state;
    int flags;
    int health;


    int movedir;
    int movecount;



    struct mobj_s* target;



    int reactiontime;



    int threshold;



    struct player_s* player;


    int lastlook;


    mapthing_t spawnpoint;


    struct mobj_s* tracer;

} mobj_t;
# 37 "d_player.h" 2




# 1 "d_ticcmd.h" 1
# 36 "d_ticcmd.h"
typedef struct
{
    char forwardmove;
    char sidemove;
    short angleturn;
    short consistancy;
    byte chatchar;
    byte buttons;
} ticcmd_t;
# 42 "d_player.h" 2
# 53 "d_player.h"
typedef enum
{

    PST_LIVE,

    PST_DEAD,

    PST_REBORN

} playerstate_t;





typedef enum
{

    CF_NOCLIP = 1,

    CF_GODMODE = 2,

    CF_NOMOMENTUM = 4

} cheat_t;





typedef struct player_s
{
    mobj_t* mo;
    playerstate_t playerstate;
    ticcmd_t cmd;




    fixed_t viewz;

    fixed_t viewheight;

    fixed_t deltaviewheight;

    fixed_t bob;



    int health;
    int armorpoints;

    int armortype;


    int powers[NUMPOWERS];
    boolean cards[NUMCARDS];
    boolean backpack;


    int frags[4];
    weapontype_t readyweapon;


    weapontype_t pendingweapon;

    boolean weaponowned[NUMWEAPONS];
    int ammo[NUMAMMO];
    int maxammo[NUMAMMO];


    int attackdown;
    int usedown;



    int cheats;


    int refire;


    int killcount;
    int itemcount;
    int secretcount;


    char* message;


    int damagecount;
    int bonuscount;


    mobj_t* attacker;


    int extralight;



    int fixedcolormap;



    int colormap;


    pspdef_t psprites[NUMPSPRITES];


    boolean didsecret;

} player_t;






typedef struct
{
    boolean in;


    int skills;
    int sitems;
    int ssecret;
    int stime;
    int frags[4];
    int score;

} wbplayerstruct_t;

typedef struct
{
    int epsd;


    boolean didsecret;


    int last;
    int next;

    int maxkills;
    int maxitems;
    int maxsecret;
    int maxfrags;


    int partime;


    int pnum;

    wbplayerstruct_t plyr[4];

} wbstartstruct_t;
# 27 "d_net.h" 2
# 51 "d_net.h"
typedef enum
{
    CMD_SEND = 1,
    CMD_GET = 2

} command_t;





typedef struct
{

    unsigned checksum;

    byte retransmitfrom;

    byte starttic;
    byte player;
    byte numtics;
    ticcmd_t cmds[12];

} doomdata_t;




typedef struct
{

    long id;


    short intnum;


    short command;

    short remotenode;


    short datalength;



    short numnodes;

    short ticdup;

    short extratics;

    short deathmatch;

    short savegame;
    short episode;
    short map;
    short skill;


    short consoleplayer;
    short numplayers;







    short angleoffset;

    short drone;


    doomdata_t data;

} doomcom_t;




void NetUpdate (void);



void D_QuitNetGame (void);


void TryRunTics (void);
# 35 "doomstat.h" 2
# 49 "doomstat.h"
extern boolean nomonsters;
extern boolean respawnparm;
extern boolean fastparm;

extern boolean devparm;






extern GameMode_t gamemode;
extern GameMission_t gamemission;


extern boolean modifiedgame;




extern Language_t language;







extern skill_t startskill;
extern int startepisode;
extern int startmap;

extern boolean autostart;


extern skill_t gameskill;
extern int gameepisode;
extern int gamemap;


extern boolean respawnmonsters;


extern boolean netgame;



extern boolean deathmatch;
# 109 "doomstat.h"
extern int snd_SfxVolume;
extern int snd_MusicVolume;





extern int snd_MusicDevice;
extern int snd_SfxDevice;

extern int snd_DesiredMusicDevice;
extern int snd_DesiredSfxDevice;
# 130 "doomstat.h"
extern boolean statusbaractive;

extern boolean automapactive;
extern boolean menuactive;
extern boolean paused;


extern boolean viewactive;

extern boolean nodrawers;
extern boolean noblit;

extern int viewwindowx;
extern int viewwindowy;
extern int viewheight;
extern int viewwidth;
extern int scaledviewwidth;
# 155 "doomstat.h"
extern int viewangleoffset;


extern int consoleplayer;
extern int displayplayer;






extern int totalkills;
extern int totalitems;
extern int totalsecret;


extern int levelstarttic;
extern int leveltime;







extern boolean usergame;


extern boolean demoplayback;
extern boolean demorecording;


extern boolean singledemo;





extern gamestate_t gamestate;
# 208 "doomstat.h"
extern int gametic;



extern player_t players[4];


extern boolean playeringame[4];




extern mapthing_t deathmatchstarts[10];
extern mapthing_t* deathmatch_p;


extern mapthing_t playerstarts[4];



extern wbstartstruct_t wminfo;




extern int maxammo[NUMAMMO];
# 244 "doomstat.h"
extern char basedefault[1024];
extern FILE* debugfile;


extern boolean precache;




extern gamestate_t wipegamestate;

extern int mouseSensitivity;


extern boolean singletics;

extern int bodyqueslot;






extern int skyflatnum;






extern doomcom_t* doomcom;


extern doomdata_t* netbuffer;


extern ticcmd_t localcmds[12];
extern int rndindex;

extern int maketic;
extern int nettics[8];

extern ticcmd_t netcmds[4][12];
extern int ticdup;
# 32 "g_game.c" 2

# 1 "z_zone.h" 1
# 47 "z_zone.h"
void Z_Init (void);
void* Z_Malloc (int size, int tag, void *ptr);
void Z_Free (void *ptr);
void Z_FreeTags (int lowtag, int hightag);
void Z_DumpHeap (int lowtag, int hightag);
void Z_FileDumpHeap (FILE *f);
void Z_CheckHeap (void);
void Z_ChangeTag2 (void *ptr, int tag);
int Z_FreeMemory (void);


typedef struct memblock_s
{
    int size;
    void** user;
    int tag;
    int id;
    struct memblock_s* next;
    struct memblock_s* prev;
} memblock_t;
# 34 "g_game.c" 2
# 1 "f_finale.h" 1
# 28 "f_finale.h"
# 1 "d_event.h" 1
# 35 "d_event.h"
typedef enum
{
    ev_keydown,
    ev_keyup,
    ev_mouse,
    ev_joystick
} evtype_t;


typedef struct
{
    evtype_t type;
    int data1;
    int data2;
    int data3;
} event_t;


typedef enum
{
    ga_nothing,
    ga_loadlevel,
    ga_newgame,
    ga_loadgame,
    ga_savegame,
    ga_playdemo,
    ga_completed,
    ga_victory,
    ga_worlddone,
    ga_screenshot
} gameaction_t;






typedef enum
{

    BT_ATTACK = 1,

    BT_USE = 2,


    BT_SPECIAL = 128,
    BT_SPECIALMASK = 3,



    BT_CHANGE = 4,

    BT_WEAPONMASK = (8+16+32),
    BT_WEAPONSHIFT = 3,


    BTS_PAUSE = 1,

    BTS_SAVEGAME = 2,



    BTS_SAVEMASK = (4+8+16),
    BTS_SAVESHIFT = 2,

} buttoncode_t;
# 110 "d_event.h"
extern event_t events[64];
extern int eventhead;
extern int eventtail;

extern gameaction_t gameaction;
# 29 "f_finale.h" 2





boolean F_Responder (event_t* ev);


void F_Ticker (void);


void F_Drawer (void);


void F_StartFinale (void);
# 35 "g_game.c" 2
# 1 "m_argv.h" 1
# 29 "m_argv.h"
extern int myargc;
extern char** myargv;



int M_CheckParm (char* check);
# 36 "g_game.c" 2
# 1 "m_misc.h" 1
# 34 "m_misc.h"
boolean
M_WriteFile
( char const* name,
  void* source,
  int length );

int
M_ReadFile
( char const* name,
  byte** buffer );

void M_ScreenShot (void);

void M_LoadDefaults (void);

void M_SaveDefaults (void);


int
M_DrawText
( int x,
  int y,
  boolean direct,
  char* string );
# 37 "g_game.c" 2
# 1 "m_menu.h" 1
# 38 "m_menu.h"
boolean M_Responder (event_t *ev);




void M_Ticker (void);



void M_Drawer (void);



void M_Init (void);



void M_StartControlPanel (void);
# 38 "g_game.c" 2
# 1 "m_random.h" 1
# 33 "m_random.h"
int M_Random (void);


int P_Random (void);


void M_ClearRandom (void);
# 39 "g_game.c" 2
# 1 "i_system.h" 1
# 35 "i_system.h"
void I_Init (void);




byte* I_ZoneBase (int *size);




int I_GetTime (void);
# 56 "i_system.h"
void I_StartFrame (void);







void I_StartTic (void);
# 74 "i_system.h"
ticcmd_t* I_BaseTiccmd (void);




void I_Quit (void);




byte* I_AllocLow (int length);

void I_Tactile (int on, int off, int total);


void I_Error (char *error, ...);
# 40 "g_game.c" 2

# 1 "p_setup.h" 1
# 33 "p_setup.h"
void
P_SetupLevel
( int episode,
  int map,
  int playermask,
  skill_t skill);


void P_Init (void);
# 42 "g_game.c" 2
# 1 "p_saveg.h" 1
# 34 "p_saveg.h"
void P_ArchivePlayers (void);
void P_UnArchivePlayers (void);
void P_ArchiveWorld (void);
void P_UnArchiveWorld (void);
void P_ArchiveThinkers (void);
void P_UnArchiveThinkers (void);
void P_ArchiveSpecials (void);
void P_UnArchiveSpecials (void);

extern byte* save_p;
# 43 "g_game.c" 2
# 1 "p_tick.h" 1
# 35 "p_tick.h"
void P_Ticker (void);
# 44 "g_game.c" 2

# 1 "d_main.h" 1
# 37 "d_main.h"
extern char* wadfiles[20];

void D_AddFile (char *file);
# 49 "d_main.h"
void D_DoomMain (void);


void D_PostEvent (event_t* ev);






void D_PageTicker (void);
void D_PageDrawer (void);
void D_AdvanceDemo (void);
void D_StartTitle (void);
# 46 "g_game.c" 2

# 1 "wi_stuff.h" 1
# 31 "wi_stuff.h"
typedef enum
{
    NoState = -1,
    StatCount,
    ShowNextLoc

} stateenum_t;


void WI_Ticker (void);



void WI_Drawer (void);


void WI_Start(wbstartstruct_t* wbstartstruct);
# 48 "g_game.c" 2
# 1 "hu_stuff.h" 1
# 50 "hu_stuff.h"
void HU_Init(void);
void HU_Start(void);

boolean HU_Responder(event_t* ev);

void HU_Ticker(void);
void HU_Drawer(void);
char HU_dequeueChatChar(void);
void HU_Erase(void);
# 49 "g_game.c" 2
# 1 "st_stuff.h" 1
# 42 "st_stuff.h"
boolean ST_Responder (event_t* ev);


void ST_Ticker (void);


void ST_Drawer (boolean fullscreen, boolean refresh);


void ST_Start (void);


void ST_Init (void);




typedef enum
{
    AutomapState,
    FirstPersonState

} st_stateenum_t;



typedef enum
{
    StartChatState,
    WaitDestState,
    GetChatState

} st_chatstateenum_t;


boolean ST_Responder(event_t* ev);
# 50 "g_game.c" 2
# 1 "am_map.h" 1
# 32 "am_map.h"
boolean AM_Responder (event_t* ev);


void AM_Ticker (void);



void AM_Drawer (void);



void AM_Stop (void);
# 51 "g_game.c" 2


# 1 "v_video.h" 1
# 33 "v_video.h"
# 1 "r_data.h" 1
# 27 "r_data.h"
# 1 "r_defs.h" 1
# 71 "r_defs.h"
typedef struct
{
    fixed_t x;
    fixed_t y;

} vertex_t;



struct line_s;







typedef struct
{
    thinker_t thinker;
    fixed_t x;
    fixed_t y;
    fixed_t z;

} degenmobj_t;





typedef struct
{
    fixed_t floorheight;
    fixed_t ceilingheight;
    short floorpic;
    short ceilingpic;
    short lightlevel;
    short special;
    short tag;


    int soundtraversed;


    mobj_t* soundtarget;


    int blockbox[4];


    degenmobj_t soundorg;


    int validcount;


    mobj_t* thinglist;


    void* specialdata;

    int linecount;
    struct line_s** lines;

} sector_t;
# 144 "r_defs.h"
typedef struct
{

    fixed_t textureoffset;


    fixed_t rowoffset;



    short toptexture;
    short bottomtexture;
    short midtexture;


    sector_t* sector;

} side_t;






typedef enum
{
    ST_HORIZONTAL,
    ST_VERTICAL,
    ST_POSITIVE,
    ST_NEGATIVE

} slopetype_t;



typedef struct line_s
{

    vertex_t* v1;
    vertex_t* v2;


    fixed_t dx;
    fixed_t dy;


    short flags;
    short special;
    short tag;



    short sidenum[2];



    fixed_t bbox[4];


    slopetype_t slopetype;



    sector_t* frontsector;
    sector_t* backsector;


    int validcount;


    void* specialdata;
} line_t;
# 227 "r_defs.h"
typedef struct subsector_s
{
    sector_t* sector;
    short numlines;
    short firstline;

} subsector_t;






typedef struct
{
    vertex_t* v1;
    vertex_t* v2;

    fixed_t offset;

    angle_t angle;

    side_t* sidedef;
    line_t* linedef;




    sector_t* frontsector;
    sector_t* backsector;

} seg_t;






typedef struct
{

    fixed_t x;
    fixed_t y;
    fixed_t dx;
    fixed_t dy;


    fixed_t bbox[2][4];


    unsigned short children[2];

} node_t;





typedef struct
{
    byte topdelta;
    byte length;
} post_t;


typedef post_t column_t;
# 314 "r_defs.h"
typedef byte lighttable_t;







typedef struct drawseg_s
{
    seg_t* curline;
    int x1;
    int x2;

    fixed_t scale1;
    fixed_t scale2;
    fixed_t scalestep;


    int silhouette;


    fixed_t bsilheight;


    fixed_t tsilheight;



    short* sprtopclip;
    short* sprbottomclip;
    short* maskedtexturecol;

} drawseg_t;
# 356 "r_defs.h"
typedef struct
{
    short width;
    short height;
    short leftoffset;
    short topoffset;
    int columnofs[8];

} patch_t;
# 375 "r_defs.h"
typedef struct vissprite_s
{

    struct vissprite_s* prev;
    struct vissprite_s* next;

    int x1;
    int x2;


    fixed_t gx;
    fixed_t gy;


    fixed_t gz;
    fixed_t gzt;


    fixed_t startfrac;

    fixed_t scale;


    fixed_t xiscale;

    fixed_t texturemid;
    int patch;



    lighttable_t* colormap;

    int mobjflags;

} vissprite_t;
# 427 "r_defs.h"
typedef struct
{



    boolean rotate;


    short lump[8];


    byte flip[8];

} spriteframe_t;







typedef struct
{
    int numframes;
    spriteframe_t* spriteframes;

} spritedef_t;






typedef struct
{
  fixed_t height;
  int picnum;
  int lightlevel;
  int minx;
  int maxx;



  byte pad1;


  byte top[320];
  byte pad2;
  byte pad3;

  byte bottom[320];
  byte pad4;

} visplane_t;
# 28 "r_data.h" 2
# 1 "r_state.h" 1
# 28 "r_state.h"
# 1 "r_data.h" 1
# 29 "r_state.h" 2
# 44 "r_state.h"
extern fixed_t* textureheight;


extern fixed_t* spritewidth;

extern fixed_t* spriteoffset;
extern fixed_t* spritetopoffset;

extern lighttable_t* colormaps;

extern int viewwidth;
extern int scaledviewwidth;
extern int viewheight;

extern int firstflat;


extern int* flattranslation;
extern int* texturetranslation;



extern int firstspritelump;
extern int lastspritelump;
extern int numspritelumps;






extern int numsprites;
extern spritedef_t* sprites;

extern int numvertexes;
extern vertex_t* vertexes;

extern int numsegs;
extern seg_t* segs;

extern int numsectors;
extern sector_t* sectors;

extern int numsubsectors;
extern subsector_t* subsectors;

extern int numnodes;
extern node_t* nodes;

extern int numlines;
extern line_t* lines;

extern int numsides;
extern side_t* sides;





extern fixed_t viewx;
extern fixed_t viewy;
extern fixed_t viewz;

extern angle_t viewangle;
extern player_t* viewplayer;



extern angle_t clipangle;

extern int viewangletox[8192/2];
extern angle_t xtoviewangle[320 +1];


extern fixed_t rw_distance;
extern angle_t rw_normalangle;




extern int rw_angle1;


extern int sscount;

extern visplane_t* floorplane;
extern visplane_t* ceilingplane;
# 29 "r_data.h" 2






byte*
R_GetColumn
( int tex,
  int col );



void R_InitData (void);
void R_PrecacheLevel (void);





int R_FlatNumForName (char* name);




int R_TextureNumForName (char *name);
int R_CheckTextureNumForName (char *name);
# 34 "v_video.h" 2
# 47 "v_video.h"
extern byte* screens[5];

extern int dirtybox[4];

extern byte gammatable[5][256];
extern int usegamma;




void V_Init (void);


void
V_CopyRect
( int srcx,
  int srcy,
  int srcscrn,
  int width,
  int height,
  int destx,
  int desty,
  int destscrn );

void
V_DrawPatch
( int x,
  int y,
  int scrn,
  patch_t* patch);

void
V_DrawPatchDirect
( int x,
  int y,
  int scrn,
  patch_t* patch );



void
V_DrawBlock
( int x,
  int y,
  int scrn,
  int width,
  int height,
  byte* src );


void
V_GetBlock
( int x,
  int y,
  int scrn,
  int width,
  int height,
  byte* dest );


void
V_MarkRect
( int x,
  int y,
  int width,
  int height );
# 54 "g_game.c" 2

# 1 "w_wad.h" 1
# 35 "w_wad.h"
typedef struct
{

    char identification[4];
    int numlumps;
    int infotableofs;

} wadinfo_t;


typedef struct
{
    int filepos;
    int size;
    char name[8];

} filelump_t;




typedef struct
{
    char name[8];
    int handle;
    int position;
    int size;
} lumpinfo_t;


extern void** lumpcache;
extern lumpinfo_t* lumpinfo;
extern int numlumps;

void W_InitMultipleFiles (char** filenames);
void W_Reload (void);

int W_CheckNumForName (char* name);
int W_GetNumForName (char* name);

int W_LumpLength (int lump);
void W_ReadLump (int lump, void *dest);

void* W_CacheLumpNum (int lump, int tag);
void* W_CacheLumpName (char* name, int tag);
# 56 "g_game.c" 2

# 1 "p_local.h" 1
# 27 "p_local.h"
# 1 "r_local.h" 1
# 40 "r_local.h"
# 1 "r_main.h" 1
# 38 "r_main.h"
extern fixed_t viewcos;
extern fixed_t viewsin;

extern int viewwidth;
extern int viewheight;
extern int viewwindowx;
extern int viewwindowy;



extern int centerx;
extern int centery;

extern fixed_t centerxfrac;
extern fixed_t centeryfrac;
extern fixed_t projection;

extern int validcount;

extern int linecount;
extern int loopcount;
# 77 "r_main.h"
extern lighttable_t* scalelight[16][48];
extern lighttable_t* scalelightfixed[48];
extern lighttable_t* zlight[16][128];

extern int extralight;
extern lighttable_t* fixedcolormap;
# 93 "r_main.h"
extern int detailshift;






extern void (*colfunc) (void);
extern void (*basecolfunc) (void);
extern void (*fuzzcolfunc) (void);

extern void (*spanfunc) (void);




int
R_PointOnSide
( fixed_t x,
  fixed_t y,
  node_t* node );

int
R_PointOnSegSide
( fixed_t x,
  fixed_t y,
  seg_t* line );

angle_t
R_PointToAngle
( fixed_t x,
  fixed_t y );

angle_t
R_PointToAngle2
( fixed_t x1,
  fixed_t y1,
  fixed_t x2,
  fixed_t y2 );

fixed_t
R_PointToDist
( fixed_t x,
  fixed_t y );


fixed_t R_ScaleFromGlobalAngle (angle_t visangle);

subsector_t*
R_PointInSubsector
( fixed_t x,
  fixed_t y );

void
R_AddPointToBox
( int x,
  int y,
  fixed_t* box );
# 159 "r_main.h"
void R_RenderPlayerView (player_t *player);


void R_Init (void);


void R_SetViewSize (int blocks, int detail);
# 41 "r_local.h" 2
# 1 "r_bsp.h" 1
# 31 "r_bsp.h"
extern seg_t* curline;
extern side_t* sidedef;
extern line_t* linedef;
extern sector_t* frontsector;
extern sector_t* backsector;

extern int rw_x;
extern int rw_stopx;

extern boolean segtextured;


extern boolean markfloor;
extern boolean markceiling;

extern boolean skymap;

extern drawseg_t drawsegs[256];
extern drawseg_t* ds_p;

extern lighttable_t** hscalelight;
extern lighttable_t** vscalelight;
extern lighttable_t** dscalelight;


typedef void (*drawfunc_t) (int start, int stop);



void R_ClearClipSegs (void);
void R_ClearDrawSegs (void);


void R_RenderBSPNode (int bspnum);
# 42 "r_local.h" 2
# 1 "r_segs.h" 1
# 32 "r_segs.h"
void
R_RenderMaskedSegRange
( drawseg_t* ds,
  int x1,
  int x2 );
# 43 "r_local.h" 2
# 1 "r_plane.h" 1
# 35 "r_plane.h"
extern short* lastopening;


typedef void (*planefunction_t) (int top, int bottom);

extern planefunction_t floorfunc;
extern planefunction_t ceilingfunc_t;

extern short floorclip[320];
extern short ceilingclip[320];

extern fixed_t yslope[200];
extern fixed_t distscale[320];

void R_InitPlanes (void);
void R_ClearPlanes (void);

void
R_MapPlane
( int y,
  int x1,
  int x2 );

void
R_MakeSpans
( int x,
  int t1,
  int b1,
  int t2,
  int b2 );

void R_DrawPlanes (void);

visplane_t*
R_FindPlane
( fixed_t height,
  int picnum,
  int lightlevel );

visplane_t*
R_CheckPlane
( visplane_t* pl,
  int start,
  int stop );
# 44 "r_local.h" 2

# 1 "r_things.h" 1
# 33 "r_things.h"
extern vissprite_t vissprites[128];
extern vissprite_t* vissprite_p;
extern vissprite_t vsprsortedhead;



extern short negonearray[320];
extern short screenheightarray[320];


extern short* mfloorclip;
extern short* mceilingclip;
extern fixed_t spryscale;
extern fixed_t sprtopscreen;

extern fixed_t pspritescale;
extern fixed_t pspriteiscale;


void R_DrawMaskedColumn (column_t* column);


void R_SortVisSprites (void);

void R_AddSprites (sector_t* sec);
void R_AddPSprites (void);
void R_DrawSprites (void);
void R_InitSprites (char** namelist);
void R_ClearSprites (void);
void R_DrawMasked (void);

void
R_ClipVisSprite
( vissprite_t* vis,
  int xl,
  int xh );
# 46 "r_local.h" 2
# 1 "r_draw.h" 1
# 32 "r_draw.h"
extern lighttable_t* dc_colormap;
extern int dc_x;
extern int dc_yl;
extern int dc_yh;
extern fixed_t dc_iscale;
extern fixed_t dc_texturemid;


extern byte* dc_source;





void R_DrawColumn (void);
void R_DrawColumnLow (void);


void R_DrawFuzzColumn (void);
void R_DrawFuzzColumnLow (void);




void R_DrawTranslatedColumn (void);
void R_DrawTranslatedColumnLow (void);

void
R_VideoErase
( unsigned ofs,
  int count );

extern int ds_y;
extern int ds_x1;
extern int ds_x2;

extern lighttable_t* ds_colormap;

extern fixed_t ds_xfrac;
extern fixed_t ds_yfrac;
extern fixed_t ds_xstep;
extern fixed_t ds_ystep;


extern byte* ds_source;

extern byte* translationtables;
extern byte* dc_translation;




void R_DrawSpan (void);


void R_DrawSpanLow (void);


void
R_InitBuffer
( int width,
  int height );




void R_InitTranslationTables (void);




void R_FillBackScreen (void);


void R_DrawViewBorder (void);
# 47 "r_local.h" 2
# 28 "p_local.h" 2
# 70 "p_local.h"
extern thinker_t thinkercap;


void P_InitThinkers (void);
void P_AddThinker (thinker_t* thinker);
void P_RemoveThinker (thinker_t* thinker);





void P_SetupPsprites (player_t* curplayer);
void P_MovePsprites (player_t* curplayer);
void P_DropWeapon (player_t* player);





void P_PlayerThink (player_t* player);
# 101 "p_local.h"
extern mapthing_t itemrespawnque[128];
extern int itemrespawntime[128];
extern int iquehead;
extern int iquetail;


void P_RespawnSpecials (void);

mobj_t*
P_SpawnMobj
( fixed_t x,
  fixed_t y,
  fixed_t z,
  mobjtype_t type );

void P_RemoveMobj (mobj_t* th);
boolean P_SetMobjState (mobj_t* mobj, statenum_t state);
void P_MobjThinker (mobj_t* mobj);

void P_SpawnPuff (fixed_t x, fixed_t y, fixed_t z);
void P_SpawnBlood (fixed_t x, fixed_t y, fixed_t z, int damage);
mobj_t* P_SpawnMissile (mobj_t* source, mobj_t* dest, mobjtype_t type);
void P_SpawnPlayerMissile (mobj_t* source, mobjtype_t type);





void P_NoiseAlert (mobj_t* target, mobj_t* emmiter);





typedef struct
{
    fixed_t x;
    fixed_t y;
    fixed_t dx;
    fixed_t dy;

} divline_t;

typedef struct
{
    fixed_t frac;
    boolean isaline;
    union {
 mobj_t* thing;
 line_t* line;
    } d;
} intercept_t;



extern intercept_t intercepts[128];
extern intercept_t* intercept_p;

typedef boolean (*traverser_t) (intercept_t *in);

fixed_t P_AproxDistance (fixed_t dx, fixed_t dy);
int P_PointOnLineSide (fixed_t x, fixed_t y, line_t* line);
int P_PointOnDivlineSide (fixed_t x, fixed_t y, divline_t* line);
void P_MakeDivline (line_t* li, divline_t* dl);
fixed_t P_InterceptVector (divline_t* v2, divline_t* v1);
int P_BoxOnLineSide (fixed_t* tmbox, line_t* ld);

extern fixed_t opentop;
extern fixed_t openbottom;
extern fixed_t openrange;
extern fixed_t lowfloor;

void P_LineOpening (line_t* linedef);

boolean P_BlockLinesIterator (int x, int y, boolean(*func)(line_t*) );
boolean P_BlockThingsIterator (int x, int y, boolean(*func)(mobj_t*) );





extern divline_t trace;

boolean
P_PathTraverse
( fixed_t x1,
  fixed_t y1,
  fixed_t x2,
  fixed_t y2,
  int flags,
  boolean (*trav) (intercept_t *));

void P_UnsetThingPosition (mobj_t* thing);
void P_SetThingPosition (mobj_t* thing);
# 203 "p_local.h"
extern boolean floatok;
extern fixed_t tmfloorz;
extern fixed_t tmceilingz;


extern line_t* ceilingline;

boolean P_CheckPosition (mobj_t *thing, fixed_t x, fixed_t y);
boolean P_TryMove (mobj_t* thing, fixed_t x, fixed_t y);
boolean P_TeleportMove (mobj_t* thing, fixed_t x, fixed_t y);
void P_SlideMove (mobj_t* mo);
boolean P_CheckSight (mobj_t* t1, mobj_t* t2);
void P_UseLines (player_t* player);

boolean P_ChangeSector (sector_t* sector, boolean crunch);

extern mobj_t* linetarget;

fixed_t
P_AimLineAttack
( mobj_t* t1,
  angle_t angle,
  fixed_t distance );

void
P_LineAttack
( mobj_t* t1,
  angle_t angle,
  fixed_t distance,
  fixed_t slope,
  int damage );

void
P_RadiusAttack
( mobj_t* spot,
  mobj_t* source,
  int damage );






extern byte* rejectmatrix;
extern short* blockmaplump;
extern short* blockmap;
extern int bmapwidth;
extern int bmapheight;
extern fixed_t bmaporgx;
extern fixed_t bmaporgy;
extern mobj_t** blocklinks;






extern int maxammo[NUMAMMO];
extern int clipammo[NUMAMMO];

void
P_TouchSpecialThing
( mobj_t* special,
  mobj_t* toucher );

void
P_DamageMobj
( mobj_t* target,
  mobj_t* inflictor,
  mobj_t* source,
  int damage );





# 1 "p_spec.h" 1
# 33 "p_spec.h"
extern boolean levelTimer;
extern int levelTimeCount;







void P_InitPicAnims (void);


void P_SpawnSpecials (void);


void P_UpdateSpecials (void);


boolean
P_UseSpecialLine
( mobj_t* thing,
  line_t* line,
  int side );

void
P_ShootSpecialLine
( mobj_t* thing,
  line_t* line );

void
P_CrossSpecialLine
( int linenum,
  int side,
  mobj_t* thing );

void P_PlayerInSpecialSector (player_t* player);

int
twoSided
( int sector,
  int line );

sector_t*
getSector
( int currentSector,
  int line,
  int side );

side_t*
getSide
( int currentSector,
  int line,
  int side );

fixed_t P_FindLowestFloorSurrounding(sector_t* sec);
fixed_t P_FindHighestFloorSurrounding(sector_t* sec);

fixed_t
P_FindNextHighestFloor
( sector_t* sec,
  int currentheight );

fixed_t P_FindLowestCeilingSurrounding(sector_t* sec);
fixed_t P_FindHighestCeilingSurrounding(sector_t* sec);

int
P_FindSectorFromLineTag
( line_t* line,
  int start );

int
P_FindMinSurroundingLight
( sector_t* sector,
  int max );

sector_t*
getNextSector
( line_t* line,
  sector_t* sec );





int EV_DoDonut(line_t* line);






typedef struct
{
    thinker_t thinker;
    sector_t* sector;
    int count;
    int maxlight;
    int minlight;

} fireflicker_t;



typedef struct
{
    thinker_t thinker;
    sector_t* sector;
    int count;
    int maxlight;
    int minlight;
    int maxtime;
    int mintime;

} lightflash_t;



typedef struct
{
    thinker_t thinker;
    sector_t* sector;
    int count;
    int minlight;
    int maxlight;
    int darktime;
    int brighttime;

} strobe_t;




typedef struct
{
    thinker_t thinker;
    sector_t* sector;
    int minlight;
    int maxlight;
    int direction;

} glow_t;







void P_SpawnFireFlicker (sector_t* sector);
void T_LightFlash (lightflash_t* flash);
void P_SpawnLightFlash (sector_t* sector);
void T_StrobeFlash (strobe_t* flash);

void
P_SpawnStrobeFlash
( sector_t* sector,
  int fastOrSlow,
  int inSync );

void EV_StartLightStrobing(line_t* line);
void EV_TurnTagLightsOff(line_t* line);

void
EV_LightTurnOn
( line_t* line,
  int bright );

void T_Glow(glow_t* g);
void P_SpawnGlowingLight(sector_t* sector);







typedef struct
{
    char name1[9];
    char name2[9];
    short episode;

} switchlist_t;


typedef enum
{
    top,
    middle,
    bottom

} bwhere_e;


typedef struct
{
    line_t* line;
    bwhere_e where;
    int btexture;
    int btimer;
    mobj_t* soundorg;

} button_t;
# 249 "p_spec.h"
extern button_t buttonlist[16];

void
P_ChangeSwitchTexture
( line_t* line,
  int useAgain );

void P_InitSwitchList(void);





typedef enum
{
    up,
    down,
    waiting,
    in_stasis

} plat_e;



typedef enum
{
    perpetualRaise,
    downWaitUpStay,
    raiseAndChange,
    raiseToNearestAndChange,
    blazeDWUS

} plattype_e;



typedef struct
{
    thinker_t thinker;
    sector_t* sector;
    fixed_t speed;
    fixed_t low;
    fixed_t high;
    int wait;
    int count;
    plat_e status;
    plat_e oldstatus;
    boolean crush;
    int tag;
    plattype_e type;

} plat_t;
# 309 "p_spec.h"
extern plat_t* activeplats[30];

void T_PlatRaise(plat_t* plat);

int
EV_DoPlat
( line_t* line,
  plattype_e type,
  int amount );

void P_AddActivePlat(plat_t* plat);
void P_RemoveActivePlat(plat_t* plat);
void EV_StopPlat(line_t* line);
void P_ActivateInStasis(int tag);





typedef enum
{
    normal,
    close30ThenOpen,
    close,
    open,
    raiseIn5Mins,
    blazeRaise,
    blazeOpen,
    blazeClose

} vldoor_e;



typedef struct
{
    thinker_t thinker;
    vldoor_e type;
    sector_t* sector;
    fixed_t topheight;
    fixed_t speed;


    int direction;


    int topwait;


    int topcountdown;

} vldoor_t;






void
EV_VerticalDoor
( line_t* line,
  mobj_t* thing );

int
EV_DoDoor
( line_t* line,
  vldoor_e type );

int
EV_DoLockedDoor
( line_t* line,
  vldoor_e type,
  mobj_t* thing );

void T_VerticalDoor (vldoor_t* door);
void P_SpawnDoorCloseIn30 (sector_t* sec);

void
P_SpawnDoorRaiseIn5Mins
( sector_t* sec,
  int secnum );
# 480 "p_spec.h"
typedef enum
{
    lowerToFloor,
    raiseToHighest,
    lowerAndCrush,
    crushAndRaise,
    fastCrushAndRaise,
    silentCrushAndRaise

} ceiling_e;



typedef struct
{
    thinker_t thinker;
    ceiling_e type;
    sector_t* sector;
    fixed_t bottomheight;
    fixed_t topheight;
    fixed_t speed;
    boolean crush;


    int direction;


    int tag;
    int olddirection;

} ceiling_t;
# 520 "p_spec.h"
extern ceiling_t* activeceilings[30];

int
EV_DoCeiling
( line_t* line,
  ceiling_e type );

void T_MoveCeiling (ceiling_t* ceiling);
void P_AddActiveCeiling(ceiling_t* c);
void P_RemoveActiveCeiling(ceiling_t* c);
int EV_CeilingCrushStop(line_t* line);
void P_ActivateInStasisCeiling(line_t* line);





typedef enum
{

    lowerFloor,


    lowerFloorToLowest,


    turboLower,


    raiseFloor,


    raiseFloorToNearest,


    raiseToTexture,



    lowerAndChange,

    raiseFloor24,
    raiseFloor24AndChange,
    raiseFloorCrush,


    raiseFloorTurbo,
    donutRaise,
    raiseFloor512

} floor_e;




typedef enum
{
    build8,
    turbo16

} stair_e;



typedef struct
{
    thinker_t thinker;
    floor_e type;
    boolean crush;
    sector_t* sector;
    int direction;
    int newspecial;
    short texture;
    fixed_t floordestheight;
    fixed_t speed;

} floormove_t;





typedef enum
{
    ok,
    crushed,
    pastdest

} result_e;

result_e
T_MovePlane
( sector_t* sector,
  fixed_t speed,
  fixed_t dest,
  boolean crush,
  int floorOrCeiling,
  int direction );

int
EV_BuildStairs
( line_t* line,
  stair_e type );

int
EV_DoFloor
( line_t* line,
  floor_e floortype );

void T_MoveFloor( floormove_t* floor);




int
EV_Teleport
( line_t* line,
  int side,
  mobj_t* thing );
# 280 "p_local.h" 2
# 58 "g_game.c" 2

# 1 "s_sound.h" 1
# 38 "s_sound.h"
void
S_Init
( int sfxVolume,
  int musicVolume );
# 51 "s_sound.h"
void S_Start(void);






void
S_StartSound
( void* origin,
  int sound_id );




void
S_StartSoundAtVolume
( void* origin,
  int sound_id,
  int volume );



void S_StopSound(void* origin);



void S_StartMusic(int music_id);



void
S_ChangeMusic
( int music_id,
  int looping );


void S_StopMusic(void);


void S_PauseSound(void);
void S_ResumeSound(void);





void S_UpdateSounds(void* listener);

void S_SetMusicVolume(int volume);
void S_SetSfxVolume(int volume);
# 60 "g_game.c" 2


# 1 "dstrings.h" 1
# 37 "dstrings.h"
# 1 "d_englsh.h" 1
# 38 "dstrings.h" 2
# 58 "dstrings.h"
extern char* endmsg[];
# 63 "g_game.c" 2
# 1 "sounds.h" 1
# 30 "sounds.h"
typedef struct sfxinfo_struct sfxinfo_t;

struct sfxinfo_struct
{

    char* name;


    int singularity;


    int priority;


    sfxinfo_t* link;


    int pitch;


    int volume;


    void* data;




    int usefulness;


    int lumpnum;
};







typedef struct
{

    char* name;


    int lumpnum;


    void* data;


    int handle;

} musicinfo_t;





extern sfxinfo_t S_sfx[];


extern musicinfo_t S_music[];





typedef enum
{
    mus_None,
    mus_e1m1,
    mus_e1m2,
    mus_e1m3,
    mus_e1m4,
    mus_e1m5,
    mus_e1m6,
    mus_e1m7,
    mus_e1m8,
    mus_e1m9,
    mus_e2m1,
    mus_e2m2,
    mus_e2m3,
    mus_e2m4,
    mus_e2m5,
    mus_e2m6,
    mus_e2m7,
    mus_e2m8,
    mus_e2m9,
    mus_e3m1,
    mus_e3m2,
    mus_e3m3,
    mus_e3m4,
    mus_e3m5,
    mus_e3m6,
    mus_e3m7,
    mus_e3m8,
    mus_e3m9,
    mus_inter,
    mus_intro,
    mus_bunny,
    mus_victor,
    mus_introa,
    mus_runnin,
    mus_stalks,
    mus_countd,
    mus_betwee,
    mus_doom,
    mus_the_da,
    mus_shawn,
    mus_ddtblu,
    mus_in_cit,
    mus_dead,
    mus_stlks2,
    mus_theda2,
    mus_doom2,
    mus_ddtbl2,
    mus_runni2,
    mus_dead2,
    mus_stlks3,
    mus_romero,
    mus_shawn2,
    mus_messag,
    mus_count2,
    mus_ddtbl3,
    mus_ampie,
    mus_theda3,
    mus_adrian,
    mus_messg2,
    mus_romer2,
    mus_tense,
    mus_shawn3,
    mus_openin,
    mus_evil,
    mus_ultima,
    mus_read_m,
    mus_dm2ttl,
    mus_dm2int,
    NUMMUSIC
} musicenum_t;






typedef enum
{
    sfx_None,
    sfx_pistol,
    sfx_shotgn,
    sfx_sgcock,
    sfx_dshtgn,
    sfx_dbopn,
    sfx_dbcls,
    sfx_dbload,
    sfx_plasma,
    sfx_bfg,
    sfx_sawup,
    sfx_sawidl,
    sfx_sawful,
    sfx_sawhit,
    sfx_rlaunc,
    sfx_rxplod,
    sfx_firsht,
    sfx_firxpl,
    sfx_pstart,
    sfx_pstop,
    sfx_doropn,
    sfx_dorcls,
    sfx_stnmov,
    sfx_swtchn,
    sfx_swtchx,
    sfx_plpain,
    sfx_dmpain,
    sfx_popain,
    sfx_vipain,
    sfx_mnpain,
    sfx_pepain,
    sfx_slop,
    sfx_itemup,
    sfx_wpnup,
    sfx_oof,
    sfx_telept,
    sfx_posit1,
    sfx_posit2,
    sfx_posit3,
    sfx_bgsit1,
    sfx_bgsit2,
    sfx_sgtsit,
    sfx_cacsit,
    sfx_brssit,
    sfx_cybsit,
    sfx_spisit,
    sfx_bspsit,
    sfx_kntsit,
    sfx_vilsit,
    sfx_mansit,
    sfx_pesit,
    sfx_sklatk,
    sfx_sgtatk,
    sfx_skepch,
    sfx_vilatk,
    sfx_claw,
    sfx_skeswg,
    sfx_pldeth,
    sfx_pdiehi,
    sfx_podth1,
    sfx_podth2,
    sfx_podth3,
    sfx_bgdth1,
    sfx_bgdth2,
    sfx_sgtdth,
    sfx_cacdth,
    sfx_skldth,
    sfx_brsdth,
    sfx_cybdth,
    sfx_spidth,
    sfx_bspdth,
    sfx_vildth,
    sfx_kntdth,
    sfx_pedth,
    sfx_skedth,
    sfx_posact,
    sfx_bgact,
    sfx_dmact,
    sfx_bspact,
    sfx_bspwlk,
    sfx_vilact,
    sfx_noway,
    sfx_barexp,
    sfx_punch,
    sfx_hoof,
    sfx_metal,
    sfx_chgun,
    sfx_tink,
    sfx_bdopn,
    sfx_bdcls,
    sfx_itmbk,
    sfx_flame,
    sfx_flamst,
    sfx_getpow,
    sfx_bospit,
    sfx_boscub,
    sfx_bossit,
    sfx_bospn,
    sfx_bosdth,
    sfx_manatk,
    sfx_mandth,
    sfx_sssit,
    sfx_ssdth,
    sfx_keenpn,
    sfx_keendt,
    sfx_skeact,
    sfx_skesit,
    sfx_skeatk,
    sfx_radio,
    NUMSFX
} sfxenum_t;
# 64 "g_game.c" 2



# 1 "r_sky.h" 1
# 37 "r_sky.h"
extern int skytexture;
extern int skytexturemid;


void R_InitSkyMap (void);
# 68 "g_game.c" 2



# 1 "g_game.h" 1
# 34 "g_game.h"
void G_DeathMatchSpawnPlayer (int playernum);

void G_InitNew (skill_t skill, int episode, int map);




void G_DeferedInitNew (skill_t skill, int episode, int map);

void G_DeferedPlayDemo (char* demo);



void G_LoadGame (char* name);

void G_DoLoadGame (void);


void G_SaveGame (int slot, char* description);


void G_RecordDemo (char* name);

void G_BeginRecording (void);

void G_PlayDemo (char* name);
void G_TimeDemo (char* name);
boolean G_CheckDemoStatus (void);

void G_ExitLevel (void);
void G_SecretExitLevel (void);

void G_WorldDone (void);

void G_Ticker (void);
boolean G_Responder (event_t* ev);

void G_ScreenShot (void);
# 72 "g_game.c" 2







boolean G_CheckDemoStatus (void);
void G_ReadDemoTiccmd (ticcmd_t* cmd);
void G_WriteDemoTiccmd (ticcmd_t* cmd);
void G_PlayerReborn (int player);
void G_InitNew (skill_t skill, int episode, int map);

void G_DoReborn (int playernum);

void G_DoLoadLevel (void);
void G_DoNewGame (void);
void G_DoLoadGame (void);
void G_DoPlayDemo (void);
void G_DoCompleted (void);
void G_DoVictory (void);
void G_DoWorldDone (void);
void G_DoSaveGame (void);


gameaction_t gameaction;
gamestate_t gamestate;
skill_t gameskill;
boolean respawnmonsters;
int gameepisode;
int gamemap;

boolean paused;
boolean sendpause;
boolean sendsave;
boolean usergame;

boolean timingdemo;
boolean nodrawers;
boolean noblit;
int starttime;

boolean viewactive;

boolean deathmatch;
boolean netgame;
boolean playeringame[4];
player_t players[4];

int consoleplayer;
int displayplayer;
int gametic;
int levelstarttic;
int totalkills, totalitems, totalsecret;

char demoname[32];
boolean demorecording;
boolean demoplayback;
boolean netdemo;
byte* demobuffer;
byte* demo_p;
byte* demoend;
boolean singledemo;

boolean precache = true;

wbstartstruct_t wminfo;

short consistancy[4][12];

byte* savebuffer;





int key_right;
int key_left;

int key_up;
int key_down;
int key_strafeleft;
int key_straferight;
int key_fire;
int key_use;
int key_strafe;
int key_speed;

int mousebfire;
int mousebstrafe;
int mousebforward;

int joybfire;
int joybstrafe;
int joybuse;
int joybspeed;







fixed_t forwardmove[2] = {0x19, 0x32};
fixed_t sidemove[2] = {0x18, 0x28};
fixed_t angleturn[3] = {640, 1280, 320};





boolean gamekeydown[256];
int turnheld;

boolean mousearray[4];
boolean* mousebuttons = &mousearray[1];


int mousex;
int mousey;

int dclicktime;
int dclickstate;
int dclicks;
int dclicktime2;
int dclickstate2;
int dclicks2;


int joyxmove;
int joyymove;
boolean joyarray[5];
boolean* joybuttons = &joyarray[1];

int savegameslot;
char savedescription[32];




mobj_t* bodyque[32];
int bodyqueslot;

void* statcopy;



int G_CmdChecksum (ticcmd_t* cmd)
{
    int i;
    int sum = 0;

    for (i=0 ; i< sizeof(*cmd)/4 - 1 ; i++)
 sum += ((int *)cmd)[i];

    return sum;
}
# 237 "g_game.c"
void G_BuildTiccmd (ticcmd_t* cmd)
{
    int i;
    boolean strafe;
    boolean bstrafe;
    int speed;
    int tspeed;
    int forward;
    int side;

    ticcmd_t* base;

    base = I_BaseTiccmd ();
    memcpy (cmd,base,sizeof(*cmd));

    cmd->consistancy =
 consistancy[consoleplayer][maketic%12];


    strafe = gamekeydown[key_strafe] || mousebuttons[mousebstrafe]
 || joybuttons[joybstrafe];
    speed = gamekeydown[key_speed] || joybuttons[joybspeed];

    forward = side = 0;



    if (joyxmove < 0
 || joyxmove > 0
 || gamekeydown[key_right]
 || gamekeydown[key_left])
 turnheld += ticdup;
    else
 turnheld = 0;

    if (turnheld < 6)
 tspeed = 2;
    else
 tspeed = speed;


    if (strafe)
    {
 if (gamekeydown[key_right])
 {

     side += sidemove[speed];
 }
 if (gamekeydown[key_left])
 {

     side -= sidemove[speed];
 }
 if (joyxmove > 0)
     side += sidemove[speed];
 if (joyxmove < 0)
     side -= sidemove[speed];

    }
    else
    {
 if (gamekeydown[key_right])
     cmd->angleturn -= angleturn[tspeed];
 if (gamekeydown[key_left])
     cmd->angleturn += angleturn[tspeed];
 if (joyxmove > 0)
     cmd->angleturn -= angleturn[tspeed];
 if (joyxmove < 0)
     cmd->angleturn += angleturn[tspeed];
    }

    if (gamekeydown[key_up])
    {

 forward += forwardmove[speed];
    }
    if (gamekeydown[key_down])
    {

 forward -= forwardmove[speed];
    }
    if (joyymove < 0)
 forward += forwardmove[speed];
    if (joyymove > 0)
 forward -= forwardmove[speed];
    if (gamekeydown[key_straferight])
 side += sidemove[speed];
    if (gamekeydown[key_strafeleft])
 side -= sidemove[speed];


    cmd->chatchar = HU_dequeueChatChar();

    if (gamekeydown[key_fire] || mousebuttons[mousebfire]
 || joybuttons[joybfire])
 cmd->buttons |= BT_ATTACK;

    if (gamekeydown[key_use] || joybuttons[joybuse] )
    {
 cmd->buttons |= BT_USE;

 dclicks = 0;
    }


    for (i=0 ; i<NUMWEAPONS-1 ; i++)
 if (gamekeydown['1'+i])
 {
     cmd->buttons |= BT_CHANGE;
     cmd->buttons |= i<<BT_WEAPONSHIFT;
     break;
 }


    if (mousebuttons[mousebforward])
 forward += forwardmove[speed];


    if (mousebuttons[mousebforward] != dclickstate && dclicktime > 1 )
    {
 dclickstate = mousebuttons[mousebforward];
 if (dclickstate)
     dclicks++;
 if (dclicks == 2)
 {
     cmd->buttons |= BT_USE;
     dclicks = 0;
 }
 else
     dclicktime = 0;
    }
    else
    {
 dclicktime += ticdup;
 if (dclicktime > 20)
 {
     dclicks = 0;
     dclickstate = 0;
 }
    }


    bstrafe =
 mousebuttons[mousebstrafe]
 || joybuttons[joybstrafe];
    if (bstrafe != dclickstate2 && dclicktime2 > 1 )
    {
 dclickstate2 = bstrafe;
 if (dclickstate2)
     dclicks2++;
 if (dclicks2 == 2)
 {
     cmd->buttons |= BT_USE;
     dclicks2 = 0;
 }
 else
     dclicktime2 = 0;
    }
    else
    {
 dclicktime2 += ticdup;
 if (dclicktime2 > 20)
 {
     dclicks2 = 0;
     dclickstate2 = 0;
 }
    }

    forward += mousey;
    if (strafe)
 side += mousex*2;
    else
 cmd->angleturn -= mousex*0x8;

    mousex = mousey = 0;

    if (forward > (forwardmove[1]))
 forward = (forwardmove[1]);
    else if (forward < -(forwardmove[1]))
 forward = -(forwardmove[1]);
    if (side > (forwardmove[1]))
 side = (forwardmove[1]);
    else if (side < -(forwardmove[1]))
 side = -(forwardmove[1]);

    cmd->forwardmove += forward;
    cmd->sidemove += side;


    if (sendpause)
    {
 sendpause = false;
 cmd->buttons = BT_SPECIAL | BTS_PAUSE;
    }

    if (sendsave)
    {
 sendsave = false;
 cmd->buttons = BT_SPECIAL | BTS_SAVEGAME | (savegameslot<<BTS_SAVESHIFT);
    }
}





extern gamestate_t wipegamestate;

void G_DoLoadLevel (void)
{
    int i;






    skyflatnum = R_FlatNumForName ( "F_SKY1" );



    if ( (gamemode == commercial)
  || ( gamemode == pack_tnt )
  || ( gamemode == pack_plut ) )
    {
 skytexture = R_TextureNumForName ("SKY3");
 if (gamemap < 12)
     skytexture = R_TextureNumForName ("SKY1");
 else
     if (gamemap < 21)
  skytexture = R_TextureNumForName ("SKY2");
    }

    levelstarttic = gametic;

    if (wipegamestate == GS_LEVEL)
 wipegamestate = -1;

    gamestate = GS_LEVEL;

    for (i=0 ; i<4 ; i++)
    {
 if (playeringame[i] && players[i].playerstate == PST_DEAD)
     players[i].playerstate = PST_REBORN;
 memset (players[i].frags,0,sizeof(players[i].frags));
    }

    P_SetupLevel (gameepisode, gamemap, 0, gameskill);
    displayplayer = consoleplayer;
    starttime = I_GetTime ();
    gameaction = ga_nothing;
    Z_CheckHeap ();


    memset (gamekeydown, 0, sizeof(gamekeydown));
    joyxmove = joyymove = 0;
    mousex = mousey = 0;
    sendpause = sendsave = paused = false;
    memset (mousebuttons, 0, sizeof(mousebuttons));
    memset (joybuttons, 0, sizeof(joybuttons));
}






boolean G_Responder (event_t* ev)
{

    if (gamestate == GS_LEVEL && ev->type == ev_keydown
 && ev->data1 == (0x80+0x58) && (singledemo || !deathmatch) )
    {

 do
 {
     displayplayer++;
     if (displayplayer == 4)
  displayplayer = 0;
 } while (!playeringame[displayplayer] && displayplayer != consoleplayer);
 return true;
    }


    if (gameaction == ga_nothing && !singledemo &&
 (demoplayback || gamestate == GS_DEMOSCREEN)
 )
    {
 if (ev->type == ev_keydown ||
     (ev->type == ev_mouse && ev->data1) ||
     (ev->type == ev_joystick && ev->data1) )
 {
     M_StartControlPanel ();
     return true;
 }
 return false;
    }

    if (gamestate == GS_LEVEL)
    {







 if (HU_Responder (ev))
     return true;
 if (ST_Responder (ev))
     return true;
 if (AM_Responder (ev))
     return true;
    }

    if (gamestate == GS_FINALE)
    {
 if (F_Responder (ev))
     return true;
    }

    switch (ev->type)
    {
      case ev_keydown:
 if (ev->data1 == 0xff)
 {
     sendpause = true;
     return true;
 }
 if (ev->data1 <256)
     gamekeydown[ev->data1] = true;
 return true;

      case ev_keyup:
 if (ev->data1 <256)
     gamekeydown[ev->data1] = false;
 return false;

      case ev_mouse:
 mousebuttons[0] = ev->data1 & 1;
 mousebuttons[1] = ev->data1 & 2;
 mousebuttons[2] = ev->data1 & 4;
 mousex = ev->data2*(mouseSensitivity+5)/10;
 mousey = ev->data3*(mouseSensitivity+5)/10;
 return true;

      case ev_joystick:
 joybuttons[0] = ev->data1 & 1;
 joybuttons[1] = ev->data1 & 2;
 joybuttons[2] = ev->data1 & 4;
 joybuttons[3] = ev->data1 & 8;
 joyxmove = ev->data2;
 joyymove = ev->data3;
 return true;

      default:
 break;
    }

    return false;
}







void G_Ticker (void)
{
    int i;
    int buf;
    ticcmd_t* cmd;


    for (i=0 ; i<4 ; i++)
 if (playeringame[i] && players[i].playerstate == PST_REBORN)
     G_DoReborn (i);


    while (gameaction != ga_nothing)
    {
 switch (gameaction)
 {
   case ga_loadlevel:
     G_DoLoadLevel ();
     break;
   case ga_newgame:
     G_DoNewGame ();
     break;
   case ga_loadgame:
     G_DoLoadGame ();
     break;
   case ga_savegame:
     G_DoSaveGame ();
     break;
   case ga_playdemo:
     G_DoPlayDemo ();
     break;
   case ga_completed:
     G_DoCompleted ();
     break;
   case ga_victory:
     F_StartFinale ();
     break;
   case ga_worlddone:
     G_DoWorldDone ();
     break;
   case ga_screenshot:
     M_ScreenShot ();
     gameaction = ga_nothing;
     break;
   case ga_nothing:
     break;
 }
    }



    buf = (gametic/ticdup)%12;

    for (i=0 ; i<4 ; i++)
    {
 if (playeringame[i])
 {
     cmd = &players[i].cmd;

     memcpy (cmd, &netcmds[i][buf], sizeof(ticcmd_t));

     if (demoplayback)
  G_ReadDemoTiccmd (cmd);
     if (demorecording)
  G_WriteDemoTiccmd (cmd);


     if (cmd->forwardmove > 0x32
  && !(gametic&31) && ((gametic>>5)&3) == i )
     {
  static char turbomessage[80];
  extern char *player_names[4];
  sprintf (turbomessage, "%s is turbo!",player_names[i]);
  players[consoleplayer].message = turbomessage;
     }

     if (netgame && !netdemo && !(gametic%ticdup) )
     {
  if (gametic > 12
      && consistancy[i][buf] != cmd->consistancy)
  {
      I_Error ("consistency failure (%i should be %i)",
        cmd->consistancy, consistancy[i][buf]);
  }
  if (players[i].mo)
      consistancy[i][buf] = players[i].mo->x;
  else
      consistancy[i][buf] = rndindex;
     }
 }
    }


    for (i=0 ; i<4 ; i++)
    {
 if (playeringame[i])
 {
     if (players[i].cmd.buttons & BT_SPECIAL)
     {
  switch (players[i].cmd.buttons & BT_SPECIALMASK)
  {
    case BTS_PAUSE:
      paused ^= 1;
      if (paused)
   S_PauseSound ();
      else
   S_ResumeSound ();
      break;

    case BTS_SAVEGAME:
      if (!savedescription[0])
   strcpy (savedescription, "NET GAME");
      savegameslot =
   (players[i].cmd.buttons & BTS_SAVEMASK)>>BTS_SAVESHIFT;
      gameaction = ga_savegame;
      break;
  }
     }
 }
    }


    switch (gamestate)
    {
      case GS_LEVEL:
 P_Ticker ();
 ST_Ticker ();
 AM_Ticker ();
 HU_Ticker ();
 break;

      case GS_INTERMISSION:
 WI_Ticker ();
 break;

      case GS_FINALE:
 F_Ticker ();
 break;

      case GS_DEMOSCREEN:
 D_PageTicker ();
 break;
    }
}
# 761 "g_game.c"
void G_InitPlayer (int player)
{
    player_t* p;


    p = &players[player];


    G_PlayerReborn (player);

}







void G_PlayerFinishLevel (int player)
{
    player_t* p;

    p = &players[player];

    memset (p->powers, 0, sizeof (p->powers));
    memset (p->cards, 0, sizeof (p->cards));
    p->mo->flags &= ~MF_SHADOW;
    p->extralight = 0;
    p->fixedcolormap = 0;
    p->damagecount = 0;
    p->bonuscount = 0;
}







void G_PlayerReborn (int player)
{
    player_t* p;
    int i;
    int frags[4];
    int killcount;
    int itemcount;
    int secretcount;

    memcpy (frags,players[player].frags,sizeof(frags));
    killcount = players[player].killcount;
    itemcount = players[player].itemcount;
    secretcount = players[player].secretcount;

    p = &players[player];
    memset (p, 0, sizeof(*p));

    memcpy (players[player].frags, frags, sizeof(players[player].frags));
    players[player].killcount = killcount;
    players[player].itemcount = itemcount;
    players[player].secretcount = secretcount;

    p->usedown = p->attackdown = true;
    p->playerstate = PST_LIVE;
    p->health = 100;
    p->readyweapon = p->pendingweapon = wp_pistol;
    p->weaponowned[wp_fist] = true;
    p->weaponowned[wp_pistol] = true;
    p->ammo[am_clip] = 50;

    for (i=0 ; i<NUMAMMO ; i++)
 p->maxammo[i] = maxammo[i];

}







void P_SpawnPlayer (mapthing_t* mthing);

boolean
G_CheckSpot
( int playernum,
  mapthing_t* mthing )
{
    fixed_t x;
    fixed_t y;
    subsector_t* ss;
    unsigned an;
    mobj_t* mo;
    int i;

    if (!players[playernum].mo)
    {

 for (i=0 ; i<playernum ; i++)
     if (players[i].mo->x == mthing->x << 16
  && players[i].mo->y == mthing->y << 16)
  return false;
 return true;
    }

    x = mthing->x << 16;
    y = mthing->y << 16;

    if (!P_CheckPosition (players[playernum].mo, x, y) )
 return false;


    if (bodyqueslot >= 32)
 P_RemoveMobj (bodyque[bodyqueslot%32]);
    bodyque[bodyqueslot%32] = players[playernum].mo;
    bodyqueslot++;


    ss = R_PointInSubsector (x,y);
    an = ( 0x20000000 * (mthing->angle/45) ) >> 19;

    mo = P_SpawnMobj (x+20*finecosine[an], y+20*finesine[an]
        , ss->sector->floorheight
        , MT_TFOG);

    if (players[consoleplayer].viewz != 1)
 S_StartSound (mo, sfx_telept);

    return true;
}







void G_DeathMatchSpawnPlayer (int playernum)
{
    int i,j;
    int selections;

    selections = deathmatch_p - deathmatchstarts;
    if (selections < 4)
 I_Error ("Only %i deathmatch spots, 4 required", selections);

    for (j=0 ; j<20 ; j++)
    {
 i = P_Random() % selections;
 if (G_CheckSpot (playernum, &deathmatchstarts[i]) )
 {
     deathmatchstarts[i].type = playernum+1;
     P_SpawnPlayer (&deathmatchstarts[i]);
     return;
 }
    }


    P_SpawnPlayer (&playerstarts[playernum]);
}




void G_DoReborn (int playernum)
{
    int i;

    if (!netgame)
    {

 gameaction = ga_loadlevel;
    }
    else
    {



 players[playernum].mo->player = 
# 938 "g_game.c" 3 4
                                ((void *)0)
# 938 "g_game.c"
                                    ;


 if (deathmatch)
 {
     G_DeathMatchSpawnPlayer (playernum);
     return;
 }

 if (G_CheckSpot (playernum, &playerstarts[playernum]) )
 {
     P_SpawnPlayer (&playerstarts[playernum]);
     return;
 }


 for (i=0 ; i<4 ; i++)
 {
     if (G_CheckSpot (playernum, &playerstarts[i]) )
     {
  playerstarts[i].type = playernum+1;
  P_SpawnPlayer (&playerstarts[i]);
  playerstarts[i].type = i+1;
  return;
     }

 }
 P_SpawnPlayer (&playerstarts[playernum]);
    }
}


void G_ScreenShot (void)
{
    gameaction = ga_screenshot;
}




int pars[4][10] =
{
    {0},
    {0,30,75,120,90,165,180,180,30,165},
    {0,90,90,90,120,90,360,240,30,170},
    {0,90,45,90,150,90,90,165,30,135}
};


int cpars[32] =
{
    30,90,120,120,90,150,120,120,270,90,
    210,150,150,150,210,150,420,150,210,150,
    240,150,180,150,150,300,330,420,300,180,
    120,30
};





boolean secretexit;
extern char* pagename;

void G_ExitLevel (void)
{
    secretexit = false;
    gameaction = ga_completed;
}


void G_SecretExitLevel (void)
{

    if ( (gamemode == commercial)
      && (W_CheckNumForName("map31")<0))
 secretexit = false;
    else
 secretexit = true;
    gameaction = ga_completed;
}

void G_DoCompleted (void)
{
    int i;

    gameaction = ga_nothing;

    for (i=0 ; i<4 ; i++)
 if (playeringame[i])
     G_PlayerFinishLevel (i);

    if (automapactive)
 AM_Stop ();

    if ( gamemode != commercial)
 switch(gamemap)
 {
   case 8:
     gameaction = ga_victory;
     return;
   case 9:
     for (i=0 ; i<4 ; i++)
  players[i].didsecret = true;
     break;
 }


    if ( (gamemap == 8)
  && (gamemode != commercial) )
    {

 gameaction = ga_victory;
 return;
    }

    if ( (gamemap == 9)
  && (gamemode != commercial) )
    {

 for (i=0 ; i<4 ; i++)
     players[i].didsecret = true;
    }



    wminfo.didsecret = players[consoleplayer].didsecret;
    wminfo.epsd = gameepisode -1;
    wminfo.last = gamemap -1;


    if ( gamemode == commercial)
    {
 if (secretexit)
     switch(gamemap)
     {
       case 15: wminfo.next = 30; break;
       case 31: wminfo.next = 31; break;
     }
 else
     switch(gamemap)
     {
       case 31:
       case 32: wminfo.next = 15; break;
       default: wminfo.next = gamemap;
     }
    }
    else
    {
 if (secretexit)
     wminfo.next = 8;
 else if (gamemap == 9)
 {

     switch (gameepisode)
     {
       case 1:
  wminfo.next = 3;
  break;
       case 2:
  wminfo.next = 5;
  break;
       case 3:
  wminfo.next = 6;
  break;
       case 4:
  wminfo.next = 2;
  break;
     }
 }
 else
     wminfo.next = gamemap;
    }

    wminfo.maxkills = totalkills;
    wminfo.maxitems = totalitems;
    wminfo.maxsecret = totalsecret;
    wminfo.maxfrags = 0;
    if ( gamemode == commercial )
 wminfo.partime = 35*cpars[gamemap-1];
    else
 wminfo.partime = 35*pars[gameepisode][gamemap];
    wminfo.pnum = consoleplayer;

    for (i=0 ; i<4 ; i++)
    {
 wminfo.plyr[i].in = playeringame[i];
 wminfo.plyr[i].skills = players[i].killcount;
 wminfo.plyr[i].sitems = players[i].itemcount;
 wminfo.plyr[i].ssecret = players[i].secretcount;
 wminfo.plyr[i].stime = leveltime;
 memcpy (wminfo.plyr[i].frags, players[i].frags
  , sizeof(wminfo.plyr[i].frags));
    }

    gamestate = GS_INTERMISSION;
    viewactive = false;
    automapactive = false;

    if (statcopy)
 memcpy (statcopy, &wminfo, sizeof(wminfo));

    WI_Start (&wminfo);
}





void G_WorldDone (void)
{
    gameaction = ga_worlddone;

    if (secretexit)
 players[consoleplayer].didsecret = true;

    if ( gamemode == commercial )
    {
 switch (gamemap)
 {
   case 15:
   case 31:
     if (!secretexit)
  break;
   case 6:
   case 11:
   case 20:
   case 30:
     F_StartFinale ();
     break;
 }
    }
}

void G_DoWorldDone (void)
{
    gamestate = GS_LEVEL;
    gamemap = wminfo.next+1;
    G_DoLoadLevel ();
    gameaction = ga_nothing;
    viewactive = true;
}







extern boolean setsizeneeded;
void R_ExecuteSetViewSize (void);

char savename[256];

void G_LoadGame (char* name)
{
    strcpy (savename, name);
    gameaction = ga_loadgame;
}




void G_DoLoadGame (void)
{
    int length;
    int i;
    int a,b,c;
    char vcheck[16];

    gameaction = ga_nothing;

    length = M_ReadFile (savename, &savebuffer);
    save_p = savebuffer + 24;


    memset (vcheck,0,sizeof(vcheck));
    sprintf (vcheck,"version %i",VERSION);
    if (strcmp (save_p, vcheck))
 return;
    save_p += 16;

    gameskill = *save_p++;
    gameepisode = *save_p++;
    gamemap = *save_p++;
    for (i=0 ; i<4 ; i++)
 playeringame[i] = *save_p++;


    G_InitNew (gameskill, gameepisode, gamemap);


    a = *save_p++;
    b = *save_p++;
    c = *save_p++;
    leveltime = (a<<16) + (b<<8) + c;


    P_UnArchivePlayers ();
    P_UnArchiveWorld ();
    P_UnArchiveThinkers ();
    P_UnArchiveSpecials ();

    if (*save_p != 0x1d)
 I_Error ("Bad savegame");


    Z_Free (savebuffer);

    if (setsizeneeded)
 R_ExecuteSetViewSize ();


    R_FillBackScreen ();
}







void
G_SaveGame
( int slot,
  char* description )
{
    savegameslot = slot;
    strcpy (savedescription, description);
    sendsave = true;
}

void G_DoSaveGame (void)
{
    char name[100];
    char name2[16];
    char* description;
    int length;
    int i;

    if (M_CheckParm("-cdrom"))
 sprintf(name,"c:\\doomdata\\""doomsav""%d.dsg",savegameslot);
    else
 sprintf (name,"doomsav""%d.dsg",savegameslot);
    description = savedescription;

    save_p = savebuffer = screens[1]+0x4000;

    memcpy (save_p, description, 24);
    save_p += 24;
    memset (name2,0,sizeof(name2));
    sprintf (name2,"version %i",VERSION);
    memcpy (save_p, name2, 16);
    save_p += 16;

    *save_p++ = gameskill;
    *save_p++ = gameepisode;
    *save_p++ = gamemap;
    for (i=0 ; i<4 ; i++)
 *save_p++ = playeringame[i];
    *save_p++ = leveltime>>16;
    *save_p++ = leveltime>>8;
    *save_p++ = leveltime;

    P_ArchivePlayers ();
    P_ArchiveWorld ();
    P_ArchiveThinkers ();
    P_ArchiveSpecials ();

    *save_p++ = 0x1d;

    length = save_p - savebuffer;
    if (length > 0x2c000)
 I_Error ("Savegame buffer overrun");
    M_WriteFile (name, savebuffer, length);
    gameaction = ga_nothing;
    savedescription[0] = 0;

    players[consoleplayer].message = "game saved.";


    R_FillBackScreen ();
}







skill_t d_skill;
int d_episode;
int d_map;

void
G_DeferedInitNew
( skill_t skill,
  int episode,
  int map)
{
    d_skill = skill;
    d_episode = episode;
    d_map = map;
    gameaction = ga_newgame;
}


void G_DoNewGame (void)
{
    demoplayback = false;
    netdemo = false;
    netgame = false;
    deathmatch = false;
    playeringame[1] = playeringame[2] = playeringame[3] = 0;
    respawnparm = false;
    fastparm = false;
    nomonsters = false;
    consoleplayer = 0;
    G_InitNew (d_skill, d_episode, d_map);
    gameaction = ga_nothing;
}


extern int skytexture;


void
G_InitNew
( skill_t skill,
  int episode,
  int map )
{
    int i;

    if (paused)
    {
 paused = false;
 S_ResumeSound ();
    }


    if (skill > sk_nightmare)
 skill = sk_nightmare;





    if (episode < 1)
      episode = 1;

    if ( gamemode == retail )
    {
      if (episode > 4)
 episode = 4;
    }
    else if ( gamemode == shareware )
    {
      if (episode > 1)
    episode = 1;
    }
    else
    {
      if (episode > 3)
 episode = 3;
    }



    if (map < 1)
 map = 1;

    if ( (map > 9)
  && ( gamemode != commercial) )
      map = 9;

    M_ClearRandom ();

    if (skill == sk_nightmare || respawnparm )
 respawnmonsters = true;
    else
 respawnmonsters = false;

    if (fastparm || (skill == sk_nightmare && gameskill != sk_nightmare) )
    {
 for (i=S_SARG_RUN1 ; i<=S_SARG_PAIN2 ; i++)
     states[i].tics >>= 1;
 mobjinfo[MT_BRUISERSHOT].speed = 20*(1<<16);
 mobjinfo[MT_HEADSHOT].speed = 20*(1<<16);
 mobjinfo[MT_TROOPSHOT].speed = 20*(1<<16);
    }
    else if (skill != sk_nightmare && gameskill == sk_nightmare)
    {
 for (i=S_SARG_RUN1 ; i<=S_SARG_PAIN2 ; i++)
     states[i].tics <<= 1;
 mobjinfo[MT_BRUISERSHOT].speed = 15*(1<<16);
 mobjinfo[MT_HEADSHOT].speed = 10*(1<<16);
 mobjinfo[MT_TROOPSHOT].speed = 10*(1<<16);
    }



    for (i=0 ; i<4 ; i++)
 players[i].playerstate = PST_REBORN;

    usergame = true;
    paused = false;
    demoplayback = false;
    automapactive = false;
    viewactive = true;
    gameepisode = episode;
    gamemap = map;
    gameskill = skill;

    viewactive = true;


    if ( gamemode == commercial)
    {
 skytexture = R_TextureNumForName ("SKY3");
 if (gamemap < 12)
     skytexture = R_TextureNumForName ("SKY1");
 else
     if (gamemap < 21)
  skytexture = R_TextureNumForName ("SKY2");
    }
    else
 switch (episode)
 {
   case 1:
     skytexture = R_TextureNumForName ("SKY1");
     break;
   case 2:
     skytexture = R_TextureNumForName ("SKY2");
     break;
   case 3:
     skytexture = R_TextureNumForName ("SKY3");
     break;
   case 4:
     skytexture = R_TextureNumForName ("SKY4");
     break;
 }

    G_DoLoadLevel ();
}
# 1491 "g_game.c"
void G_ReadDemoTiccmd (ticcmd_t* cmd)
{
    if (*demo_p == 0x80)
    {

 G_CheckDemoStatus ();
 return;
    }
    cmd->forwardmove = ((signed char)*demo_p++);
    cmd->sidemove = ((signed char)*demo_p++);
    cmd->angleturn = ((unsigned char)*demo_p++)<<8;
    cmd->buttons = (unsigned char)*demo_p++;
}


void G_WriteDemoTiccmd (ticcmd_t* cmd)
{
    if (gamekeydown['q'])
 G_CheckDemoStatus ();
    *demo_p++ = cmd->forwardmove;
    *demo_p++ = cmd->sidemove;
    *demo_p++ = (cmd->angleturn+128)>>8;
    *demo_p++ = cmd->buttons;
    demo_p -= 4;
    if (demo_p > demoend - 16)
    {

 G_CheckDemoStatus ();
 return;
    }

    G_ReadDemoTiccmd (cmd);
}






void G_RecordDemo (char* name)
{
    int i;
    int maxsize;

    usergame = false;
    strcpy (demoname, name);
    strcat (demoname, ".lmp");
    maxsize = 0x20000;
    i = M_CheckParm ("-maxdemo");
    if (i && i<myargc-1)
 maxsize = atoi(myargv[i+1])*1024;
    demobuffer = Z_Malloc (maxsize,1,
# 1542 "g_game.c" 3 4
                                            ((void *)0)
# 1542 "g_game.c"
                                                );
    demoend = demobuffer + maxsize;

    demorecording = true;
}


void G_BeginRecording (void)
{
    int i;

    demo_p = demobuffer;

    *demo_p++ = VERSION;
    *demo_p++ = gameskill;
    *demo_p++ = gameepisode;
    *demo_p++ = gamemap;
    *demo_p++ = deathmatch;
    *demo_p++ = respawnparm;
    *demo_p++ = fastparm;
    *demo_p++ = nomonsters;
    *demo_p++ = consoleplayer;

    for (i=0 ; i<4 ; i++)
 *demo_p++ = playeringame[i];
}






char* defdemoname;

void G_DeferedPlayDemo (char* name)
{
    defdemoname = name;
    gameaction = ga_playdemo;
}

void G_DoPlayDemo (void)
{
    skill_t skill;
    int i, episode, map;

    gameaction = ga_nothing;
    demobuffer = demo_p = W_CacheLumpName (defdemoname, 1);
    if ( *demo_p++ != VERSION)
    {
      fprintf( 
# 1591 "g_game.c" 3 4
              stderr
# 1591 "g_game.c"
                    , "Demo is from a different game version!\n");
      gameaction = ga_nothing;
      return;
    }

    skill = *demo_p++;
    episode = *demo_p++;
    map = *demo_p++;
    deathmatch = *demo_p++;
    respawnparm = *demo_p++;
    fastparm = *demo_p++;
    nomonsters = *demo_p++;
    consoleplayer = *demo_p++;

    for (i=0 ; i<4 ; i++)
 playeringame[i] = *demo_p++;
    if (playeringame[1])
    {
 netgame = true;
 netdemo = true;
    }


    precache = false;
    G_InitNew (skill, episode, map);
    precache = true;

    usergame = false;
    demoplayback = true;
}




void G_TimeDemo (char* name)
{
    nodrawers = M_CheckParm ("-nodraw");
    noblit = M_CheckParm ("-noblit");
    timingdemo = true;
    singletics = true;

    defdemoname = name;
    gameaction = ga_playdemo;
}
# 1647 "g_game.c"
boolean G_CheckDemoStatus (void)
{
    int endtime;

    if (timingdemo)
    {
 endtime = I_GetTime ();
 I_Error ("timed %i gametics in %i realtics",gametic
   , endtime-starttime);
    }

    if (demoplayback)
    {
 if (singledemo)
     I_Quit ();

 { if (( (memblock_t *)( (byte *)(demobuffer) - sizeof(memblock_t)))->id!=0x1d4a11) I_Error("Z_CT at ""g_game.c"":%i",1663); Z_ChangeTag2(demobuffer,101); };;
 demoplayback = false;
 netdemo = false;
 netgame = false;
 deathmatch = false;
 playeringame[1] = playeringame[2] = playeringame[3] = 0;
 respawnparm = false;
 fastparm = false;
 nomonsters = false;
 consoleplayer = 0;
 D_AdvanceDemo ();
 return true;
    }

    if (demorecording)
    {
 *demo_p++ = 0x80;
 M_WriteFile (demoname, demobuffer, demo_p - demobuffer);
 Z_Free (demobuffer);
 demorecording = false;
 I_Error ("Demo %s recorded",demoname);
    }

    return false;
}
