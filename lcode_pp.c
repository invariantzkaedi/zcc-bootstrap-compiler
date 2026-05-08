/*
** $Id: lcode.c $
** Code generator for Lua
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










































double sin(double);
double cos(double);
double atan2(double, double);
double sqrt(double);
double floor(double);
double ceil(double);
double fabs(double);
double ldexp(double, int);
double pow(double, double);
double frexp(double, int *);
double fmod(double, double);
double tan(double);
double asin(double);
double acos(double);
double log2(double);
double log10(double);


typedef unsigned long size_t;

void* malloc(size_t);
void free(void*);
void* calloc(size_t, size_t);
void* realloc(void*, size_t);
int atoi(const char*);
void exit(int);

int rand(void);
int abs(int);
void abort(void);
long strtol(const char*, char**, int);
unsigned long strtoul(const char*, char**, int);
unsigned long long strtoull(const char*, char**, int);
char* getenv(const char*);


int mkstemp(char *template);


/*
** $Id: lua.h $
** Lua - A Scripting Language
** Lua.org, PUC-Rio, Brazil (www.lua.org)
** See Copyright Notice at the end of this file
*/





typedef void* va_list;







typedef unsigned long size_t;



















/*
** $Id: luaconf.h $
** Configuration file for Lua
** See Copyright Notice in lua.h
*/














typedef unsigned long size_t;







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
** When POSIX DLL ('LUA_USE_DLOPEN') is enabled, the Lua stand-alone
** application will try to dynamically link a 'readline' facility
** for its REPL.  In that case, LUA_READLINELIB is the name of the
** library it will look for those facilities.  If lua.c cannot open
** the specified library, it will generate a warning and then run
** without 'readline'.  If that macro is not defined, lua.c will not
** use 'readline'.
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
/* #define LUA_32BITS */


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











/*
** LUA_IGMARK is a mark to ignore all after it when building the
** module name (e.g., used to build the luaopen_ function name).
** Typically, the suffix after the mark is the module version,
** as in "mod-v1.2.so".
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









/* }================================================================== */


/*
** {==================================================================
** Compatibility with previous versions
** ===================================================================
*/

/*
@@ LUA_COMPAT_GLOBAL avoids 'global' being a reserved word
*/





/*
@@ LUA_COMPAT_LOOPVAR makes for-loop control variables not read-only,
** as they were in previous versions.
*/





/*
@@ LUA_COMPAT_MATHLIB controls the presence of several deprecated
** functions in the mathematical library.
** (These functions were already officially removed in 5.3;
** nevertheless they are still available here.)
*/
/* #define LUA_COMPAT_MATHLIB */


/*
@@ The following macros supply trivial compatibility for some
** changes in the API. The macros themselves document how to
** change your code to avoid using them.
** (Once more, these macros were officially removed in 5.3, but they are
** still available here.)
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
@@ LUA_NUMBER_FMT is the format for writing floats with the maximum
** number of digits that respects tostring(tonumber(numeral)) == numeral.
** (That would be floor(log10(2^n)), where n is the number of bits in
** the float mantissa.)
@@ LUA_NUMBER_FMT_N is the format for writing floats with the minimum
** number of digits that ensures tonumber(tostring(number)) == number.
** (That would be LUA_NUMBER_FMT+2.)
@@ l_mathop allows the addition of an 'l' or 'f' to all math operations.
@@ l_floor takes the floor of a float.
@@ lua_str2number converts a decimal numeral to a number.
*/


/* The following definition is good for most cases here */




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

/* use ISO C99 stuff */
































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
/* #define LUA_USE_APICHECK */

/* }================================================================== */


/*
** {==================================================================
** Macros that affect the API and must be stable (that is, must be the
** same when you compile Lua and when you compile code that links to
** Lua).
** =====================================================================
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
@@ LUAI_MAXALIGN defines fields that ensure proper alignment for
** memory areas offered by Lua (e.g., userdata memory).
** Add fields to it if you need alignment for non-ISO objects.
*/

/* use ISO C99 stuff */






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
** (The stack size is limited to INT_MAX/2; we keep some free empty
** space after that to help overflow detection.)
*/




/* thread status */








typedef struct lua_State lua_State;


/*
** basic types
*/
















/* minimum Lua stack available to a C function */



/* predefined values in the registry */
/* index 1 is reserved for the reference mechanism */





/* type of numbers in Lua */
typedef double lua_Number;


/* type for integer functions */
typedef long long lua_Integer;

/* unsigned integer type */
typedef unsigned long long lua_Unsigned;

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
extern lua_State *(lua_newstate) (lua_Alloc f, void *ud, unsigned seed);
extern void       (lua_close) (lua_State *L);
extern lua_State *(lua_newthread) (lua_State *L);
extern int        (lua_closethread) (lua_State *L, lua_State *from);

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
extern const char *(lua_pushexternalstring) (lua_State *L,
		const char *s, size_t len, lua_Alloc falloc, void *ud);
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
** garbage-collection options
*/













/*
** garbage-collection parameters
*/
/* parameters for generational mode */




/* parameters for incremental mode */




/* number of parameters */



extern int (lua_gc) (lua_State *L, int what, ...);


/*
** miscellaneous functions
*/

extern int   (lua_error) (lua_State *L);

extern int   (lua_next) (lua_State *L, int idx);

extern void  (lua_concat) (lua_State *L, int n);
extern void  (lua_len)    (lua_State *L, int idx);


extern unsigned  (lua_numbertocstring) (lua_State *L, int idx, char *buff);
extern size_t  (lua_stringtonumber) (lua_State *L, const char *s);

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
  unsigned char extraargs;  /* (t) number of extra arguments */
  char istailcall;	/* (t) */
  int ftransfer;   /* (r) index of first value transferred */
  int ntransfer;   /* (r) number of transferred values */
  char short_src[60]; /* (S) */
  /* private part */
  struct CallInfo *i_ci;  /* active function */
};

/* }====================================================================== */













/******************************************************************************
* Copyright (C) 1994-2026 Lua.org, PUC-Rio.
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
** $Id: lcode.h $
** Code generator for Lua
** See Copyright Notice in lua.h
*/




/*
** $Id: llex.h $
** Lexical Analyzer
** See Copyright Notice in lua.h
*/











/*
** $Id: lobject.h $
** Type definitions for Lua objects
** See Copyright Notice in lua.h
*/






typedef void* va_list;






/*
** $Id: llimits.h $
** Limits, basic types, and some other 'installation-dependent' definitions
** See Copyright Notice in lua.h
*/














typedef unsigned long size_t;







/*
** $Id: lua.h $
** Lua - A Scripting Language
** Lua.org, PUC-Rio, Brazil (www.lua.org)
** See Copyright Notice at the end of this file
*/










































































































































































































































































































































































































































































/*
** 'l_mem' is a signed integer big enough to count the total memory
** used by Lua.  (It is signed due to the use of debt in several
** computations.) 'lu_mem' is a corresponding unsigned type.  Usually,
** 'ptrdiff_t' should work, but we use 'long' for 16-bit machines.
*/




typedef ptrdiff_t l_mem;
typedef size_t lu_mem;








/* chars used as small naturals (so that 'char' is reserved for characters) */
typedef unsigned char lu_byte;
typedef signed char ls_byte;


/* Type for thread status/error codes */
typedef lu_byte TStatus;

/* The C API still uses 'int' for status/error codes */


/* maximum value for size_t */


/*
** Maximum size for strings and userdata visible for Lua; should be
** representable as a lua_Integer and as a size_t.
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
typedef long long l_uacInt;


/*
** Internal assertions for in-house debugging
*/














/* to avoid problems with conditions too long */



/* macro to avoid warnings about unused variables */





/* type casts (a macro highlights casts in the code) */

















/* cast a signed lua_Integer to lua_Unsigned */




/*
** cast a lua_Unsigned to a signed lua_Integer; this cast is
** not strict ISO C, but two-complement architectures should
** work fine.
*/




/*
** cast a size_t to lua_Integer: These casts are always valid for
** sizes of Lua objects (see MAX_SIZE)
*/


/* Cast a ptrdiff_t to size_t, when it is known that the minuend
** comes from the subtrahend (the base)
*/


/* ptrdiff_t to lua_Integer */


/*
** Special type equivalent to '(void*)' for functions (to suppress some
** warnings when converting function pointers)
*/
typedef void (*voidf)(void);

/*
** Macro to convert pointer-to-void* to pointer-to-function. This cast
** is undefined according to ISO C, but POSIX assumes that it works.
** (The '__extension__' in gnu compilers is only to avoid warnings.)
*/








/*
** non-return type
*/













/*
** Inline functions
*/











/*
** An unsigned with (at least) 4 bytes
*/

typedef unsigned int l_uint32;





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
** lua_numbertointeger converts a float number with an integral value
** to an integer, or returns 0 if the float is not within the range of
** a lua_Integer.  (The range comparisons are tricky because of
** rounding. The tests here assume a two-complement representation,
** where MININTEGER always has an exact representation as a float;
** MAXINTEGER may not have one, and therefore its conversion to float
** may have an ill-defined value.)
*/




/*
** LUAI_FUNC is a mark for all extern functions that are not to be
** exported to outside modules.
** LUAI_DDEF and LUAI_DDEC are marks for all extern (const) variables,
** none of which to be exported to outside modules (LUAI_DDEF for
** definitions and LUAI_DDEC for declarations).
** Elf and MACH/gcc (versions 3.2 and later) mark them as "hidden" to
** optimize access when Lua is compiled as a shared library. Not all elf
** targets support this attribute. Unfortunately, gcc does not offer
** a way to check whether the target offers that support, and those
** without support give a warning about it. To avoid these warnings,
** change to the default definition.
*/














/* Give these macros simpler names for internal use */



/*
** {==================================================================
** "Abstraction Layer" for basic report of messages and errors
** ===================================================================
*/

/* print a string */




/* print a newline and flush the output */




/* print an error message */




/* }================================================================== */




/*
** $Id: lua.h $
** Lua - A Scripting Language
** Lua.org, PUC-Rio, Brazil (www.lua.org)
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


/* Special variant to signal that a fast get is accessing a non-table */



/* macro to test for (any kind of) nil */


/*
** Macro to test the result of a table access. Formally, it should
** distinguish between LUA_VEMPTY/LUA_VABSTKEY/LUA_VNOTABLE and
** other tags. As currently nil is equivalent to LUA_VEMPTY, it is
** simpler to just test whether the value is nil.
*/



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



/* Kinds of long strings (stored in 'shrlen') */





/*
** Header for a string value.
*/
typedef struct TString {
  struct GCObject *next; lu_byte tt; lu_byte marked;
  lu_byte extra;  /* reserved words for short strings; "has hash" for longs */
  ls_byte shrlen;  /* length for short strings, negative for long strings */
  unsigned int hash;
  union {
    size_t lnglen;  /* length for long strings */
    struct TString *hnext;  /* linked list for hash table */
  } u;
  char *contents;  /* pointer to content in long strings */
  lua_Alloc falloc;  /* deallocation function for external strings */
  void *ud;  /* user data for external strings */
} TString;






/*
** Get the actual string (array of bytes) from a 'TString'. (Generic
** version and specialized versions for long and short strings.)
*/






/* get string length from 'TString *ts' */


/*
** Get string and length */


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
  long double u; void *s; long long l;  /* ensures maximum alignment for udata bytes */
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
  union {long double u; void *s; long long l;} bindata;
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




typedef l_uint32 Instruction;


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
** Flags in Prototypes
*/




/* a vararg function either has hidden args. or a vararg table */


/*
** mark that a function needs a vararg table. (The flag PF_VAHID will
** be cleared later.)
*/


/*
** Function Prototypes
*/
typedef struct Proto {
  struct GCObject *next; lu_byte tt; lu_byte marked;
  lu_byte numparams;  /* number of fixed (named) parameters */
  lu_byte flag;
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




typedef struct Table {
  struct GCObject *next; lu_byte tt; lu_byte marked;
  lu_byte flags;  /* 1<<p means tagmethod(p) is not present */
  lu_byte lsizenode;  /* log2 of number of slots of 'node' array */
  unsigned int asize;  /* number of slots in 'array' array */
  Value *array;  /* array part */
  Node *node;
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



/* macro to call 'luaO_pushvfstring' correctly */



extern int luaO_utf8esc (char *buff, l_uint32 x);
extern lu_byte luaO_ceillog2 (unsigned int x);
extern lu_byte luaO_codeparam (unsigned int p);
extern l_mem luaO_applyparam (lu_byte p, l_mem x);

extern int luaO_rawarith (lua_State *L, int op, const TValue *p1,
                             const TValue *p2, TValue *res);
extern void luaO_arith (lua_State *L, int op, const TValue *p1,
                           const TValue *p2, StkId res);
extern size_t luaO_str2num (const char *s, TValue *o);
extern unsigned luaO_tostringbuff (const TValue *obj, char *buff);
extern lu_byte luaO_hexavalue (int c);
extern void luaO_tostring (lua_State *L, TValue *obj);
extern const char *luaO_pushvfstring (lua_State *L, const char *fmt,
                                                       va_list argp);
extern const char *luaO_pushfstring (lua_State *L, const char *fmt, ...);
extern void luaO_chunkid (char *out, const char *source, size_t srclen);





/*
** $Id: lzio.h $
** Buffered streams
** See Copyright Notice in lua.h
*/





/*
** $Id: lua.h $
** Lua - A Scripting Language
** Lua.org, PUC-Rio, Brazil (www.lua.org)
** See Copyright Notice at the end of this file
*/







































































































































































































































































































































































































































































/*
** $Id: lmem.h $
** Interface to Memory Manager
** See Copyright Notice in lua.h
*/








typedef unsigned long size_t;






/*
** $Id: llimits.h $
** Limits, basic types, and some other 'installation-dependent' definitions
** See Copyright Notice in lua.h
*/











































































































































































































































































/*
** $Id: lua.h $
** Lua - A Scripting Language
** Lua.org, PUC-Rio, Brazil (www.lua.org)
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
** 'int' and that 'int' is not larger than 'size_t'.)
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
                               int *size, unsigned size_elem, int limit,
                               const char *what);
extern void *luaM_shrinkvector_ (lua_State *L, void *block, int *nelem,
                                    int final_n, unsigned size_elem);
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

extern const void *luaZ_getaddr (ZIO* z, size_t n);


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
** Single-char tokens (terminal symbols) are represented by their own
** numeric code. Other tokens start at the following value.
*/








/*
* WARNING: if you change the order of this enumeration,
* grep "ORDER RESERVED"
*/
enum RESERVED {
  /* terminal symbols denoted by reserved words */
  TK_AND = (255 + 1), TK_BREAK,
  TK_DO, TK_ELSE, TK_ELSEIF, TK_END, TK_FALSE, TK_FOR, TK_FUNCTION,
  TK_GLOBAL, TK_GOTO, TK_IF, TK_IN, TK_LOCAL, TK_NIL, TK_NOT, TK_OR,
  TK_REPEAT, TK_RETURN, TK_THEN, TK_TRUE, TK_UNTIL, TK_WHILE,
  /* other terminal symbols */
  TK_IDIV, TK_CONCAT, TK_DOTS, TK_EQ, TK_GE, TK_LE, TK_NE,
  TK_SHL, TK_SHR,
  TK_DBCOLON, TK_EOS,
  TK_FLT, TK_INT, TK_NAME, TK_STRING
};

/* number of reserved words */



typedef union {
  lua_Number r;
  lua_Integer i;
  TString *ts;
} SemInfo;  /* semantics information */


typedef struct Token {
  int token;
  SemInfo seminfo;
} Token;


/* state of the scanner plus state of the parser when shared by all
   functions */
typedef struct LexState {
  int current;  /* current character (charint) */
  int linenumber;  /* input line counter */
  int lastline;  /* line of last token 'consumed' */
  Token t;  /* current token */
  Token lookahead;  /* look ahead token */
  struct FuncState *fs;  /* current function (parser) */
  struct lua_State *L;
  ZIO *z;  /* input stream */
  Mbuffer *buff;  /* buffer for tokens */
  Table *h;  /* to avoid collection/reuse strings */
  struct Dyndata *dyd;  /* dynamic structures used by the parser */
  TString *source;  /* current source name */
  TString *envn;  /* environment variable name */
  TString *brkn;  /* "break" name (used as a label) */
  TString *glbn;  /* "global" name (when not a reserved word) */
} LexState;


extern void luaX_init (lua_State *L);
extern void luaX_setinput (lua_State *L, LexState *ls, ZIO *z,
                              TString *source, int firstchar);
extern TString *luaX_newstring (LexState *ls, const char *str, size_t l);
extern void luaX_next (LexState *ls);
extern int luaX_lookahead (LexState *ls);
extern void  luaX_syntaxerror (LexState *ls, const char *s);
extern const char *luaX_token2str (LexState *ls, int token);




/*
** $Id: lobject.h $
** Type definitions for Lua objects
** See Copyright Notice in lua.h
*/



























































































































































































































































































































































































































































































































































































































































































/*
** $Id: lopcodes.h $
** Opcodes for Lua virtual machine
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





























































































































































































































































































































































































































































































































































































































































































/*===========================================================================
  We assume that instructions are unsigned 32-bit integers.
  All instructions have an opcode in the first 7 bits.
  Instructions can have the following formats:

        3 3 2 2 2 2 2 2 2 2 2 2 1 1 1 1 1 1 1 1 1 1 0 0 0 0 0 0 0 0 0 0
        1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0 9 8 7 6 5 4 3 2 1 0
iABC          C(8)     |      B(8)     |k|     A(8)      |   Op(7)     |
ivABC         vC(10)     |     vB(6)   |k|     A(8)      |   Op(7)     |
iABx                Bx(17)               |     A(8)      |   Op(7)     |
iAsBx              sBx (signed)(17)      |     A(8)      |   Op(7)     |
iAx                           Ax(25)                     |   Op(7)     |
isJ                           sJ (signed)(25)            |   Op(7)     |

  ('v' stands for "variant", 's' for "signed", 'x' for "extended".)
  A signed argument is represented in excess K: The represented value is
  the written unsigned value minus K, where K is half (rounded down) the
  maximum value for the corresponding unsigned argument.
===========================================================================*/


/* basic instruction formats */
enum OpMode {iABC, ivABC, iABx, iAsBx, iAx, isJ};


/*
** size and position of opcode arguments.
*/



























/*
** limits for opcode arguments.
** we use (signed) 'int' to manipulate most arguments,
** so they must fit in ints.
*/

/*
** Check whether type 'int' has at least 'b' + 1 bits.
** 'b' < 32; +1 for the sign bit.
*/






































/* creates a mask with 'n' 1 bits at position 'p' */


/* creates a mask with 'n' 0 bits at position 'p' */


/*
** the following macros help to manipulate instructions
*/


























































/*
** Maximum size for the stack of a Lua function. It must fit in 8 bits.
** The highest valid register is one less than this value.
*/


/*
** Invalid register (one more than last valid register).
*/




/*
** R[x] - register
** K[x] - constant (in constant table)
** RK(x) == if k(i) then K[x] else R[x]
*/


/*
** Grep "ORDER OP" if you change this enum.
** See "Notes" below for more information about some instructions.
*/

typedef enum {
/*----------------------------------------------------------------------
  name		args	description
------------------------------------------------------------------------*/
OP_MOVE,/*	A B	R[A] := R[B]					*/
OP_LOADI,/*	A sBx	R[A] := sBx					*/
OP_LOADF,/*	A sBx	R[A] := (lua_Number)sBx				*/
OP_LOADK,/*	A Bx	R[A] := K[Bx]					*/
OP_LOADKX,/*	A	R[A] := K[extra arg]				*/
OP_LOADFALSE,/*	A	R[A] := false					*/
OP_LFALSESKIP,/*A	R[A] := false; pc++				*/
OP_LOADTRUE,/*	A	R[A] := true					*/
OP_LOADNIL,/*	A B	R[A], R[A+1], ..., R[A+B] := nil		*/
OP_GETUPVAL,/*	A B	R[A] := UpValue[B]				*/
OP_SETUPVAL,/*	A B	UpValue[B] := R[A]				*/

OP_GETTABUP,/*	A B C	R[A] := UpValue[B][K[C]:shortstring]		*/
OP_GETTABLE,/*	A B C	R[A] := R[B][R[C]]				*/
OP_GETI,/*	A B C	R[A] := R[B][C]					*/
OP_GETFIELD,/*	A B C	R[A] := R[B][K[C]:shortstring]			*/

OP_SETTABUP,/*	A B C	UpValue[A][K[B]:shortstring] := RK(C)		*/
OP_SETTABLE,/*	A B C	R[A][R[B]] := RK(C)				*/
OP_SETI,/*	A B C	R[A][B] := RK(C)				*/
OP_SETFIELD,/*	A B C	R[A][K[B]:shortstring] := RK(C)			*/

OP_NEWTABLE,/*	A vB vC k	R[A] := {}				*/

OP_SELF,/*	A B C	R[A+1] := R[B]; R[A] := R[B][K[C]:shortstring]	*/

OP_ADDI,/*	A B sC	R[A] := R[B] + sC				*/

OP_ADDK,/*	A B C	R[A] := R[B] + K[C]:number			*/
OP_SUBK,/*	A B C	R[A] := R[B] - K[C]:number			*/
OP_MULK,/*	A B C	R[A] := R[B] * K[C]:number			*/
OP_MODK,/*	A B C	R[A] := R[B] % K[C]:number			*/
OP_POWK,/*	A B C	R[A] := R[B] ^ K[C]:number			*/
OP_DIVK,/*	A B C	R[A] := R[B] / K[C]:number			*/
OP_IDIVK,/*	A B C	R[A] := R[B] // K[C]:number			*/

OP_BANDK,/*	A B C	R[A] := R[B] & K[C]:integer			*/
OP_BORK,/*	A B C	R[A] := R[B] | K[C]:integer			*/
OP_BXORK,/*	A B C	R[A] := R[B] ~ K[C]:integer			*/

OP_SHLI,/*	A B sC	R[A] := sC << R[B]				*/
OP_SHRI,/*	A B sC	R[A] := R[B] >> sC				*/

OP_ADD,/*	A B C	R[A] := R[B] + R[C]				*/
OP_SUB,/*	A B C	R[A] := R[B] - R[C]				*/
OP_MUL,/*	A B C	R[A] := R[B] * R[C]				*/
OP_MOD,/*	A B C	R[A] := R[B] % R[C]				*/
OP_POW,/*	A B C	R[A] := R[B] ^ R[C]				*/
OP_DIV,/*	A B C	R[A] := R[B] / R[C]				*/
OP_IDIV,/*	A B C	R[A] := R[B] // R[C]				*/

OP_BAND,/*	A B C	R[A] := R[B] & R[C]				*/
OP_BOR,/*	A B C	R[A] := R[B] | R[C]				*/
OP_BXOR,/*	A B C	R[A] := R[B] ~ R[C]				*/
OP_SHL,/*	A B C	R[A] := R[B] << R[C]				*/
OP_SHR,/*	A B C	R[A] := R[B] >> R[C]				*/

OP_MMBIN,/*	A B C	call C metamethod over R[A] and R[B]		*/
OP_MMBINI,/*	A sB C k	call C metamethod over R[A] and sB	*/
OP_MMBINK,/*	A B C k		call C metamethod over R[A] and K[B]	*/

OP_UNM,/*	A B	R[A] := -R[B]					*/
OP_BNOT,/*	A B	R[A] := ~R[B]					*/
OP_NOT,/*	A B	R[A] := not R[B]				*/
OP_LEN,/*	A B	R[A] := #R[B] (length operator)			*/

OP_CONCAT,/*	A B	R[A] := R[A].. ... ..R[A + B - 1]		*/

OP_CLOSE,/*	A	close all upvalues >= R[A]			*/
OP_TBC,/*	A	mark variable A "to be closed"			*/
OP_JMP,/*	sJ	pc += sJ					*/
OP_EQ,/*	A B k	if ((R[A] == R[B]) ~= k) then pc++		*/
OP_LT,/*	A B k	if ((R[A] <  R[B]) ~= k) then pc++		*/
OP_LE,/*	A B k	if ((R[A] <= R[B]) ~= k) then pc++		*/

OP_EQK,/*	A B k	if ((R[A] == K[B]) ~= k) then pc++		*/
OP_EQI,/*	A sB k	if ((R[A] == sB) ~= k) then pc++		*/
OP_LTI,/*	A sB k	if ((R[A] < sB) ~= k) then pc++			*/
OP_LEI,/*	A sB k	if ((R[A] <= sB) ~= k) then pc++		*/
OP_GTI,/*	A sB k	if ((R[A] > sB) ~= k) then pc++			*/
OP_GEI,/*	A sB k	if ((R[A] >= sB) ~= k) then pc++		*/

OP_TEST,/*	A k	if (not R[A] == k) then pc++			*/
OP_TESTSET,/*	A B k	if (not R[B] == k) then pc++ else R[A] := R[B]  */

OP_CALL,/*	A B C	R[A], ... ,R[A+C-2] := R[A](R[A+1], ... ,R[A+B-1]) */
OP_TAILCALL,/*	A B C k	return R[A](R[A+1], ... ,R[A+B-1])		*/

OP_RETURN,/*	A B C k	return R[A], ... ,R[A+B-2]			*/
OP_RETURN0,/*		return						*/
OP_RETURN1,/*	A	return R[A]					*/

OP_FORLOOP,/*	A Bx	update counters; if loop continues then pc-=Bx; */
OP_FORPREP,/*	A Bx	<check values and prepare counters>;
                        if not to run then pc+=Bx+1;			*/

OP_TFORPREP,/*	A Bx	create upvalue for R[A + 3]; pc+=Bx		*/
OP_TFORCALL,/*	A C	R[A+4], ... ,R[A+3+C] := R[A](R[A+1], R[A+2]);	*/
OP_TFORLOOP,/*	A Bx	if R[A+2] ~= nil then { R[A]=R[A+2]; pc -= Bx }	*/

OP_SETLIST,/*	A vB vC k	R[A][vC+i] := R[A+i], 1 <= i <= vB	*/

OP_CLOSURE,/*	A Bx	R[A] := closure(KPROTO[Bx])			*/

OP_VARARG,/*	A B C k	R[A], ..., R[A+C-2] = varargs  			*/

OP_GETVARG, /* A B C	R[A] := R[B][R[C]], R[B] is vararg parameter    */

OP_ERRNNIL,/*	A Bx	raise error if R[A] ~= nil (K[Bx - 1] is global name)*/

OP_VARARGPREP,/* 	(adjust varargs)				*/

OP_EXTRAARG/*	Ax	extra (larger) argument for previous opcode	*/
} OpCode;






/*===========================================================================
  Notes:

  (*) Opcode OP_LFALSESKIP is used to convert a condition to a boolean
  value, in a code equivalent to (not cond ? false : true).  (It
  produces false and skips the next instruction producing true.)

  (*) Opcodes OP_MMBIN and variants follow each arithmetic and
  bitwise opcode. If the operation succeeds, it skips this next
  opcode. Otherwise, this opcode calls the corresponding metamethod.

  (*) Opcode OP_TESTSET is used in short-circuit expressions that need
  both to jump and to produce a value, such as (a = b or c).

  (*) In OP_CALL, if (B == 0) then B = top - A. If (C == 0), then
  'top' is set to last_result+1, so next open instruction (OP_CALL,
  OP_RETURN*, OP_SETLIST) may use 'top'.

  (*) In OP_VARARG, if (C == 0) then use actual number of varargs and
  set top (like in OP_CALL with C == 0). 'k' means function has a
  vararg table, which is in R[B].

  (*) In OP_RETURN, if (B == 0) then return up to 'top'.

  (*) In OP_LOADKX and OP_NEWTABLE, the next instruction is always
  OP_EXTRAARG.

  (*) In OP_SETLIST, if (B == 0) then real B = 'top'; if k, then
  real C = EXTRAARG _ C (the bits of EXTRAARG concatenated with the
  bits of C).

  (*) In OP_NEWTABLE, vB is log2 of the hash size (which is always a
  power of 2) plus 1, or zero for size zero. If not k, the array size
  is vC. Otherwise, the array size is EXTRAARG _ vC.

  (*) In OP_ERRNNIL, (Bx == 0) means index of global name doesn't
  fit in Bx. (So, that name is not available for the error message.)

  (*) For comparisons, k specifies what condition the test should accept
  (true or false).

  (*) In OP_MMBINI/OP_MMBINK, k means the arguments were flipped
  (the constant is the first operand).

  (*) All comparison and test instructions assume that the instruction
  being skipped (pc++) is a jump.

  (*) In instructions OP_RETURN/OP_TAILCALL, 'k' specifies that the
  function builds upvalues, which may need to be closed. C > 0 means
  the function has hidden vararg arguments, so that its 'func' must be
  corrected before returning; in this case, (C - 1) is its number of
  fixed parameters.

  (*) In comparisons with an immediate operand, C signals whether the
  original operand was a float. (It must be corrected in case of
  metamethods.)

===========================================================================*/


/*
** masks for instruction properties. The format is:
** bits 0-2: op mode
** bit 3: instruction set register A
** bit 4: operator is a test (next instruction must be a jump)
** bit 5: instruction uses 'L->top' set by previous instruction (when B == 0)
** bit 6: instruction sets 'L->top' for next instruction (when C == 0)
** bit 7: instruction is an MM instruction (call a metamethod)
*/

extern const lu_byte luaP_opmodes[((int)(OP_EXTRAARG) + 1)];









extern int luaP_isOT (Instruction i);
extern int luaP_isIT (Instruction i);




/*
** $Id: lparser.h $
** Lua Parser
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






























































/*
** Expression and variable descriptor.
** Code generation for variables and expressions can be delayed to allow
** optimizations; An 'expdesc' structure describes a potentially-delayed
** variable/expression. It has a description of its "main" value plus a
** list of conditional jumps that can also produce its value (generated
** by short-circuit operators 'and'/'or').
*/

/* kinds of variables/expressions */
typedef enum {
  VVOID,  /* when 'expdesc' describes the last expression of a list,
             this kind means an empty list (so, no expression) */
  VNIL,  /* constant nil */
  VTRUE,  /* constant true */
  VFALSE,  /* constant false */
  VK,  /* constant in 'k'; info = index of constant in 'k' */
  VKFLT,  /* floating constant; nval = numerical float value */
  VKINT,  /* integer constant; ival = numerical integer value */
  VKSTR,  /* string constant; strval = TString address;
             (string is fixed by the scanner) */
  VNONRELOC,  /* expression has its value in a fixed register;
                 info = result register */
  VLOCAL,  /* local variable; var.ridx = register index;
              var.vidx = relative index in 'actvar.arr'  */
  VVARGVAR,  /* vararg parameter; var.ridx = register index;
              var.vidx = relative index in 'actvar.arr'  */
  VGLOBAL,  /* global variable;
               info = relative index in 'actvar.arr' (or -1 for
                      implicit declaration) */
  VUPVAL,  /* upvalue variable; info = index of upvalue in 'upvalues' */
  VCONST,  /* compile-time <const> variable;
              info = absolute index in 'actvar.arr'  */
  VINDEXED,  /* indexed variable;
                ind.t = table register;
                ind.idx = key's R index;
                ind.ro = true if it represents a read-only global;
                ind.keystr = if key is a string, index in 'k' of that string;
                             -1 if key is not a string */
  VVARGIND,  /* indexed vararg parameter;
                ind.* as in VINDEXED */
  VINDEXUP,  /* indexed upvalue;
                ind.idx = key's K index;
                ind.* as in VINDEXED */
  VINDEXI, /* indexed variable with constant integer;
                ind.t = table register;
                ind.idx = key's value */
  VINDEXSTR, /* indexed variable with literal string;
                ind.idx = key's K index;
                ind.* as in VINDEXED */
  VJMP,  /* expression is a test/comparison;
            info = pc of corresponding jump instruction */
  VRELOC,  /* expression can put result in any register;
              info = instruction pc */
  VCALL,  /* expression is a function call; info = instruction pc */
  VVARARG  /* vararg expression; info = instruction pc */
} expkind;






typedef struct expdesc {
  expkind k;
  union {
    lua_Integer ival;    /* for VKINT */
    lua_Number nval;  /* for VKFLT */
    TString *strval;  /* for VKSTR */
    int info;  /* for generic use */
    struct {  /* for indexed variables */
      short idx;  /* index (R or "long" K) */
      lu_byte t;  /* table (register or upvalue) */
      lu_byte ro;  /* true if variable is read-only */
      int keystr;  /* index in 'k' of string key, or -1 if not a string */
    } ind;
    struct {  /* for local variables */
      lu_byte ridx;  /* register holding the variable */
      short vidx;  /* index in 'actvar.arr' */
    } var;
  } u;
  int t;  /* patch list of 'exit when true' */
  int f;  /* patch list of 'exit when false' */
} expdesc;


/* kinds of variables */








/* variables that live in registers */


/* test for global variables */



/* description of an active variable */
typedef union Vardesc {
  struct {
    Value value_; lu_byte tt_;  /* constant value (if it is a compile-time constant) */
    lu_byte kind;
    lu_byte ridx;  /* register holding the variable */
    short pidx;  /* index of the variable in the Proto's 'locvars' array */
    TString *name;  /* variable name */
  } vd;
  TValue k;  /* constant value (if any) */
} Vardesc;



/* description of pending goto statements and label statements */
typedef struct Labeldesc {
  TString *name;  /* label identifier */
  int pc;  /* position in code */
  int line;  /* line where it appeared */
  short nactvar;  /* number of active variables in that position */
  lu_byte close;  /* true for goto that escapes upvalues */
} Labeldesc;


/* list of labels or gotos */
typedef struct Labellist {
  Labeldesc *arr;  /* array */
  int n;  /* number of entries in use */
  int size;  /* array size */
} Labellist;


/* dynamic structures used by the parser */
typedef struct Dyndata {
  struct {  /* list of all active local variables */
    Vardesc *arr;
    int n;
    int size;
  } actvar;
  Labellist gt;  /* list of pending gotos */
  Labellist label;   /* list of active labels */
} Dyndata;


/* control of blocks */
struct BlockCnt;  /* defined in lparser.c */


/* state needed to generate code for a given function */
typedef struct FuncState {
  Proto *f;  /* current function header */
  struct FuncState *prev;  /* enclosing function */
  struct LexState *ls;  /* lexical state */
  struct BlockCnt *bl;  /* chain of current blocks */
  Table *kcache;  /* cache for reusing constants */
  int pc;  /* next position to code (equivalent to 'ncode') */
  int lasttarget;   /* 'label' of last 'jump label' */
  int previousline;  /* last line that was saved in 'lineinfo' */
  int nk;  /* number of elements in 'k' */
  int np;  /* number of elements in 'p' */
  int nabslineinfo;  /* number of elements in 'abslineinfo' */
  int firstlocal;  /* index of first local var (in Dyndata array) */
  int firstlabel;  /* index of first label (in 'dyd->label->arr') */
  short ndebugvars;  /* number of elements in 'f->locvars' */
  short nactvar;  /* number of active variable declarations */
  lu_byte nups;  /* number of upvalues */
  lu_byte freereg;  /* first free register */
  lu_byte iwthabs;  /* instructions issued since last absolute line info */
  lu_byte needclose;  /* function needs to close upvalues when returning */
} FuncState;


extern lu_byte luaY_nvarstack (FuncState *fs);
extern void luaY_checklimit (FuncState *fs, int v, int l,
                                const char *what);
extern LClosure *luaY_parser (lua_State *L, ZIO *z, Mbuffer *buff,
                                 Dyndata *dyd, const char *name, int firstchar);






/*
** Marks the end of a patch list. It is an invalid value both as an absolute
** address, and as a list link (would link an element to itself).
*/



/*
** grep "ORDER OPR" if you change these enums  (ORDER OP)
*/
typedef enum BinOpr {
  /* arithmetic operators */
  OPR_ADD, OPR_SUB, OPR_MUL, OPR_MOD, OPR_POW,
  OPR_DIV, OPR_IDIV,
  /* bitwise operators */
  OPR_BAND, OPR_BOR, OPR_BXOR,
  OPR_SHL, OPR_SHR,
  /* string operator */
  OPR_CONCAT,
  /* comparison operators */
  OPR_EQ, OPR_LT, OPR_LE,
  OPR_NE, OPR_GT, OPR_GE,
  /* logical operators */
  OPR_AND, OPR_OR,
  OPR_NOBINOPR
} BinOpr;


/* true if operation is foldable (that is, it is arithmetic or bitwise) */






typedef enum UnOpr { OPR_MINUS, OPR_BNOT, OPR_NOT, OPR_LEN, OPR_NOUNOPR } UnOpr;


/* get (pointer to) instruction of given 'expdesc' */







extern int luaK_code (FuncState *fs, Instruction i);
extern int luaK_codeABx (FuncState *fs, OpCode o, int A, int Bx);
extern int luaK_codeABCk (FuncState *fs, OpCode o, int A, int B, int C,
                                            int k);
extern int luaK_codevABCk (FuncState *fs, OpCode o, int A, int B, int C,
                                             int k);
extern int luaK_exp2const (FuncState *fs, const expdesc *e, TValue *v);
extern void luaK_fixline (FuncState *fs, int line);
extern void luaK_nil (FuncState *fs, int from, int n);
extern void luaK_codecheckglobal (FuncState *fs, expdesc *var, int k,
                                                    int line);
extern void luaK_reserveregs (FuncState *fs, int n);
extern void luaK_checkstack (FuncState *fs, int n);
extern void luaK_int (FuncState *fs, int reg, lua_Integer n);
extern void luaK_vapar2local (FuncState *fs, expdesc *var);
extern void luaK_dischargevars (FuncState *fs, expdesc *e);
extern int luaK_exp2anyreg (FuncState *fs, expdesc *e);
extern void luaK_exp2anyregup (FuncState *fs, expdesc *e);
extern void luaK_exp2nextreg (FuncState *fs, expdesc *e);
extern void luaK_exp2val (FuncState *fs, expdesc *e);
extern void luaK_self (FuncState *fs, expdesc *e, expdesc *key);
extern void luaK_indexed (FuncState *fs, expdesc *t, expdesc *k);
extern void luaK_goiftrue (FuncState *fs, expdesc *e);
extern void luaK_storevar (FuncState *fs, expdesc *var, expdesc *e);
extern void luaK_setreturns (FuncState *fs, expdesc *e, int nresults);
extern void luaK_setoneret (FuncState *fs, expdesc *e);
extern int luaK_jump (FuncState *fs);
extern void luaK_ret (FuncState *fs, int first, int nret);
extern void luaK_patchlist (FuncState *fs, int list, int target);
extern void luaK_patchtohere (FuncState *fs, int list);
extern void luaK_concat (FuncState *fs, int *l1, int l2);
extern int luaK_getlabel (FuncState *fs);
extern void luaK_prefix (FuncState *fs, UnOpr op, expdesc *v, int line);
extern void luaK_infix (FuncState *fs, BinOpr op, expdesc *v);
extern void luaK_posfix (FuncState *fs, BinOpr op, expdesc *v1,
                            expdesc *v2, int line);
extern void luaK_settablesize (FuncState *fs, int pc,
                                  int ra, int asize, int hsize);
extern void luaK_setlist (FuncState *fs, int base, int nelems, int tostore);
extern void luaK_finish (FuncState *fs);
extern void  luaK_semerror (LexState *ls, const char *fmt, ...);




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




/*
** $Id: lua.h $
** Lua - A Scripting Language
** Lua.org, PUC-Rio, Brazil (www.lua.org)
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
** corresponding metamethod field. (Bit 6 of the flag indicates that
** the table is using the dummy node.)
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
extern lu_byte luaT_callTMres (lua_State *L, const TValue *f,
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

extern void luaT_adjustvarargs (lua_State *L, struct CallInfo *ci,
                                                 const Proto *p);
extern void luaT_getvararg (CallInfo *ci, StkId ra, TValue *rc);
extern void luaT_getvarargs (lua_State *L, struct CallInfo *ci, StkId where,
                                              int wanted, int vatab);




/*
** $Id: lzio.h $
** Buffered streams
** See Copyright Notice in lua.h
*/






























































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
** - Open upvalues are kept gray to avoid barriers, but they stay out
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

typedef int sig_atomic_t;
typedef void (*sighandler_t)(int);





sighandler_t signal(int, sighandler_t);
struct sigaction { sighandler_t sa_handler; unsigned long sa_flags; unsigned long sa_mask; };
int sigaction(int, const struct sigaction*, struct sigaction*);



int sigemptyset(sigset_t *set);





/*
** Extra stack space to handle TM calls and some other extras. This
** space is not included in 'stack_last'. It is used only to avoid stack
** checks, either because the element will be promptly popped or because
** there will be a stack check soon after the push. Function frames
** never use this extra space, so it does not need to be kept clean.
*/



/*
** Size of cache for strings in the API. 'N' is the number of
** sets (better be a prime) and "M" is the size of each set.
** (M == 1 makes a direct cache.)
*/











/* kinds of Garbage Collection */





typedef struct stringtable {
  TString **hash;  /* array of buckets (linked lists of strings) */
  int nuse;  /* number of elements */
  int size;  /* number of buckets */
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
*/
struct CallInfo {
  StkIdRel func;  /* function index in the stack */
  StkIdRel top;  /* top for this function */
  struct CallInfo *previous, *next;  /* dynamic call link */
  union {
    struct {  /* only for Lua functions */
      const Instruction *savedpc;
      volatile sig_atomic_t trap;  /* function is tracing lines/counts */
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
  } u2;
  l_uint32 callstatus;
};


/*
** Maximum expected number of results from a function
** (must fit in CIST_NRESULTS).
*/



/*
** Bits in CallInfo status
*/
/* bits 0-7 are the expected number of results from this function + 1 */


/* bits 8-11 count call metamethods (and their extra arguments) */



/* Bits 12-14 are used for CIST_RECST (see below) */


/* call is running a C function (still in first 16 bits) */

/* call is on a fresh "luaV_execute" frame */

/* function is closing tbc variables */

/* function has tbc variables to close */

/* original value of 'allowhook' */

/* call is running a debug hook */

/* doing a yieldable protected call */

/* call was tail called */

/* last hook called yielded */

/* function "called" a finalizer */





/*
** Field CIST_RECST stores the "recover status", used to keep the error
** status while closing to-be-closed variables in coroutines, so that
** Lua can correctly resume after an yield from a __close method called
** because of an error.  (Three bits are enough for error status.)
*/




/* active function is a Lua function */


/* call is running Lua code (not a hook) */







/*
** 'per thread' state
*/
struct lua_State {
  struct GCObject *next; lu_byte tt; lu_byte marked;
  lu_byte allowhook;
  TStatus status;
  StkIdRel top;  /* first free slot in the stack */
  struct global_State *l_G;
  CallInfo *ci;  /* call info for current function */
  StkIdRel stack_last;  /* end of stack (last element + 1) */
  StkIdRel stack;  /* stack base */
  UpVal *openupval;  /* list of open upvalues in this stack */
  StkIdRel tbclist;  /* list of to-be-closed variables */
  GCObject *gclist;
  struct lua_State *twups;  /* list of threads with open upvalues */
  struct lua_longjmp *errorJmp;  /* current error recover point */
  CallInfo base_ci;  /* CallInfo for first level (C host) */
  volatile lua_Hook hook;
  ptrdiff_t errfunc;  /* current error handling function (stack index) */
  l_uint32 nCcalls;  /* number of nested non-yieldable or C calls */
  int oldpc;  /* last pc traced */
  int nci;  /* number of items in 'ci' list */
  int basehookcount;
  int hookcount;
  volatile sig_atomic_t hookmask;
  struct {  /* info about transferred values (for call/return hooks) */
    int ftransfer;  /* offset of first value transferred */
    int ntransfer;  /* number of values transferred */
  } transferinfo;
};


/*
** thread state + extra space
*/
typedef struct LX {
  lu_byte extra_[(sizeof(void *))];
  lua_State l;
} LX;


/*
** 'global state', shared by all threads of this state
*/
typedef struct global_State {
  lua_Alloc frealloc;  /* function to reallocate memory */
  void *ud;         /* auxiliary data to 'frealloc' */
  l_mem GCtotalbytes;  /* number of bytes currently allocated + debt */
  l_mem GCdebt;  /* bytes counted but not yet allocated */
  l_mem GCmarked;  /* number of objects marked in a GC cycle */
  l_mem GCmajorminor;  /* auxiliary counter to control major-minor shifts */
  stringtable strt;  /* hash table for strings */
  TValue l_registry;
  TValue nilvalue;  /* a nil value */
  unsigned int seed;  /* randomized seed for hashes */
  lu_byte gcparams[6];
  lu_byte currentwhite;
  lu_byte gcstate;  /* state of garbage collector */
  lu_byte gckind;  /* kind of GC running */
  lu_byte gcstopem;  /* stops emergency collections */
  lu_byte gcstp;  /* control whether GC is running */
  lu_byte gcemergency;  /* true if this is an emergency collection */
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
  TString *memerrmsg;  /* message for memory-allocation errors */
  TString *tmname[TM_N];  /* array with tag-method names */
  struct Table *mt[9];  /* metatables for basic types */
  TString *strcache[53][2];  /* cache for strings in API */
  lua_WarnFunction warnf;  /* warning function */
  void *ud_warn;         /* auxiliary data to 'warnf' */
  LX mainth;  /* main thread of this state */
} global_State;





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
*/



/* actual number of total memory allocated */



extern void luaE_setdebt (global_State *g, l_mem debt);
extern void luaE_freethread (lua_State *L, lua_State *L1);
extern lu_mem luaE_threadsize (lua_State *L);
extern CallInfo *luaE_extendCI (lua_State *L, int err);
extern void luaE_shrinkCI (lua_State *L);
extern void luaE_checkcstack (lua_State *L);
extern void luaE_incCstack (lua_State *L);
extern void luaE_warning (lua_State *L, const char *msg, int tocont);
extern void luaE_warnerror (lua_State *L, const char *where);
extern TStatus luaE_resetthread (lua_State *L, TStatus status);










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
extern void  luaG_errnnil (lua_State *L, LClosure *cl, int k);
extern void  luaG_runerror (lua_State *L, const char *fmt, ...);
extern const char *luaG_addinfo (lua_State *L, const char *msg,
                                                  TString *src, int line);
extern void  luaG_errormsg (lua_State *L);
extern int luaG_traceexec (lua_State *L, const Instruction *pc);
extern int luaG_tracecall (lua_State *L);




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



/*
** Maximum depth for nested C calls, syntactical nested non-terminals,
** and other features implemented through recursion in C. (Value must
** fit in a 16-bit unsigned integer. It must also be compatible with
** the size of the C stack.)
*/





/* type of protected functions, to be ran by 'runprotected' */
typedef void (*Pfunc) (lua_State *L, void *ud);

extern void  luaD_errerr (lua_State *L);
extern void luaD_seterrorobj (lua_State *L, TStatus errcode, StkId oldtop);
extern TStatus luaD_protectedparser (lua_State *L, ZIO *z,
                                                  const char *name,
                                                  const char *mode);
extern void luaD_hook (lua_State *L, int event, int line,
                                        int fTransfer, int nTransfer);
extern void luaD_hookcall (lua_State *L, CallInfo *ci);
extern int luaD_pretailcall (lua_State *L, CallInfo *ci, StkId func,
                                              int narg1, int delta);
extern CallInfo *luaD_precall (lua_State *L, StkId func, int nResults);
extern void luaD_call (lua_State *L, StkId func, int nResults);
extern void luaD_callnoyield (lua_State *L, StkId func, int nResults);
extern TStatus luaD_closeprotected (lua_State *L, ptrdiff_t level,
                                                     TStatus status);
extern TStatus luaD_pcall (lua_State *L, Pfunc func, void *u,
                                        ptrdiff_t oldtop, ptrdiff_t ef);
extern void luaD_poscall (lua_State *L, CallInfo *ci, int nres);
extern int luaD_reallocstack (lua_State *L, int newsize, int raiseerror);
extern int luaD_growstack (lua_State *L, int n, int raiseerror);
extern void luaD_shrinkstack (lua_State *L);
extern void luaD_inctop (lua_State *L);
extern int luaD_checkminstack (lua_State *L);

extern void  luaD_throw (lua_State *L, TStatus errcode);
extern void  luaD_throwbaselevel (lua_State *L, TStatus errcode);
extern TStatus luaD_rawrunprotected (lua_State *L, Pfunc f, void *ud);




/*
** $Id: lgc.h $
** Garbage Collector
** See Copyright Notice in lua.h
*/








typedef unsigned long size_t;







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
** upvalues are an exception to this rule, as they are attached to
** a corresponding thread.)  These lists have no meaning when the
** invariant is not being enforced (e.g., sweep phase).
*/


/*
** Possible states of the Garbage Collector
*/














/*
** macro to tell when main invariant (white objects cannot point to black
** ones) must be kept. During a collection, the sweep phase may break
** the invariant, as objects turned white may point to still-black
** objects. The invariant is restored when sweep ends and all objects
** are white again.
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















/*
** In generational mode, objects are created 'new'. After surviving one
** cycle, they become 'survival'. Both 'new' and 'survival' can point
** to any other object, as they are traversed at the end of the cycle.
** We call them both 'young' objects.
** If a survival object survives another cycle, it becomes 'old1'.
** 'old1' objects can still point to survival objects (but not to
** new objects), so they still must be traversed. After another cycle
** (that, being old, 'old1' objects will "survive" no matter what)
** finally the 'old1' object becomes really 'old', and then they
** are no more traversed.
**
** To keep its invariants, the generational mode uses the same barriers
** also used by the incremental mode. If a young object is caught in a
** forward barrier, it cannot become old immediately, because it can
** still point to other young objects. Instead, it becomes 'old0',
** which in the next cycle becomes 'old1'. So, 'old0' objects is
** old but can point to new and survival objects; 'old1' is old
** but cannot point to new objects; and 'old' cannot point to any
** young object.
**
** If any old object ('old0', 'old1', 'old') is caught in a back
** barrier, it becomes 'touched1' and goes into a gray list, to be
** visited at the end of the cycle.  There it evolves to 'touched2',
** which can point to survivals but not to new objects. In yet another
** cycle then it becomes 'old' again.
**
** The generational mode must also control the colors of objects,
** because of the barriers.  While the mutator is running, young objects
** are kept white. 'old', 'old1', and 'touched2' objects are kept black,
** as they cannot point to new objects; exceptions are threads and open
** upvalues, which age to 'old1' and 'old' but are kept gray. 'old0'
** objects may be gray or black, as in the incremental mode. 'touched1'
** objects are kept gray, as they must be visited again at the end of
** the cycle.
*/


/*
** {======================================================
** Default Values for GC parameters
** =======================================================
*/

/*
** Minor collections will shift to major ones after LUAI_MINORMAJOR%
** bytes become old.
*/


/*
** Major collections will shift to minor ones after a collection
** collects at least LUAI_MAJORMINOR% of the new bytes.
*/


/*
** A young (minor) collection will run after creating LUAI_GENMINORMUL%
** new bytes.
*/



/* incremental */

/* Number of bytes must be LUAI_GCPAUSE% before starting new cycle */


/*
** Step multiplier: The collector handles LUAI_GCMUL% work units for
** each new allocated word. (Each "work unit" corresponds roughly to
** sweeping one object or traversing one slot.)
*/


/* How many bytes to allocate before next GC step */






/* }====================================================== */


/*
** Control when GC is running:
*/






/*
** Does one step of collection when debt becomes zero. 'pre'/'pos'
** allows some adjustments to be done only when needed. macro
** 'condchangemem' is used only for heavy tests (forcing a full
** GC cycle on every opportunity)
*/









/* more often than not, 'pre'/'pos' are empty */











extern void luaC_fix (lua_State *L, GCObject *o);
extern void luaC_freeallobjects (lua_State *L);
extern void luaC_step (lua_State *L);
extern void luaC_runtilstate (lua_State *L, int state, int fast);
extern void luaC_fullgc (lua_State *L, int isemergency);
extern GCObject *luaC_newobj (lua_State *L, lu_byte tt, size_t sz);
extern GCObject *luaC_newobjdt (lua_State *L, lu_byte tt, size_t sz,
                                                 size_t offset);
extern void luaC_barrier_ (lua_State *L, GCObject *o, GCObject *v);
extern void luaC_barrierback_ (lua_State *L, GCObject *o);
extern void luaC_checkfinalizer (lua_State *L, GCObject *o, Table *mt);
extern void luaC_changemode (lua_State *L, int newmode);




/*
** $Id: llex.h $
** Lexical Analyzer
** See Copyright Notice in lua.h
*/


















































































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
** $Id: lopcodes.h $
** Opcodes for Lua virtual machine
** See Copyright Notice in lua.h
*/
















































































































































































































































































































/*
** $Id: lparser.h $
** Lua Parser
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
** Maximum length for short strings, that is, strings that are
** internalized. (Cannot be smaller than reserved words or tags for
** metamethods, as these strings must be internalized;
** #("function") = 8, #("__newindex") = 10.)
*/





/*
** Size of a short TString: Size of the header plus space for the string
** itself (including final '\0').
*/






/*
** test whether a string is a reserved word
*/



/*
** equality for short strings, which are always internalized
*/



extern unsigned luaS_hashlongstr (TString *ts);
extern int luaS_eqstr (TString *a, TString *b);
extern void luaS_resize (lua_State *L, int newsize);
extern void luaS_clearcache (global_State *g);
extern void luaS_init (lua_State *L);
extern void luaS_remove (lua_State *L, TString *ts);
extern Udata *luaS_newudata (lua_State *L, size_t s,
                                              unsigned short nuvalue);
extern TString *luaS_newlstr (lua_State *L, const char *str, size_t l);
extern TString *luaS_new (lua_State *L, const char *str);
extern TString *luaS_createlngstrobj (lua_State *L, size_t l);
extern TString *luaS_newextlstr (lua_State *L,
		const char *s, size_t len, lua_Alloc falloc, void *ud);
extern size_t luaS_sizelngstr (size_t len, int kind);
extern TString *luaS_normstr (lua_State *L, TString *ts);



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



/*
** Bit BITDUMMY set in 'flags' means the table is using the dummy node
** for its hash part.
*/










/* allocated size for hash nodes */



/* returns the Node, given the value of a table entry */










/* results from pset */





/*
** 'luaH_get*' operations set 'res', unless the value is absent, and
** return the tag of the result.
** The 'luaH_pset*' (pre-set) operations set the given value and return
** HOK, unless the original value is absent. In that case, if the key
** is really absent, they return HNOTFOUND. Otherwise, if there is a
** slot with that key but with no value, 'luaH_pset*' return an encoding
** of where the key is (usually called 'hres'). (pset cannot set that
** value because there might be a metamethod.) If the slot is in the
** hash part, the encoding is (HFIRSTNODE + hash index); if the slot is
** in the array part, the encoding is (~array index), a negative value.
** The value HNOTATABLE is used by the fast macros to signal that the
** value being indexed is not a table.
** (The size for the array part is limited by the maximum power of two
** that fits in an unsigned integer; that is INT_MAX+1. So, the C-index
** ranges from 0, which encodes to -1, to INT_MAX, which encodes to
** INT_MIN. The size of the hash part is limited by the maximum power of
** two that fits in a signed integer; that is (INT_MAX+1)/2. So, it is
** safe to add HFIRSTNODE to any index there.)
*/


/*
** The array part of a table is represented by an inverted array of
** values followed by an array of tags, to avoid wasting space with
** padding. In between them there is an unsigned int, explained later.
** The 'array' pointer points between the two arrays, so that values are
** indexed with negative indices and tags with non-negative indices.

             Values                              Tags
  --------------------------------------------------------
  ...  |   Value 1     |   Value 0     |unsigned|0|1|...
  --------------------------------------------------------
                                       ^ t->array

** All accesses to 't->array' should be through the macros 'getArrTag'
** and 'getArrVal'.
*/

/* Computes the address of the tag for the abstract C-index 'k' */


/* Computes the address of the value for the abstract C-index 'k' */



/*
** The unsigned between the two arrays is used as a hint for #t;
** see luaH_getn. It is stored there to avoid wasting space in
** the structure Table for tables with no array part.
*/



/*
** Move TValues to/from arrays, using C indices
*/





/*
** Often, we need to check the tag of a value before moving it. The
** following macros also move TValues to/from arrays, but receive the
** precomputed tag value or address as an extra argument.
*/





extern lu_byte luaH_get (Table *t, const TValue *key, TValue *res);
extern lu_byte luaH_getshortstr (Table *t, TString *key, TValue *res);
extern lu_byte luaH_getstr (Table *t, TString *key, TValue *res);
extern lu_byte luaH_getint (Table *t, lua_Integer key, TValue *res);

/* Special get for metamethods */
extern const TValue *luaH_Hgetshortstr (Table *t, TString *key);

extern int luaH_psetint (Table *t, lua_Integer key, TValue *val);
extern int luaH_psetshortstr (Table *t, TString *key, TValue *val);
extern int luaH_psetstr (Table *t, TString *key, TValue *val);
extern int luaH_pset (Table *t, const TValue *key, TValue *val);

extern void luaH_setint (lua_State *L, Table *t, lua_Integer key,
                                                    TValue *value);
extern void luaH_set (lua_State *L, Table *t, const TValue *key,
                                                 TValue *value);

extern void luaH_finishset (lua_State *L, Table *t, const TValue *key,
                                              TValue *value, int hres);
extern Table *luaH_new (lua_State *L);
extern void luaH_resize (lua_State *L, Table *t, unsigned nasize,
                                                    unsigned nhsize);
extern void luaH_resizearray (lua_State *L, Table *t, unsigned nasize);
extern lu_mem luaH_size (Table *t);
extern void luaH_free (lua_State *L, Table *t);
extern int luaH_next (lua_State *L, Table *t, StkId key);
extern lua_Unsigned luaH_getn (lua_State *L, Table *t);









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
  F2Iceil    /* takes the ceiling of the number */
} F2Imod;


/* convert an object to a float (including string coercion) */



/* convert an object to a float (without string coercion) */



/* convert an object to an integer (including string coercion) */



/* convert an object to an integer (without string coercion) */








/*
** fast track for 'gettable'
*/



/*
** Special case of 'luaV_fastget' for integers, inlining the fast case
** of 'luaH_getint'.
*/








/*
** Finish a fast set operation (when fast set succeeds).
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
extern lu_byte luaV_finishget (lua_State *L, const TValue *t, TValue *key,
                                                StkId val, lu_byte tag);
extern void luaV_finishset (lua_State *L, const TValue *t, TValue *key,
                                             TValue *val, int aux);
extern void luaV_finishOp (lua_State *L);
extern void luaV_execute (lua_State *L, CallInfo *ci);
extern void luaV_concat (lua_State *L, int total);
extern lua_Integer luaV_idiv (lua_State *L, lua_Integer x, lua_Integer y);
extern lua_Integer luaV_mod (lua_State *L, lua_Integer x, lua_Integer y);
extern lua_Number luaV_modf (lua_State *L, lua_Number x, lua_Number y);
extern lua_Integer luaV_shiftl (lua_Integer x, lua_Integer y);
extern void luaV_objlen (lua_State *L, StkId ra, const TValue *rb);





/* (note that expressions VJMP also have jumps.) */



static int codesJ (FuncState *fs, OpCode o, int sj, int k);



/* semantic error */
void  luaK_semerror (LexState *ls, const char *fmt, ...) {
  const char *msg;
  va_list argp;
    { (argp=(void*)((char*)&fmt + sizeof(fmt)));   msg = luaO_pushvfstring(ls->L, fmt, argp);   (argp=0);   if (msg == 0) luaD_throw(ls->L, 4);   };
  ls->t.token = 0;  /* remove "near <token>" from final message */
  ls->linenumber = ls->lastline;  /* back to line of last used token */
  luaX_syntaxerror(ls, msg);
}


/*
** If expression is a numeric constant, fills 'v' with its value
** and returns 1. Otherwise, returns 0.
*/
static int tonumeral (const expdesc *e, TValue *v) {
  if (((e)->t != (e)->f))
    return 0;  /* not a numeral */
  switch (e->k) {
    case VKINT:
      if (v)   { TValue *io=(v); ((io)->value_).i=(e->u.ival); ((io)->tt_=(((3) | ((0) << 4))  )); };
      return 1;
    case VKFLT:
      if (v)   { TValue *io=(v); ((io)->value_).n=(e->u.nval); ((io)->tt_=(((3) | ((1) << 4))  )); };
      return 1;
    default: return 0;
  }
}


/*
** Get the constant value from a constant expression
*/
static TValue *const2val (FuncState *fs, const expdesc *e) {
  ((void)0);
  return &fs->ls->dyd->actvar.arr[e->u.info].k;
}


/*
** If expression is a constant, fills 'v' with its value
** and returns 1. Otherwise, returns 0.
*/
int luaK_exp2const (FuncState *fs, const expdesc *e, TValue *v) {
  if (((e)->t != (e)->f))
    return 0;  /* not a constant */
  switch (e->k) {
    case VFALSE:
      ((v)->tt_=(((1) | ((0) << 4))));
      return 1;
    case VTRUE:
      ((v)->tt_=(((1) | ((1) << 4))));
      return 1;
    case VNIL:
      ((v)->tt_=(((0) | ((0) << 4))));
      return 1;
    case VKSTR: {
        { TValue *io = (v); TString *x_ = (e->u.strval);     ((io)->value_).gc = 	(((void)0), (&(((union GCUnion *)((x_)))->gc))); ((io)->tt_=(((x_->tt) | (1 << 6))));     	((void)fs->ls->L, ((void)0)); };
      return 1;
    }
    case VCONST: {
      	{ TValue *io1=(v); const TValue *io2=(const2val(fs, e));           io1->value_ = io2->value_; ((io1)->tt_=(io2->tt_)); 	  	((void)fs->ls->L, ((void)0)); ((void)0); };
      return 1;
    }
    default: return tonumeral(e, v);
  }
}


/*
** Return the previous instruction of the current code. If there
** may be a jump target between the current instruction and the
** previous one, return an invalid instruction (to avoid wrong
** optimizations).
*/
static Instruction *previousinstruction (FuncState *fs) {
  static const Instruction invalidinstruction = ~(Instruction)0;
  if (fs->pc > fs->lasttarget)
    return &fs->f->code[fs->pc - 1];  /* previous instruction */
  else
    return ((Instruction*)(&invalidinstruction));
}


/*
** Create a OP_LOADNIL instruction, but try to optimize: if the previous
** instruction is also OP_LOADNIL and ranges are compatible, adjust
** range of previous instruction instead of emitting a new one. (For
** instance, 'local a; local b' will generate a single opcode.)
*/
void luaK_nil (FuncState *fs, int from, int n) {
  int l = from + n - 1;  /* last register to set nil */
  Instruction *previous = previousinstruction(fs);
  if ((((OpCode)(((*previous)>>0) & ((~((~(Instruction)0)<<(7)))<<(0))))) == OP_LOADNIL) {  /* previous is LOADNIL? */
    int pfrom = (((int)((((*previous)>>((0 + 7))) & ((~((~(Instruction)0)<<(8)))<<(0))))));  /* get previous range */
    int pl = pfrom + 	(((void)0), ((((int)((((*previous)>>((((0 + 7) + 8) + 1))) & ((~((~(Instruction)0)<<(8)))<<(0))))))));
    if ((pfrom <= from && from <= pl + 1) ||
        (from <= pfrom && pfrom <= l + 1)) {  /* can connect both? */
      if (pfrom < from) from = pfrom;  /* from = min(from, pfrom) */
      if (pl > l) l = pl;  /* l = max(l, pl) */
      ((*previous) = (((*previous)&(~((~((~(Instruction)0)<<(8)))<<((0 + 7))))) |                 ((((Instruction)((from)))<<(0 + 7))&((~((~(Instruction)0)<<(8)))<<((0 + 7))))));
      ((*previous) = (((*previous)&(~((~((~(Instruction)0)<<(8)))<<((((0 + 7) + 8) + 1))))) |                 ((((Instruction)((l - from)))<<(((0 + 7) + 8) + 1))&((~((~(Instruction)0)<<(8)))<<((((0 + 7) + 8) + 1))))));
      return;
    }  /* else go through */
  }
  luaK_codeABCk(fs,OP_LOADNIL,from,n - 1,0,0);  /* else no optimization */
}


/*
** Gets the destination address of a jump instruction. Used to traverse
** a list of jumps.
*/
static int getjump (FuncState *fs, int pc) {
  int offset = 	(((void)0), ((((int)((((fs->f->code[pc])>>((0 + 7))) & ((~((~(Instruction)0)<<(((8 + 8 + 1) + 8))))<<(0)))))) - (((1 << ((8 + 8 + 1) + 8)) - 1) >> 1)));
  if (offset == (-1))  /* point to itself represents end of list */
    return (-1);  /* end of list */
  else
    return (pc+1)+offset;  /* turn offset into absolute position */
}


/*
** Fix jump instruction at position 'pc' to jump to 'dest'.
** (Jump addresses are relative in Lua)
*/
static void fixjump (FuncState *fs, int pc, int dest) {
  Instruction *jmp = &fs->f->code[pc];
  int offset = dest - (pc + 1);
  ((void)0);
  if (!(-(((1 << ((8 + 8 + 1) + 8)) - 1) >> 1) <= offset && offset <= ((1 << ((8 + 8 + 1) + 8)) - 1) - (((1 << ((8 + 8 + 1) + 8)) - 1) >> 1)))
    luaX_syntaxerror(fs->ls, "control structure too long");
  ((void)0);
  	((*jmp) = (((*jmp)&(~((~((~(Instruction)0)<<(((8 + 8 + 1) + 8))))<<((0 + 7))))) |                 ((((Instruction)((((unsigned int)(((offset)+(((1 << ((8 + 8 + 1) + 8)) - 1) >> 1)))))))<<(0 + 7))&((~((~(Instruction)0)<<(((8 + 8 + 1) + 8))))<<((0 + 7))))));
}


/*
** Concatenate jump-list 'l2' into jump-list 'l1'
*/
void luaK_concat (FuncState *fs, int *l1, int l2) {
  if (l2 == (-1)) return;  /* nothing to concatenate? */
  else if (*l1 == (-1))  /* no original list? */
    *l1 = l2;  /* 'l1' points to 'l2' */
  else {
    int list = *l1;
    int next;
    while ((next = getjump(fs, list)) != (-1))  /* find last element */
      list = next;
    fixjump(fs, list, l2);  /* last element links to 'l2' */
  }
}


/*
** Create a jump instruction and return its position, so its destination
** can be fixed later (with 'fixjump').
*/
int luaK_jump (FuncState *fs) {
  return codesJ(fs, OP_JMP, (-1), 0);
}


/*
** Code a 'return' instruction
*/
void luaK_ret (FuncState *fs, int first, int nret) {
  OpCode op;
  switch (nret) {
    case 0: op = OP_RETURN0; break;
    case 1: op = OP_RETURN1; break;
    default: op = OP_RETURN; break;
  }
  luaY_checklimit(fs, nret + 1, ((1<<8)-1), "returns");
  luaK_codeABCk(fs,op,first,nret + 1,0,0);
}


/*
** Code a "conditional jump", that is, a test or comparison opcode
** followed by a jump. Return jump position.
*/
static int condjump (FuncState *fs, OpCode op, int A, int B, int C, int k) {
  luaK_codeABCk(fs, op, A, B, C, k);
  return luaK_jump(fs);
}


/*
** returns current 'pc' and marks it as a jump target (to avoid wrong
** optimizations with consecutive instructions not in the same basic block).
*/
int luaK_getlabel (FuncState *fs) {
  fs->lasttarget = fs->pc;
  return fs->pc;
}


/*
** Returns the position of the instruction "controlling" a given
** jump (that is, its condition), or the jump itself if it is
** unconditional.
*/
static Instruction *getjumpcontrol (FuncState *fs, int pc) {
  Instruction *pi = &fs->f->code[pc];
  if (pc >= 1 && (luaP_opmodes[(((OpCode)(((*(pi-1))>>0) & ((~((~(Instruction)0)<<(7)))<<(0)))))] & (1 << 4)))
    return pi-1;
  else
    return pi;
}


/*
** Patch destination register for a TESTSET instruction.
** If instruction in position 'node' is not a TESTSET, return 0 ("fails").
** Otherwise, if 'reg' is not 'NO_REG', set it as the destination
** register. Otherwise, change instruction to a simple 'TEST' (produces
** no register value)
*/
static int patchtestreg (FuncState *fs, int node, int reg) {
  Instruction *i = getjumpcontrol(fs, node);
  if ((((OpCode)(((*i)>>0) & ((~((~(Instruction)0)<<(7)))<<(0))))) != OP_TESTSET)
    return 0;  /* cannot patch other instructions */
  if (reg != ((1<<8)-1) && reg != 	(((void)0), ((((int)((((*i)>>((((0 + 7) + 8) + 1))) & ((~((~(Instruction)0)<<(8)))<<(0)))))))))
    ((*i) = (((*i)&(~((~((~(Instruction)0)<<(8)))<<((0 + 7))))) |                 ((((Instruction)((reg)))<<(0 + 7))&((~((~(Instruction)0)<<(8)))<<((0 + 7))))));
  else {
     /* no register to put value or register already has the value;
        change instruction to simple test */
    *i = ((((Instruction)((OP_TEST)))<<0) 			| (((Instruction)(((((void)0), ((((int)((((*i)>>((((0 + 7) + 8) + 1))) & ((~((~(Instruction)0)<<(8)))<<(0)))))))))))<<(0 + 7)) 			| (((Instruction)((0)))<<(((0 + 7) + 8) + 1)) 			| (((Instruction)((0)))<<((((0 + 7) + 8) + 1) + 8)) 			| (((Instruction)(((((int)((((*i)>>(((0 + 7) + 8))) & ((~((~(Instruction)0)<<(1)))<<(0)))))))))<<((0 + 7) + 8)));
  }
  return 1;
}


/*
** Traverse a list of tests ensuring no one produces a value
*/
static void removevalues (FuncState *fs, int list) {
  for (; list != (-1); list = getjump(fs, list))
      patchtestreg(fs, list, ((1<<8)-1));
}


/*
** Traverse a list of tests, patching their destination address and
** registers: tests producing values jump to 'vtarget' (and put their
** values in 'reg'), other tests jump to 'dtarget'.
*/
static void patchlistaux (FuncState *fs, int list, int vtarget, int reg,
                          int dtarget) {
  while (list != (-1)) {
    int next = getjump(fs, list);
    if (patchtestreg(fs, list, reg))
      fixjump(fs, list, vtarget);
    else
      fixjump(fs, list, dtarget);  /* jump to default target */
    list = next;
  }
}


/*
** Path all jumps in 'list' to jump to 'target'.
** (The assert means that we cannot fix a jump to a forward address
** because we only know addresses once code is generated.)
*/
void luaK_patchlist (FuncState *fs, int list, int target) {
  ((void)0);
  patchlistaux(fs, list, target, ((1<<8)-1), target);
}


void luaK_patchtohere (FuncState *fs, int list) {
  int hr = luaK_getlabel(fs);  /* mark "here" as a jump target */
  luaK_patchlist(fs, list, hr);
}


/* limit for difference between lines in relative line info. */



/*
** Save line info for a new instruction. If difference from last line
** does not fit in a byte, of after that many instructions, save a new
** absolute line info; (in that case, the special value 'ABSLINEINFO'
** in 'lineinfo' signals the existence of this absolute information.)
** Otherwise, store the difference from last line in 'lineinfo'.
*/
static void savelineinfo (FuncState *fs, Proto *f, int line) {
  int linedif = line - fs->previousline;
  int pc = fs->pc - 1;  /* last instruction coded */
  if (abs(linedif) >= 0x80 || fs->iwthabs++ >= 128) {
    	((f->abslineinfo)=((AbsLineInfo *)(luaM_growaux_(fs->ls->L,f->abslineinfo,fs->nabslineinfo,&(f->sizeabslineinfo),sizeof(AbsLineInfo),                            ((((size_t)((2147483647))) <= ((size_t)(~(size_t)0))/sizeof(AbsLineInfo)) ? (2147483647) :       ((int)(((((size_t)(~(size_t)0))/sizeof(AbsLineInfo)))))),"lines"))));
    f->abslineinfo[fs->nabslineinfo].pc = pc;
    f->abslineinfo[fs->nabslineinfo++].line = line;
    linedif = (-0x80);  /* signal that there is absolute information */
    fs->iwthabs = 1;  /* restart counter */
  }
  	((f->lineinfo)=((ls_byte *)(luaM_growaux_(fs->ls->L,f->lineinfo,pc,&(f->sizelineinfo),sizeof(ls_byte),                            ((((size_t)((2147483647))) <= ((size_t)(~(size_t)0))/sizeof(ls_byte)) ? (2147483647) :       ((int)(((((size_t)(~(size_t)0))/sizeof(ls_byte)))))),"opcodes"))));
  f->lineinfo[pc] = ((ls_byte)(linedif));
  fs->previousline = line;  /* last line saved */
}


/*
** Remove line information from the last instruction.
** If line information for that instruction is absolute, set 'iwthabs'
** above its max to force the new (replacing) instruction to have
** absolute line info, too.
*/
static void removelastlineinfo (FuncState *fs) {
  Proto *f = fs->f;
  int pc = fs->pc - 1;  /* last instruction coded */
  if (f->lineinfo[pc] != (-0x80)) {  /* relative line info? */
    fs->previousline -= f->lineinfo[pc];  /* correct last line saved */
    fs->iwthabs--;  /* undo previous increment */
  }
  else {  /* absolute line information */
    ((void)0);
    fs->nabslineinfo--;  /* remove it */
    fs->iwthabs = 128 + 1;  /* force next line info to be absolute */
  }
}


/*
** Remove the last instruction created, correcting line information
** accordingly.
*/
static void removelastinstruction (FuncState *fs) {
  removelastlineinfo(fs);
  fs->pc--;
}


/*
** Emit instruction 'i', checking for array sizes and saving also its
** line information. Return 'i' position.
*/
int luaK_code (FuncState *fs, Instruction i) {
  Proto *f = fs->f;
  /* put new instruction in code array */
  	((f->code)=((Instruction *)(luaM_growaux_(fs->ls->L,f->code,fs->pc,&(f->sizecode),sizeof(Instruction),                            ((((size_t)((2147483647))) <= ((size_t)(~(size_t)0))/sizeof(Instruction)) ? (2147483647) :       ((int)(((((size_t)(~(size_t)0))/sizeof(Instruction)))))),"opcodes"))));
  f->code[fs->pc++] = i;
  savelineinfo(fs, f, fs->ls->lastline);
  return fs->pc - 1;  /* index of new instruction */
}


/*
** Format and emit an 'iABC' instruction. (Assertions check consistency
** of parameters versus opcode.)
*/
int luaK_codeABCk (FuncState *fs, OpCode o, int A, int B, int C, int k) {
  ((void)0);
  ((void)0);
  return luaK_code(fs, ((((Instruction)((o)))<<0) 			| (((Instruction)((A)))<<(0 + 7)) 			| (((Instruction)((B)))<<(((0 + 7) + 8) + 1)) 			| (((Instruction)((C)))<<((((0 + 7) + 8) + 1) + 8)) 			| (((Instruction)((k)))<<((0 + 7) + 8))));
}


int luaK_codevABCk (FuncState *fs, OpCode o, int A, int B, int C, int k) {
  ((void)0);
  ((void)0);
  return luaK_code(fs, ((((Instruction)((o)))<<0) 			| (((Instruction)((A)))<<(0 + 7)) 			| (((Instruction)((B)))<<(((0 + 7) + 8) + 1)) 			| (((Instruction)((C)))<<((((0 + 7) + 8) + 1) + 6)) 			| (((Instruction)((k)))<<((0 + 7) + 8))));
}


/*
** Format and emit an 'iABx' instruction.
*/
int luaK_codeABx (FuncState *fs, OpCode o, int A, int Bc) {
  ((void)0);
  ((void)0);
  return luaK_code(fs, ((((Instruction)((o)))<<0) 			| (((Instruction)((A)))<<(0 + 7)) 			| (((Instruction)((Bc)))<<((0 + 7) + 8))));
}


/*
** Format and emit an 'iAsBx' instruction.
*/
static int codeAsBx (FuncState *fs, OpCode o, int A, int Bc) {
  int b = Bc + (((1<<(8 + 8 + 1))-1)>>1)         ;
  ((void)0);
  ((void)0);
  return luaK_code(fs, ((((Instruction)((o)))<<0) 			| (((Instruction)((A)))<<(0 + 7)) 			| (((Instruction)((b)))<<((0 + 7) + 8))));
}


/*
** Format and emit an 'isJ' instruction.
*/
static int codesJ (FuncState *fs, OpCode o, int sj, int k) {
  int j = sj + (((1 << ((8 + 8 + 1) + 8)) - 1) >> 1);
  ((void)0);
  ((void)0);
  return luaK_code(fs, ((((Instruction)((o))) << 0) 			| (((Instruction)((j))) << (0 + 7)) 			| (((Instruction)((k))) << ((0 + 7) + 8))));
}


/*
** Emit an "extra argument" instruction (format 'iAx')
*/
static int codeextraarg (FuncState *fs, int A) {
  ((void)0);
  return luaK_code(fs, ((((Instruction)((OP_EXTRAARG)))<<0) 			| (((Instruction)((A)))<<(0 + 7))));
}


/*
** Emit a "load constant" instruction, using either 'OP_LOADK'
** (if constant index 'k' fits in 18 bits) or an 'OP_LOADKX'
** instruction with "extra argument".
*/
static int luaK_codek (FuncState *fs, int reg, int k) {
  if (k <= ((1<<(8 + 8 + 1))-1))
    return luaK_codeABx(fs, OP_LOADK, reg, k);
  else {
    int p = luaK_codeABx(fs, OP_LOADKX, reg, 0);
    codeextraarg(fs, k);
    return p;
  }
}


/*
** Check register-stack level, keeping track of its maximum size
** in field 'maxstacksize'
*/
void luaK_checkstack (FuncState *fs, int n) {
  int newstack = fs->freereg + n;
  if (newstack > fs->f->maxstacksize) {
    luaY_checklimit(fs, newstack, ((1<<8)-1), "registers");
    fs->f->maxstacksize = ((lu_byte)((newstack)));
  }
}


/*
** Reserve 'n' registers in register stack
*/
void luaK_reserveregs (FuncState *fs, int n) {
  luaK_checkstack(fs, n);
  fs->freereg =  ((lu_byte)((fs->freereg + n)));
}


/*
** Free register 'reg', if it is neither a constant index nor
** a local variable.
)
*/
static void freereg (FuncState *fs, int reg) {
  if (reg >= luaY_nvarstack(fs)) {
    fs->freereg--;
    ((void)0);
  }
}


/*
** Free two registers in proper order
*/
static void freeregs (FuncState *fs, int r1, int r2) {
  if (r1 > r2) {
    freereg(fs, r1);
    freereg(fs, r2);
  }
  else {
    freereg(fs, r2);
    freereg(fs, r1);
  }
}


/*
** Free register used by expression 'e' (if any)
*/
static void freeexp (FuncState *fs, expdesc *e) {
  if (e->k == VNONRELOC)
    freereg(fs, e->u.info);
}


/*
** Free registers used by expressions 'e1' and 'e2' (if any) in proper
** order.
*/
static void freeexps (FuncState *fs, expdesc *e1, expdesc *e2) {
  int r1 = (e1->k == VNONRELOC) ? e1->u.info : -1;
  int r2 = (e2->k == VNONRELOC) ? e2->u.info : -1;
  freeregs(fs, r1, r2);
}


/*
** Add constant 'v' to prototype's list of constants (field 'k').
*/
static int addk (FuncState *fs, Proto *f, TValue *v) {
  lua_State *L = fs->ls->L;
  int oldsize = f->sizek;
  int k = fs->nk;
  	((f->k)=((TValue *)(luaM_growaux_(L,f->k,k,&(f->sizek),sizeof(TValue),                            ((((size_t)((((1<<((8 + 8 + 1) + 8))-1)))) <= ((size_t)(~(size_t)0))/sizeof(TValue)) ? (((1<<((8 + 8 + 1) + 8))-1)) :       ((int)(((((size_t)(~(size_t)0))/sizeof(TValue)))))),"constants"))));
  while (oldsize < f->sizek)
    ((&f->k[oldsize++])->tt_=(((0) | ((0) << 4))));
  	{ TValue *io1=(&f->k[k]); const TValue *io2=(v);           io1->value_ = io2->value_; ((io1)->tt_=(io2->tt_)); 	  	((void)L, ((void)0)); ((void)0); };
  fs->nk++;
  (  	(((v)->tt_) & (1 << 6)) ? (  	((((f)->marked) & ((1<<(5)))) && ((((((void)0), (((v)->value_).gc)))->marked) & (((1<<(3)) | (1<<(4)))))) ? 	luaC_barrier_(L,	(((void)0), (&(((union GCUnion *)((f)))->gc))),	(((void)0), (&(((union GCUnion *)(((((void)0), (((v)->value_).gc)))))->gc)))) : ((void)((0)))) : ((void)((0))));
  return k;
}


/*
** Use scanner's table to cache position of constants in constant list
** and try to reuse constants. Because some values should not be used
** as keys (nil cannot be a key, integer keys can collapse with float
** keys), the caller must provide a useful 'key' for indexing the cache.
*/
static int k2proto (FuncState *fs, TValue *key, TValue *v) {
  TValue val;
  Proto *f = fs->f;
  int tag = luaH_get(fs->kcache, key, &val);  /* query scanner table */
  if (!(((tag) & 0x0F) == 0)) {  /* is there an index there? */
    int k = ((int)(((((void)0), (((&val)->value_).i)))));
    /* collisions can happen only for float keys */
    ((void)0);
    return k;  /* reuse index */
  }
  else {  /* constant not found; create a new entry */
    int k = addk(fs, f, v);
    /* cache it for reuse; numerical value does not need GC barrier;
       table is not a metatable, so it does not need to invalidate cache */
      { TValue *io=(&val); ((io)->value_).i=(k); ((io)->tt_=(((3) | ((0) << 4))  )); };
    luaH_set(fs->ls->L, fs->kcache, key, &val);
    return k;
  }
}


/*
** Add a string to list of constants and return its index.
*/
static int stringK (FuncState *fs, TString *s) {
  TValue o;
    { TValue *io = (&o); TString *x_ = (s);     ((io)->value_).gc = 	(((void)0), (&(((union GCUnion *)((x_)))->gc))); ((io)->tt_=(((x_->tt) | (1 << 6))));     	((void)fs->ls->L, ((void)0)); };
  return k2proto(fs, &o, &o);  /* use string itself as key */
}


/*
** Add an integer to list of constants and return its index.
*/
static int luaK_intK (FuncState *fs, lua_Integer n) {
  TValue o;
    { TValue *io=(&o); ((io)->value_).i=(n); ((io)->tt_=(((3) | ((0) << 4))  )); };
  return k2proto(fs, &o, &o);  /* use integer itself as key */
}

/*
** Add a float to list of constants and return its index. Floats
** with integral values need a different key, to avoid collision
** with actual integers. To that end, we add to the number its smaller
** power-of-two fraction that is still significant in its scale.
** (For doubles, the fraction would be 2^-52).
** This method is not bulletproof: different numbers may generate the
** same key (e.g., very large numbers will overflow to 'inf') and for
** floats larger than 2^53 the result is still an integer. For those
** cases, just generate a new entry. At worst, this only wastes an entry
** with a duplicate.
*/
static int luaK_numberK (FuncState *fs, lua_Number r) {
  TValue o, kv;
    { TValue *io=(&o); ((io)->value_).n=(r); ((io)->tt_=(((3) | ((1) << 4))  )); };  /* value as a TValue */
  if (r == 0) {  /* handle zero as a special case */
      { TValue *io=(&kv); ((io)->value_).p=(fs); ((io)->tt_=(((2) | ((0) << 4)))); };  /* use FuncState as index */
    return k2proto(fs, &kv, &o);  /* cannot collide */
  }
  else {
    const int nbm = (53);
    const lua_Number q = (lua_Number)ldexp  ((lua_Number)1.0  , -nbm + 1);
    const lua_Number k =  r * (1 + q);  /* key */
    lua_Integer ik;
      { TValue *io=(&kv); ((io)->value_).n=(k); ((io)->tt_=(((3) | ((1) << 4))  )); };  /* key as a TValue */
    if (!luaV_flttointeger(k, &ik, F2Ieq)) {  /* not an integer value? */
      int n = k2proto(fs, &kv, &o);  /* use key */
      if (luaV_equalobj(0,&fs->f->k[n],&o))  /* correct value? */
        return n;
    }
    /* else, either key is still an integer or there was a collision;
       anyway, do not try to reuse constant; instead, create a new one */
    return addk(fs, fs->f, &o);
  }
}


/*
** Add a false to list of constants and return its index.
*/
static int boolF (FuncState *fs) {
  TValue o;
  ((&o)->tt_=(((1) | ((0) << 4))));
  return k2proto(fs, &o, &o);  /* use boolean itself as key */
}


/*
** Add a true to list of constants and return its index.
*/
static int boolT (FuncState *fs) {
  TValue o;
  ((&o)->tt_=(((1) | ((1) << 4))));
  return k2proto(fs, &o, &o);  /* use boolean itself as key */
}


/*
** Add nil to list of constants and return its index.
*/
static int nilK (FuncState *fs) {
  TValue k, v;
  ((&v)->tt_=(((0) | ((0) << 4))));
  /* cannot use nil as key; instead use table itself */
    { TValue *io = (&k); Table *x_ = (fs->kcache);     ((io)->value_).gc = 	(((void)0), (&(((union GCUnion *)((x_)))->gc))); ((io)->tt_=(((((5) | ((0) << 4))) | (1 << 6))));     	((void)fs->ls->L, ((void)0)); };
  return k2proto(fs, &k, &v);
}


/*
** Check whether 'i' can be stored in an 'sC' operand. Equivalent to
** (0 <= int2sC(i) && int2sC(i) <= MAXARG_C) but without risk of
** overflows in the hidden addition inside 'int2sC'.
*/
static int fitsC (lua_Integer i) {
  return (((lua_Unsigned)(i)) + (((1<<8)-1) >> 1) <= ((unsigned int)((((1<<8)-1)))));
}


/*
** Check whether 'i' can be stored in an 'sBx' operand.
*/
static int fitsBx (lua_Integer i) {
  return (-(((1<<(8 + 8 + 1))-1)>>1)          <= i && i <= ((1<<(8 + 8 + 1))-1) - (((1<<(8 + 8 + 1))-1)>>1)         );
}


void luaK_int (FuncState *fs, int reg, lua_Integer i) {
  if (fitsBx(i))
    codeAsBx(fs, OP_LOADI, reg, ((int)((i))));
  else
    luaK_codek(fs, reg, luaK_intK(fs, i));
}


static void luaK_float (FuncState *fs, int reg, lua_Number f) {
  lua_Integer fi;
  if (luaV_flttointeger(f, &fi, F2Ieq) && fitsBx(fi))
    codeAsBx(fs, OP_LOADF, reg, ((int)((fi))));
  else
    luaK_codek(fs, reg, luaK_numberK(fs, f));
}


/*
** Get the value of 'var' in a register and generate an opcode to check
** whether that register is nil. 'k' is the index of the variable name
** in the list of constants. If its value cannot be encoded in Bx, a 0
** will use '?' for the name.
*/
void luaK_codecheckglobal (FuncState *fs, expdesc *var, int k, int line) {
  luaK_exp2anyreg(fs, var);
  luaK_fixline(fs, line);
  k = (k >= ((1<<(8 + 8 + 1))-1)) ? 0 : k + 1;
  luaK_codeABx(fs, OP_ERRNNIL, var->u.info, k);
  luaK_fixline(fs, line);
  freeexp(fs, var);
}


/*
** Convert a constant in 'v' into an expression description 'e'
*/
static void const2exp (TValue *v, expdesc *e) {
  switch (((((v)->tt_)) & 0x3F)) {
    case ((3) | ((0) << 4))  :
      e->k = VKINT; e->u.ival = (((void)0), (((v)->value_).i));
      break;
    case ((3) | ((1) << 4))  :
      e->k = VKFLT; e->u.nval = (((void)0), (((v)->value_).n));
      break;
    case ((1) | ((0) << 4)):
      e->k = VFALSE;
      break;
    case ((1) | ((1) << 4)):
      e->k = VTRUE;
      break;
    case ((0) | ((0) << 4)):
      e->k = VNIL;
      break;
    case ((4) | ((0) << 4))  :  case ((4) | ((1) << 4))  :
      e->k = VKSTR; e->u.strval = (((void)0), (	(((void)0), (&((((union GCUnion *)((((v)->value_).gc))))->ts)))));
      break;
    default: ((void)0);
  }
}


/*
** Fix an expression to return the number of results 'nresults'.
** 'e' must be a multi-ret expression (function call or vararg).
*/
void luaK_setreturns (FuncState *fs, expdesc *e, int nresults) {
  Instruction *pc = &((fs)->f->code[(e)->u.info]);
  luaY_checklimit(fs, nresults + 1, ((1<<8)-1), "multiple results");
  if (e->k == VCALL)  /* expression is an open function call? */
    ((*pc) = (((*pc)&(~((~((~(Instruction)0)<<(8)))<<(((((0 + 7) + 8) + 1) + 8))))) |                 ((((Instruction)((nresults + 1)))<<((((0 + 7) + 8) + 1) + 8))&((~((~(Instruction)0)<<(8)))<<(((((0 + 7) + 8) + 1) + 8))))));
  else {
    ((void)0);
    ((*pc) = (((*pc)&(~((~((~(Instruction)0)<<(8)))<<(((((0 + 7) + 8) + 1) + 8))))) |                 ((((Instruction)((nresults + 1)))<<((((0 + 7) + 8) + 1) + 8))&((~((~(Instruction)0)<<(8)))<<(((((0 + 7) + 8) + 1) + 8))))));
    ((*pc) = (((*pc)&(~((~((~(Instruction)0)<<(8)))<<((0 + 7))))) |                 ((((Instruction)((fs->freereg)))<<(0 + 7))&((~((~(Instruction)0)<<(8)))<<((0 + 7))))));
    luaK_reserveregs(fs, 1);
  }
}


/*
** Convert a VKSTR to a VK
*/
static int str2K (FuncState *fs, expdesc *e) {
  ((void)0);
  e->u.info = stringK(fs, e->u.strval);
  e->k = VK;
  return e->u.info;
}


/*
** Fix an expression to return one result.
** If expression is not a multi-ret expression (function call or
** vararg), it already returns one result, so nothing needs to be done.
** Function calls become VNONRELOC expressions (as its result comes
** fixed in the base register of the call), while vararg expressions
** become VRELOC (as OP_VARARG puts its results where it wants).
** (Calls are created returning one result, so that does not need
** to be fixed.)
*/
void luaK_setoneret (FuncState *fs, expdesc *e) {
  if (e->k == VCALL) {  /* expression is an open function call? */
    /* already returns 1 value */
    ((void)0);
    e->k = VNONRELOC;  /* result has fixed position */
    e->u.info = (((int)((((((fs)->f->code[(e)->u.info]))>>((0 + 7))) & ((~((~(Instruction)0)<<(8)))<<(0))))));
  }
  else if (e->k == VVARARG) {
    ((((fs)->f->code[(e)->u.info])) = (((((fs)->f->code[(e)->u.info]))&(~((~((~(Instruction)0)<<(8)))<<(((((0 + 7) + 8) + 1) + 8))))) |                 ((((Instruction)((2)))<<((((0 + 7) + 8) + 1) + 8))&((~((~(Instruction)0)<<(8)))<<(((((0 + 7) + 8) + 1) + 8))))));
    e->k = VRELOC;  /* can relocate its simple result */
  }
}

/*
** Change a vararg parameter into a regular local variable
*/
void luaK_vapar2local (FuncState *fs, expdesc *var) {
  ((fs->f)->flag |= 2  );  /* function will need a vararg table */
  /* now a vararg parameter is equivalent to a regular local variable */
  var->k = VLOCAL;
}


/*
** Ensure that expression 'e' is not a variable (nor a <const>).
** (Expression still may have jump lists.)
*/
void luaK_dischargevars (FuncState *fs, expdesc *e) {
  switch (e->k) {
    case VCONST: {
      const2exp(const2val(fs, e), e);
      break;
    }
    case VVARGVAR: {
      luaK_vapar2local(fs, e);  /* turn it into a local variable */
    }  /* FALLTHROUGH */
    case VLOCAL: {  /* already in a register */
      int temp = e->u.var.ridx;
      e->u.info = temp;  /* (avoid a direct assignment; values overlap) */
      e->k = VNONRELOC;  /* becomes a non-relocatable value */
      break;
    }
    case VUPVAL: {  /* move value to some (pending) register */
      e->u.info = luaK_codeABCk(fs,OP_GETUPVAL,0,e->u.info,0,0);
      e->k = VRELOC;
      break;
    }
    case VINDEXUP: {
      e->u.info = luaK_codeABCk(fs,OP_GETTABUP,0,e->u.ind.t,e->u.ind.idx,0);
      e->k = VRELOC;
      break;
    }
    case VINDEXI: {
      freereg(fs, e->u.ind.t);
      e->u.info = luaK_codeABCk(fs,OP_GETI,0,e->u.ind.t,e->u.ind.idx,0);
      e->k = VRELOC;
      break;
    }
    case VINDEXSTR: {
      freereg(fs, e->u.ind.t);
      e->u.info = luaK_codeABCk(fs,OP_GETFIELD,0,e->u.ind.t,e->u.ind.idx,0);
      e->k = VRELOC;
      break;
    }
    case VINDEXED: {
      freeregs(fs, e->u.ind.t, e->u.ind.idx);
      e->u.info = luaK_codeABCk(fs,OP_GETTABLE,0,e->u.ind.t,e->u.ind.idx,0);
      e->k = VRELOC;
      break;
    }
    case VVARGIND: {
      freeregs(fs, e->u.ind.t, e->u.ind.idx);
      e->u.info = luaK_codeABCk(fs,OP_GETVARG,0,e->u.ind.t,e->u.ind.idx,0);
      e->k = VRELOC;
      break;
    }
    case VVARARG: case VCALL: {
      luaK_setoneret(fs, e);
      break;
    }
    default: break;  /* there is one value available (somewhere) */
  }
}


/*
** Ensure expression value is in register 'reg', making 'e' a
** non-relocatable expression.
** (Expression still may have jump lists.)
*/
static void discharge2reg (FuncState *fs, expdesc *e, int reg) {
  luaK_dischargevars(fs, e);
  switch (e->k) {
    case VNIL: {
      luaK_nil(fs, reg, 1);
      break;
    }
    case VFALSE: {
      luaK_codeABCk(fs,OP_LOADFALSE,reg,0,0,0);
      break;
    }
    case VTRUE: {
      luaK_codeABCk(fs,OP_LOADTRUE,reg,0,0,0);
      break;
    }
    case VKSTR: {
      str2K(fs, e);
    }  /* FALLTHROUGH */
    case VK: {
      luaK_codek(fs, reg, e->u.info);
      break;
    }
    case VKFLT: {
      luaK_float(fs, reg, e->u.nval);
      break;
    }
    case VKINT: {
      luaK_int(fs, reg, e->u.ival);
      break;
    }
    case VRELOC: {
      Instruction *pc = &((fs)->f->code[(e)->u.info]);
      ((*pc) = (((*pc)&(~((~((~(Instruction)0)<<(8)))<<((0 + 7))))) |                 ((((Instruction)((reg)))<<(0 + 7))&((~((~(Instruction)0)<<(8)))<<((0 + 7))))));  /* instruction will put result in 'reg' */
      break;
    }
    case VNONRELOC: {
      if (reg != e->u.info)
        luaK_codeABCk(fs,OP_MOVE,reg,e->u.info,0,0);
      break;
    }
    default: {
      ((void)0);
      return;  /* nothing to do... */
    }
  }
  e->u.info = reg;
  e->k = VNONRELOC;
}


/*
** Ensure expression value is in a register, making 'e' a
** non-relocatable expression.
** (Expression still may have jump lists.)
*/
static void discharge2anyreg (FuncState *fs, expdesc *e) {
  if (e->k != VNONRELOC) {  /* no fixed register yet? */
    luaK_reserveregs(fs, 1);  /* get a register */
    discharge2reg(fs, e, fs->freereg-1);  /* put value there */
  }
}


static int code_loadbool (FuncState *fs, int A, OpCode op) {
  luaK_getlabel(fs);  /* those instructions may be jump targets */
  return luaK_codeABCk(fs,op,A,0,0,0);
}


/*
** check whether list has any jump that do not produce a value
** or produce an inverted value
*/
static int need_value (FuncState *fs, int list) {
  for (; list != (-1); list = getjump(fs, list)) {
    Instruction i = *getjumpcontrol(fs, list);
    if ((((OpCode)(((i)>>0) & ((~((~(Instruction)0)<<(7)))<<(0))))) != OP_TESTSET) return 1;
  }
  return 0;  /* not found */
}


/*
** Ensures final expression result (which includes results from its
** jump lists) is in register 'reg'.
** If expression has jumps, need to patch these jumps either to
** its final position or to "load" instructions (for those tests
** that do not produce values).
*/
static void exp2reg (FuncState *fs, expdesc *e, int reg) {
  discharge2reg(fs, e, reg);
  if (e->k == VJMP)  /* expression itself is a test? */
    luaK_concat(fs, &e->t, e->u.info);  /* put this jump in 't' list */
  if (((e)->t != (e)->f)) {
    int final;  /* position after whole expression */
    int p_f = (-1);  /* position of an eventual LOAD false */
    int p_t = (-1);  /* position of an eventual LOAD true */
    if (need_value(fs, e->t) || need_value(fs, e->f)) {
      int fj = (e->k == VJMP) ? (-1) : luaK_jump(fs);
      p_f = code_loadbool(fs, reg, OP_LFALSESKIP);  /* skip next inst. */
      p_t = code_loadbool(fs, reg, OP_LOADTRUE);
      /* jump around these booleans if 'e' is not a test */
      luaK_patchtohere(fs, fj);
    }
    final = luaK_getlabel(fs);
    patchlistaux(fs, e->f, final, reg, p_f);
    patchlistaux(fs, e->t, final, reg, p_t);
  }
  e->f = e->t = (-1);
  e->u.info = reg;
  e->k = VNONRELOC;
}


/*
** Ensures final expression result is in next available register.
*/
void luaK_exp2nextreg (FuncState *fs, expdesc *e) {
  luaK_dischargevars(fs, e);
  freeexp(fs, e);
  luaK_reserveregs(fs, 1);
  exp2reg(fs, e, fs->freereg - 1);
}


/*
** Ensures final expression result is in some (any) register
** and return that register.
*/
int luaK_exp2anyreg (FuncState *fs, expdesc *e) {
  luaK_dischargevars(fs, e);
  if (e->k == VNONRELOC) {  /* expression already has a register? */
    if (!((e)->t != (e)->f))  /* no jumps? */
      return e->u.info;  /* result is already in a register */
    if (e->u.info >= luaY_nvarstack(fs)) {  /* reg. is not a local? */
      exp2reg(fs, e, e->u.info);  /* put final result in it */
      return e->u.info;
    }
    /* else expression has jumps and cannot change its register
       to hold the jump values, because it is a local variable.
       Go through to the default case. */
  }
  luaK_exp2nextreg(fs, e);  /* default: use next available register */
  return e->u.info;
}


/*
** Ensures final expression result is either in a register,
** in an upvalue, or it is the vararg parameter.
*/
void luaK_exp2anyregup (FuncState *fs, expdesc *e) {
  if ((e->k != VUPVAL && e->k != VVARGVAR) || ((e)->t != (e)->f))
    luaK_exp2anyreg(fs, e);
}


/*
** Ensures final expression result is either in a register
** or it is a constant.
*/
void luaK_exp2val (FuncState *fs, expdesc *e) {
  if (e->k == VJMP || ((e)->t != (e)->f))
    luaK_exp2anyreg(fs, e);
  else
    luaK_dischargevars(fs, e);
}


/*
** Try to make 'e' a K expression with an index in the range of R/K
** indices. Return true iff succeeded.
*/
static int luaK_exp2K (FuncState *fs, expdesc *e) {
  if (!((e)->t != (e)->f)) {
    int info;
    switch (e->k) {  /* move constants to 'k' */
      case VTRUE: info = boolT(fs); break;
      case VFALSE: info = boolF(fs); break;
      case VNIL: info = nilK(fs); break;
      case VKINT: info = luaK_intK(fs, e->u.ival); break;
      case VKFLT: info = luaK_numberK(fs, e->u.nval); break;
      case VKSTR: info = stringK(fs, e->u.strval); break;
      case VK: info = e->u.info; break;
      default: return 0;  /* not a constant */
    }
    if (info <= ((1<<8)-1)) {  /* does constant fit in 'argC'? */
      e->k = VK;  /* make expression a 'K' expression */
      e->u.info = info;
      return 1;
    }
  }
  /* else, expression doesn't fit; leave it unchanged */
  return 0;
}


/*
** Ensures final expression result is in a valid R/K index
** (that is, it is either in a register or in 'k' with an index
** in the range of R/K indices).
** Returns 1 iff expression is K.
*/
static int exp2RK (FuncState *fs, expdesc *e) {
  if (luaK_exp2K(fs, e))
    return 1;
  else {  /* not a constant in the right range: put it in a register */
    luaK_exp2anyreg(fs, e);
    return 0;
  }
}


static void codeABRK (FuncState *fs, OpCode o, int A, int B,
                      expdesc *ec) {
  int k = exp2RK(fs, ec);
  luaK_codeABCk(fs, o, A, B, ec->u.info, k);
}


/*
** Generate code to store result of expression 'ex' into variable 'var'.
*/
void luaK_storevar (FuncState *fs, expdesc *var, expdesc *ex) {
  switch (var->k) {
    case VLOCAL: {
      freeexp(fs, ex);
      exp2reg(fs, ex, var->u.var.ridx);  /* compute 'ex' into proper place */
      return;
    }
    case VUPVAL: {
      int e = luaK_exp2anyreg(fs, ex);
      luaK_codeABCk(fs,OP_SETUPVAL,e,var->u.info,0,0);
      break;
    }
    case VINDEXUP: {
      codeABRK(fs, OP_SETTABUP, var->u.ind.t, var->u.ind.idx, ex);
      break;
    }
    case VINDEXI: {
      codeABRK(fs, OP_SETI, var->u.ind.t, var->u.ind.idx, ex);
      break;
    }
    case VINDEXSTR: {
      codeABRK(fs, OP_SETFIELD, var->u.ind.t, var->u.ind.idx, ex);
      break;
    }
    case VVARGIND: {
      ((fs->f)->flag |= 2  );  /* function will need a vararg table */
      /* now, assignment is to a regular table */
    }  /* FALLTHROUGH */
    case VINDEXED: {
      codeABRK(fs, OP_SETTABLE, var->u.ind.t, var->u.ind.idx, ex);
      break;
    }
    default: ((void)0);  /* invalid var kind to store */
  }
  freeexp(fs, ex);
}


/*
** Negate condition 'e' (where 'e' is a comparison).
*/
static void negatecondition (FuncState *fs, expdesc *e) {
  Instruction *pc = getjumpcontrol(fs, e->u.info);
  ((void)0);
  ((*pc) = (((*pc)&(~((~((~(Instruction)0)<<(1)))<<(((0 + 7) + 8))))) |                 ((((Instruction)((((((int)((((*pc)>>(((0 + 7) + 8))) & ((~((~(Instruction)0)<<(1)))<<(0)))))) ^ 1))))<<((0 + 7) + 8))&((~((~(Instruction)0)<<(1)))<<(((0 + 7) + 8))))));
}


/*
** Emit instruction to jump if 'e' is 'cond' (that is, if 'cond'
** is true, code will jump if 'e' is true.) Return jump position.
** Optimize when 'e' is 'not' something, inverting the condition
** and removing the 'not'.
*/
static int jumponcond (FuncState *fs, expdesc *e, int cond) {
  if (e->k == VRELOC) {
    Instruction ie = ((fs)->f->code[(e)->u.info]);
    if ((((OpCode)(((ie)>>0) & ((~((~(Instruction)0)<<(7)))<<(0))))) == OP_NOT) {
      removelastinstruction(fs);  /* remove previous OP_NOT */
      return condjump(fs, OP_TEST, 	(((void)0), ((((int)((((ie)>>((((0 + 7) + 8) + 1))) & ((~((~(Instruction)0)<<(8)))<<(0)))))))), 0, 0, !cond);
    }
    /* else go through */
  }
  discharge2anyreg(fs, e);
  freeexp(fs, e);
  return condjump(fs, OP_TESTSET, ((1<<8)-1), e->u.info, 0, cond);
}


/*
** Emit code to go through if 'e' is true, jump otherwise.
*/
void luaK_goiftrue (FuncState *fs, expdesc *e) {
  int pc;  /* pc of new jump */
  luaK_dischargevars(fs, e);
  switch (e->k) {
    case VJMP: {  /* condition? */
      negatecondition(fs, e);  /* jump when it is false */
      pc = e->u.info;  /* save jump position */
      break;
    }
    case VK: case VKFLT: case VKINT: case VKSTR: case VTRUE: {
      pc = (-1);  /* always true; do nothing */
      break;
    }
    default: {
      pc = jumponcond(fs, e, 0);  /* jump when false */
      break;
    }
  }
  luaK_concat(fs, &e->f, pc);  /* insert new jump in false list */
  luaK_patchtohere(fs, e->t);  /* true list jumps to here (to go through) */
  e->t = (-1);
}


/*
** Emit code to go through if 'e' is false, jump otherwise.
*/
static void luaK_goiffalse (FuncState *fs, expdesc *e) {
  int pc;  /* pc of new jump */
  luaK_dischargevars(fs, e);
  switch (e->k) {
    case VJMP: {
      pc = e->u.info;  /* already jump if true */
      break;
    }
    case VNIL: case VFALSE: {
      pc = (-1);  /* always false; do nothing */
      break;
    }
    default: {
      pc = jumponcond(fs, e, 1);  /* jump if true */
      break;
    }
  }
  luaK_concat(fs, &e->t, pc);  /* insert new jump in 't' list */
  luaK_patchtohere(fs, e->f);  /* false list jumps to here (to go through) */
  e->f = (-1);
}


/*
** Code 'not e', doing constant folding.
*/
static void codenot (FuncState *fs, expdesc *e) {
  switch (e->k) {
    case VNIL: case VFALSE: {
      e->k = VTRUE;  /* true == not nil == not false */
      break;
    }
    case VK: case VKFLT: case VKINT: case VKSTR: case VTRUE: {
      e->k = VFALSE;  /* false == not "x" == not 0.5 == not 1 == not true */
      break;
    }
    case VJMP: {
      negatecondition(fs, e);
      break;
    }
    case VRELOC:
    case VNONRELOC: {
      discharge2anyreg(fs, e);
      freeexp(fs, e);
      e->u.info = luaK_codeABCk(fs,OP_NOT,0,e->u.info,0,0);
      e->k = VRELOC;
      break;
    }
    default: ((void)0);  /* cannot happen */
  }
  /* interchange true and false lists */
  { int temp = e->f; e->f = e->t; e->t = temp; }
  removevalues(fs, e->f);  /* values are useless when negated */
  removevalues(fs, e->t);
}


/*
** Check whether expression 'e' is a short literal string
*/
static int isKstr (FuncState *fs, expdesc *e) {
  return (e->k == VK && !((e)->t != (e)->f) && e->u.info <= ((1<<8)-1) &&
          ((((&fs->f->k[e->u.info]))->tt_) == (((((4) | ((0) << 4))  ) | (1 << 6)))));
}

/*
** Check whether expression 'e' is a literal integer.
*/
static int isKint (expdesc *e) {
  return (e->k == VKINT && !((e)->t != (e)->f));
}


/*
** Check whether expression 'e' is a literal integer in
** proper range to fit in register C
*/
static int isCint (expdesc *e) {
  return isKint(e) && (((lua_Unsigned)(e->u.ival)) <= ((lua_Unsigned)(((1<<8)-1))));
}


/*
** Check whether expression 'e' is a literal integer in
** proper range to fit in register sC
*/
static int isSCint (expdesc *e) {
  return isKint(e) && fitsC(e->u.ival);
}


/*
** Check whether expression 'e' is a literal integer or float in
** proper range to fit in a register (sB or sC).
*/
static int isSCnumber (expdesc *e, int *pi, int *isfloat) {
  lua_Integer i;
  if (e->k == VKINT)
    i = e->u.ival;
  else if (e->k == VKFLT && luaV_flttointeger(e->u.nval, &i, F2Ieq))
    *isfloat = 1;
  else
    return 0;  /* not a number */
  if (!((e)->t != (e)->f) && fitsC(i)) {
    *pi = ((((int)((i)))) + (((1<<8)-1) >> 1));
    return 1;
  }
  else
    return 0;
}


/*
** Emit SELF instruction or equivalent: the code will convert
** expression 'e' into 'e.key(e,'.
*/
void luaK_self (FuncState *fs, expdesc *e, expdesc *key) {
  int ereg, base;
  luaK_exp2anyreg(fs, e);
  ereg = e->u.info;  /* register where 'e' (the receiver) was placed */
  freeexp(fs, e);
  base = e->u.info = fs->freereg;  /* base register for op_self */
  e->k = VNONRELOC;  /* self expression has a fixed register */
  luaK_reserveregs(fs, 2);  /* method and 'self' produced by op_self */
  ((void)0);
  /* is method name a short string in a valid K index? */
  if (((key->u.strval)->shrlen >= 0) && luaK_exp2K(fs, key)) {
    /* can use 'self' opcode */
    luaK_codeABCk(fs, OP_SELF, base, ereg, key->u.info, 0);
  }
  else {  /* cannot use 'self' opcode; use move+gettable */
    luaK_exp2anyreg(fs, key);  /* put method name in a register */
    luaK_codeABCk(fs,OP_MOVE,base + 1,ereg,0,0);  /* copy self to base+1 */
    luaK_codeABCk(fs,OP_GETTABLE,base,ereg,key->u.info,0);  /* get method */
  }
  freeexp(fs, key);
}


/* auxiliary function to define indexing expressions */
static void fillidxk (expdesc *t, int idx, expkind k) {
  t->u.ind.idx = ((lu_byte)((idx)));
  t->k = k;
}


/*
** Create expression 't[k]'. 't' must have its final result already in a
** register or upvalue. Upvalues can only be indexed by literal strings.
** Keys can be literal strings in the constant table or arbitrary
** values in registers.
*/
void luaK_indexed (FuncState *fs, expdesc *t, expdesc *k) {
  int keystr = -1;
  if (k->k == VKSTR)
    keystr = str2K(fs, k);
  ((void)0);
  if (t->k == VUPVAL && !isKstr(fs, k))  /* upvalue indexed by non 'Kstr'? */
    luaK_exp2anyreg(fs, t);  /* put it in a register */
  if (t->k == VUPVAL) {
    lu_byte temp = ((lu_byte)((t->u.info)));  /* upvalue index */
    t->u.ind.t = temp;  /* (avoid a direct assignment; values overlap) */
    ((void)0);
    fillidxk(t, k->u.info, VINDEXUP);  /* literal short string */
  }
  else if (t->k == VVARGVAR) {  /* indexing the vararg parameter? */
    int kreg = luaK_exp2anyreg(fs, k);  /* put key in some register */
    lu_byte vreg = ((lu_byte)((t->u.var.ridx)));  /* register with vararg param. */
    ((void)0);
    t->u.ind.t = vreg;  /* (avoid a direct assignment; values may overlap?) */
    fillidxk(t, kreg, VVARGIND);  /* 't' represents 'vararg[k]' */
  }
  else {
    /* register index of the table */
    lu_byte temp = ((lu_byte)(((t->k == VLOCAL) ? t->u.var.ridx: t->u.info)));
    t->u.ind.t = temp;  /* (avoid a direct assignment; values may overlap?) */
    if (isKstr(fs, k))
      fillidxk(t, k->u.info, VINDEXSTR);  /* literal short string */
    else if (isCint(k))  /* int. constant in proper range? */
      fillidxk(t, ((int)((k->u.ival))), VINDEXI);
    else
      fillidxk(t, luaK_exp2anyreg(fs, k), VINDEXED);  /* register */
  }
  t->u.ind.keystr = keystr;  /* string index in 'k' */
  t->u.ind.ro = 0;  /* by default, not read-only */
}


/*
** Return false if folding can raise an error.
** Bitwise operations need operands convertible to integers; division
** operations cannot have 0 as divisor.
*/
static int validop (int op, TValue *v1, TValue *v2) {
  switch (op) {
    case 7: case 8: case 9:
    case 10: case 11: case 13: {  /* conversion errors */
      lua_Integer i;
      return (luaV_tointegerns(v1, &i, F2Ieq) &&
              luaV_tointegerns(v2, &i, F2Ieq));
    }
    case 5: case 6: case 3:  /* division by 0 */
      return ((((void)0), ((((((v2))->tt_) == (((3) | ((0) << 4))  )) ? ((lua_Number)(((((void)0), (((v2)->value_).i))))) : (((void)0), (((v2)->value_).n))))) != 0);
    default: return 1;  /* everything else is valid */
  }
}


/*
** Try to "constant-fold" an operation; return 1 iff successful.
** (In this case, 'e1' has the final result.)
*/
static int constfolding (FuncState *fs, int op, expdesc *e1,
                                        const expdesc *e2) {
  TValue v1, v2, res;
  if (!tonumeral(e1, &v1) || !tonumeral(e2, &v2) || !validop(op, &v1, &v2))
    return 0;  /* non-numeric operands or not safe to fold */
  luaO_rawarith(fs->ls->L, op, &v1, &v2, &res);  /* does operation */
  if (((((&res))->tt_) == (((3) | ((0) << 4))  ))) {
    e1->k = VKINT;
    e1->u.ival = (((void)0), (((&res)->value_).i));
  }
  else {  /* folds neither NaN nor 0.0 (to avoid problems with -0.0) */
    lua_Number n = (((void)0), (((&res)->value_).n));
    if ((!(((n))==((n)))) || n == 0)
      return 0;
    e1->k = VKFLT;
    e1->u.nval = n;
  }
  return 1;
}


/*
** Convert a BinOpr to an OpCode  (ORDER OPR - ORDER OP)
*/
static inline OpCode binopr2op (BinOpr opr, BinOpr baser, OpCode base) {
  ((void)0);
  return ((OpCode)((((int)((opr))) - ((int)((baser)))) + ((int)((base)))));
}


/*
** Convert a UnOpr to an OpCode  (ORDER OPR - ORDER OP)
*/
static inline OpCode unopr2op (UnOpr opr) {
  return ((OpCode)((((int)((opr))) - ((int)((OPR_MINUS)))) +
                                       ((int)((OP_UNM)))));
}


/*
** Convert a BinOpr to a tag method  (ORDER OPR - ORDER TM)
*/
static inline TMS binopr2TM (BinOpr opr) {
  ((void)0);
  return ((TMS)((((int)((opr))) - ((int)((OPR_ADD)))) + ((int)((TM_ADD)))));
}


/*
** Emit code for unary expressions that "produce values"
** (everything but 'not').
** Expression to produce final result will be encoded in 'e'.
*/
static void codeunexpval (FuncState *fs, OpCode op, expdesc *e, int line) {
  int r = luaK_exp2anyreg(fs, e);  /* opcodes operate only on registers */
  freeexp(fs, e);
  e->u.info = luaK_codeABCk(fs,op,0,r,0,0);  /* generate opcode */
  e->k = VRELOC;  /* all those operations are relocatable */
  luaK_fixline(fs, line);
}


/*
** Emit code for binary expressions that "produce values"
** (everything but logical operators 'and'/'or' and comparison
** operators).
** Expression to produce final result will be encoded in 'e1'.
*/
static void finishbinexpval (FuncState *fs, expdesc *e1, expdesc *e2,
                             OpCode op, int v2, int flip, int line,
                             OpCode mmop, TMS event) {
  int v1 = luaK_exp2anyreg(fs, e1);
  int pc = luaK_codeABCk(fs, op, 0, v1, v2, 0);
  freeexps(fs, e1, e2);
  e1->u.info = pc;
  e1->k = VRELOC;  /* all those operations are relocatable */
  luaK_fixline(fs, line);
  luaK_codeABCk(fs, mmop, v1, v2, ((int)((event))), flip);  /* metamethod */
  luaK_fixline(fs, line);
}


/*
** Emit code for binary expressions that "produce values" over
** two registers.
*/
static void codebinexpval (FuncState *fs, BinOpr opr,
                           expdesc *e1, expdesc *e2, int line) {
  OpCode op = binopr2op(opr, OPR_ADD, OP_ADD);
  int v2 = luaK_exp2anyreg(fs, e2);  /* make sure 'e2' is in a register */
  /* 'e1' must be already in a register or it is a constant */
  ((void)0);
  ((void)0);
  finishbinexpval(fs, e1, e2, op, v2, 0, line, OP_MMBIN, binopr2TM(opr));
}


/*
** Code binary operators with immediate operands.
*/
static void codebini (FuncState *fs, OpCode op,
                       expdesc *e1, expdesc *e2, int flip, int line,
                       TMS event) {
  int v2 = ((((int)((e2->u.ival)))) + (((1<<8)-1) >> 1));  /* immediate operand */
  ((void)0);
  finishbinexpval(fs, e1, e2, op, v2, flip, line, OP_MMBINI, event);
}


/*
** Code binary operators with K operand.
*/
static void codebinK (FuncState *fs, BinOpr opr,
                      expdesc *e1, expdesc *e2, int flip, int line) {
  TMS event = binopr2TM(opr);
  int v2 = e2->u.info;  /* K index */
  OpCode op = binopr2op(opr, OPR_ADD, OP_ADDK);
  finishbinexpval(fs, e1, e2, op, v2, flip, line, OP_MMBINK, event);
}


/* Try to code a binary operator negating its second operand.
** For the metamethod, 2nd operand must keep its original value.
*/
static int finishbinexpneg (FuncState *fs, expdesc *e1, expdesc *e2,
                             OpCode op, int line, TMS event) {
  if (!isKint(e2))
    return 0;  /* not an integer constant */
  else {
    lua_Integer i2 = e2->u.ival;
    if (!(fitsC(i2) && fitsC(-i2)))
      return 0;  /* not in the proper range */
    else {  /* operating a small integer constant */
      int v2 = ((int)((i2)));
      finishbinexpval(fs, e1, e2, op, ((-v2) + (((1<<8)-1) >> 1)), 0, line, OP_MMBINI, event);
      /* correct metamethod argument */
      ((fs->f->code[fs->pc - 1]) = (((fs->f->code[fs->pc - 1])&(~((~((~(Instruction)0)<<(8)))<<((((0 + 7) + 8) + 1))))) |                 ((((Instruction)((((v2) + (((1<<8)-1) >> 1)))))<<(((0 + 7) + 8) + 1))&((~((~(Instruction)0)<<(8)))<<((((0 + 7) + 8) + 1))))));
      return 1;  /* successfully coded */
    }
  }
}


static void swapexps (expdesc *e1, expdesc *e2) {
  expdesc temp = *e1; *e1 = *e2; *e2 = temp;  /* swap 'e1' and 'e2' */
}


/*
** Code binary operators with no constant operand.
*/
static void codebinNoK (FuncState *fs, BinOpr opr,
                        expdesc *e1, expdesc *e2, int flip, int line) {
  if (flip)
    swapexps(e1, e2);  /* back to original order */
  codebinexpval(fs, opr, e1, e2, line);  /* use standard operators */
}


/*
** Code arithmetic operators ('+', '-', ...). If second operand is a
** constant in the proper range, use variant opcodes with K operands.
*/
static void codearith (FuncState *fs, BinOpr opr,
                       expdesc *e1, expdesc *e2, int flip, int line) {
  if (tonumeral(e2, 0) && luaK_exp2K(fs, e2))  /* K operand? */
    codebinK(fs, opr, e1, e2, flip, line);
  else  /* 'e2' is neither an immediate nor a K operand */
    codebinNoK(fs, opr, e1, e2, flip, line);
}


/*
** Code commutative operators ('+', '*'). If first operand is a
** numeric constant, change order of operands to try to use an
** immediate or K operator.
*/
static void codecommutative (FuncState *fs, BinOpr op,
                             expdesc *e1, expdesc *e2, int line) {
  int flip = 0;
  if (tonumeral(e1, 0)) {  /* is first operand a numeric constant? */
    swapexps(e1, e2);  /* change order */
    flip = 1;
  }
  if (op == OPR_ADD && isSCint(e2))  /* immediate operand? */
    codebini(fs, OP_ADDI, e1, e2, flip, line, TM_ADD);
  else
    codearith(fs, op, e1, e2, flip, line);
}


/*
** Code bitwise operations; they are all commutative, so the function
** tries to put an integer constant as the 2nd operand (a K operand).
*/
static void codebitwise (FuncState *fs, BinOpr opr,
                         expdesc *e1, expdesc *e2, int line) {
  int flip = 0;
  if (e1->k == VKINT) {
    swapexps(e1, e2);  /* 'e2' will be the constant operand */
    flip = 1;
  }
  if (e2->k == VKINT && luaK_exp2K(fs, e2))  /* K operand? */
    codebinK(fs, opr, e1, e2, flip, line);
  else  /* no constants */
    codebinNoK(fs, opr, e1, e2, flip, line);
}


/*
** Emit code for order comparisons. When using an immediate operand,
** 'isfloat' tells whether the original value was a float.
*/
static void codeorder (FuncState *fs, BinOpr opr, expdesc *e1, expdesc *e2) {
  int r1, r2;
  int im;
  int isfloat = 0;
  OpCode op;
  if (isSCnumber(e2, &im, &isfloat)) {
    /* use immediate operand */
    r1 = luaK_exp2anyreg(fs, e1);
    r2 = im;
    op = binopr2op(opr, OPR_LT, OP_LTI);
  }
  else if (isSCnumber(e1, &im, &isfloat)) {
    /* transform (A < B) to (B > A) and (A <= B) to (B >= A) */
    r1 = luaK_exp2anyreg(fs, e2);
    r2 = im;
    op = binopr2op(opr, OPR_LT, OP_GTI);
  }
  else {  /* regular case, compare two registers */
    r1 = luaK_exp2anyreg(fs, e1);
    r2 = luaK_exp2anyreg(fs, e2);
    op = binopr2op(opr, OPR_LT, OP_LT);
  }
  freeexps(fs, e1, e2);
  e1->u.info = condjump(fs, op, r1, r2, isfloat, 1);
  e1->k = VJMP;
}


/*
** Emit code for equality comparisons ('==', '~=').
** 'e1' was already put as RK by 'luaK_infix'.
*/
static void codeeq (FuncState *fs, BinOpr opr, expdesc *e1, expdesc *e2) {
  int r1, r2;
  int im;
  int isfloat = 0;  /* not needed here, but kept for symmetry */
  OpCode op;
  if (e1->k != VNONRELOC) {
    ((void)0);
    swapexps(e1, e2);
  }
  r1 = luaK_exp2anyreg(fs, e1);  /* 1st expression must be in register */
  if (isSCnumber(e2, &im, &isfloat)) {
    op = OP_EQI;
    r2 = im;  /* immediate operand */
  }
  else if (exp2RK(fs, e2)) {  /* 2nd expression is constant? */
    op = OP_EQK;
    r2 = e2->u.info;  /* constant index */
  }
  else {
    op = OP_EQ;  /* will compare two registers */
    r2 = luaK_exp2anyreg(fs, e2);
  }
  freeexps(fs, e1, e2);
  e1->u.info = condjump(fs, op, r1, r2, isfloat, (opr == OPR_EQ));
  e1->k = VJMP;
}


/*
** Apply prefix operation 'op' to expression 'e'.
*/
void luaK_prefix (FuncState *fs, UnOpr opr, expdesc *e, int line) {
  static const expdesc ef = {VKINT, {0}, (-1), (-1)};
  luaK_dischargevars(fs, e);
  switch (opr) {
    case OPR_MINUS: case OPR_BNOT:  /* use 'ef' as fake 2nd operand */
      if (constfolding(fs, ((int)((opr + 12))), e, &ef))
        break;
      /* else */ /* FALLTHROUGH */
    case OPR_LEN:
      codeunexpval(fs, unopr2op(opr), e, line);
      break;
    case OPR_NOT: codenot(fs, e); break;
    default: ((void)0);
  }
}


/*
** Process 1st operand 'v' of binary operation 'op' before reading
** 2nd operand.
*/
void luaK_infix (FuncState *fs, BinOpr op, expdesc *v) {
  luaK_dischargevars(fs, v);
  switch (op) {
    case OPR_AND: {
      luaK_goiftrue(fs, v);  /* go ahead only if 'v' is true */
      break;
    }
    case OPR_OR: {
      luaK_goiffalse(fs, v);  /* go ahead only if 'v' is false */
      break;
    }
    case OPR_CONCAT: {
      luaK_exp2nextreg(fs, v);  /* operand must be on the stack */
      break;
    }
    case OPR_ADD: case OPR_SUB:
    case OPR_MUL: case OPR_DIV: case OPR_IDIV:
    case OPR_MOD: case OPR_POW:
    case OPR_BAND: case OPR_BOR: case OPR_BXOR:
    case OPR_SHL: case OPR_SHR: {
      if (!tonumeral(v, 0))
        luaK_exp2anyreg(fs, v);
      /* else keep numeral, which may be folded or used as an immediate
         operand */
      break;
    }
    case OPR_EQ: case OPR_NE: {
      if (!tonumeral(v, 0))
        exp2RK(fs, v);
      /* else keep numeral, which may be an immediate operand */
      break;
    }
    case OPR_LT: case OPR_LE:
    case OPR_GT: case OPR_GE: {
      int dummy, dummy2;
      if (!isSCnumber(v, &dummy, &dummy2))
        luaK_exp2anyreg(fs, v);
      /* else keep numeral, which may be an immediate operand */
      break;
    }
    default: ((void)0);
  }
}

/*
** Create code for '(e1 .. e2)'.
** For '(e1 .. e2.1 .. e2.2)' (which is '(e1 .. (e2.1 .. e2.2))',
** because concatenation is right associative), merge both CONCATs.
*/
static void codeconcat (FuncState *fs, expdesc *e1, expdesc *e2, int line) {
  Instruction *ie2 = previousinstruction(fs);
  if ((((OpCode)(((*ie2)>>0) & ((~((~(Instruction)0)<<(7)))<<(0))))) == OP_CONCAT) {  /* is 'e2' a concatenation? */
    int n = 	(((void)0), ((((int)((((*ie2)>>((((0 + 7) + 8) + 1))) & ((~((~(Instruction)0)<<(8)))<<(0))))))));  /* # of elements concatenated in 'e2' */
    ((void)0);
    freeexp(fs, e2);
    ((*ie2) = (((*ie2)&(~((~((~(Instruction)0)<<(8)))<<((0 + 7))))) |                 ((((Instruction)((e1->u.info)))<<(0 + 7))&((~((~(Instruction)0)<<(8)))<<((0 + 7))))));  /* correct first element ('e1') */
    ((*ie2) = (((*ie2)&(~((~((~(Instruction)0)<<(8)))<<((((0 + 7) + 8) + 1))))) |                 ((((Instruction)((n + 1)))<<(((0 + 7) + 8) + 1))&((~((~(Instruction)0)<<(8)))<<((((0 + 7) + 8) + 1))))));  /* will concatenate one more element */
  }
  else {  /* 'e2' is not a concatenation */
    luaK_codeABCk(fs,OP_CONCAT,e1->u.info,2,0,0);  /* new concat opcode */
    freeexp(fs, e2);
    luaK_fixline(fs, line);
  }
}


/*
** Finalize code for binary operation, after reading 2nd operand.
*/
void luaK_posfix (FuncState *fs, BinOpr opr,
                  expdesc *e1, expdesc *e2, int line) {
  luaK_dischargevars(fs, e2);
  if (((opr) <= OPR_SHR) && constfolding(fs, ((int)((opr + 0	))), e1, e2))
    return;  /* done by folding */
  switch (opr) {
    case OPR_AND: {
      ((void)0);  /* list closed by 'luaK_infix' */
      luaK_concat(fs, &e2->f, e1->f);
      *e1 = *e2;
      break;
    }
    case OPR_OR: {
      ((void)0);  /* list closed by 'luaK_infix' */
      luaK_concat(fs, &e2->t, e1->t);
      *e1 = *e2;
      break;
    }
    case OPR_CONCAT: {  /* e1 .. e2 */
      luaK_exp2nextreg(fs, e2);
      codeconcat(fs, e1, e2, line);
      break;
    }
    case OPR_ADD: case OPR_MUL: {
      codecommutative(fs, opr, e1, e2, line);
      break;
    }
    case OPR_SUB: {
      if (finishbinexpneg(fs, e1, e2, OP_ADDI, line, TM_SUB))
        break; /* coded as (r1 + -I) */
      /* ELSE */
    }  /* FALLTHROUGH */
    case OPR_DIV: case OPR_IDIV: case OPR_MOD: case OPR_POW: {
      codearith(fs, opr, e1, e2, 0, line);
      break;
    }
    case OPR_BAND: case OPR_BOR: case OPR_BXOR: {
      codebitwise(fs, opr, e1, e2, line);
      break;
    }
    case OPR_SHL: {
      if (isSCint(e1)) {
        swapexps(e1, e2);
        codebini(fs, OP_SHLI, e1, e2, 1, line, TM_SHL);  /* I << r2 */
      }
      else if (finishbinexpneg(fs, e1, e2, OP_SHRI, line, TM_SHL)) {
        /* coded as (r1 >> -I) */;
      }
      else  /* regular case (two registers) */
       codebinexpval(fs, opr, e1, e2, line);
      break;
    }
    case OPR_SHR: {
      if (isSCint(e2))
        codebini(fs, OP_SHRI, e1, e2, 0, line, TM_SHR);  /* r1 >> I */
      else  /* regular case (two registers) */
        codebinexpval(fs, opr, e1, e2, line);
      break;
    }
    case OPR_EQ: case OPR_NE: {
      codeeq(fs, opr, e1, e2);
      break;
    }
    case OPR_GT: case OPR_GE: {
      /* '(a > b)' <=> '(b < a)';  '(a >= b)' <=> '(b <= a)' */
      swapexps(e1, e2);
      opr = ((BinOpr)((opr - OPR_GT) + OPR_LT));
    }  /* FALLTHROUGH */
    case OPR_LT: case OPR_LE: {
      codeorder(fs, opr, e1, e2);
      break;
    }
    default: ((void)0);
  }
}


/*
** Change line information associated with current position, by removing
** previous info and adding it again with new line.
*/
void luaK_fixline (FuncState *fs, int line) {
  removelastlineinfo(fs);
  savelineinfo(fs, fs->f, line);
}


void luaK_settablesize (FuncState *fs, int pc, int ra, int asize, int hsize) {
  Instruction *inst = &fs->f->code[pc];
  int extra = asize / (((1<<10)-1) + 1);  /* higher bits of array size */
  int rc = asize % (((1<<10)-1) + 1);  /* lower bits of array size */
  int k = (extra > 0);  /* true iff needs extra argument */
  hsize = (hsize != 0) ? luaO_ceillog2(((unsigned int)((hsize)))) + 1 : 0;
  *inst = ((((Instruction)((OP_NEWTABLE)))<<0) 			| (((Instruction)((ra)))<<(0 + 7)) 			| (((Instruction)((hsize)))<<(((0 + 7) + 8) + 1)) 			| (((Instruction)((rc)))<<((((0 + 7) + 8) + 1) + 6)) 			| (((Instruction)((k)))<<((0 + 7) + 8)));
  *(inst + 1) = ((((Instruction)((OP_EXTRAARG)))<<0) 			| (((Instruction)((extra)))<<(0 + 7)));
}


/*
** Emit a SETLIST instruction.
** 'base' is register that keeps table;
** 'nelems' is #table plus those to be stored now;
** 'tostore' is number of values (in registers 'base + 1',...) to add to
** table (or LUA_MULTRET to add up to stack top).
*/
void luaK_setlist (FuncState *fs, int base, int nelems, int tostore) {
  ((void)0);
  if (tostore == (-1))
    tostore = 0;
  if (nelems <= ((1<<10)-1))
    luaK_codevABCk(fs, OP_SETLIST, base, tostore, nelems, 0);
  else {
    int extra = nelems / (((1<<10)-1) + 1);
    nelems %= (((1<<10)-1) + 1);
    luaK_codevABCk(fs, OP_SETLIST, base, tostore, nelems, 1);
    codeextraarg(fs, extra);
  }
  fs->freereg = ((lu_byte)((base + 1)));  /* free registers with list values */
}


/*
** return the final target of a jump (skipping jumps to jumps)
*/
static int finaltarget (Instruction *code, int i) {
  int count;
  for (count = 0; count < 100; count++) {  /* avoid infinite loops */
    Instruction pc = code[i];
    if ((((OpCode)(((pc)>>0) & ((~((~(Instruction)0)<<(7)))<<(0))))) != OP_JMP)
      break;
    else
      i += 	(((void)0), ((((int)((((pc)>>((0 + 7))) & ((~((~(Instruction)0)<<(((8 + 8 + 1) + 8))))<<(0)))))) - (((1 << ((8 + 8 + 1) + 8)) - 1) >> 1))) + 1;
  }
  return i;
}


/*
** Do a final pass over the code of a function, doing small peephole
** optimizations and adjustments.
*/
/*
** $Id: lopnames.h $
** Opcode names
** See Copyright Notice in lua.h
*/







typedef unsigned long size_t;







/* ORDER OP */

static const char *const opnames[] = {
  "MOVE",
  "LOADI",
  "LOADF",
  "LOADK",
  "LOADKX",
  "LOADFALSE",
  "LFALSESKIP",
  "LOADTRUE",
  "LOADNIL",
  "GETUPVAL",
  "SETUPVAL",
  "GETTABUP",
  "GETTABLE",
  "GETI",
  "GETFIELD",
  "SETTABUP",
  "SETTABLE",
  "SETI",
  "SETFIELD",
  "NEWTABLE",
  "SELF",
  "ADDI",
  "ADDK",
  "SUBK",
  "MULK",
  "MODK",
  "POWK",
  "DIVK",
  "IDIVK",
  "BANDK",
  "BORK",
  "BXORK",
  "SHLI",
  "SHRI",
  "ADD",
  "SUB",
  "MUL",
  "MOD",
  "POW",
  "DIV",
  "IDIV",
  "BAND",
  "BOR",
  "BXOR",
  "SHL",
  "SHR",
  "MMBIN",
  "MMBINI",
  "MMBINK",
  "UNM",
  "BNOT",
  "NOT",
  "LEN",
  "CONCAT",
  "CLOSE",
  "TBC",
  "JMP",
  "EQ",
  "LT",
  "LE",
  "EQK",
  "EQI",
  "LTI",
  "LEI",
  "GTI",
  "GEI",
  "TEST",
  "TESTSET",
  "CALL",
  "TAILCALL",
  "RETURN",
  "RETURN0",
  "RETURN1",
  "FORLOOP",
  "FORPREP",
  "TFORPREP",
  "TFORCALL",
  "TFORLOOP",
  "SETLIST",
  "CLOSURE",
  "VARARG",
  "GETVARG",
  "ERRNNIL",
  "VARARGPREP",
  "EXTRAARG",
  0
};




void luaK_finish (FuncState *fs) {
  int i;
  Proto *p = fs->f;
  if (p->flag & 2  )  /* will it use a vararg table? */
    p->flag &= ((lu_byte)((~1  )));  /* then it will not use hidden args. */
  for (i = 0; i < fs->pc; i++) {
    Instruction *pc = &p->code[i];
    /* avoid "not used" warnings when assert is off (for 'onelua.c') */
    (void)luaP_isOT; (void)luaP_isIT;
    ((void)0);
    switch ((((OpCode)(((*pc)>>0) & ((~((~(Instruction)0)<<(7)))<<(0)))))) {
      case OP_RETURN0: case OP_RETURN1: {
        if (!(fs->needclose || (p->flag & 1  )))
          break;  /* no extra work */
        /* else use OP_RETURN to do the extra work */
        ((*pc) = (((*pc)&(~((~((~(Instruction)0)<<(7)))<<(0)))) | 		((((Instruction)((OP_RETURN)))<<0)&((~((~(Instruction)0)<<(7)))<<(0)))));
      }  /* FALLTHROUGH */
      case OP_RETURN: case OP_TAILCALL: {
        if (fs->needclose)
          ((*pc) = (((*pc)&(~((~((~(Instruction)0)<<(1)))<<(((0 + 7) + 8))))) |                 ((((Instruction)((1)))<<((0 + 7) + 8))&((~((~(Instruction)0)<<(1)))<<(((0 + 7) + 8))))));  /* signal that it needs to close */
        if (p->flag & 1  )  /* does it use hidden arguments? */
          ((*pc) = (((*pc)&(~((~((~(Instruction)0)<<(8)))<<(((((0 + 7) + 8) + 1) + 8))))) |                 ((((Instruction)((p->numparams + 1)))<<((((0 + 7) + 8) + 1) + 8))&((~((~(Instruction)0)<<(8)))<<(((((0 + 7) + 8) + 1) + 8))))));  /* signal that */
        break;
      }
      case OP_GETVARG: {
        if (p->flag & 2  )  /* function has a vararg table? */
          ((*pc) = (((*pc)&(~((~((~(Instruction)0)<<(7)))<<(0)))) | 		((((Instruction)((OP_GETTABLE)))<<0)&((~((~(Instruction)0)<<(7)))<<(0)))));  /* must get vararg there */
        break;
      }
      case OP_VARARG: {
        if (p->flag & 2  )  /* function has a vararg table? */
          ((*pc) = (((*pc)&(~((~((~(Instruction)0)<<(1)))<<(((0 + 7) + 8))))) |                 ((((Instruction)((1)))<<((0 + 7) + 8))&((~((~(Instruction)0)<<(1)))<<(((0 + 7) + 8))))));  /* must get vararg there */
        break;
      }
      case OP_JMP: {  /* to optimize jumps to jumps */
        int target = finaltarget(p->code, i);
        fixjump(fs, i, target);  /* jump directly to final target */
        break;
      }
      default: break;
    }
  }
}
