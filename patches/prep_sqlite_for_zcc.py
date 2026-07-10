#!/usr/bin/env python3
"""Preprocess sqlite3.c for ZCC compilation.

Usage:
    gcc -E -P -DSQLITE_OMIT_LOAD_EXTENSION \
        -DSQLITE_THREADSAFE=0 -DSQLITE_OMIT_WAL -DSQLITE_OMIT_AUTHORIZATION \
        -DSQLITE_OMIT_DEPRECATED -DSQLITE_OMIT_PROGRESS_CALLBACK \
        -DSQLITE_OMIT_TRACE sqlite3.c -o sqlite3_pp.c
    python3 prep_sqlite_for_zcc.py sqlite3_pp.c sqlite3_zcc.c
"""
import re, sys

infile = sys.argv[1] if len(sys.argv) > 1 else 'sqlite3_pp.c'
outfile = sys.argv[2] if len(sys.argv) > 2 else 'sqlite3_zcc.c'

src = open(infile).read()

# 1. Strip __attribute__((...))
def strip_attributes(s):
    result = []; i = 0; attr = '__attribute__'
    while i < len(s):
        if s[i:i+len(attr)] == attr:
            i += len(attr)
            while i < len(s) and s[i] in ' \t\n': i += 1
            if i < len(s) and s[i] == '(':
                depth = 1; i += 1
                while i < len(s) and depth > 0:
                    if s[i] == '(': depth += 1
                    elif s[i] == ')': depth -= 1
                    i += 1
            continue
        result.append(s[i]); i += 1
    return ''.join(result)

src = strip_attributes(src)

# 2. Strip GCC extensions
src = re.sub(r'__asm__\s*\([^)]*\)', '', src)
src = re.sub(r'__asm\s*\([^)]*\)', '', src)
src = src.replace('__restrict', '')
src = src.replace('__extension__', '')
src = src.replace('__inline__', '')
src = src.replace('__inline', '')
src = re.sub(r'\bvolatile\b', '', src)
src = re.sub(r'\brestrict\b', '', src)

# 7. Function pointer syntax fixes
src = src.replace('void (*sqlite3_wsd_init(int N, int J))(void)', 'sqlite3_syscall_ptr sqlite3_wsd_init(int N, int J)')
# memdbDlSym — both the definition (with {) and the forward declaration (with ;)
src = src.replace('static void (*memdbDlSym(sqlite3_vfs *pVfs, void *p, const char *zSym))(void){', 'static sqlite3_syscall_ptr memdbDlSym(sqlite3_vfs *pVfs, void *p, const char *zSym){')
src = src.replace('static void (*memdbDlSym(sqlite3_vfs *pVfs, void *p, const char*zSym))(void);', 'static sqlite3_syscall_ptr memdbDlSym(sqlite3_vfs *pVfs, void *p, const char*zSym);')
src = src.replace('static void (*memdbDlSym(sqlite3_vfs*,void*,const char*))(void);', 'static sqlite3_syscall_ptr memdbDlSym(sqlite3_vfs*,void*,const char*);')
# Catch any remaining function-returning-function-pointer variants
import re as _re
src = _re.sub(r'static void \(\*memdbDlSym\([^)]*\)\)\(void\)([;{])',
              lambda m: 'static sqlite3_syscall_ptr memdbDlSym(sqlite3_vfs*, void*, const char*)' + m.group(1),
              src)
src = src.replace('void (*)(void)', 'sqlite3_syscall_ptr')

# 3. Replace __builtins
src = re.sub(r'__builtin_offsetof\s*\(\s*([^,]+)\s*,\s*([^)]+)\)', r'((long)&((\1*)0)->\2)', src)
src = re.sub(r'__builtin_bswap32\s*\(([^)]+)\)', r'zcc_bswap32(\1)', src)
src = re.sub(r'__builtin_bswap16\s*\(([^)]+)\)', r'zcc_bswap16(\1)', src)
src = re.sub(r'__builtin_bswap64\s*\(([^)]+)\)', r'zcc_bswap64(\1)', src)
src = re.sub(r'__builtin_add_overflow\s*\(([^,]+),([^,]+),([^)]+)\)', r'((*(\3) = (\1)+(\2)), 0)', src)
src = re.sub(r'__builtin_sub_overflow\s*\(([^,]+),([^,]+),([^)]+)\)', r'((*(\3) = (\1)-(\2)), 0)', src)
src = re.sub(r'__builtin_mul_overflow\s*\(([^,]+),([^,]+),([^)]+)\)', r'((*(\3) = (\1)*(\2)), 0)', src)
src = re.sub(r'__builtin_clzll\s*\(([^)]+)\)', r'zcc_clzll(\1)', src)
src = src.replace('__builtin_free', 'free')
src = re.sub(r'__builtin_inff\s*\([^)]*\)', '(1e300)', src)
# Replace GCC atomic builtins -- safe for SQLITE_THREADSAFE=0 (single-threaded)
src = re.sub(r'__atomic_store_n\s*\(([^,]+),\s*([^,]+),\s*[^)]+\)', r'(*(\1) = (\2))', src)
src = re.sub(r'__atomic_load_n\s*\(([^,]+),\s*[^)]+\)', r'(*(\1))', src)
src = re.sub(r'__atomic_fetch_add\s*\(([^,]+),\s*([^,]+),\s*[^)]+\)', r'((*(\1))++, (*(\1)) - (\2))', src)
src = re.sub(r'__atomic_fetch_sub\s*\(([^,]+),\s*([^,]+),\s*[^)]+\)', r'((*(\1))--, (*(\1)) + (\2))', src)

# 4. Fix va_list and type casts
src = src.replace('(signed)', '(int)')
src = src.replace('__gnuc_va_list', 'va_list')

# 5. Fix type ordering
src = re.sub(r'\blong\s+unsigned\s+int\b', 'unsigned long int', src)
src = re.sub(r'\blong\s+unsigned\b', 'unsigned long', src)
src = re.sub(r'\bshort\s+unsigned\s+int\b', 'unsigned short int', src)
src = re.sub(r'\bshort\s+unsigned\b', 'unsigned short', src)
src = re.sub(r'\blong\s+long\s+unsigned\s+int\b', 'unsigned long long int', src)
src = re.sub(r'\blong\s+long\s+unsigned\b', 'unsigned long long', src)

# 5.5 Float types hack for ZCC
src = re.sub(r'\blong\s+double\b', 'double', src)
src = re.sub(r'\b_Float\w+\b', 'double', src)


# 6. Precompute constant array sizes
def eval_arr(m):
    try:
        v = eval(m.group(1).strip())
        if isinstance(v,(int,float)) and v==int(v) and int(v)>0: return f'[{int(v)}]'
    except: pass
    return m.group(0)
src = re.sub(r'\[([^\]]+)\]', eval_arr, src)

# 7. Fix sizeof in array sizes
def fix_sizeof_arrays(m):
    expr = m.group(1)
    expr = re.sub(r'sizeof\s*\(\s*int\s*\)', '4', expr)
    expr = re.sub(r'sizeof\s*\(\s*void\s*\*\s*\)', '8', expr)
    expr = re.sub(r'sizeof\s*\(\s*long\s*\)', '8', expr)
    expr = re.sub(r'sizeof\s*\(\s*size_t\s*\)', '8', expr)
    expr = re.sub(r'sizeof\s*\(\s*unsigned\s+long(\s+int)?\s*\)', '8', expr)
    expr = re.sub(r'sizeof\s*\(\s*unsigned\s+int\s*\)', '4', expr)
    expr = re.sub(r'sizeof\s*\(\s*__fd_mask\s*\)', '8', expr)
    # Also strip casts like (int) to allow eval to work
    expr = re.sub(r'\(\s*int\s*\)', '', expr)
    try:
        v = eval(expr)
        if isinstance(v,(int,float)) and v==int(v) and int(v)>0: return f'[{int(v)}]'
    except: pass
    return m.group(0)
src = re.sub(r'\[([^\]]*sizeof[^\]]*)\]', fix_sizeof_arrays, src)

# Bypass sizeof logic struct array size
src = src.replace('((int)(sizeof(sqlite3Stat.nowValue)/sizeof(sqlite3Stat.nowValue[0])))', '10')

# Strip bitfields safely: match types commonly used in bitfields (or missing names)
src = re.sub(r'((?:unsigned\s+|signed\s+)?(?:int|char|short|long|u8|u16|u32|unsigned|signed)\s+\w*\s*):\s*\d+\s*;', r'\1;', src)
src = re.sub(r'((?:unsigned\s+|signed\s+)?(?:int|char|short|long|u8|u16|u32|unsigned|signed)\s+):\s*\d+\s*;', r'\1;', src)

# Strip trailing commas in struct/array initializers (fixes unexpected TK_COMMA)
src = re.sub(r',\s*\}', '}', src)

# 8b. Fix global pointer-to-array-offset initializers.
src = src.replace(
    'static const unsigned char *sqlite3aLTb = &sqlite3UpperToLower[204];',
    '#define sqlite3aLTb (&sqlite3UpperToLower[204])'
)
src = src.replace(
    'static const unsigned char *sqlite3aEQb = &sqlite3UpperToLower[210];',
    '#define sqlite3aEQb (&sqlite3UpperToLower[210])'
)
src = src.replace(
    'static const unsigned char *sqlite3aGTb = &sqlite3UpperToLower[216];',
    '#define sqlite3aGTb (&sqlite3UpperToLower[216])'
)

# 8. Fix double function pointer
src = re.sub(r'\w+\s*\(\*\(\*(\w+)\)\(([^)]*)\)\)\([^)]*\)\s*;',
             r'void *(*\1)(\2);', src)

# 9. Shims
shims = """/* ZCC compatibility shims */
typedef struct { unsigned int gp_offset; unsigned int fp_offset; void *overflow_arg_area; void *reg_save_area; } va_list[1];
typedef va_list __builtin_va_list;
static unsigned int zcc_bswap32(unsigned int x){return((x>>24)&0xff)|((x>>8)&0xff00)|((x<<8)&0xff0000)|((x<<24)&0xff000000u);}
static unsigned short zcc_bswap16(unsigned short x){return(x>>8)|(x<<8);}
static unsigned long zcc_bswap64(unsigned long x){return zcc_bswap32(x>>32)|((unsigned long)zcc_bswap32(x)<<32);}
static int zcc_clzll(unsigned long long x){int n=0;if(!x)return 64;while(!(x&(1ULL<<63))){n++;x<<=1;}return n;}

"""
src = shims + src

# Strip glibc fopencookie stuff entirely 
src = re.sub(r'typedef\s+__ssize_t\s+cookie_read_function_t[^;]+;', '/* removed */', src)
src = re.sub(r'typedef\s+__ssize_t\s+cookie_write_function_t[^;]+;', '/* removed */', src)
src = re.sub(r'typedef\s+int\s+cookie_seek_function_t[^;]+;', '/* removed */', src)
src = re.sub(r'typedef\s+int\s+cookie_close_function_t[^;]+;', '/* removed */', src)
src = re.sub(r'typedef\s+struct\s+_IO_cookie_io_functions_t[^{]*\{[^}]*\}[^;]*;', '/* removed */', src)
src = re.sub(r'extern\s+FILE\s*\*\s*fopencookie\s*\([^;]+;', '/* removed */', src)

open(outfile, 'w').write(src)
print(f"Output: {outfile} — {src.count(chr(10))} lines, {len(src)} bytes")
