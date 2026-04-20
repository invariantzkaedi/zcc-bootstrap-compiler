/*
** $Id: lapi.c $
** Lua API
** See Copyright Notice in lua.h
*/




/*
** $Id: lprefix.h $
** Definitions for Lua code that must come before any other header file
** See Copyright Notice in lua.h
*/





/*
** Allows POSIX/XSI stuff
*/








/*
** Allows manipulation of large files in gcc and some other compilers
*/








/*
** Windows stuff
*/




















































typedef struct { unsigned int gp_offset; unsigned int fp_offset; void *overflow_arg_area; void *reg_save_area; } __va_list_struct[1];










typedef int int32_t;
typedef unsigned int uint32_t;
typedef long int64_t;
typedef unsigned long uint64_t;
typedef char int8_t;
typedef unsigned char uint8_t;
typedef short int16_t;
typedef unsigned short uint16_t;
typedef unsigned long size_t;
typedef long ssize_t;
typedef struct _IO_FILE FILE;
extern FILE *stdin, *stdout, *stderr;


































































/*
** $Id: lua.h $
** Lua - A Scripting Language
** Lua.org, PUC-Rio, Brazil (http://www.lua.org)
** See Copyright Notice at the end of this file
*/





































































































































/*
** $Id: luaconf.h $
** Configuration file for Lua
** See Copyright Notice in lua.h
*/







































































/*
** ===================================================================
** General Configuration File for Lua
**
** Some definitions here can be changed externally, through the compiler
** (e.g., with '-D' options): They are commented out or protected
** by '#if !defined' guards. However, several other definitions
** should be changed directly here, either because they affect the
** Lua ABI (by making the changes here, you ensure that all software
** connected to Lua, such as C libraries, will be compiled with the same
** configuration); or because they are seldom changed.
**
** Search for "@@" to find all configurable definitions.
** ===================================================================
*/


/*
** {====================================================================
** System Configuration: macros to adapt (if needed) Lua to some
** particular platform, for instance restricting it to C89.
** =====================================================================
*/

/*
@@ LUA_USE_C89 controls the use of non-ISO-C89 features.
** Define it if you want Lua to avoid the use of a few C99 features
** or Windows-specific features on Windows.
*/
/* #define LUA_USE_C89 */


/*
** By default, Lua on Windows use (some) specific Windows features
*/





























/*
@@ LUAI_IS32INT is true iff 'int' has (at least) 32 bits.
*/


/* }================================================================== */



/*
** {==================================================================
** Configuration for Number types. These options should not be
** set externally, because any other code connected to Lua must
** use the same configuration.
** ===================================================================
*/

/*
@@ LUA_INT_TYPE defines the type for Lua integers.
@@ LUA_FLOAT_TYPE defines the type for Lua floats.
** Lua should work fine with any mix of these options supported
** by your C compiler. The usual configurations are 64-bit integers
** and 'double' (the default), 32-bit integers and 'float' (for
** restricted platforms), and 'long'/'double' (for C compilers not
** compliant with C99, which may not have support for 'long long').
*/

/* predefined options for LUA_INT_TYPE */




/* predefined options for LUA_FLOAT_TYPE */





/* Default configuration ('long long' and 'double', for 64-bit Lua) */




/*
@@ LUA_32BITS enables Lua with 32-bit integers and 32-bit floats.
*/



/*
@@ LUA_C89_NUMBERS ensures that Lua uses the largest types available for
** C89 ('long' and 'double'); Windows always has '__int64', so it does
** not need to use this case.
*/






















/* use defaults */







/* }================================================================== */



/*
** {==================================================================
** Configuration for Paths.
** ===================================================================
*/

/*
** LUA_PATH_SEP is the character that separates templates in a path.
** LUA_PATH_MARK is the string that marks the substitution points in a
** template.
** LUA_EXEC_DIR in a Windows path is replaced by the executable's
** directory.
*/





/*
@@ LUA_PATH_DEFAULT is the default path that Lua uses to look for
** Lua libraries.
@@ LUA_CPATH_DEFAULT is the default path that Lua uses to look for
** C libraries.
** CHANGE them if your machine has a non-conventional directory
** hierarchy or if you want to install your libraries in
** non-conventional directories.
*/

































/*
@@ LUA_DIRSEP is the directory separator (for submodules).
** CHANGE it if your machine does not use "/" as the directory separator
** and is not Windows. (On Windows Lua automatically uses "\".)
*/










/* }================================================================== */


/*
** {==================================================================
** Marks for exported symbols in the C code
** ===================================================================
*/

/*
@@ LUA_API is a mark for all core API functions.
@@ LUALIB_API is a mark for all auxiliary library functions.
@@ LUAMOD_API is a mark for all standard library opening functions.
** CHANGE them if you need to define those functions in some special way.
** For instance, if you want to create one Windows DLL with the core and
** the libraries, you may want to use the following definition (define
** LUA_BUILD_AS_DLL to get it).
*/















/*
** More often than not the libs go together with the core.
*/




/*
@@ LUAI_FUNC is a mark for all extern functions that are not to be
** exported to outside modules.
@@ LUAI_DDEF and LUAI_DDEC are marks for all extern (const) variables,
** none of which to be exported to outside modules (LUAI_DDEF for
** definitions and LUAI_DDEC for declarations).
** CHANGE them if you need to mark them in some special way. Elf/gcc
** (versions 3.2 and later) mark them as "hidden" to optimize access
** when Lua is compiled as a shared library. Not all elf targets support
** this attribute. Unfortunately, gcc does not offer a way to check
** whether the target offers that support, and those without support
** give a warning about it. To avoid these warnings, change to the
** default definition.
*/









/* }================================================================== */


/*
** {==================================================================
** Compatibility with previous versions
** ===================================================================
*/

/*
@@ LUA_COMPAT_5_3 controls other macros for compatibility with Lua 5.3.
** You can define it to get all options, or change specific options
** to fit your specific needs.
*/























/* }================================================================== */



/*
** {==================================================================
** Configuration for Numbers (low-level part).
** Change these definitions if no predefined LUA_FLOAT_* / LUA_INT_*
** satisfy your needs.
** ===================================================================
*/

/*
@@ LUAI_UACNUMBER is the result of a 'default argument promotion'
@@ over a floating number.
@@ l_floatatt(x) corrects float attribute 'x' to the proper float type
** by prefixing it with one of FLT/DBL/LDBL.
@@ LUA_NUMBER_FRMLEN is the length modifier for writing floats.
@@ LUA_NUMBER_FMT is the format for writing floats.
@@ lua_number2str converts a float to a string.
@@ l_mathop allows the addition of an 'l' or 'f' to all math operations.
@@ l_floor takes the floor of a float.
@@ lua_str2number converts a decimal numeral to a number.
*/


/* The following definitions are good for most cases here */





/*
@@ lua_numbertointeger converts a float number with an integral value
** to an integer, or returns 0 if float is not within the range of
** a lua_Integer.  (The range comparisons are tricky because of
** rounding. The tests here assume a two-complement representation,
** where MININTEGER always has an exact representation as a float;
** MAXINTEGER may not have one, and therefore its conversion to float
** may have an ill-defined value.)
*/



/* now the variable definitions */























































/*
@@ LUA_UNSIGNED is the unsigned version of LUA_INTEGER.
@@ LUAI_UACINT is the result of a 'default argument promotion'
@@ over a LUA_INTEGER.
@@ LUA_INTEGER_FRMLEN is the length modifier for reading/writing integers.
@@ LUA_INTEGER_FMT is the format for writing integers.
@@ LUA_MAXINTEGER is the maximum value for a LUA_INTEGER.
@@ LUA_MININTEGER is the minimum value for a LUA_INTEGER.
@@ LUA_MAXUNSIGNED is the maximum value for a LUA_UNSIGNED.
@@ lua_integer2str converts an integer to a string.
*/


/* The following definitions are good for most cases here */







/*
** use LUAI_UACINT here to avoid problems with promotions (which
** can turn a comparison between unsigneds into a signed comparison)
*/



/* now the variable definitions */























/* use presence of macro LLONG_MAX as proxy for C99 compliance */


































/* }================================================================== */


/*
** {==================================================================
** Dependencies with C99 and other C details
** ===================================================================
*/

/*
@@ l_sprintf is equivalent to 'snprintf' or 'sprintf' in C89.
** (All uses in Lua have only one format item.)
*/







/*
@@ lua_strx2number converts a hexadecimal numeral to a number.
** In C99, 'strtod' does that conversion. Otherwise, you can
** leave 'lua_strx2number' undefined and Lua will provide its own
** implementation.
*/





/*
@@ lua_pointer2str converts a pointer to a readable string in a
** non-specified way.
*/



/*
@@ lua_number2strx converts a float to a hexadecimal numeral.
** In C99, 'sprintf' (with format specifiers '%a'/'%A') does that.
** Otherwise, you can leave 'lua_number2strx' undefined and Lua will
** provide its own implementation.
*/





/*
** 'strtof' and 'opf' variants for math functions are not valid in
** C89. Otherwise, the macro 'HUGE_VALF' is a good proxy for testing the
** availability of these variants. ('math.h' is already included in
** all files that use these macros.)
*/








/*
@@ LUA_KCONTEXT is the type of the context ('ctx') for continuation
** functions.  It must be a numerical type; Lua will use 'intptr_t' if
** available, otherwise it will use 'ptrdiff_t' (the nearest thing to
** 'intptr_t' in C89)
*/











/*
@@ lua_getlocaledecpoint gets the locale "radix character" (decimal point).
** Change that if you do not want to use C locales. (Code using this
** macro must include the header 'locale.h'.)
*/





/*
** macros to improve jump prediction, used mostly for error handling
** and debug facilities. (Some macros in the Lua API use these macros.
** Define LUA_NOBUILTIN if you do not want '__builtin_expect' in your
** code.)
*/














/* shorter names for Lua's own use */






/* }================================================================== */


/*
** {==================================================================
** Language Variations
** =====================================================================
*/

/*
@@ LUA_NOCVTN2S/LUA_NOCVTS2N control how Lua performs some
** coercions. Define LUA_NOCVTN2S to turn off automatic coercion from
** numbers to strings. Define LUA_NOCVTS2N to turn off automatic
** coercion from strings to numbers.
*/
/* #define LUA_NOCVTN2S */
/* #define LUA_NOCVTS2N */


/*
@@ LUA_USE_APICHECK turns on several consistency checks on the C API.
** Define it as a help when debugging C code.
*/





/* }================================================================== */


/*
** {==================================================================
** Macros that affect the API and must be stable (that is, must be the
** same when you compile Lua and when you compile code that links to
** Lua).
** =====================================================================
*/

/*
@@ LUAI_MAXSTACK limits the size of the Lua stack.
** CHANGE it if you need a different limit. This limit is arbitrary;
** its only purpose is to stop Lua from consuming unlimited stack
** space (and to reserve some numbers for pseudo-indices).
** (It must fit into max(size_t)/32 and max(int)/2.)
*/







/*
@@ LUA_EXTRASPACE defines the size of a raw memory area associated with
** a Lua state with very fast access.
** CHANGE it if you need a different size.
*/



/*
@@ LUA_IDSIZE gives the maximum size for the description of the source
** of a function in debug information.
** CHANGE it if you want a different size.
*/



/*
@@ LUAL_BUFFERSIZE is the initial buffer size used by the lauxlib
** buffer system.
*/



/*
@@ LUAI_MAXALIGN defines fields that, when used in a union, ensure
** maximum alignment for the other items in that union.
*/


/* }================================================================== */





/* =================================================================== */

/*
** Local configuration. You can use this space to add your redefinitions
** without modifying the main part of the file.
*/























/* mark for precompiled code ('<esc>Lua') */


/* option for multiple returns in 'lua_pcall' and 'lua_call' */



/*
** Pseudo-indices
** (-LUAI_MAXSTACK is the minimum valid index; we keep some free empty
** space after that to help overflow detection)
*/




/* thread status */








typedef struct lua_State lua_State;


/*
** basic types
*/
















/* minimum Lua stack available to a C function */



/* predefined values in the registry */





/* type of numbers in Lua */
typedef double lua_Number;


/* type for integer functions */
typedef LUA_INTEGER lua_Integer;

/* unsigned integer type */
typedef unsigned LUA_INTEGER lua_Unsigned;

/* type for continuation-function contexts */
typedef ptrdiff_t lua_KContext;


/*
** Type for C functions registered with Lua
*/
typedef int (*lua_CFunction) (lua_State *L);

/*
** Type for continuation functions
*/
typedef int (*lua_KFunction) (lua_State *L, int status, lua_KContext ctx);


/*
** Type for functions that read/write blocks when loading/dumping Lua chunks
*/
typedef const char * (*lua_Reader) (lua_State *L, void *ud, size_t *sz);

typedef int (*lua_Writer) (lua_State *L, const void *p, size_t sz, void *ud);


/*
** Type for memory-allocation functions
*/
typedef void * (*lua_Alloc) (void *ud, void *ptr, size_t osize, size_t nsize);


/*
** Type for warning functions
*/
typedef void (*lua_WarnFunction) (void *ud, const char *msg, int tocont);


/*
** Type used by the debug API to collect debug information
*/
typedef struct lua_Debug lua_Debug;


/*
** Functions to be called by the debugger in specific events
*/
typedef void (*lua_Hook) (lua_State *L, lua_Debug *ar);


/*
** generic extra include file
*/





/*
** RCS ident string
*/
extern const char lua_ident[];


/*
** state manipulation
*/
extern lua_State *(lua_newstate) (lua_Alloc f, void *ud);
extern void       (lua_close) (lua_State *L);
extern lua_State *(lua_newthread) (lua_State *L);
extern int        (lua_closethread) (lua_State *L, lua_State *from);
extern int        (lua_resetthread) (lua_State *L);  /* Deprecated! */

extern lua_CFunction (lua_atpanic) (lua_State *L, lua_CFunction panicf);


extern lua_Number (lua_version) (lua_State *L);


/*
** basic stack manipulation
*/
extern int   (lua_absindex) (lua_State *L, int idx);
extern int   (lua_gettop) (lua_State *L);
extern void  (lua_settop) (lua_State *L, int idx);
extern void  (lua_pushvalue) (lua_State *L, int idx);
extern void  (lua_rotate) (lua_State *L, int idx, int n);
extern void  (lua_copy) (lua_State *L, int fromidx, int toidx);
extern int   (lua_checkstack) (lua_State *L, int n);

extern void  (lua_xmove) (lua_State *from, lua_State *to, int n);


/*
** access functions (stack -> C)
*/

extern int             (lua_isnumber) (lua_State *L, int idx);
extern int             (lua_isstring) (lua_State *L, int idx);
extern int             (lua_iscfunction) (lua_State *L, int idx);
extern int             (lua_isinteger) (lua_State *L, int idx);
extern int             (lua_isuserdata) (lua_State *L, int idx);
extern int             (lua_type) (lua_State *L, int idx);
extern const char     *(lua_typename) (lua_State *L, int tp);

extern lua_Number      (lua_tonumberx) (lua_State *L, int idx, int *isnum);
extern lua_Integer     (lua_tointegerx) (lua_State *L, int idx, int *isnum);
extern int             (lua_toboolean) (lua_State *L, int idx);
extern const char     *(lua_tolstring) (lua_State *L, int idx, size_t *len);
extern lua_Unsigned    (lua_rawlen) (lua_State *L, int idx);
extern lua_CFunction   (lua_tocfunction) (lua_State *L, int idx);
extern void	       *(lua_touserdata) (lua_State *L, int idx);
extern lua_State      *(lua_tothread) (lua_State *L, int idx);
extern const void     *(lua_topointer) (lua_State *L, int idx);


/*
** Comparison and arithmetic functions
*/
















extern void  (lua_arith) (lua_State *L, int op);





extern int   (lua_rawequal) (lua_State *L, int idx1, int idx2);
extern int   (lua_compare) (lua_State *L, int idx1, int idx2, int op);


/*
** push functions (C -> stack)
*/
extern void        (lua_pushnil) (lua_State *L);
extern void        (lua_pushnumber) (lua_State *L, lua_Number n);
extern void        (lua_pushinteger) (lua_State *L, lua_Integer n);
extern const char *(lua_pushlstring) (lua_State *L, const char *s, size_t len);
extern const char *(lua_pushstring) (lua_State *L, const char *s);
extern const char *(lua_pushvfstring) (lua_State *L, const char *fmt,
                                                      va_list argp);
extern const char *(lua_pushfstring) (lua_State *L, const char *fmt, ...);
extern void  (lua_pushcclosure) (lua_State *L, lua_CFunction fn, int n);
extern void  (lua_pushboolean) (lua_State *L, int b);
extern void  (lua_pushlightuserdata) (lua_State *L, void *p);
extern int   (lua_pushthread) (lua_State *L);


/*
** get functions (Lua -> stack)
*/
extern int (lua_getglobal) (lua_State *L, const char *name);
extern int (lua_gettable) (lua_State *L, int idx);
extern int (lua_getfield) (lua_State *L, int idx, const char *k);
extern int (lua_geti) (lua_State *L, int idx, lua_Integer n);
extern int (lua_rawget) (lua_State *L, int idx);
extern int (lua_rawgeti) (lua_State *L, int idx, lua_Integer n);
extern int (lua_rawgetp) (lua_State *L, int idx, const void *p);

extern void  (lua_createtable) (lua_State *L, int narr, int nrec);
extern void *(lua_newuserdatauv) (lua_State *L, size_t sz, int nuvalue);
extern int   (lua_getmetatable) (lua_State *L, int objindex);
extern int  (lua_getiuservalue) (lua_State *L, int idx, int n);


/*
** set functions (stack -> Lua)
*/
extern void  (lua_setglobal) (lua_State *L, const char *name);
extern void  (lua_settable) (lua_State *L, int idx);
extern void  (lua_setfield) (lua_State *L, int idx, const char *k);
extern void  (lua_seti) (lua_State *L, int idx, lua_Integer n);
extern void  (lua_rawset) (lua_State *L, int idx);
extern void  (lua_rawseti) (lua_State *L, int idx, lua_Integer n);
extern void  (lua_rawsetp) (lua_State *L, int idx, const void *p);
extern int   (lua_setmetatable) (lua_State *L, int objindex);
extern int   (lua_setiuservalue) (lua_State *L, int idx, int n);


/*
** 'load' and 'call' functions (load and run Lua code)
*/
extern void  (lua_callk) (lua_State *L, int nargs, int nresults,
                           lua_KContext ctx, lua_KFunction k);


extern int   (lua_pcallk) (lua_State *L, int nargs, int nresults, int errfunc,
                            lua_KContext ctx, lua_KFunction k);


extern int   (lua_load) (lua_State *L, lua_Reader reader, void *dt,
                          const char *chunkname, const char *mode);

extern int (lua_dump) (lua_State *L, lua_Writer writer, void *data, int strip);


/*
** coroutine functions
*/
extern int  (lua_yieldk)     (lua_State *L, int nresults, lua_KContext ctx,
                               lua_KFunction k);
extern int  (lua_resume)     (lua_State *L, lua_State *from, int narg,
                               int *nres);
extern int  (lua_status)     (lua_State *L);
extern int (lua_isyieldable) (lua_State *L);




/*
** Warning-related functions
*/
extern void (lua_setwarnf) (lua_State *L, lua_WarnFunction f, void *ud);
extern void (lua_warning)  (lua_State *L, const char *msg, int tocont);


/*
** garbage-collection function and options
*/













extern int (lua_gc) (lua_State *L, int what, ...);


/*
** miscellaneous functions
*/

extern int   (lua_error) (lua_State *L);

extern int   (lua_next) (lua_State *L, int idx);

extern void  (lua_concat) (lua_State *L, int n);
extern void  (lua_len)    (lua_State *L, int idx);

extern size_t   (lua_stringtonumber) (lua_State *L, const char *s);

extern lua_Alloc (lua_getallocf) (lua_State *L, void **ud);
extern void      (lua_setallocf) (lua_State *L, lua_Alloc f, void *ud);

extern void (lua_toclose) (lua_State *L, int idx);
extern void (lua_closeslot) (lua_State *L, int idx);


/*
** {==============================================================
** some useful macros
** ===============================================================
*/




































/* }============================================================== */


/*
** {==============================================================
** compatibility macros
** ===============================================================
*/














/* }============================================================== */

/*
** {======================================================================
** Debug API
** =======================================================================
*/


/*
** Event codes
*/







/*
** Event masks
*/






extern int (lua_getstack) (lua_State *L, int level, lua_Debug *ar);
extern int (lua_getinfo) (lua_State *L, const char *what, lua_Debug *ar);
extern const char *(lua_getlocal) (lua_State *L, const lua_Debug *ar, int n);
extern const char *(lua_setlocal) (lua_State *L, const lua_Debug *ar, int n);
extern const char *(lua_getupvalue) (lua_State *L, int funcindex, int n);
extern const char *(lua_setupvalue) (lua_State *L, int funcindex, int n);

extern void *(lua_upvalueid) (lua_State *L, int fidx, int n);
extern void  (lua_upvaluejoin) (lua_State *L, int fidx1, int n1,
                                               int fidx2, int n2);

extern void (lua_sethook) (lua_State *L, lua_Hook func, int mask, int count);
extern lua_Hook (lua_gethook) (lua_State *L);
extern int (lua_gethookmask) (lua_State *L);
extern int (lua_gethookcount) (lua_State *L);

extern int (lua_setcstacklimit) (lua_State *L, unsigned int limit);

struct lua_Debug {
  int event;
  const char *name;	/* (n) */
  const char *namewhat;	/* (n) 'global', 'local', 'field', 'method' */
  const char *what;	/* (S) 'Lua', 'C', 'main', 'tail' */
  const char *source;	/* (S) */
  size_t srclen;	/* (S) */
  int currentline;	/* (l) */
  int linedefined;	/* (S) */
  int lastlinedefined;	/* (S) */
  unsigned char nups;	/* (u) number of upvalues */
  unsigned char nparams;/* (u) number of parameters */
  char isvararg;        /* (u) */
  char istailcall;	/* (t) */
  unsigned short ftransfer;   /* (r) index of first value transferred */
  unsigned short ntransfer;   /* (r) number of transferred values */
  char short_src[60]; /* (S) */
  /* private part */
  struct CallInfo *i_ci;  /* active function */
};

/* }====================================================================== */


/******************************************************************************
* Copyright (C) 1994-2023 Lua.org, PUC-Rio.
*
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be
* included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
******************************************************************************/





/*
** $Id: lapi.h $
** Auxiliary functions from Lua API
** See Copyright Notice in lua.h
*/





/*
** $Id: llimits.h $
** Limits, basic types, and some other 'installation-dependent' definitions
** See Copyright Notice in lua.h
*/







































































/*
** $Id: lua.h $
** Lua - A Scripting Language
** Lua.org, PUC-Rio, Brazil (http://www.lua.org)
** See Copyright Notice at the end of this file
*/


















































































































































































































































































































































































































































/*
** 'lu_mem' and 'l_mem' are unsigned/signed integers big enough to count
** the total memory used by Lua (in bytes). Usually, 'size_t' and
** 'ptrdiff_t' should work, but we use 'long' for 16-bit machines.
*/







typedef unsigned long lu_mem;
typedef long l_mem;



/* chars used as small naturals (so that 'char' is reserved for characters) */
typedef unsigned char lu_byte;
typedef signed char ls_byte;


/* maximum value for size_t */


/* maximum size visible for Lua (must be representable in a lua_Integer) */











/*
** floor of the log2 of the maximum signed value for integral type 't'.
** (That is, maximum 'n' such that '2^n' fits in the given signed type.)
*/



/*
** test whether an unsigned value is a power of 2 (or zero)
*/



/* number of chars of a literal string without the ending \0 */



/*
** conversion of pointer to unsigned integer: this is for hashing only;
** there is no problem if the integer cannot hold the whole pointer
** value. (In strict ISO C this may cause undefined behavior, but no
** actual machine seems to bother.)
*/















/* types of 'usual argument conversions' for lua_Number and lua_Integer */
typedef double l_uacNumber;
typedef LUA_INTEGER l_uacInt;


/*
** Internal assertions for in-house debugging
*/
















/*
** assertion for checking API calls
*/







/* macro to avoid warnings about unused variables */





/* type casts (a macro highlights casts in the code) */














/* cast a signed lua_Integer to lua_Unsigned */




/*
** cast a lua_Unsigned to a signed lua_Integer; this cast is
** not strict ISO C, but two-complement architectures should
** work fine.
*/





/*
** non-return type
*/













/*
** Inline functions
*/











/*
** type for virtual-machine instructions;
** must be an unsigned with (at least) 4 bytes (see details in lopcodes.h)
*/



typedef unsigned long l_uint32;


typedef l_uint32 Instruction;



/*
** Maximum length for short strings, that is, strings that are
** internalized. (Cannot be smaller than reserved words or tags for
** metamethods, as these strings must be internalized;
** #("function") = 8, #("__newindex") = 10.)
*/





/*
** Initial size for the string table (must be power of 2).
** The Lua core alone registers ~50 strings (reserved words +
** metaevent keys + a few others). Libraries would typically add
** a few dozens more.
*/





/*
** Size of cache for strings in the API. 'N' is the number of
** sets (better be a prime) and "M" is the size of each set (M == 1
** makes a direct cache.)
*/






/* minimum size for string buffer */





/*
** Maximum depth for nested C calls, syntactical nested non-terminals,
** and other features implemented through recursion in C. (Value must
** fit in a 16-bit unsigned integer. It must also be compatible with
** the size of the C stack.)
*/





/*
** macros that are executed whenever program enters the Lua core
** ('lua_lock') and leaves the core ('lua_unlock')
*/





/*
** macro executed during Lua functions at points where the
** function can yield.
*/





/*
** these macros allow user-specific actions when a thread is
** created/deleted/resumed/yielded.
*/


























/*
** The luai_num* macros define the primitive operations over numbers.
*/

/* floor division (defined as 'floor(a/b)') */




/* float division */




/*
** modulo: defined as 'a - floor(a/b)*b'; the direct computation
** using this definition has several problems with rounding errors,
** so it is better to use 'fmod'. 'fmod' gives the result of
** 'a - trunc(a/b)*b', and therefore must be corrected when
** 'trunc(a/b) ~= floor(a/b)'. That happens when the division has a
** non-integer negative result: non-integer result is equivalent to
** a non-zero remainder 'm'; negative result is equivalent to 'a' and
** 'b' with different signs, or 'm' and 'b' with different signs
** (as the result 'm' of 'fmod' has the same sign of 'a').
*/




/* exponentiation */




/* the others are quite standard operations */

















/*
** macro to control inclusion of some hard tests on stack reallocation
*/















/*
** $Id: lstate.h $
** Global State
** See Copyright Notice in lua.h
*/




/*
** $Id: lua.h $
** Lua - A Scripting Language
** Lua.org, PUC-Rio, Brazil (http://www.lua.org)
** See Copyright Notice at the end of this file
*/


















































































































































































































































































































































































































































/* Some header files included here need this definition */
typedef struct CallInfo CallInfo;


/*
** $Id: lobject.h $
** Type definitions for Lua objects
** See Copyright Notice in lua.h
*/







































































/*
** $Id: llimits.h $
** Limits, basic types, and some other 'installation-dependent' definitions
** See Copyright Notice in lua.h
*/










































































































































































































































































































/*
** $Id: lua.h $
** Lua - A Scripting Language
** Lua.org, PUC-Rio, Brazil (http://www.lua.org)
** See Copyright Notice at the end of this file
*/


















































































































































































































































































































































































































































/*
** Extra types for collectable non-values
*/






/*
** number of all possible types (including LUA_TNONE but excluding DEADKEY)
*/



/*
** tags for Tagged Values have the following use of bits:
** bits 0-3: actual tag (a LUA_T* constant)
** bits 4-5: variant bits
** bit 6: whether value is collectable
*/

/* add variant bits to a type */




/*
** Union of all Lua values
*/
typedef union Value {
  struct GCObject *gc;    /* collectable objects */
  void *p;         /* light userdata */
  lua_CFunction f; /* light C functions */
  lua_Integer i;   /* integer numbers */
  lua_Number n;    /* float numbers */
  /* not used, but may avoid warnings for uninitialized value */
  lu_byte ub;
} Value;


/*
** Tagged Values. This is the basic representation of values in Lua:
** an actual value plus a tag with its type.
*/



typedef struct TValue {
  Value value_; lu_byte tt_;
} TValue;






/* raw type tag of a TValue */


/* tag with no variants (bits 0-3) */


/* type tag of a TValue (bits 0-3 for tags + variant bits 4-5) */



/* type of a TValue */



/* Macros to test type */




/* Macros for internal tests */

/* collectable object has the same tag as the original value */


/*
** Any value being manipulated by the program either is non
** collectable, or the collectable object has the right tag
** and it is not dead. The option 'L == NULL' allows other
** macros using this one to be used where L is not available.
*/



/* Macros to set values */

/* set a value's tag */



/* main macro to copy values (from 'obj2' to 'obj1') */


/*
** Different types of assignments, according to source and destination.
** (They are mostly equal now, but may be different in the future.)
*/

/* from stack to stack */

/* to stack (not from same stack) */

/* from table to same table */

/* to new object */

/* to table */



/*
** Entries in a Lua stack. Field 'tbclist' forms a list of all
** to-be-closed variables active in this stack. Dummy entries are
** used when the distance between two tbc variables does not fit
** in an unsigned short. They are represented by delta==0, and
** their real delta is always the maximum value that fits in
** that field.
*/
typedef union StackValue {
  TValue val;
  struct {
    Value value_; lu_byte tt_;
    unsigned short delta;
  } tbclist;
} StackValue;


/* index to stack elements */
typedef StackValue *StkId;


/*
** When reallocating the stack, change all pointers to the stack into
** proper offsets.
*/
typedef union {
  StkId p;  /* actual pointer */
  ptrdiff_t offset;  /* used while the stack is being reallocated */
} StkIdRel;


/* convert a 'StackValue' to a 'TValue' */




/*
** {==================================================================
** Nil
** ===================================================================
*/

/* Standard nil */


/* Empty slot (which might be different from a slot containing nil) */


/* Value returned for a key not found in a table (absent key) */



/* macro to test for (any kind of) nil */



/* macro to test for a standard nil */









/*
** macro to detect non-standard nils (used only in assertions)
*/



/*
** By default, entries with any kind of nil are considered empty.
** (In any definition, values associated with absent keys must also
** be accepted as empty.)
*/



/* macro defining a value corresponding to an absent key */



/* mark an entry as empty */




/* }================================================================== */


/*
** {==================================================================
** Booleans
** ===================================================================
*/
















/* }================================================================== */


/*
** {==================================================================
** Threads
** ===================================================================
*/











/* }================================================================== */


/*
** {==================================================================
** Collectable Objects
** ===================================================================
*/

/*
** Common Header for all collectable objects (in macro form, to be
** included in other objects)
*/



/* Common type for all collectable objects */
typedef struct GCObject {
  struct GCObject *next; lu_byte tt; lu_byte marked;
} GCObject;


/* Bit mark for collectable types */




/* mark a tag as collectable */








/* }================================================================== */


/*
** {==================================================================
** Numbers
** ===================================================================
*/

/* Variant tags for numbers */






















/* }================================================================== */


/*
** {==================================================================
** Strings
** ===================================================================
*/

/* Variant tags for strings */













/* set a string to the stack */


/* set a string to a new object */



/*
** Header for a string value.
*/
typedef struct TString {
  struct GCObject *next; lu_byte tt; lu_byte marked;
  lu_byte extra;  /* reserved words for short strings; "has hash" for longs */
  lu_byte shrlen;  /* length for short strings */
  unsigned int hash;
  union {
    size_t lnglen;  /* length for long strings */
    struct TString *hnext;  /* linked list for hash table */
  } u;
  char contents[1];
} TString;



/*
** Get the actual string (array of bytes) from a 'TString'.
*/



/* get the actual string (array of bytes) from a Lua value */


/* get string length from 'TString *s' */


/* get string length from 'TValue *o' */


/* }================================================================== */


/*
** {==================================================================
** Userdata
** ===================================================================
*/


/*
** Light userdata should be a variant of userdata, but for compatibility
** reasons they are also different types.
*/

















/* Ensures that addresses after this type are always fully aligned. */
typedef union UValue {
  TValue uv;
  lua_Number n; double u; void *s; lua_Integer i; long l;  /* ensures maximum alignment for udata bytes */
} UValue;


/*
** Header for userdata with user values;
** memory area follows the end of this structure.
*/
typedef struct Udata {
  struct GCObject *next; lu_byte tt; lu_byte marked;
  unsigned short nuvalue;  /* number of user values */
  size_t len;  /* number of bytes */
  struct Table *metatable;
  GCObject *gclist;
  UValue uv[1];  /* user values */
} Udata;


/*
** Header for userdata with no user values. These userdata do not need
** to be gray during GC, and therefore do not need a 'gclist' field.
** To simplify, the code always use 'Udata' for both kinds of userdata,
** making sure it never accesses 'gclist' on userdata with no user values.
** This structure here is used only to compute the correct size for
** this representation. (The 'bindata' field in its end ensures correct
** alignment for binary data following this header.)
*/
typedef struct Udata0 {
  struct GCObject *next; lu_byte tt; lu_byte marked;
  unsigned short nuvalue;  /* number of user values */
  size_t len;  /* number of bytes */
  struct Table *metatable;
  union {lua_Number n; double u; void *s; lua_Integer i; long l;} bindata;
} Udata0;


/* compute the offset of the memory area of a userdata */


/* get the address of the memory block inside 'Udata' */


/* compute the size of a userdata */


/* }================================================================== */


/*
** {==================================================================
** Prototypes
** ===================================================================
*/




/*
** Description of an upvalue for function prototypes
*/
typedef struct Upvaldesc {
  TString *name;  /* upvalue name (for debug information) */
  lu_byte instack;  /* whether it is in stack (register) */
  lu_byte idx;  /* index of upvalue (in stack or in outer function's list) */
  lu_byte kind;  /* kind of corresponding variable */
} Upvaldesc;


/*
** Description of a local variable for function prototypes
** (used for debug information)
*/
typedef struct LocVar {
  TString *varname;
  int startpc;  /* first point where variable is active */
  int endpc;    /* first point where variable is dead */
} LocVar;


/*
** Associates the absolute line source for a given instruction ('pc').
** The array 'lineinfo' gives, for each instruction, the difference in
** lines from the previous instruction. When that difference does not
** fit into a byte, Lua saves the absolute line for that instruction.
** (Lua also saves the absolute line periodically, to speed up the
** computation of a line number: we can use binary search in the
** absolute-line array, but we must traverse the 'lineinfo' array
** linearly to compute a line.)
*/
typedef struct AbsLineInfo {
  int pc;
  int line;
} AbsLineInfo;

/*
** Function Prototypes
*/
typedef struct Proto {
  struct GCObject *next; lu_byte tt; lu_byte marked;
  lu_byte numparams;  /* number of fixed (named) parameters */
  lu_byte is_vararg;
  lu_byte maxstacksize;  /* number of registers needed by this function */
  int sizeupvalues;  /* size of 'upvalues' */
  int sizek;  /* size of 'k' */
  int sizecode;
  int sizelineinfo;
  int sizep;  /* size of 'p' */
  int sizelocvars;
  int sizeabslineinfo;  /* size of 'abslineinfo' */
  int linedefined;  /* debug information  */
  int lastlinedefined;  /* debug information  */
  TValue *k;  /* constants used by the function */
  Instruction *code;  /* opcodes */
  struct Proto **p;  /* functions defined inside the function */
  Upvaldesc *upvalues;  /* upvalue information */
  ls_byte *lineinfo;  /* information about source lines (debug information) */
  AbsLineInfo *abslineinfo;  /* idem */
  LocVar *locvars;  /* information about local variables (debug information) */
  TString  *source;  /* used for debug information */
  GCObject *gclist;
} Proto;

/* }================================================================== */


/*
** {==================================================================
** Functions
** ===================================================================
*/




/* Variant tags for functions */





























/*
** Upvalues for Lua closures
*/
typedef struct UpVal {
  struct GCObject *next; lu_byte tt; lu_byte marked;
  union {
    TValue *p;  /* points to stack or to its own value */
    ptrdiff_t offset;  /* used while the stack is being reallocated */
  } v;
  union {
    struct {  /* (when open) */
      struct UpVal *next;  /* linked list */
      struct UpVal **previous;
    } open;
    TValue value;  /* the value (when closed) */
  } u;
} UpVal;





typedef struct CClosure {
  	struct GCObject *next; lu_byte tt; lu_byte marked; lu_byte nupvalues; GCObject *gclist;
  lua_CFunction f;
  TValue upvalue[1];  /* list of upvalues */
} CClosure;


typedef struct LClosure {
  	struct GCObject *next; lu_byte tt; lu_byte marked; lu_byte nupvalues; GCObject *gclist;
  struct Proto *p;
  UpVal *upvals[1];  /* list of upvalues */
} LClosure;


typedef union Closure {
  CClosure c;
  LClosure l;
} Closure;




/* }================================================================== */


/*
** {==================================================================
** Tables
** ===================================================================
*/












/*
** Nodes for Hash tables: A pack of two TValue's (key-value pairs)
** plus a 'next' field to link colliding entries. The distribution
** of the key's fields ('key_tt' and 'key_val') not forming a proper
** 'TValue' allows for a smaller size for 'Node' both in 4-byte
** and 8-byte alignments.
*/
typedef union Node {
  struct NodeKey {
    Value value_; lu_byte tt_;  /* fields for value */
    lu_byte key_tt;  /* key type */
    int next;  /* for chaining */
    Value key_val;  /* key value */
  } u;
  TValue i_val;  /* direct access to node's value as a proper 'TValue' */
} Node;


/* copy a value into a key */



/* copy a value from a key */



/*
** About 'alimit': if 'isrealasize(t)' is true, then 'alimit' is the
** real size of 'array'. Otherwise, the real size of 'array' is the
** smallest power of two not smaller than 'alimit' (or zero iff 'alimit'
** is zero); 'alimit' is then used as a hint for #t.
*/







typedef struct Table {
  struct GCObject *next; lu_byte tt; lu_byte marked;
  lu_byte flags;  /* 1<<p means tagmethod(p) is not present */
  lu_byte lsizenode;  /* log2 of size of 'node' array */
  unsigned int alimit;  /* "limit" of 'array' array */
  TValue *array;  /* array part */
  Node *node;
  Node *lastfree;  /* any free position is before this position */
  struct Table *metatable;
  GCObject *gclist;
} Table;


/*
** Macros to manipulate keys inserted in nodes
*/

















/*
** Dead keys in tables have the tag DEADKEY but keep their original
** gcvalue. This distinguishes them from regular keys but allows them to
** be found when searched in a special way. ('next' needs that to find
** keys removed from a table during a traversal.)
*/



/* }================================================================== */



/*
** 'module' operation for hashing (size is always a power of 2)
*/







/* size of buffer for 'luaO_utf8esc' function */


extern int luaO_utf8esc (char *buff, unsigned long x);
extern int luaO_ceillog2 (unsigned int x);
extern int luaO_rawarith (lua_State *L, int op, const TValue *p1,
                             const TValue *p2, TValue *res);
extern void luaO_arith (lua_State *L, int op, const TValue *p1,
                           const TValue *p2, StkId res);
extern size_t luaO_str2num (const char *s, TValue *o);
extern int luaO_hexavalue (int c);
extern void luaO_tostring (lua_State *L, TValue *obj);
extern const char *luaO_pushvfstring (lua_State *L, const char *fmt,
                                                       va_list argp);
extern const char *luaO_pushfstring (lua_State *L, const char *fmt, ...);
extern void luaO_chunkid (char *out, const char *source, size_t srclen);





/*
** $Id: ltm.h $
** Tag methods
** See Copyright Notice in lua.h
*/





/*
** $Id: lobject.h $
** Type definitions for Lua objects
** See Copyright Notice in lua.h
*/
























































































































































































































































































































































































































































































































































































































































/*
** $Id: lstate.h $
** Global State
** See Copyright Notice in lua.h
*/






















































































































































































































































































/*
* WARNING: if you change the order of this enumeration,
* grep "ORDER TM" and "ORDER OP"
*/
typedef enum {
  TM_INDEX,
  TM_NEWINDEX,
  TM_GC,
  TM_MODE,
  TM_LEN,
  TM_EQ,  /* last tag method with fast access */
  TM_ADD,
  TM_SUB,
  TM_MUL,
  TM_MOD,
  TM_POW,
  TM_DIV,
  TM_IDIV,
  TM_BAND,
  TM_BOR,
  TM_BXOR,
  TM_SHL,
  TM_SHR,
  TM_UNM,
  TM_BNOT,
  TM_LT,
  TM_LE,
  TM_CONCAT,
  TM_CALL,
  TM_CLOSE,
  TM_N		/* number of elements in the enum */
} TMS;


/*
** Mask with 1 in all fast-access methods. A 1 in any of these bits
** in the flag of a (meta)table means the metatable does not have the
** corresponding metamethod field. (Bit 7 of the flag is used for
** 'isrealasize'.)
*/



/*
** Test whether there is no tagmethod.
** (Because tagmethods use raw accesses, the result may be an "empty" nil.)
*/









extern const char *const luaT_typenames_[((9+1)   + 2)];


extern const char *luaT_objtypename (lua_State *L, const TValue *o);

extern const TValue *luaT_gettm (Table *events, TMS event, TString *ename);
extern const TValue *luaT_gettmbyobj (lua_State *L, const TValue *o,
                                                       TMS event);
extern void luaT_init (lua_State *L);

extern void luaT_callTM (lua_State *L, const TValue *f, const TValue *p1,
                            const TValue *p2, const TValue *p3);
extern void luaT_callTMres (lua_State *L, const TValue *f,
                            const TValue *p1, const TValue *p2, StkId p3);
extern void luaT_trybinTM (lua_State *L, const TValue *p1, const TValue *p2,
                              StkId res, TMS event);
extern void luaT_tryconcatTM (lua_State *L);
extern void luaT_trybinassocTM (lua_State *L, const TValue *p1,
       const TValue *p2, int inv, StkId res, TMS event);
extern void luaT_trybiniTM (lua_State *L, const TValue *p1, lua_Integer i2,
                               int inv, StkId res, TMS event);
extern int luaT_callorderTM (lua_State *L, const TValue *p1,
                                const TValue *p2, TMS event);
extern int luaT_callorderiTM (lua_State *L, const TValue *p1, int v2,
                                 int inv, int isfloat, TMS event);

extern void luaT_adjustvarargs (lua_State *L, int nfixparams,
                                   CallInfo *ci, const Proto *p);
extern void luaT_getvarargs (lua_State *L, CallInfo *ci,
                                              StkId where, int wanted);




/*
** $Id: lzio.h $
** Buffered streams
** See Copyright Notice in lua.h
*/





/*
** $Id: lua.h $
** Lua - A Scripting Language
** Lua.org, PUC-Rio, Brazil (http://www.lua.org)
** See Copyright Notice at the end of this file
*/

















































































































































































































































































































































































































































/*
** $Id: lmem.h $
** Interface to Memory Manager
** See Copyright Notice in lua.h
*/





































































/*
** $Id: llimits.h $
** Limits, basic types, and some other 'installation-dependent' definitions
** See Copyright Notice in lua.h
*/










































































































































































































































































































/*
** $Id: lua.h $
** Lua - A Scripting Language
** Lua.org, PUC-Rio, Brazil (http://www.lua.org)
** See Copyright Notice at the end of this file
*/





















































































































































































































































































































































































































































/*
** This macro tests whether it is safe to multiply 'n' by the size of
** type 't' without overflows. Because 'e' is always constant, it avoids
** the runtime division MAX_SIZET/(e).
** (The macro is somewhat complex to avoid warnings:  The 'sizeof'
** comparison avoids a runtime comparison when overflow cannot occur.
** The compiler should be able to optimize the real test by itself, but
** when it does it, it may give a warning about "comparison is always
** false due to limited range of data type"; the +1 tricks the compiler,
** avoiding this warning but also this optimization.)
*/





/*
** Computes the minimum between 'n' and 'MAX_SIZET/sizeof(t)', so that
** the result is not larger than 'n' and cannot overflow a 'size_t'
** when multiplied by the size of type 't'. (Assumes that 'n' is an
** 'int' or 'unsigned int' and that 'int' is not larger than 'size_t'.)
*/



/*
** Arrays of chars do not need any test
*/


















extern void  luaM_toobig (lua_State *L);

/* not to be called directly */
extern void *luaM_realloc_ (lua_State *L, void *block, size_t oldsize,
                                                          size_t size);
extern void *luaM_saferealloc_ (lua_State *L, void *block, size_t oldsize,
                                                              size_t size);
extern void luaM_free_ (lua_State *L, void *block, size_t osize);
extern void *luaM_growaux_ (lua_State *L, void *block, int nelems,
                               int *size, int size_elem, int limit,
                               const char *what);
extern void *luaM_shrinkvector_ (lua_State *L, void *block, int *nelem,
                                    int final_n, int size_elem);
extern void *luaM_malloc_ (lua_State *L, size_t size, int tag);








typedef struct Zio ZIO;




typedef struct Mbuffer {
  char *buffer;
  size_t n;
  size_t buffsize;
} Mbuffer;
















extern void luaZ_init (lua_State *L, ZIO *z, lua_Reader reader,
                                        void *data);
extern size_t luaZ_read (ZIO* z, void *b, size_t n);	/* read next n bytes */



/* --------- Private Part ------------------ */

struct Zio {
  size_t n;			/* bytes still unread */
  const char *p;		/* current position in buffer */
  lua_Reader reader;		/* reader function */
  void *data;			/* additional data */
  lua_State *L;			/* Lua state (for reader) */
};


extern int luaZ_fill (ZIO *z);





/*
** Some notes about garbage-collected objects: All objects in Lua must
** be kept somehow accessible until being freed, so all objects always
** belong to one (and only one) of these lists, using field 'next' of
** the 'CommonHeader' for the link:
**
** 'allgc': all objects not marked for finalization;
** 'finobj': all objects marked for finalization;
** 'tobefnz': all objects ready to be finalized;
** 'fixedgc': all objects that are not to be collected (currently
** only small strings, such as reserved words).
**
** For the generational collector, some of these lists have marks for
** generations. Each mark points to the first element in the list for
** that particular generation; that generation goes until the next mark.
**
** 'allgc' -> 'survival': new objects;
** 'survival' -> 'old': objects that survived one collection;
** 'old1' -> 'reallyold': objects that became old in last collection;
** 'reallyold' -> NULL: objects old for more than one cycle.
**
** 'finobj' -> 'finobjsur': new objects marked for finalization;
** 'finobjsur' -> 'finobjold1': survived   """";
** 'finobjold1' -> 'finobjrold': just old  """";
** 'finobjrold' -> NULL: really old       """".
**
** All lists can contain elements older than their main ages, due
** to 'luaC_checkfinalizer' and 'udata2finalize', which move
** objects between the normal lists and the "marked for finalization"
** lists. Moreover, barriers can age young objects in young lists as
** OLD0, which then become OLD1. However, a list never contains
** elements younger than their main ages.
**
** The generational collector also uses a pointer 'firstold1', which
** points to the first OLD1 object in the list. It is used to optimize
** 'markold'. (Potentially OLD1 objects can be anywhere between 'allgc'
** and 'reallyold', but often the list has no OLD1 objects or they are
** after 'old1'.) Note the difference between it and 'old1':
** 'firstold1': no OLD1 objects before this point; there can be all
**   ages after it.
** 'old1': no objects younger than OLD1 after this point.
*/

/*
** Moreover, there is another set of lists that control gray objects.
** These lists are linked by fields 'gclist'. (All objects that
** can become gray have such a field. The field is not the same
** in all objects, but it always has this name.)  Any gray object
** must belong to one of these lists, and all objects in these lists
** must be gray (with two exceptions explained below):
**
** 'gray': regular gray objects, still waiting to be visited.
** 'grayagain': objects that must be revisited at the atomic phase.
**   That includes
**   - black objects got in a write barrier;
**   - all kinds of weak tables during propagation phase;
**   - all threads.
** 'weak': tables with weak values to be cleared;
** 'ephemeron': ephemeron tables with white->white entries;
** 'allweak': tables with weak keys and/or weak values to be cleared.
**
** The exceptions to that "gray rule" are:
** - TOUCHED2 objects in generational mode stay in a gray list (because
** they must be visited again at the end of the cycle), but they are
** marked black because assignments to them must activate barriers (to
** move them back to TOUCHED1).
** - Open upvales are kept gray to avoid barriers, but they stay out
** of gray lists. (They don't even have a 'gclist' field.)
*/



/*
** About 'nCcalls':  This count has two parts: the lower 16 bits counts
** the number of recursive invocations in the C stack; the higher
** 16 bits counts the number of non-yieldable calls in the stack.
** (They are together so that we can change and save both with one
** instruction.)
*/


/* true if this thread does not have non-yieldable calls in the stack */


/* real number of C calls */



/* Increment the number of non-yieldable calls */


/* Decrement the number of non-yieldable calls */


/* Non-yieldable call increment */





struct lua_longjmp;  /* defined in ldo.c */


/*
** Atomic type (relative to signals) to better ensure that 'lua_sethook'
** is thread safe
*/






/*
** Extra stack space to handle TM calls and some other extras. This
** space is not included in 'stack_last'. It is used only to avoid stack
** checks, either because the element will be promptly popped or because
** there will be a stack check soon after the push. Function frames
** never use this extra space, so it does not need to be kept clean.
*/








/* kinds of Garbage Collection */




typedef struct stringtable {
  TString **hash;
  int nuse;  /* number of elements */
  int size;
} stringtable;


/*
** Information about a call.
** About union 'u':
** - field 'l' is used only for Lua functions;
** - field 'c' is used only for C functions.
** About union 'u2':
** - field 'funcidx' is used only by C functions while doing a
** protected call;
** - field 'nyield' is used only while a function is "doing" an
** yield (from the yield until the next resume);
** - field 'nres' is used only while closing tbc variables when
** returning from a function;
** - field 'transferinfo' is used only during call/returnhooks,
** before the function starts or after it ends.
*/
struct CallInfo {
  StkIdRel func;  /* function index in the stack */
  StkIdRel	top;  /* top for this function */
  struct CallInfo *previous, *next;  /* dynamic call link */
  union {
    struct {  /* only for Lua functions */
      const Instruction *savedpc;
      volatile sig_atomic_t trap;
      int nextraargs;  /* # of extra arguments in vararg functions */
    } l;
    struct {  /* only for C functions */
      lua_KFunction k;  /* continuation in case of yields */
      ptrdiff_t old_errfunc;
      lua_KContext ctx;  /* context info. in case of yields */
    } c;
  } u;
  union {
    int funcidx;  /* called-function index */
    int nyield;  /* number of values yielded */
    int nres;  /* number of values returned */
    struct {  /* info about transferred values (for call/return hooks) */
      unsigned short ftransfer;  /* offset of first value transferred */
      unsigned short ntransfer;  /* number of values transferred */
    } transferinfo;
  } u2;
  short nresults;  /* expected number of results from this function */
  unsigned short callstatus;
};


/*
** Bits in CallInfo status
*/










/* Bits 10-12 are used for CIST_RECST (see below) */






/*
** Field CIST_RECST stores the "recover status", used to keep the error
** status while closing to-be-closed variables in coroutines, so that
** Lua can correctly resume after an yield from a __close method called
** because of an error.  (Three bits are enough for error status.)
*/




/* active function is a Lua function */


/* call is running Lua code (not a hook) */


/* assume that CIST_OAH has offset 0 and that 'v' is strictly 0/1 */




/*
** 'global state', shared by all threads of this state
*/
typedef struct global_State {
  lua_Alloc frealloc;  /* function to reallocate memory */
  void *ud;         /* auxiliary data to 'frealloc' */
  l_mem totalbytes;  /* number of bytes currently allocated - GCdebt */
  l_mem GCdebt;  /* bytes allocated not yet compensated by the collector */
  lu_mem GCestimate;  /* an estimate of the non-garbage memory in use */
  lu_mem lastatomic;  /* see function 'genstep' in file 'lgc.c' */
  stringtable strt;  /* hash table for strings */
  TValue l_registry;
  TValue nilvalue;  /* a nil value */
  unsigned int seed;  /* randomized seed for hashes */
  lu_byte currentwhite;
  lu_byte gcstate;  /* state of garbage collector */
  lu_byte gckind;  /* kind of GC running */
  lu_byte gcstopem;  /* stops emergency collections */
  lu_byte genminormul;  /* control for minor generational collections */
  lu_byte genmajormul;  /* control for major generational collections */
  lu_byte gcstp;  /* control whether GC is running */
  lu_byte gcemergency;  /* true if this is an emergency collection */
  lu_byte gcpause;  /* size of pause between successive GCs */
  lu_byte gcstepmul;  /* GC "speed" */
  lu_byte gcstepsize;  /* (log2 of) GC granularity */
  GCObject *allgc;  /* list of all collectable objects */
  GCObject **sweepgc;  /* current position of sweep in list */
  GCObject *finobj;  /* list of collectable objects with finalizers */
  GCObject *gray;  /* list of gray objects */
  GCObject *grayagain;  /* list of objects to be traversed atomically */
  GCObject *weak;  /* list of tables with weak values */
  GCObject *ephemeron;  /* list of ephemeron tables (weak keys) */
  GCObject *allweak;  /* list of all-weak tables */
  GCObject *tobefnz;  /* list of userdata to be GC */
  GCObject *fixedgc;  /* list of objects not to be collected */
  /* fields for generational collector */
  GCObject *survival;  /* start of objects that survived one GC cycle */
  GCObject *old1;  /* start of old1 objects */
  GCObject *reallyold;  /* objects more than one cycle old ("really old") */
  GCObject *firstold1;  /* first OLD1 object in the list (if any) */
  GCObject *finobjsur;  /* list of survival objects with finalizers */
  GCObject *finobjold1;  /* list of old1 objects with finalizers */
  GCObject *finobjrold;  /* list of really old objects with finalizers */
  struct lua_State *twups;  /* list of threads with open upvalues */
  lua_CFunction panic;  /* to be called in unprotected errors */
  struct lua_State *mainthread;
  TString *memerrmsg;  /* message for memory-allocation errors */
  TString *tmname[TM_N];  /* array with tag-method names */
  struct Table *mt[9];  /* metatables for basic types */
  TString *strcache[53][2];  /* cache for strings in API */
  lua_WarnFunction warnf;  /* warning function */
  void *ud_warn;         /* auxiliary data to 'warnf' */
} global_State;


/*
** 'per thread' state
*/
struct lua_State {
  struct GCObject *next; lu_byte tt; lu_byte marked;
  lu_byte status;
  lu_byte allowhook;
  unsigned short nci;  /* number of items in 'ci' list */
  StkIdRel top;  /* first free slot in the stack */
  global_State *l_G;
  CallInfo *ci;  /* call info for current function */
  StkIdRel stack_last;  /* end of stack (last element + 1) */
  StkIdRel stack;  /* stack base */
  UpVal *openupval;  /* list of open upvalues in this stack */
  StkIdRel tbclist;  /* list of to-be-closed variables */
  GCObject *gclist;
  struct lua_State *twups;  /* list of threads with open upvalues */
  struct lua_longjmp *errorJmp;  /* current error recover point */
  CallInfo base_ci;  /* CallInfo for first level (C calling Lua) */
  volatile lua_Hook hook;
  ptrdiff_t errfunc;  /* current error handling function (stack index) */
  l_uint32 nCcalls;  /* number of nested (non-yieldable | C)  calls */
  int oldpc;  /* last pc traced */
  int basehookcount;
  int hookcount;
  volatile sig_atomic_t hookmask;
};




/*
** 'g->nilvalue' being a nil value flags that the state was completely
** build.
*/



/*
** Union of all collectable objects (only for conversions)
** ISO C99, 6.5.2.3 p.5:
** "if a union contains several structures that share a common initial
** sequence [...], and if the union object currently contains one
** of these structures, it is permitted to inspect the common initial
** part of any of them anywhere that a declaration of the complete type
** of the union is visible."
*/
union GCUnion {
  GCObject gc;  /* common header */
  struct TString ts;
  struct Udata u;
  union Closure cl;
  struct Table h;
  struct Proto p;
  struct lua_State th;  /* thread */
  struct UpVal upv;
};


/*
** ISO C99, 6.7.2.1 p.14:
** "A pointer to a union object, suitably converted, points to each of
** its members [...], and vice versa."
*/


/* macros to convert a GCObject into a specific value */











/*
** macro to convert a Lua object into a GCObject
** (The access to 'tt' tries to ensure that 'v' is actually a Lua object.)
*/



/* actual number of total bytes allocated */


extern void luaE_setdebt (global_State *g, l_mem debt);
extern void luaE_freethread (lua_State *L, lua_State *L1);
extern CallInfo *luaE_extendCI (lua_State *L);
extern void luaE_freeCI (lua_State *L);
extern void luaE_shrinkCI (lua_State *L);
extern void luaE_checkcstack (lua_State *L);
extern void luaE_incCstack (lua_State *L);
extern void luaE_warning (lua_State *L, const char *msg, int tocont);
extern void luaE_warnerror (lua_State *L, const char *where);
extern int luaE_resetthread (lua_State *L, int status);







/* Increments 'L->top.p', checking for stack overflows */



/*
** If a call returns too many multiple returns, the callee may not have
** stack space to accommodate all results. In this case, this macro
** increases its stack space ('L->ci->top.p').
*/



/* Ensure the stack has at least 'n' elements */



/*
** To reduce the overhead of returning from C functions, the presence of
** to-be-closed variables in these functions is coded in the CallInfo's
** field 'nresults', in a way that functions with no to-be-closed variables
** with zero, one, or "all" wanted results have no overhead. Functions
** with other number of wanted results, as well as functions with
** variables to be closed, have an extra check.
*/



/* Map [-1, inf) (range of 'nresults') into (-inf, -2] */





/*
** $Id: ldebug.h $
** Auxiliary functions from Debug Interface module
** See Copyright Notice in lua.h
*/





/*
** $Id: lstate.h $
** Global State
** See Copyright Notice in lua.h
*/

























































































































































































































































































/* Active Lua function (given call info) */





/*
** mark for entries in 'lineinfo' array that has absolute information in
** 'abslineinfo' array
*/



/*
** MAXimum number of successive Instructions WiTHout ABSolute line
** information. (A power of two allows fast divisions.)
*/





extern int luaG_getfuncline (const Proto *f, int pc);
extern const char *luaG_findlocal (lua_State *L, CallInfo *ci, int n,
                                                    StkId *pos);
extern void  luaG_typeerror (lua_State *L, const TValue *o,
                                                const char *opname);
extern void  luaG_callerror (lua_State *L, const TValue *o);
extern void  luaG_forerror (lua_State *L, const TValue *o,
                                               const char *what);
extern void  luaG_concaterror (lua_State *L, const TValue *p1,
                                                  const TValue *p2);
extern void  luaG_opinterror (lua_State *L, const TValue *p1,
                                                 const TValue *p2,
                                                 const char *msg);
extern void  luaG_tointerror (lua_State *L, const TValue *p1,
                                                 const TValue *p2);
extern void  luaG_ordererror (lua_State *L, const TValue *p1,
                                                 const TValue *p2);
extern void  luaG_runerror (lua_State *L, const char *fmt, ...);
extern const char *luaG_addinfo (lua_State *L, const char *msg,
                                                  TString *src, int line);
extern void  luaG_errormsg (lua_State *L);
extern int luaG_traceexec (lua_State *L, const Instruction *pc);




/*
** $Id: ldo.h $
** Stack and Call structure of Lua
** See Copyright Notice in lua.h
*/





/*
** $Id: llimits.h $
** Limits, basic types, and some other 'installation-dependent' definitions
** See Copyright Notice in lua.h
*/










































































































































































































































































































/*
** $Id: lobject.h $
** Type definitions for Lua objects
** See Copyright Notice in lua.h
*/
























































































































































































































































































































































































































































































































































































































































/*
** $Id: lstate.h $
** Global State
** See Copyright Notice in lua.h
*/




















































































































































































































































































/*
** $Id: lzio.h $
** Buffered streams
** See Copyright Notice in lua.h
*/





























































/*
** Macro to check stack size and grow stack if needed.  Parameters
** 'pre'/'pos' allow the macro to preserve a pointer into the
** stack across reallocations, doing the work only when needed.
** It also allows the running of one GC step when the stack is
** reallocated.
** 'condmovestack' is used in heavy tests to force a stack reallocation
** at every check.
*/


/* In general, 'pre'/'pos' are empty (nothing to save) */








/* macro to check stack size, preserving 'p' */



/* macro to check stack size and GC, preserving 'p' */



/* macro to check stack size and GC */



/* type of protected functions, to be ran by 'runprotected' */
typedef void (*Pfunc) (lua_State *L, void *ud);

extern void luaD_seterrorobj (lua_State *L, int errcode, StkId oldtop);
extern int luaD_protectedparser (lua_State *L, ZIO *z, const char *name,
                                                  const char *mode);
extern void luaD_hook (lua_State *L, int event, int line,
                                        int fTransfer, int nTransfer);
extern void luaD_hookcall (lua_State *L, CallInfo *ci);
extern int luaD_pretailcall (lua_State *L, CallInfo *ci, StkId func,
                                              int narg1, int delta);
extern CallInfo *luaD_precall (lua_State *L, StkId func, int nResults);
extern void luaD_call (lua_State *L, StkId func, int nResults);
extern void luaD_callnoyield (lua_State *L, StkId func, int nResults);
extern StkId luaD_tryfuncTM (lua_State *L, StkId func);
extern int luaD_closeprotected (lua_State *L, ptrdiff_t level, int status);
extern int luaD_pcall (lua_State *L, Pfunc func, void *u,
                                        ptrdiff_t oldtop, ptrdiff_t ef);
extern void luaD_poscall (lua_State *L, CallInfo *ci, int nres);
extern int luaD_reallocstack (lua_State *L, int newsize, int raiseerror);
extern int luaD_growstack (lua_State *L, int n, int raiseerror);
extern void luaD_shrinkstack (lua_State *L);
extern void luaD_inctop (lua_State *L);

extern void  luaD_throw (lua_State *L, int errcode);
extern int luaD_rawrunprotected (lua_State *L, Pfunc f, void *ud);




/*
** $Id: lfunc.h $
** Auxiliary functions to manipulate prototypes and closures
** See Copyright Notice in lua.h
*/





/*
** $Id: lobject.h $
** Type definitions for Lua objects
** See Copyright Notice in lua.h
*/































































































































































































































































































































































































































































































































































































































































/* test whether thread is in 'twups' list */



/*
** maximum number of upvalues in a closure (both C and Lua). (Value
** must fit in a VM register.)
*/









/*
** maximum number of misses before giving up the cache of closures
** in prototypes
*/




/* special status to close upvalues preserving the top of the stack */



extern Proto *luaF_newproto (lua_State *L);
extern CClosure *luaF_newCclosure (lua_State *L, int nupvals);
extern LClosure *luaF_newLclosure (lua_State *L, int nupvals);
extern void luaF_initupvals (lua_State *L, LClosure *cl);
extern UpVal *luaF_findupval (lua_State *L, StkId level);
extern void luaF_newtbcupval (lua_State *L, StkId level);
extern void luaF_closeupval (lua_State *L, StkId level);
extern StkId luaF_close (lua_State *L, StkId level, int status, int yy);
extern void luaF_unlinkupval (UpVal *uv);
extern void luaF_freeproto (lua_State *L, Proto *f);
extern const char *luaF_getlocalname (const Proto *func, int local_number,
                                         int pc);




/*
** $Id: lgc.h $
** Garbage Collector
** See Copyright Notice in lua.h
*/





/*
** $Id: lobject.h $
** Type definitions for Lua objects
** See Copyright Notice in lua.h
*/
























































































































































































































































































































































































































































































































































































































































/*
** $Id: lstate.h $
** Global State
** See Copyright Notice in lua.h
*/





















































































































































































































































































/*
** Collectable objects may have one of three colors: white, which means
** the object is not marked; gray, which means the object is marked, but
** its references may be not marked; and black, which means that the
** object and all its references are marked.  The main invariant of the
** garbage collector, while marking objects, is that a black object can
** never point to a white one. Moreover, any gray object must be in a
** "gray list" (gray, grayagain, weak, allweak, ephemeron) so that it
** can be visited again before finishing the collection cycle. (Open
** upvalues are an exception to this rule.)  These lists have no meaning
** when the invariant is not being enforced (e.g., sweep phase).
*/


/*
** Possible states of the Garbage Collector
*/














/*
** macro to tell when main invariant (white objects cannot point to black
** ones) must be kept. During a collection, the sweep
** phase may break the invariant, as objects turned white may point to
** still-black objects. The invariant is restored when sweep ends and
** all objects are white again.
*/




/*
** some useful bit tricks
*/










/*
** Layout for bit use in 'marked' field. First three bits are
** used for object "age" in generational mode. Last bit is used
** by tests.
*/




























/* object age in generational mode */

















/* Default Values for GC parameters */



/* wait memory to double before starting new cycle */


/*
** some gc parameters are stored divided by 4 to allow a maximum value
** up to 1023 in a 'lu_byte'.
*/





/* how much to allocate before next GC step (log2) */



/*
** Check whether the declared GC mode is generational. While in
** generational mode, the collector can go temporarily to incremental
** mode to improve performance. This is signaled by 'g->lastatomic != 0'.
*/



/*
** Control when GC is running:
*/






/*
** Does one step of collection when debt becomes positive. 'pre'/'pos'
** allows some adjustments to be done only when needed. macro
** 'condchangemem' is used only for heavy tests (forcing a full
** GC cycle on every opportunity)
*/


/* more often than not, 'pre'/'pos' are empty */











extern void luaC_fix (lua_State *L, GCObject *o);
extern void luaC_freeallobjects (lua_State *L);
extern void luaC_step (lua_State *L);
extern void luaC_runtilstate (lua_State *L, int statesmask);
extern void luaC_fullgc (lua_State *L, int isemergency);
extern GCObject *luaC_newobj (lua_State *L, int tt, size_t sz);
extern GCObject *luaC_newobjdt (lua_State *L, int tt, size_t sz,
                                                 size_t offset);
extern void luaC_barrier_ (lua_State *L, GCObject *o, GCObject *v);
extern void luaC_barrierback_ (lua_State *L, GCObject *o);
extern void luaC_checkfinalizer (lua_State *L, GCObject *o, Table *mt);
extern void luaC_changemode (lua_State *L, int newmode);




/*
** $Id: lmem.h $
** Interface to Memory Manager
** See Copyright Notice in lua.h
*/





























































/*
** $Id: lobject.h $
** Type definitions for Lua objects
** See Copyright Notice in lua.h
*/
























































































































































































































































































































































































































































































































































































































































/*
** $Id: lstate.h $
** Global State
** See Copyright Notice in lua.h
*/




















































































































































































































































































/*
** $Id: lstring.h $
** String table (keep all strings handled by Lua)
** See Copyright Notice in lua.h
*/




/*
** $Id: lgc.h $
** Garbage Collector
** See Copyright Notice in lua.h
*/




















































































































































/*
** $Id: lobject.h $
** Type definitions for Lua objects
** See Copyright Notice in lua.h
*/
























































































































































































































































































































































































































































































































































































































































/*
** $Id: lstate.h $
** Global State
** See Copyright Notice in lua.h
*/






















































































































































































































































































/*
** Memory-allocation error message must be preallocated (it cannot
** be created after memory is exhausted)
*/



/*
** Size of a TString: Size of the header plus space for the string
** itself (including final '\0').
*/





/*
** test whether a string is a reserved word
*/



/*
** equality for short strings, which are always internalized
*/



extern unsigned int luaS_hash (const char *str, size_t l, unsigned int seed);
extern unsigned int luaS_hashlongstr (TString *ts);
extern int luaS_eqlngstr (TString *a, TString *b);
extern void luaS_resize (lua_State *L, int newsize);
extern void luaS_clearcache (global_State *g);
extern void luaS_init (lua_State *L);
extern void luaS_remove (lua_State *L, TString *ts);
extern Udata *luaS_newudata (lua_State *L, size_t s, int nuvalue);
extern TString *luaS_newlstr (lua_State *L, const char *str, size_t l);
extern TString *luaS_new (lua_State *L, const char *str);
extern TString *luaS_createlngstrobj (lua_State *L, size_t l);




/*
** $Id: ltable.h $
** Lua tables (hash)
** See Copyright Notice in lua.h
*/




/*
** $Id: lobject.h $
** Type definitions for Lua objects
** See Copyright Notice in lua.h
*/































































































































































































































































































































































































































































































































































































































































/*
** Clear all bits of fast-access metamethods, which means that the table
** may have any of these metamethods. (First access that fails after the
** clearing will set the bit again.)
*/



/* true when 't' is using 'dummynode' as its hash part */



/* allocated size for hash nodes */



/* returns the Node, given the value of a table entry */



extern const TValue *luaH_getint (Table *t, lua_Integer key);
extern void luaH_setint (lua_State *L, Table *t, lua_Integer key,
                                                    TValue *value);
extern const TValue *luaH_getshortstr (Table *t, TString *key);
extern const TValue *luaH_getstr (Table *t, TString *key);
extern const TValue *luaH_get (Table *t, const TValue *key);
extern void luaH_newkey (lua_State *L, Table *t, const TValue *key,
                                                    TValue *value);
extern void luaH_set (lua_State *L, Table *t, const TValue *key,
                                                 TValue *value);
extern void luaH_finishset (lua_State *L, Table *t, const TValue *key,
                                       const TValue *slot, TValue *value);
extern Table *luaH_new (lua_State *L);
extern void luaH_resize (lua_State *L, Table *t, unsigned int nasize,
                                                    unsigned int nhsize);
extern void luaH_resizearray (lua_State *L, Table *t, unsigned int nasize);
extern void luaH_free (lua_State *L, Table *t);
extern int luaH_next (lua_State *L, Table *t, StkId key);
extern lua_Unsigned luaH_getn (Table *t);
extern unsigned int luaH_realasize (const Table *t);









/*
** $Id: ltm.h $
** Tag methods
** See Copyright Notice in lua.h
*/
























































































/*
** $Id: lundump.h $
** load precompiled Lua chunks
** See Copyright Notice in lua.h
*/




/*
** $Id: llimits.h $
** Limits, basic types, and some other 'installation-dependent' definitions
** See Copyright Notice in lua.h
*/










































































































































































































































































































/*
** $Id: lobject.h $
** Type definitions for Lua objects
** See Copyright Notice in lua.h
*/
























































































































































































































































































































































































































































































































































































































































/*
** $Id: lzio.h $
** Buffered streams
** See Copyright Notice in lua.h
*/





























































/* data to catch conversion errors */





/*
** Encode major-minor version in one byte, one nibble for each
*/





/* load one chunk; from lundump.c */
extern LClosure* luaU_undump (lua_State* L, ZIO* Z, const char* name);

/* dump one chunk; from ldump.c */
extern int luaU_dump (lua_State* L, const Proto* f, lua_Writer w,
                         void* data, int strip);



/*
** $Id: lvm.h $
** Lua virtual machine
** See Copyright Notice in lua.h
*/





/*
** $Id: ldo.h $
** Stack and Call structure of Lua
** See Copyright Notice in lua.h
*/

































































/*
** $Id: lobject.h $
** Type definitions for Lua objects
** See Copyright Notice in lua.h
*/
























































































































































































































































































































































































































































































































































































































































/*
** $Id: ltm.h $
** Tag methods
** See Copyright Notice in lua.h
*/








































































































/*
** You can define LUA_FLOORN2I if you want to convert floats to integers
** by flooring them (instead of raising an error if they are not
** integral values)
*/





/*
** Rounding modes for float->integer coercion
 */
typedef enum {
  F2Ieq,     /* no rounding; accepts only integral values */
  F2Ifloor,  /* takes the floor of the number */
  F2Iceil    /* takes the ceil of the number */
} F2Imod;


/* convert an object to a float (including string coercion) */



/* convert an object to a float (without string coercion) */



/* convert an object to an integer (including string coercion) */



/* convert an object to an integer (without string coercion) */








/*
** fast track for 'gettable': if 't' is a table and 't[k]' is present,
** return 1 with 'slot' pointing to 't[k]' (position of final result).
** Otherwise, return 0 (meaning it will have to check metamethod)
** with 'slot' pointing to an empty 't[k]' (if 't' is a table) or NULL
** (otherwise). 'f' is the raw get function to use.
*/



/*
** Special case of 'luaV_fastget' for integers, inlining the fast case
** of 'luaH_getint'.
*/



/*
** Finish a fast set operation (when fast get succeeds). In that case,
** 'slot' points to the place to put the value.
*/



/*
** Shift right is the same as shift left with a negative 'y'
*/




extern int luaV_equalobj (lua_State *L, const TValue *t1, const TValue *t2);
extern int luaV_lessthan (lua_State *L, const TValue *l, const TValue *r);
extern int luaV_lessequal (lua_State *L, const TValue *l, const TValue *r);
extern int luaV_tonumber_ (const TValue *obj, lua_Number *n);
extern int luaV_tointeger (const TValue *obj, lua_Integer *p, F2Imod mode);
extern int luaV_tointegerns (const TValue *obj, lua_Integer *p,
                                F2Imod mode);
extern int luaV_flttointeger (lua_Number n, lua_Integer *p, F2Imod mode);
extern void luaV_finishget (lua_State *L, const TValue *t, TValue *key,
                               StkId val, const TValue *slot);
extern void luaV_finishset (lua_State *L, const TValue *t, TValue *key,
                               TValue *val, const TValue *slot);
extern void luaV_finishOp (lua_State *L);
extern void luaV_execute (lua_State *L, CallInfo *ci);
extern void luaV_concat (lua_State *L, int total);
extern lua_Integer luaV_idiv (lua_State *L, lua_Integer x, lua_Integer y);
extern lua_Integer luaV_mod (lua_State *L, lua_Integer x, lua_Integer y);
extern lua_Number luaV_modf (lua_State *L, lua_Number x, lua_Number y);
extern lua_Integer luaV_shiftl (lua_Integer x, lua_Integer y);
extern void luaV_objlen (lua_State *L, StkId ra, const TValue *rb);






const char lua_ident[] =
  "$LuaVersion: " "Lua " "5" "." "4" "." "6" "  Copyright (C) 1994-2023 Lua.org, PUC-Rio" " $"
  "$LuaAuthors: " "R. Ierusalimschy, L. H. de Figueiredo, W. Celes" " $";



/*
** Test for a valid index (one that is not the 'nilvalue').
** '!ttisnil(o)' implies 'o != &G(L)->nilvalue', so it is not needed.
** However, it covers the most common cases in a faster way.
*/



/* test for pseudo index */


/* test for upvalue */



/*
** Convert an acceptable index to a pointer to its respective value.
** Non-valid indices return the special nil value 'G(L)->nilvalue'.
*/
static TValue *index2value (lua_State *L, int idx) {
  CallInfo *ci = L->ci;
  if (idx > 0) {
    StkId o = ci->func.p + idx;
    ((void)L, ((void)0));
    if (o >= L->top.p) return &(L->l_G)->nilvalue;
    else return (&(o)->val);
  }
  else if (!((idx) <= (-15000 - 1000))) {  /* negative index */
    ((void)L, ((void)0));
    return (&(L->top.p + idx)->val);
  }
  else if (idx == (-15000 - 1000))
    return &(L->l_G)->l_registry;
  else {  /* upvalues */
    idx = (-15000 - 1000) - idx;
    ((void)L, ((void)0));
    if ((((((&(ci->func.p)->val)))->tt_) == ( ((((6) | (( 2) << 4))  ) | (1 << 6))))) {  /* C closure? */
      CClosure *func = ( check_exp(((((&(ci->func.p)->val))->value_).gc)->tt == ((6) | (( 2) << 4))  , &((((union GCUnion *)( ((((&(ci->func.p)->val))->value_).gc))))->cl.c)));
      return (idx <= func->nupvalues) ? &func->upvalue[idx-1]
                                      : &(L->l_G)->nilvalue;
    }
    else {  /* light C function or Lua function (through a hook)?) */
      ((void)L, ((void)0));
      return &(L->l_G)->nilvalue;  /* no upvalues */
    }
  }
}



/*
** Convert a valid actual index (not a pseudo-index) to its address.
*/
static  StkId index2stack (lua_State *L, int idx) {
  CallInfo *ci = L->ci;
  if (idx > 0) {
    StkId o = ci->func.p + idx;
    ((void)L, ((void)0));
    return o;
  }
  else {    /* non-positive index */
    ((void)L, ((void)0));
    ((void)L, ((void)0));
    return L->top.p + idx;
  }
}


extern int lua_checkstack (lua_State *L, int n) {
  int res;
  CallInfo *ci;
  ((void) 0);
  ci = L->ci;
  ((void)L, ((void)0));
  if (L->stack_last.p - L->top.p > n)  /* stack large enough? */
    res = 1;  /* yes; check is OK */
  else  /* need to grow stack */
    res = luaD_growstack(L, n, 0);
  if (res && ci->top.p < L->top.p + n)
    ci->top.p = L->top.p + n;  /* adjust frame top */
  ((void) 0);
  return res;
}


extern void lua_xmove (lua_State *from, lua_State *to, int n) {
  int i;
  if (from == to) return;
  ((void) 0);
  	((void)from, ((void)0));
  ((void)from, ((void)0));
  ((void)from, ((void)0));
  from->top.p -= n;
  for (i = 0; i < n; i++) {
    	{ TValue *io1=((&( to->top.p)->val)); const TValue *io2=((&( from->top.p + i)->val));           io1->value_ = io2->value_; ((io1)->tt_=( io2->tt_)); 	  	((void)to, ((void)0)); ((void)0); };
    to->top.p++;  /* stack already checked by previous 'api_check' */
  }
  ((void) 0);
}


extern lua_CFunction lua_atpanic (lua_State *L, lua_CFunction panicf) {
  lua_CFunction old;
  ((void) 0);
  old = (L->l_G)->panic;
  (L->l_G)->panic = panicf;
  ((void) 0);
  return old;
}


extern lua_Number lua_version (lua_State *L) {
  ((void)(L));
  return 504;
}



/*
** basic stack manipulation
*/


/*
** convert an acceptable stack index into an absolute index
*/
extern int lua_absindex (lua_State *L, int idx) {
  return (idx > 0 || ((idx) <= (-15000 - 1000)))
         ? idx
         : ((int)( (L->top.p - L->ci->func.p))) + idx;
}


extern int lua_gettop (lua_State *L) {
  return ((int)( (L->top.p - (L->ci->func.p + 1))));
}


extern void lua_settop (lua_State *L, int idx) {
  CallInfo *ci;
  StkId func, newtop;
  ptrdiff_t diff;  /* difference for new top */
  ((void) 0);
  ci = L->ci;
  func = ci->func.p;
  if (idx >= 0) {
    ((void)L, ((void)0));
    diff = ((func + 1) + idx) - L->top.p;
    for (; diff > 0; diff--)
      (((&(L->top.p++)->val))->tt_=( ((0) | (( 0) << 4))));  /* clear new slots */
  }
  else {
    ((void)L, ((void)0));
    diff = idx + 1;  /* will "subtract" index (as it is negative) */
  }
  ((void)L, ((void)0));
  newtop = L->top.p + diff;
  if (diff < 0 && L->tbclist.p >= newtop) {
    ((void)0);
    newtop = luaF_close(L, newtop, (-1), 0);
  }
  L->top.p = newtop;  /* correct top only after closing any upvalue */
  ((void) 0);
}


extern void lua_closeslot (lua_State *L, int idx) {
  StkId level;
  ((void) 0);
  level = index2stack(L, idx);
  ((void)L, ((void)0));
  level = luaF_close(L, level, (-1), 0);
  (((&(level)->val))->tt_=( ((0) | (( 0) << 4))));
  ((void) 0);
}


/*
** Reverse the stack segment from 'from' to 'to'
** (auxiliary to 'lua_rotate')
** Note that we move(copy) only the value inside the stack.
** (We do not move additional fields that may exist.)
*/
static  void reverse (lua_State *L, StkId from, StkId to) {
  for (; from < to; from++, to--) {
    TValue temp;
    	{ TValue *io1=( &temp); const TValue *io2=( (&(from)->val));           io1->value_ = io2->value_; ((io1)->tt_=( io2->tt_)); 	  	((void)L, ((void)0)); ((void)0); };
    	{ TValue *io1=((&( from)->val)); const TValue *io2=((&( to)->val));           io1->value_ = io2->value_; ((io1)->tt_=( io2->tt_)); 	  	((void)L, ((void)0)); ((void)0); };
    	{ TValue *io1=((&( to)->val)); const TValue *io2=( &temp);           io1->value_ = io2->value_; ((io1)->tt_=( io2->tt_)); 	  	((void)L, ((void)0)); ((void)0); };
  }
}


/*
** Let x = AB, where A is a prefix of length 'n'. Then,
** rotate x n == BA. But BA == (A^r . B^r)^r.
*/
extern void lua_rotate (lua_State *L, int idx, int n) {
  StkId p, t, m;
  ((void) 0);
  t = L->top.p - 1;  /* end of stack segment being rotated */
  p = index2stack(L, idx);  /* start of segment */
  ((void)L, ((void)0));
  m = (n >= 0 ? t - n : p - n - 1);  /* end of prefix */
  reverse(L, p, m);  /* reverse the prefix with length 'n' */
  reverse(L, m + 1, t);  /* reverse the suffix */
  reverse(L, p, t);  /* reverse the entire segment */
  ((void) 0);
}


extern void lua_copy (lua_State *L, int fromidx, int toidx) {
  TValue *fr, *to;
  ((void) 0);
  fr = index2value(L, fromidx);
  to = index2value(L, toidx);
  ((void)L, ((void)0));
  	{ TValue *io1=( to); const TValue *io2=( fr);           io1->value_ = io2->value_; ((io1)->tt_=( io2->tt_)); 	  	((void)L, ((void)0)); ((void)0); };
  if (((toidx) < (-15000 - 1000)))  /* function upvalue? */
    (  	((( fr)->tt_) & (1 << 6)) ? (  	(((( ( ( &((((union GCUnion *)( ((((&(L->ci->func.p)->val))->value_).gc))))->cl.c))))->marked) & ( (1<<( 5  )))) && (((( (( fr)->value_).gc))->marked) & ( ((1<<(3  )) | (1<<( 4  )))))) ? 	luaC_barrier_(L,( &(((union GCUnion *)( ( ( ( &((((union GCUnion *)( ((((&(L->ci->func.p)->val))->value_).gc))))->cl.c))))))->gc)),( &(((union GCUnion *)( (( (( fr)->value_).gc))))->gc))) : ((void)( (0)))) : ((void)( (0))));
  /* LUA_REGISTRYINDEX does not need gc barrier
     (collector revisits it before finishing collection) */
  ((void) 0);
}


extern void lua_pushvalue (lua_State *L, int idx) {
  ((void) 0);
  	{ TValue *io1=((&( L->top.p)->val)); const TValue *io2=( index2value(L, idx));           io1->value_ = io2->value_; ((io1)->tt_=( io2->tt_)); 	  	((void)L, ((void)0)); ((void)0); };
  {L->top.p++; 			 ((void)L, ((void)0));};
  ((void) 0);
}



/*
** access functions (stack -> C)
*/


extern int lua_type (lua_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return ((!((((((( o))->tt_)) & 0x0F)) == ( 0)) ||  o != &(L->l_G)->nilvalue) ? (((((o)->tt_)) & 0x0F)) : (-1));
}


extern const char *lua_typename (lua_State *L, int t) {
  ((void)(L));
  ((void)L, ((void)0));
  return luaT_typenames_[(t) + 1];
}


extern int lua_iscfunction (lua_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (((((o))->tt_) == ( ((6) | (( 1) << 4))  )) || (((((o))->tt_) == ( ((((6) | (( 2) << 4))  ) | (1 << 6))))));
}


extern int lua_isinteger (lua_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return ((((o))->tt_) == ( ((3) | (( 0) << 4))  ));
}


extern int lua_isnumber (lua_State *L, int idx) {
  lua_Number n;
  const TValue *o = index2value(L, idx);
  return 	(((((o))->tt_) == ( ((3) | (( 1) << 4))  )) ? (*( &n) = ( ((o)->value_).n), 1) : luaV_tonumber_(o, &n));
}


extern int lua_isstring (lua_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return ((((((((o))->tt_)) & 0x0F)) == ( 4)) || (((((((o))->tt_)) & 0x0F)) == ( 3)));
}


extern int lua_isuserdata (lua_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (((((o))->tt_) == ( ((((7) | (( 0) << 4))) | (1 << 6)))) || ((((o))->tt_) == ( ((2) | (( 0) << 4)))));
}


extern int lua_rawequal (lua_State *L, int index1, int index2) {
  const TValue *o1 = index2value(L, index1);
  const TValue *o2 = index2value(L, index2);
  return ((!((((((( o1))->tt_)) & 0x0F)) == ( 0)) ||  o1 != &(L->l_G)->nilvalue) && (!((((((( o2))->tt_)) & 0x0F)) == ( 0)) ||  o2 != &(L->l_G)->nilvalue)) ? luaV_equalobj(((void*)0),o1, o2) : 0;
}


extern void lua_arith (lua_State *L, int op) {
  ((void) 0);
  if (op != 12 && op != 13)
    	((void)L, ((void)0));  /* all other operations expect two operands */
  else {  /* for unary operations, add fake 2nd operand */
    	((void)L, ((void)0));
    	{ TValue *io1=((&( L->top.p)->val)); const TValue *io2=((&( L->top.p - 1)->val));           io1->value_ = io2->value_; ((io1)->tt_=( io2->tt_)); 	  	((void)L, ((void)0)); ((void)0); };
    {L->top.p++; 			 ((void)L, ((void)0));};
  }
  /* first operand at top - 2, second at top - 1; result go to top - 2 */
  luaO_arith(L, op, (&(L->top.p - 2)->val), (&(L->top.p - 1)->val), L->top.p - 2);
  L->top.p--;  /* remove second operand */
  ((void) 0);
}


extern int lua_compare (lua_State *L, int index1, int index2, int op) {
  const TValue *o1;
  const TValue *o2;
  int i = 0;
  ((void) 0);  /* may call tag method */
  o1 = index2value(L, index1);
  o2 = index2value(L, index2);
  if ((!((((((( o1))->tt_)) & 0x0F)) == ( 0)) ||  o1 != &(L->l_G)->nilvalue) && (!((((((( o2))->tt_)) & 0x0F)) == ( 0)) ||  o2 != &(L->l_G)->nilvalue)) {
    switch (op) {
      case 0: i = luaV_equalobj(L, o1, o2); break;
      case 1: i = luaV_lessthan(L, o1, o2); break;
      case 2: i = luaV_lessequal(L, o1, o2); break;
      default: ((void)L, ((void)0));
    }
  }
  ((void) 0);
  return i;
}


extern size_t lua_stringtonumber (lua_State *L, const char *s) {
  size_t sz = luaO_str2num(s, (&(L->top.p)->val));
  if (sz != 0)
    {L->top.p++; 			 ((void)L, ((void)0));};
  return sz;
}


extern lua_Number lua_tonumberx (lua_State *L, int idx, int *pisnum) {
  lua_Number n = 0;
  const TValue *o = index2value(L, idx);
  int isnum = 	(((((o))->tt_) == ( ((3) | (( 1) << 4))  )) ? (*( &n) = ( ((o)->value_).n), 1) : luaV_tonumber_(o, &n));
  if (pisnum)
    *pisnum = isnum;
  return n;
}


extern lua_Integer lua_tointegerx (lua_State *L, int idx, int *pisnum) {
  lua_Integer res = 0;
  const TValue *o = index2value(L, idx);
  int isnum =   (((((((((o))->tt_) == ( ((3) | (( 0) << 4))  ))) != 0))) ? (*( &res) = ( ((o)->value_).i), 1)                           : luaV_tointeger(o, &res,F2Ieq));
  if (pisnum)
    *pisnum = isnum;
  return res;
}


extern int lua_toboolean (lua_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return !(((((o))->tt_) == ( ((1) | (( 0) << 4)))) || (((((((o))->tt_)) & 0x0F)) == ( 0)));
}


extern const char *lua_tolstring (lua_State *L, int idx, size_t *len) {
  TValue *o;
  ((void) 0);
  o = index2value(L, idx);
  if (!(((((((o))->tt_)) & 0x0F)) == ( 4))) {
    if (!(((((((o))->tt_)) & 0x0F)) == ( 3))) {  /* not convertible? */
      if (len != ((void*)0)) *len = 0;
      ((void) 0);
      return ((void*)0);
    }
    luaO_tostring(L, o);
    	{ if ((L->l_G)->GCdebt > 0) { (void)0; luaC_step(L); (void)0;}; 	  ((void)0); };
    o = index2value(L, idx);  /* previous call may reallocate the stack */
  }
  if (len != ((void*)0))
    *len = ((( 	( &((((union GCUnion *)( (((o)->value_).gc))))->ts))))->tt == ((4) | (( 0) << 4))   ? (( 	( &((((union GCUnion *)( (((o)->value_).gc))))->ts))))->shrlen : (( 	( &((((union GCUnion *)( (((o)->value_).gc))))->ts))))->u.lnglen);
  ((void) 0);
  return ((( 	( &((((union GCUnion *)( (((o)->value_).gc))))->ts))))->contents);
}


extern lua_Unsigned lua_rawlen (lua_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  switch (((((o)->tt_)) & 0x3F)) {
    case ((4) | (( 0) << 4))  : return ( 	check_exp((((((o)->value_).gc)->tt) & 0x0F) == 4, &((((union GCUnion *)( (((o)->value_).gc))))->ts)))->shrlen;
    case ((4) | (( 1) << 4))  : return ( 	check_exp((((((o)->value_).gc)->tt) & 0x0F) == 4, &((((union GCUnion *)( (((o)->value_).gc))))->ts)))->u.lnglen;
    case ((7) | (( 0) << 4)): return ( check_exp((((o)->value_).gc)->tt == ((7) | (( 0) << 4)), &((((union GCUnion *)( (((o)->value_).gc))))->u)))->len;
    case ((5) | (( 0) << 4)): return luaH_getn(( check_exp((((o)->value_).gc)->tt == ((5) | (( 0) << 4)), &((((union GCUnion *)( (((o)->value_).gc))))->h))));
    default: return 0;
  }
}


extern lua_CFunction lua_tocfunction (lua_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  if (((((o))->tt_) == ( ((6) | (( 1) << 4))  ))) return ( ((o)->value_).f);
  else if (((((o))->tt_) == ( ((((6) | (( 2) << 4))  ) | (1 << 6)))))
    return ( check_exp((((o)->value_).gc)->tt == ((6) | (( 2) << 4))  , &((((union GCUnion *)( (((o)->value_).gc))))->cl.c)))->f;
  else return ((void*)0);  /* not a C function */
}


static  void *touserdata (const TValue *o) {
  switch ((((((o)->tt_)) & 0x0F))) {
    case 7: return (((char *)( (( ( &((((union GCUnion *)( (((o)->value_).gc))))->u)))))) + 	(((( ( &((((union GCUnion *)( (((o)->value_).gc))))->u))))->nuvalue) == 0 ? ((unsigned long)&(((Udata0*)0)-> bindata))                      : ((unsigned long)&(((Udata*)0)-> uv)) + (sizeof(UValue) * ((( ( &((((union GCUnion *)( (((o)->value_).gc))))->u))))->nuvalue))));
    case 2: return ( ((o)->value_).p);
    default: return ((void*)0);
  }
}


extern void *lua_touserdata (lua_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return touserdata(o);
}


extern lua_State *lua_tothread (lua_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  return (!((((o))->tt_) == ( ((((8) | (( 0) << 4))) | (1 << 6))))) ? ((void*)0) : ( check_exp((((o)->value_).gc)->tt == ((8) | (( 0) << 4)), &((((union GCUnion *)( (((o)->value_).gc))))->th)));
}


/*
** Returns a pointer to the internal representation of an object.
** Note that ANSI C does not allow the conversion of a pointer to
** function to a 'void*', so the conversion here goes through
** a 'size_t'. (As the returned pointer is only informative, this
** conversion should not be a problem.)
*/
extern const void *lua_topointer (lua_State *L, int idx) {
  const TValue *o = index2value(L, idx);
  switch (((((o)->tt_)) & 0x3F)) {
    case ((6) | (( 1) << 4))  : return ((void *)( (((size_t)( (( ((o)->value_).f)))))));
    case ((7) | (( 0) << 4)): case ((2) | (( 0) << 4)):
      return touserdata(o);
    default: {
      if ((((o)->tt_) & (1 << 6)))
        return ( ((o)->value_).gc);
      else
        return ((void*)0);
    }
  }
}



/*
** push functions (C -> stack)
*/


extern void lua_pushnil (lua_State *L) {
  ((void) 0);
  (((&(L->top.p)->val))->tt_=( ((0) | (( 0) << 4))));
  {L->top.p++; 			 ((void)L, ((void)0));};
  ((void) 0);
}


extern void lua_pushnumber (lua_State *L, lua_Number n) {
  ((void) 0);
    { TValue *io=((&(L->top.p)->val)); ((io)->value_).n=( n); ((io)->tt_=( ((3) | (( 1) << 4))  )); };
  {L->top.p++; 			 ((void)L, ((void)0));};
  ((void) 0);
}


extern void lua_pushinteger (lua_State *L, lua_Integer n) {
  ((void) 0);
    { TValue *io=((&(L->top.p)->val)); ((io)->value_).i=( n); ((io)->tt_=( ((3) | (( 0) << 4))  )); };
  {L->top.p++; 			 ((void)L, ((void)0));};
  ((void) 0);
}


/*
** Pushes on the stack a string with given length. Avoid using 's' when
** 'len' == 0 (as 's' can be NULL in that case), due to later use of
** 'memcmp' and 'memcpy'.
*/
extern const char *lua_pushlstring (lua_State *L, const char *s, size_t len) {
  TString *ts;
  ((void) 0);
  ts = (len == 0) ? luaS_new(L, "") : luaS_newlstr(L, s, len);
    { TValue *io = ((&( L->top.p)->val)); TString *x_ = ( ts);     ((io)->value_).gc = ( &(((union GCUnion *)( (x_)))->gc)); ((io)->tt_=( ((x_->tt) | (1 << 6))));     	((void)L, ((void)0)); };
  {L->top.p++; 			 ((void)L, ((void)0));};
  	{ if ((L->l_G)->GCdebt > 0) { (void)0; luaC_step(L); (void)0;}; 	  ((void)0); };
  ((void) 0);
  return ((ts)->contents);
}


extern const char *lua_pushstring (lua_State *L, const char *s) {
  ((void) 0);
  if (s == ((void*)0))
    (((&(L->top.p)->val))->tt_=( ((0) | (( 0) << 4))));
  else {
    TString *ts;
    ts = luaS_new(L, s);
      { TValue *io = ((&( L->top.p)->val)); TString *x_ = ( ts);     ((io)->value_).gc = ( &(((union GCUnion *)( (x_)))->gc)); ((io)->tt_=( ((x_->tt) | (1 << 6))));     	((void)L, ((void)0)); };
    s = ((ts)->contents);  /* internal copy's address */
  }
  {L->top.p++; 			 ((void)L, ((void)0));};
  	{ if ((L->l_G)->GCdebt > 0) { (void)0; luaC_step(L); (void)0;}; 	  ((void)0); };
  ((void) 0);
  return s;
}


extern const char *lua_pushvfstring (lua_State *L, const char *fmt,
                                      va_list argp) {
  const char *ret;
  ((void) 0);
  ret = luaO_pushvfstring(L, fmt, argp);
  	{ if ((L->l_G)->GCdebt > 0) { (void)0; luaC_step(L); (void)0;}; 	  ((void)0); };
  ((void) 0);
  return ret;
}


extern const char *lua_pushfstring (lua_State *L, const char *fmt, ...) {
  const char *ret;
  va_list argp;
  ((void) 0);
  va_start(argp, fmt);
  ret = luaO_pushvfstring(L, fmt, argp);
  va_end(argp);
  	{ if ((L->l_G)->GCdebt > 0) { (void)0; luaC_step(L); (void)0;}; 	  ((void)0); };
  ((void) 0);
  return ret;
}


extern void lua_pushcclosure (lua_State *L, lua_CFunction fn, int n) {
  ((void) 0);
  if (n == 0) {
      { TValue *io=((&(L->top.p)->val)); ((io)->value_).f=( fn); ((io)->tt_=( ((6) | (( 1) << 4))  )); };
    {L->top.p++; 			 ((void)L, ((void)0));};
  }
  else {
    CClosure *cl;
    	((void)L, ((void)0));
    ((void)L, ((void)0));
    cl = luaF_newCclosure(L, n);
    cl->f = fn;
    L->top.p -= n;
    while (n--) {
      	{ TValue *io1=( &cl->upvalue[n]); const TValue *io2=( (&(L->top.p + n)->val));           io1->value_ = io2->value_; ((io1)->tt_=( io2->tt_)); 	  	((void)L, ((void)0)); ((void)0); };
      /* does not need barrier because closure is white */
      ((void)0);
    }
      { TValue *io = ( (&(L->top.p)->val)); CClosure *x_ = ( cl);     ((io)->value_).gc = ( &(((union GCUnion *)( (x_)))->gc)); ((io)->tt_=( ((((6) | (( 2) << 4))  ) | (1 << 6))));     	((void)L, ((void)0)); };
    {L->top.p++; 			 ((void)L, ((void)0));};
    	{ if ((L->l_G)->GCdebt > 0) { (void)0; luaC_step(L); (void)0;}; 	  ((void)0); };
  }
  ((void) 0);
}


extern void lua_pushboolean (lua_State *L, int b) {
  ((void) 0);
  if (b)
    (((&(L->top.p)->val))->tt_=( ((1) | (( 1) << 4))));
  else
    (((&(L->top.p)->val))->tt_=( ((1) | (( 0) << 4))));
  {L->top.p++; 			 ((void)L, ((void)0));};
  ((void) 0);
}


extern void lua_pushlightuserdata (lua_State *L, void *p) {
  ((void) 0);
    { TValue *io=((&(L->top.p)->val)); ((io)->value_).p=( p); ((io)->tt_=( ((2) | (( 0) << 4)))); };
  {L->top.p++; 			 ((void)L, ((void)0));};
  ((void) 0);
}


extern int lua_pushthread (lua_State *L) {
  ((void) 0);
    { TValue *io = ( (&(L->top.p)->val)); lua_State *x_ = ( L);     ((io)->value_).gc = ( &(((union GCUnion *)( (x_)))->gc)); ((io)->tt_=( ((((8) | (( 0) << 4))) | (1 << 6))));     	((void)L, ((void)0)); };
  {L->top.p++; 			 ((void)L, ((void)0));};
  ((void) 0);
  return ((L->l_G)->mainthread == L);
}



/*
** get functions (Lua -> stack)
*/


static  int auxgetstr (lua_State *L, const TValue *t, const char *k) {
  const TValue *slot;
  TString *str = luaS_new(L, k);
  if (  (!(((( t))->tt_) == ( ((((5) | (( 0) << 4))) | (1 << 6))))     ? ( slot = ((void*)0), 0)       : ( slot =  luaH_getstr(( check_exp(((( t)->value_).gc)->tt == ((5) | (( 0) << 4)), &((((union GCUnion *)( ((( t)->value_).gc))))->h))),  str),          !((((((( slot))->tt_)) & 0x0F)) == ( 0))))  ) {
    	{ TValue *io1=((&( L->top.p)->val)); const TValue *io2=( slot);           io1->value_ = io2->value_; ((io1)->tt_=( io2->tt_)); 	  	((void)L, ((void)0)); ((void)0); };
    {L->top.p++; 			 ((void)L, ((void)0));};
  }
  else {
      { TValue *io = ((&( L->top.p)->val)); TString *x_ = ( str);     ((io)->value_).gc = ( &(((union GCUnion *)( (x_)))->gc)); ((io)->tt_=( ((x_->tt) | (1 << 6))));     	((void)L, ((void)0)); };
    {L->top.p++; 			 ((void)L, ((void)0));};
    luaV_finishget(L, t, (&(L->top.p - 1)->val), L->top.p - 1, slot);
  }
  ((void) 0);
  return ((((((&(L->top.p - 1)->val))->tt_)) & 0x0F));
}


/*
** Get the global table in the registry. Since all predefined
** indices in the registry were inserted right when the registry
** was created and never removed, they must always be in the array
** part of the registry.
*/



extern int lua_getglobal (lua_State *L, const char *name) {
  const TValue *G;
  ((void) 0);
  G = 	(&( check_exp((((&(L->l_G)->l_registry)->value_).gc)->tt == ((5) | (( 0) << 4)), &((((union GCUnion *)( (((&(L->l_G)->l_registry)->value_).gc))))->h)))->array[2 - 1]);
  return auxgetstr(L, G, name);
}


extern int lua_gettable (lua_State *L, int idx) {
  const TValue *slot;
  TValue *t;
  ((void) 0);
  t = index2value(L, idx);
  if (  (!(((( t))->tt_) == ( ((((5) | (( 0) << 4))) | (1 << 6))))     ? ( slot = ((void*)0), 0)       : ( slot =  luaH_get(( check_exp(((( t)->value_).gc)->tt == ((5) | (( 0) << 4)), &((((union GCUnion *)( ((( t)->value_).gc))))->h))),  (&(L->top.p - 1)->val)),          !((((((( slot))->tt_)) & 0x0F)) == ( 0))))  ) {
    	{ TValue *io1=((&( L->top.p - 1)->val)); const TValue *io2=( slot);           io1->value_ = io2->value_; ((io1)->tt_=( io2->tt_)); 	  	((void)L, ((void)0)); ((void)0); };
  }
  else
    luaV_finishget(L, t, (&(L->top.p - 1)->val), L->top.p - 1, slot);
  ((void) 0);
  return ((((((&(L->top.p - 1)->val))->tt_)) & 0x0F));
}


extern int lua_getfield (lua_State *L, int idx, const char *k) {
  ((void) 0);
  return auxgetstr(L, index2value(L, idx), k);
}


extern int lua_geti (lua_State *L, int idx, lua_Integer n) {
  TValue *t;
  const TValue *slot;
  ((void) 0);
  t = index2value(L, idx);
  if (  (!(((( t))->tt_) == ( ((((5) | (( 0) << 4))) | (1 << 6))))     ? ( slot = ((void*)0), 0)       : ( slot = (((lua_Unsigned)( n)) - 1u < ( check_exp(((( t)->value_).gc)->tt == ((5) | (( 0) << 4)), &((((union GCUnion *)( ((( t)->value_).gc))))->h)))->alimit)               ? &( check_exp(((( t)->value_).gc)->tt == ((5) | (( 0) << 4)), &((((union GCUnion *)( ((( t)->value_).gc))))->h)))->array[ n - 1] : luaH_getint(( check_exp(((( t)->value_).gc)->tt == ((5) | (( 0) << 4)), &((((union GCUnion *)( ((( t)->value_).gc))))->h))),  n),       !((((((( slot))->tt_)) & 0x0F)) == ( 0))))  ) {
    	{ TValue *io1=((&( L->top.p)->val)); const TValue *io2=( slot);           io1->value_ = io2->value_; ((io1)->tt_=( io2->tt_)); 	  	((void)L, ((void)0)); ((void)0); };
  }
  else {
    TValue aux;
      { TValue *io=(&aux); ((io)->value_).i=( n); ((io)->tt_=( ((3) | (( 0) << 4))  )); };
    luaV_finishget(L, t, &aux, L->top.p, slot);
  }
  {L->top.p++; 			 ((void)L, ((void)0));};
  ((void) 0);
  return ((((((&(L->top.p - 1)->val))->tt_)) & 0x0F));
}


static  int finishrawget (lua_State *L, const TValue *val) {
  if ((((((((val))->tt_)) & 0x0F)) == ( 0)))  /* avoid copying empty items to the stack */
    (((&(L->top.p)->val))->tt_=( ((0) | (( 0) << 4))));
  else
    	{ TValue *io1=((&( L->top.p)->val)); const TValue *io2=( val);           io1->value_ = io2->value_; ((io1)->tt_=( io2->tt_)); 	  	((void)L, ((void)0)); ((void)0); };
  {L->top.p++; 			 ((void)L, ((void)0));};
  ((void) 0);
  return ((((((&(L->top.p - 1)->val))->tt_)) & 0x0F));
}


static Table *gettable (lua_State *L, int idx) {
  TValue *t = index2value(L, idx);
  ((void)L, ((void)0));
  return ( check_exp((((t)->value_).gc)->tt == ((5) | (( 0) << 4)), &((((union GCUnion *)( (((t)->value_).gc))))->h)));
}


extern int lua_rawget (lua_State *L, int idx) {
  Table *t;
  const TValue *val;
  ((void) 0);
  	((void)L, ((void)0));
  t = gettable(L, idx);
  val = luaH_get(t, (&(L->top.p - 1)->val));
  L->top.p--;  /* remove key */
  return finishrawget(L, val);
}


extern int lua_rawgeti (lua_State *L, int idx, lua_Integer n) {
  Table *t;
  ((void) 0);
  t = gettable(L, idx);
  return finishrawget(L, luaH_getint(t, n));
}


extern int lua_rawgetp (lua_State *L, int idx, const void *p) {
  Table *t;
  TValue k;
  ((void) 0);
  t = gettable(L, idx);
    { TValue *io=(&k); ((io)->value_).p=( ((void *)( (p)))); ((io)->tt_=( ((2) | (( 0) << 4)))); };
  return finishrawget(L, luaH_get(t, &k));
}


extern void lua_createtable (lua_State *L, int narray, int nrec) {
  Table *t;
  ((void) 0);
  t = luaH_new(L);
    { TValue *io = ((&( L->top.p)->val)); Table *x_ = ( t);     ((io)->value_).gc = ( &(((union GCUnion *)( (x_)))->gc)); ((io)->tt_=( ((((5) | (( 0) << 4))) | (1 << 6))));     	((void)L, ((void)0)); };
  {L->top.p++; 			 ((void)L, ((void)0));};
  if (narray > 0 || nrec > 0)
    luaH_resize(L, t, narray, nrec);
  	{ if ((L->l_G)->GCdebt > 0) { (void)0; luaC_step(L); (void)0;}; 	  ((void)0); };
  ((void) 0);
}


extern int lua_getmetatable (lua_State *L, int objindex) {
  const TValue *obj;
  Table *mt;
  int res = 0;
  ((void) 0);
  obj = index2value(L, objindex);
  switch ((((((obj)->tt_)) & 0x0F))) {
    case 5:
      mt = ( check_exp((((obj)->value_).gc)->tt == ((5) | (( 0) << 4)), &((((union GCUnion *)( (((obj)->value_).gc))))->h)))->metatable;
      break;
    case 7:
      mt = ( check_exp((((obj)->value_).gc)->tt == ((7) | (( 0) << 4)), &((((union GCUnion *)( (((obj)->value_).gc))))->u)))->metatable;
      break;
    default:
      mt = (L->l_G)->mt[(((((obj)->tt_)) & 0x0F))];
      break;
  }
  if (mt != ((void*)0)) {
      { TValue *io = ((&( L->top.p)->val)); Table *x_ = ( mt);     ((io)->value_).gc = ( &(((union GCUnion *)( (x_)))->gc)); ((io)->tt_=( ((((5) | (( 0) << 4))) | (1 << 6))));     	((void)L, ((void)0)); };
    {L->top.p++; 			 ((void)L, ((void)0));};
    res = 1;
  }
  ((void) 0);
  return res;
}


extern int lua_getiuservalue (lua_State *L, int idx, int n) {
  TValue *o;
  int t;
  ((void) 0);
  o = index2value(L, idx);
  ((void)L, ((void)0));
  if (n <= 0 || n > ( check_exp((((o)->value_).gc)->tt == ((7) | (( 0) << 4)), &((((union GCUnion *)( (((o)->value_).gc))))->u)))->nuvalue) {
    (((&(L->top.p)->val))->tt_=( ((0) | (( 0) << 4))));
    t = (-1);
  }
  else {
    	{ TValue *io1=((&( L->top.p)->val)); const TValue *io2=( &( ( &((((union GCUnion *)( (((o)->value_).gc))))->u)))->uv[n - 1].uv);           io1->value_ = io2->value_; ((io1)->tt_=( io2->tt_)); 	  	((void)L, ((void)0)); ((void)0); };
    t = ((((((&(L->top.p)->val))->tt_)) & 0x0F));
  }
  {L->top.p++; 			 ((void)L, ((void)0));};
  ((void) 0);
  return t;
}


/*
** set functions (stack -> Lua)
*/

/*
** t[k] = value at the top of the stack (where 'k' is a string)
*/
static void auxsetstr (lua_State *L, const TValue *t, const char *k) {
  const TValue *slot;
  TString *str = luaS_new(L, k);
  	((void)L, ((void)0));
  if (  (!(((( t))->tt_) == ( ((((5) | (( 0) << 4))) | (1 << 6))))     ? ( slot = ((void*)0), 0)       : ( slot =  luaH_getstr(( check_exp(((( t)->value_).gc)->tt == ((5) | (( 0) << 4)), &((((union GCUnion *)( ((( t)->value_).gc))))->h))),  str),          !((((((( slot))->tt_)) & 0x0F)) == ( 0))))  ) {
        { 	{ TValue *io1=( ((TValue *)( slot))); const TValue *io2=(  (&(L->top.p - 1)->val));           io1->value_ = io2->value_; ((io1)->tt_=( io2->tt_)); 	  	((void)L, ((void)0)); ((void)0); };       (  	(((  (&(L->top.p - 1)->val))->tt_) & (1 << 6)) ? (  	((((  ( (( t)->value_).gc))->marked) & ( (1<<( 5  )))) && ((( ( ((  (&(L->top.p - 1)->val))->value_).gc))->marked) & ( ((1<<(3  )) | (1<<( 4  )))))) ? luaC_barrierback_(L,  ( (( t)->value_).gc)) : ((void)( (0)))) : ((void)( (0)))); };
    L->top.p--;  /* pop value */
  }
  else {
      { TValue *io = ((&( L->top.p)->val)); TString *x_ = ( str);     ((io)->value_).gc = ( &(((union GCUnion *)( (x_)))->gc)); ((io)->tt_=( ((x_->tt) | (1 << 6))));     	((void)L, ((void)0)); };  /* push 'str' (to make it a TValue) */
    {L->top.p++; 			 ((void)L, ((void)0));};
    luaV_finishset(L, t, (&(L->top.p - 1)->val), (&(L->top.p - 2)->val), slot);
    L->top.p -= 2;  /* pop value and key */
  }
  ((void) 0);  /* lock done by caller */
}


extern void lua_setglobal (lua_State *L, const char *name) {
  const TValue *G;
  ((void) 0);  /* unlock done in 'auxsetstr' */
  G = 	(&( check_exp((((&(L->l_G)->l_registry)->value_).gc)->tt == ((5) | (( 0) << 4)), &((((union GCUnion *)( (((&(L->l_G)->l_registry)->value_).gc))))->h)))->array[2 - 1]);
  auxsetstr(L, G, name);
}


extern void lua_settable (lua_State *L, int idx) {
  TValue *t;
  const TValue *slot;
  ((void) 0);
  	((void)L, ((void)0));
  t = index2value(L, idx);
  if (  (!(((( t))->tt_) == ( ((((5) | (( 0) << 4))) | (1 << 6))))     ? ( slot = ((void*)0), 0)       : ( slot =  luaH_get(( check_exp(((( t)->value_).gc)->tt == ((5) | (( 0) << 4)), &((((union GCUnion *)( ((( t)->value_).gc))))->h))),  (&(L->top.p - 2)->val)),          !((((((( slot))->tt_)) & 0x0F)) == ( 0))))  ) {
        { 	{ TValue *io1=( ((TValue *)( slot))); const TValue *io2=(  (&(L->top.p - 1)->val));           io1->value_ = io2->value_; ((io1)->tt_=( io2->tt_)); 	  	((void)L, ((void)0)); ((void)0); };       (  	(((  (&(L->top.p - 1)->val))->tt_) & (1 << 6)) ? (  	((((  ( (( t)->value_).gc))->marked) & ( (1<<( 5  )))) && ((( ( ((  (&(L->top.p - 1)->val))->value_).gc))->marked) & ( ((1<<(3  )) | (1<<( 4  )))))) ? luaC_barrierback_(L,  ( (( t)->value_).gc)) : ((void)( (0)))) : ((void)( (0)))); };
  }
  else
    luaV_finishset(L, t, (&(L->top.p - 2)->val), (&(L->top.p - 1)->val), slot);
  L->top.p -= 2;  /* pop index and value */
  ((void) 0);
}


extern void lua_setfield (lua_State *L, int idx, const char *k) {
  ((void) 0);  /* unlock done in 'auxsetstr' */
  auxsetstr(L, index2value(L, idx), k);
}


extern void lua_seti (lua_State *L, int idx, lua_Integer n) {
  TValue *t;
  const TValue *slot;
  ((void) 0);
  	((void)L, ((void)0));
  t = index2value(L, idx);
  if (  (!(((( t))->tt_) == ( ((((5) | (( 0) << 4))) | (1 << 6))))     ? ( slot = ((void*)0), 0)       : ( slot = (((lua_Unsigned)( n)) - 1u < ( check_exp(((( t)->value_).gc)->tt == ((5) | (( 0) << 4)), &((((union GCUnion *)( ((( t)->value_).gc))))->h)))->alimit)               ? &( check_exp(((( t)->value_).gc)->tt == ((5) | (( 0) << 4)), &((((union GCUnion *)( ((( t)->value_).gc))))->h)))->array[ n - 1] : luaH_getint(( check_exp(((( t)->value_).gc)->tt == ((5) | (( 0) << 4)), &((((union GCUnion *)( ((( t)->value_).gc))))->h))),  n),       !((((((( slot))->tt_)) & 0x0F)) == ( 0))))  ) {
        { 	{ TValue *io1=( ((TValue *)( slot))); const TValue *io2=(  (&(L->top.p - 1)->val));           io1->value_ = io2->value_; ((io1)->tt_=( io2->tt_)); 	  	((void)L, ((void)0)); ((void)0); };       (  	(((  (&(L->top.p - 1)->val))->tt_) & (1 << 6)) ? (  	((((  ( (( t)->value_).gc))->marked) & ( (1<<( 5  )))) && ((( ( ((  (&(L->top.p - 1)->val))->value_).gc))->marked) & ( ((1<<(3  )) | (1<<( 4  )))))) ? luaC_barrierback_(L,  ( (( t)->value_).gc)) : ((void)( (0)))) : ((void)( (0)))); };
  }
  else {
    TValue aux;
      { TValue *io=(&aux); ((io)->value_).i=( n); ((io)->tt_=( ((3) | (( 0) << 4))  )); };
    luaV_finishset(L, t, &aux, (&(L->top.p - 1)->val), slot);
  }
  L->top.p--;  /* pop value */
  ((void) 0);
}


static void aux_rawset (lua_State *L, int idx, TValue *key, int n) {
  Table *t;
  ((void) 0);
  	((void)L, ((void)0));
  t = gettable(L, idx);
  luaH_set(L, t, key, (&(L->top.p - 1)->val));
  ((t)->flags &= ~(~(~0u << (TM_EQ + 1))));
  (  	((( (&(L->top.p - 1)->val))->tt_) & (1 << 6)) ? (  	((((  ( &(((union GCUnion *)( (t)))->gc)))->marked) & ( (1<<( 5  )))) && ((( ( (( (&(L->top.p - 1)->val))->value_).gc))->marked) & ( ((1<<(3  )) | (1<<( 4  )))))) ? luaC_barrierback_(L,  ( &(((union GCUnion *)( (t)))->gc))) : ((void)( (0)))) : ((void)( (0))));
  L->top.p -= n;
  ((void) 0);
}


extern void lua_rawset (lua_State *L, int idx) {
  aux_rawset(L, idx, (&(L->top.p - 2)->val), 2);
}


extern void lua_rawsetp (lua_State *L, int idx, const void *p) {
  TValue k;
    { TValue *io=(&k); ((io)->value_).p=( ((void *)( (p)))); ((io)->tt_=( ((2) | (( 0) << 4)))); };
  aux_rawset(L, idx, &k, 1);
}


extern void lua_rawseti (lua_State *L, int idx, lua_Integer n) {
  Table *t;
  ((void) 0);
  	((void)L, ((void)0));
  t = gettable(L, idx);
  luaH_setint(L, t, n, (&(L->top.p - 1)->val));
  (  	((( (&(L->top.p - 1)->val))->tt_) & (1 << 6)) ? (  	((((  ( &(((union GCUnion *)( (t)))->gc)))->marked) & ( (1<<( 5  )))) && ((( ( (( (&(L->top.p - 1)->val))->value_).gc))->marked) & ( ((1<<(3  )) | (1<<( 4  )))))) ? luaC_barrierback_(L,  ( &(((union GCUnion *)( (t)))->gc))) : ((void)( (0)))) : ((void)( (0))));
  L->top.p--;
  ((void) 0);
}


extern int lua_setmetatable (lua_State *L, int objindex) {
  TValue *obj;
  Table *mt;
  ((void) 0);
  	((void)L, ((void)0));
  obj = index2value(L, objindex);
  if (((((((((&(L->top.p - 1)->val)))->tt_)) & 0x0F)) == ( 0)))
    mt = ((void*)0);
  else {
    ((void)L, ((void)0));
    mt = ( check_exp(((((&(L->top.p - 1)->val))->value_).gc)->tt == ((5) | (( 0) << 4)), &((((union GCUnion *)( ((((&(L->top.p - 1)->val))->value_).gc))))->h)));
  }
  switch ((((((obj)->tt_)) & 0x0F))) {
    case 5: {
      ( check_exp((((obj)->value_).gc)->tt == ((5) | (( 0) << 4)), &((((union GCUnion *)( (((obj)->value_).gc))))->h)))->metatable = mt;
      if (mt) {
        (  	(((( ( ((obj)->value_).gc))->marked) & ( (1<<( 5  )))) && ((( mt)->marked) & ( ((1<<(3  )) | (1<<( 4  )))))) ? 	luaC_barrier_(L,( &(((union GCUnion *)( ( ( ((obj)->value_).gc))))->gc)),( &(((union GCUnion *)( ( mt)))->gc))) : ((void)( (0))));
        luaC_checkfinalizer(L, ( ((obj)->value_).gc), mt);
      }
      break;
    }
    case 7: {
      ( check_exp((((obj)->value_).gc)->tt == ((7) | (( 0) << 4)), &((((union GCUnion *)( (((obj)->value_).gc))))->u)))->metatable = mt;
      if (mt) {
        (  	(((( ( ( &((((union GCUnion *)( (((obj)->value_).gc))))->u))))->marked) & ( (1<<( 5  )))) && ((( mt)->marked) & ( ((1<<(3  )) | (1<<( 4  )))))) ? 	luaC_barrier_(L,( &(((union GCUnion *)( ( ( ( &((((union GCUnion *)( (((obj)->value_).gc))))->u))))))->gc)),( &(((union GCUnion *)( ( mt)))->gc))) : ((void)( (0))));
        luaC_checkfinalizer(L, ( ((obj)->value_).gc), mt);
      }
      break;
    }
    default: {
      (L->l_G)->mt[(((((obj)->tt_)) & 0x0F))] = mt;
      break;
    }
  }
  L->top.p--;
  ((void) 0);
  return 1;
}


extern int lua_setiuservalue (lua_State *L, int idx, int n) {
  TValue *o;
  int res;
  ((void) 0);
  	((void)L, ((void)0));
  o = index2value(L, idx);
  ((void)L, ((void)0));
  if (!(((unsigned int)( (n))) - 1u < ((unsigned int)( (( ( &((((union GCUnion *)( (((o)->value_).gc))))->u)))->nuvalue)))))
    res = 0;  /* 'n' not in [1, uvalue(o)->nuvalue] */
  else {
    	{ TValue *io1=( &( ( &((((union GCUnion *)( (((o)->value_).gc))))->u)))->uv[n - 1].uv); const TValue *io2=( (&(L->top.p - 1)->val));           io1->value_ = io2->value_; ((io1)->tt_=( io2->tt_)); 	  	((void)L, ((void)0)); ((void)0); };
    (  	((( (&(L->top.p - 1)->val))->tt_) & (1 << 6)) ? (  	((((  ( ((o)->value_).gc))->marked) & ( (1<<( 5  )))) && ((( ( (( (&(L->top.p - 1)->val))->value_).gc))->marked) & ( ((1<<(3  )) | (1<<( 4  )))))) ? luaC_barrierback_(L,  ( ((o)->value_).gc)) : ((void)( (0)))) : ((void)( (0))));
    res = 1;
  }
  L->top.p--;
  ((void) 0);
  return res;
}


/*
** 'load' and 'call' functions (run Lua code)
*/





extern void lua_callk (lua_State *L, int nargs, int nresults,
                        lua_KContext ctx, lua_KFunction k) {
  StkId func;
  ((void) 0);
  ((void)L, ((void)0));
  	((void)L, ((void)0));
  ((void)L, ((void)0));
       ((void)L, ((void)0));
  func = L->top.p - (nargs+1);
  if (k != ((void*)0) && (((L)->nCcalls & 0xffff0000) == 0)) {  /* need to prepare continuation? */
    L->ci->u.c.k = k;  /* save continuation */
    L->ci->u.c.ctx = ctx;  /* save context */
    luaD_call(L, func, nresults);  /* do the call */
  }
  else  /* no continuation or no yieldable */
    luaD_callnoyield(L, func, nresults);  /* just do the call */
      { if (( nresults) <= (-1) && L->ci->top.p < L->top.p) 	L->ci->top.p = L->top.p; };
  ((void) 0);
}



/*
** Execute a protected call.
*/
struct CallS {  /* data to 'f_call' */
  StkId func;
  int nresults;
};


static void f_call (lua_State *L, void *ud) {
  struct CallS *c = ((struct CallS *)( ud));
  luaD_callnoyield(L, c->func, c->nresults);
}



extern int lua_pcallk (lua_State *L, int nargs, int nresults, int errfunc,
                        lua_KContext ctx, lua_KFunction k) {
  struct CallS c;
  int status;
  ptrdiff_t func;
  ((void) 0);
  ((void)L, ((void)0));
  	((void)L, ((void)0));
  ((void)L, ((void)0));
       ((void)L, ((void)0));
  if (errfunc == 0)
    func = 0;
  else {
    StkId o = index2stack(L, errfunc);
    ((void)L, ((void)0));
    func = (((char *)( ( o))) - ((char *)( (L->stack.p))));
  }
  c.func = L->top.p - (nargs+1);  /* function to be called */
  if (k == ((void*)0) || !(((L)->nCcalls & 0xffff0000) == 0)) {  /* no continuation or no yieldable? */
    c.nresults = nresults;  /* do a 'conventional' protected call */
    status = luaD_pcall(L, f_call, &c, (((char *)( ( c.func))) - ((char *)( (L->stack.p)))), func);
  }
  else {  /* prepare continuation (call is already protected by 'resume') */
    CallInfo *ci = L->ci;
    ci->u.c.k = k;  /* save continuation */
    ci->u.c.ctx = ctx;  /* save context */
    /* save information for error recovery */
    ci->u2.funcidx = ((int)( ((((char *)( ( c.func))) - ((char *)( (L->stack.p)))))));
    ci->u.c.old_errfunc = L->errfunc;
    L->errfunc = func;
    ((ci->callstatus) = ((ci->callstatus) & ~(1<<0)	) | ( L->allowhook));  /* save value of 'allowhook' */
    ci->callstatus |= (1<<4)	;  /* function can do error recovery */
    luaD_call(L, c.func, nresults);  /* do the call */
    ci->callstatus &= ~(1<<4)	;
    L->errfunc = ci->u.c.old_errfunc;
    status = 0;  /* if it is here, there were no errors */
  }
      { if (( nresults) <= (-1) && L->ci->top.p < L->top.p) 	L->ci->top.p = L->top.p; };
  ((void) 0);
  return status;
}


extern int lua_load (lua_State *L, lua_Reader reader, void *data,
                      const char *chunkname, const char *mode) {
  ZIO z;
  int status;
  ((void) 0);
  if (!chunkname) chunkname = "?";
  luaZ_init(L, &z, reader, data);
  status = luaD_protectedparser(L, &z, chunkname, mode);
  if (status == 0) {  /* no errors? */
    LClosure *f = ( check_exp(((((&(L->top.p - 1)->val))->value_).gc)->tt == ((6) | (( 0) << 4))  , &((((union GCUnion *)( ((((&(L->top.p - 1)->val))->value_).gc))))->cl.l)));  /* get new function */
    if (f->nupvalues >= 1) {  /* does it have an upvalue? */
      /* get global table from registry */
      const TValue *gt = 	(&( check_exp((((&(L->l_G)->l_registry)->value_).gc)->tt == ((5) | (( 0) << 4)), &((((union GCUnion *)( (((&(L->l_G)->l_registry)->value_).gc))))->h)))->array[2 - 1]);
      /* set global table as 1st upvalue of 'f' (may be LUA_ENV) */
      	{ TValue *io1=( f->upvals[0]->v.p); const TValue *io2=( gt);           io1->value_ = io2->value_; ((io1)->tt_=( io2->tt_)); 	  	((void)L, ((void)0)); ((void)0); };
      (  	((( gt)->tt_) & (1 << 6)) ? (  	(((( f->upvals[0])->marked) & ( (1<<( 5  )))) && (((( (( gt)->value_).gc))->marked) & ( ((1<<(3  )) | (1<<( 4  )))))) ? 	luaC_barrier_(L,( &(((union GCUnion *)( ( f->upvals[0])))->gc)),( &(((union GCUnion *)( (( (( gt)->value_).gc))))->gc))) : ((void)( (0)))) : ((void)( (0))));
    }
  }
  ((void) 0);
  return status;
}


extern int lua_dump (lua_State *L, lua_Writer writer, void *data, int strip) {
  int status;
  TValue *o;
  ((void) 0);
  	((void)L, ((void)0));
  o = (&(L->top.p - 1)->val);
  if (((((o))->tt_) == ( ((((6) | (( 0) << 4))  ) | (1 << 6)))))
    status = luaU_dump(L, (( check_exp((((o)->value_).gc)->tt == ((6) | (( 0) << 4))  , &((((union GCUnion *)( (((o)->value_).gc))))->cl.l)))->p), writer, data, strip);
  else
    status = 1;
  ((void) 0);
  return status;
}


extern int lua_status (lua_State *L) {
  return L->status;
}


/*
** Garbage-collection function
*/
extern int lua_gc (lua_State *L, int what, ...) {
  va_list argp;
  int res = 0;
  global_State *g = (L->l_G);
  if (g->gcstp & 2  )  /* internal stop? */
    return -1;  /* all options are invalid when stopped */
  ((void) 0);
  va_start(argp, what);
  switch (what) {
    case 0: {
      g->gcstp = 1  ;  /* stopped by the user */
      break;
    }
    case 1: {
      luaE_setdebt(g, 0);
      g->gcstp = 0;  /* (GCSTPGC must be already zero here) */
      break;
    }
    case 2: {
      luaC_fullgc(L, 0);
      break;
    }
    case 3: {
      /* GC values are expressed in Kbytes: #bytes/2^10 */
      res = ((int)( (((lu_mem)( (g)->totalbytes + (g)->GCdebt)) >> 10)));
      break;
    }
    case 4: {
      res = ((int)( (((lu_mem)( (g)->totalbytes + (g)->GCdebt)) & 0x3ff)));
      break;
    }
    case 5: {
      int data = va_arg(argp, int);
      l_mem debt = 1;  /* =1 to signal that it did an actual step */
      lu_byte oldstp = g->gcstp;
      g->gcstp = 0;  /* allow GC to run (GCSTPGC must be zero here) */
      if (data == 0) {
        luaE_setdebt(g, 0);  /* do a basic step */
        luaC_step(L);
      }
      else {  /* add 'data' to total debt */
        debt = ((l_mem)( data)) * 1024 + g->GCdebt;
        luaE_setdebt(g, debt);
        	{ if ((L->l_G)->GCdebt > 0) { (void)0; luaC_step(L); (void)0;}; 	  ((void)0); };
      }
      g->gcstp = oldstp;  /* restore previous state */
      if (debt > 0 && g->gcstate == 8)  /* end of cycle? */
        res = 1;  /* signal it */
      break;
    }
    case 6: {
      int data = va_arg(argp, int);
      res = ((g->gcpause) * 4);
      ((g->gcpause) = ( data) / 4);
      break;
    }
    case 7: {
      int data = va_arg(argp, int);
      res = ((g->gcstepmul) * 4);
      ((g->gcstepmul) = ( data) / 4);
      break;
    }
    case 9: {
      res = ((g)->gcstp == 0);
      break;
    }
    case 10: {
      int minormul = va_arg(argp, int);
      int majormul = va_arg(argp, int);
      res = (g->gckind == 1	 || g->lastatomic != 0) ? 10 : 11;
      if (minormul != 0)
        g->genminormul = minormul;
      if (majormul != 0)
        ((g->genmajormul) = ( majormul) / 4);
      luaC_changemode(L, 1	);
      break;
    }
    case 11: {
      int pause = va_arg(argp, int);
      int stepmul = va_arg(argp, int);
      int stepsize = va_arg(argp, int);
      res = (g->gckind == 1	 || g->lastatomic != 0) ? 10 : 11;
      if (pause != 0)
        ((g->gcpause) = ( pause) / 4);
      if (stepmul != 0)
        ((g->gcstepmul) = ( stepmul) / 4);
      if (stepsize != 0)
        g->gcstepsize = stepsize;
      luaC_changemode(L, 0	);
      break;
    }
    default: res = -1;  /* invalid option */
  }
  va_end(argp);
  ((void) 0);
  return res;
}



/*
** miscellaneous functions
*/


extern int lua_error (lua_State *L) {
  TValue *errobj;
  ((void) 0);
  errobj = (&(L->top.p - 1)->val);
  	((void)L, ((void)0));
  /* error object is the memory error message? */
  if (((((errobj))->tt_) == ( ((((4) | (( 0) << 4))  ) | (1 << 6)))) && ( (( 	check_exp((((((errobj)->value_).gc)->tt) & 0x0F) == 4, &((((union GCUnion *)( (((errobj)->value_).gc))))->ts)))) == ( (L->l_G)->memerrmsg)))
    luaD_throw(L, 4);  /* raise a memory error */
  else
    luaG_errormsg(L);  /* raise a regular error */
  /* code unreachable; will unlock when control actually leaves the kernel */
  return 0;  /* to avoid warnings */
}


extern int lua_next (lua_State *L, int idx) {
  Table *t;
  int more;
  ((void) 0);
  	((void)L, ((void)0));
  t = gettable(L, idx);
  more = luaH_next(L, t, L->top.p - 1);
  if (more) {
    {L->top.p++; 			 ((void)L, ((void)0));};
  }
  else  /* no more elements */
    L->top.p -= 1;  /* remove key */
  ((void) 0);
  return more;
}


extern void lua_toclose (lua_State *L, int idx) {
  int nresults;
  StkId o;
  ((void) 0);
  o = index2stack(L, idx);
  nresults = L->ci->nresults;
  ((void)L, ((void)0));
  luaF_newtbcupval(L, o);  /* create new to-be-closed upvalue */
  if (!((nresults) < (-1)))  /* function not marked yet? */
    L->ci->nresults = (-(nresults) - 3);  /* mark it */
  ((void)0);
  ((void) 0);
}


extern void lua_concat (lua_State *L, int n) {
  ((void) 0);
  	((void)L, ((void)0));
  if (n > 0)
    luaV_concat(L, n);
  else {  /* nothing to concatenate */
      { TValue *io = ((&( L->top.p)->val)); TString *x_ = ( luaS_newlstr(L, "", 0));     ((io)->value_).gc = ( &(((union GCUnion *)( (x_)))->gc)); ((io)->tt_=( ((x_->tt) | (1 << 6))));     	((void)L, ((void)0)); };  /* push empty string */
    {L->top.p++; 			 ((void)L, ((void)0));};
  }
  	{ if ((L->l_G)->GCdebt > 0) { (void)0; luaC_step(L); (void)0;}; 	  ((void)0); };
  ((void) 0);
}


extern void lua_len (lua_State *L, int idx) {
  TValue *t;
  ((void) 0);
  t = index2value(L, idx);
  luaV_objlen(L, L->top.p, t);
  {L->top.p++; 			 ((void)L, ((void)0));};
  ((void) 0);
}


extern lua_Alloc lua_getallocf (lua_State *L, void **ud) {
  lua_Alloc f;
  ((void) 0);
  if (ud) *ud = (L->l_G)->ud;
  f = (L->l_G)->frealloc;
  ((void) 0);
  return f;
}


extern void lua_setallocf (lua_State *L, lua_Alloc f, void *ud) {
  ((void) 0);
  (L->l_G)->ud = ud;
  (L->l_G)->frealloc = f;
  ((void) 0);
}


void lua_setwarnf (lua_State *L, lua_WarnFunction f, void *ud) {
  ((void) 0);
  (L->l_G)->ud_warn = ud;
  (L->l_G)->warnf = f;
  ((void) 0);
}


void lua_warning (lua_State *L, const char *msg, int tocont) {
  ((void) 0);
  luaE_warning(L, msg, tocont);
  ((void) 0);
}



extern void *lua_newuserdatauv (lua_State *L, size_t size, int nuvalue) {
  Udata *u;
  ((void) 0);
  ((void)L, ((void)0));
  u = luaS_newudata(L, size, nuvalue);
    { TValue *io = ( (&(L->top.p)->val)); Udata *x_ = ( u);     ((io)->value_).gc = ( &(((union GCUnion *)( (x_)))->gc)); ((io)->tt_=( ((((7) | (( 0) << 4))) | (1 << 6))));     	((void)L, ((void)0)); };
  {L->top.p++; 			 ((void)L, ((void)0));};
  	{ if ((L->l_G)->GCdebt > 0) { (void)0; luaC_step(L); (void)0;}; 	  ((void)0); };
  ((void) 0);
  return (((char *)( (u))) + 	(((u)->nuvalue) == 0 ? ((unsigned long)&(((Udata0*)0)-> bindata))                      : ((unsigned long)&(((Udata*)0)-> uv)) + (sizeof(UValue) * ((u)->nuvalue))));
}



static const char *aux_upvalue (TValue *fi, int n, TValue **val,
                                GCObject **owner) {
  switch (((((fi)->tt_)) & 0x3F)) {
    case ((6) | (( 2) << 4))  : {  /* C closure */
      CClosure *f = ( check_exp((((fi)->value_).gc)->tt == ((6) | (( 2) << 4))  , &((((union GCUnion *)( (((fi)->value_).gc))))->cl.c)));
      if (!(((unsigned int)( (n))) - 1u < ((unsigned int)( (f->nupvalues)))))
        return ((void*)0);  /* 'n' not in [1, f->nupvalues] */
      *val = &f->upvalue[n-1];
      if (owner) *owner = ( &(((union GCUnion *)( (f)))->gc));
      return "";
    }
    case ((6) | (( 0) << 4))  : {  /* Lua closure */
      LClosure *f = ( check_exp((((fi)->value_).gc)->tt == ((6) | (( 0) << 4))  , &((((union GCUnion *)( (((fi)->value_).gc))))->cl.l)));
      TString *name;
      Proto *p = f->p;
      if (!(((unsigned int)( (n))) - 1u  < ((unsigned int)( (p->sizeupvalues)))))
        return ((void*)0);  /* 'n' not in [1, p->sizeupvalues] */
      *val = f->upvals[n-1]->v.p;
      if (owner) *owner = ( &(((union GCUnion *)( (f->upvals[n - 1])))->gc));
      name = p->upvalues[n-1].name;
      return (name == ((void*)0)) ? "(no name)" : ((name)->contents);
    }
    default: return ((void*)0);  /* not a closure */
  }
}


extern const char *lua_getupvalue (lua_State *L, int funcindex, int n) {
  const char *name;
  TValue *val = ((void*)0);  /* to avoid warnings */
  ((void) 0);
  name = aux_upvalue(index2value(L, funcindex), n, &val, ((void*)0));
  if (name) {
    	{ TValue *io1=((&( L->top.p)->val)); const TValue *io2=( val);           io1->value_ = io2->value_; ((io1)->tt_=( io2->tt_)); 	  	((void)L, ((void)0)); ((void)0); };
    {L->top.p++; 			 ((void)L, ((void)0));};
  }
  ((void) 0);
  return name;
}


extern const char *lua_setupvalue (lua_State *L, int funcindex, int n) {
  const char *name;
  TValue *val = ((void*)0);  /* to avoid warnings */
  GCObject *owner = ((void*)0);  /* to avoid warnings */
  TValue *fi;
  ((void) 0);
  fi = index2value(L, funcindex);
  	((void)L, ((void)0));
  name = aux_upvalue(fi, n, &val, &owner);
  if (name) {
    L->top.p--;
    	{ TValue *io1=( val); const TValue *io2=( (&(L->top.p)->val));           io1->value_ = io2->value_; ((io1)->tt_=( io2->tt_)); 	  	((void)L, ((void)0)); ((void)0); };
    (  	((( val)->tt_) & (1 << 6)) ? (  	(((( owner)->marked) & ( (1<<( 5  )))) && (((( (( val)->value_).gc))->marked) & ( ((1<<(3  )) | (1<<( 4  )))))) ? 	luaC_barrier_(L,( &(((union GCUnion *)( ( owner)))->gc)),( &(((union GCUnion *)( (( (( val)->value_).gc))))->gc))) : ((void)( (0)))) : ((void)( (0))));
  }
  ((void) 0);
  return name;
}


static UpVal **getupvalref (lua_State *L, int fidx, int n, LClosure **pf) {
  static const UpVal *const nullup = ((void*)0);
  LClosure *f;
  TValue *fi = index2value(L, fidx);
  ((void)L, ((void)0));
  f = ( check_exp((((fi)->value_).gc)->tt == ((6) | (( 0) << 4))  , &((((union GCUnion *)( (((fi)->value_).gc))))->cl.l)));
  if (pf) *pf = f;
  if (1 <= n && n <= f->p->sizeupvalues)
    return &f->upvals[n - 1];  /* get its upvalue pointer */
  else
    return (UpVal**)&nullup;
}


extern void *lua_upvalueid (lua_State *L, int fidx, int n) {
  TValue *fi = index2value(L, fidx);
  switch (((((fi)->tt_)) & 0x3F)) {
    case ((6) | (( 0) << 4))  : {  /* lua closure */
      return *getupvalref(L, fidx, n, ((void*)0));
    }
    case ((6) | (( 2) << 4))  : {  /* C closure */
      CClosure *f = ( check_exp((((fi)->value_).gc)->tt == ((6) | (( 2) << 4))  , &((((union GCUnion *)( (((fi)->value_).gc))))->cl.c)));
      if (1 <= n && n <= f->nupvalues)
        return &f->upvalue[n - 1];
      /* else */
    }  /* FALLTHROUGH */
    case ((6) | (( 1) << 4))  :
      return ((void*)0);  /* light C functions have no upvalues */
    default: {
      ((void)L, ((void)0));
      return ((void*)0);
    }
  }
}


extern void lua_upvaluejoin (lua_State *L, int fidx1, int n1,
                                            int fidx2, int n2) {
  LClosure *f1;
  UpVal **up1 = getupvalref(L, fidx1, n1, &f1);
  UpVal **up2 = getupvalref(L, fidx2, n2, ((void*)0));
  ((void)L, ((void)0));
  *up1 = *up2;
  (  	(((( f1)->marked) & ( (1<<( 5  )))) && ((( *up1)->marked) & ( ((1<<(3  )) | (1<<( 4  )))))) ? 	luaC_barrier_(L,( &(((union GCUnion *)( ( f1)))->gc)),( &(((union GCUnion *)( ( *up1)))->gc))) : ((void)( (0))));
}


