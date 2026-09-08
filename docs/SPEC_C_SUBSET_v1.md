# Specification: ZCC Supported C Language Subset (v1.0)

**Document ID**: `SPEC-C-SUBSET-v1`  
**Status**: ACTIVE / CANONICAL BOUNDARY  
**Target Architecture**: x86-64 (System V AMD64 ABI)  

---

## 1. Scope & Epistemic Boundary

This specification formally defines the grammatical and semantic subset of C supported by the ZCC v1 compiler. Claims of general C compliance are intentionally rejected. Only programs adhering strictly to the constraints outlined in this document are guaranteed to compile, link, and execute with semantic equivalence to GCC 13.x and Clang 18.x.

---

## 2. Supported Language Constructs

### 2.1 Types & Declarations
- **Scalar Types**:
  - `char`, `signed char`, `unsigned char` (8-bit)
  - `short`, `unsigned short` (16-bit)
  - `int`, `unsigned int` (32-bit)
  - `long`, `unsigned long`, `long long`, `unsigned long long` (64-bit)
  - `uint8_t`, `uint16_t`, `uint32_t`, `uint64_t`, `int8_t`, `int16_t`, `int32_t`, `int64_t`, `size_t`
  - `void` (for function returns and opaque pointers `void*`)
- **Pointers**:
  - Single indirection (`T*`) and double indirection (`T**`) for scalar and aggregate types.
  - Pointer arithmetic: addition (`ptr + offset`), subtraction (`ptr - offset`, `ptr1 - ptr2`).
  - Pointer dereferencing (`*ptr`, `ptr[idx]`).
  - Address-of operator (`&var`).
- **Arrays**:
  - One-dimensional arrays with statically known bounds (`T arr[N]`).
  - Standard decay to pointer on function argument passing.
- **Structures**:
  - Standard `struct Name { ... }` with scalar and pointer fields.
  - Dot member access (`s.field`) and arrow member access (`ptr->field`).
  - Basic struct assignment and copy (`s1 = s2`).

### 2.2 Expressions & Operators
- **Arithmetic**:
  - Binary addition (`+`), subtraction (`-`), multiplication (`*`), division (`/`), modulo (`%`).
  - Unary plus (`+`), negation (`-`), pre/post increment (`++`), pre/post decrement (`--`).
- **Bitwise**:
  - Bitwise AND (`&`), OR (`|`), XOR (`^`), NOT (`~`).
  - Bitwise left shift (`<<`), logical right shift (unsigned operand), arithmetic right shift (signed operand).
- **Relational & Equality**:
  - Equality (`==`), inequality (`!=`).
  - Relational (`<`, `<=`, `>`, `>=`) adhering to signed vs. unsigned comparison rules.
- **Logical**:
  - Logical AND (`&&`), logical OR (`||`) with standard short-circuit evaluation semantics.
  - Logical NOT (`!`).
- **Assignment**:
  - Simple assignment (`=`).
  - Compound assignment (`+=`, `-=`, `*=`, `/=`, `%=`, `&=`, `|=`, `^=`, `<<=`, `>>=`).
- **Conditionals & Grouping**:
  - Ternary conditional operator (`cond ? expr1 : expr2`).
  - Parenthesized grouping (`(expr)`).
  - Explicit casts (`(type)expr`).

### 2.3 Statements & Control Flow
- **Block Statements**: `{ ... }` with local lexical scoping and variable shadowing.
- **Conditionals**:
  - `if (cond) stmt`
  - `if (cond) stmt1 else stmt2`
- **Loops**:
  - `while (cond) stmt`
  - `for (init; cond; step) stmt`
  - `do stmt while (cond);`
  - `break` and `continue` with proper loop-nest binding.
- **Jumps & Returns**:
  - `return;` and `return expr;`
  - Function epilogue resolution with stack frame teardown.

### 2.4 Calling Convention & ABI (System V AMD64)
- **Argument Passing**:
  - First 6 integer/pointer arguments passed via registers:
    1. `%rdi`
    2. `%rsi`
    3. `%rdx`
    4. `%rcx`
    5. `%r8`
    6. `%r9`
  - Arguments 7+ pushed to the stack in right-to-left order, aligned to 8 bytes.
- **Return Values**:
  - 64-bit scalars and pointers returned in `%rax`.
  - 32-bit scalars returned in `%eax` (with zero-extended upper 32 bits).
  - 16-byte pairs (tested subset) returned in `%rax` (low eightbyte) and `%rdx` (high eightbyte).
- **Stack Alignment**:
  - 16-byte stack frame alignment maintained before any `call` instruction.
  - Callee-saved registers (`%rbx`, `%rbp`, `%r12`, `%r13`, `%r14`, `%r15`) preserved across calls.

---

## 3. Explicit Non-Claims (Outside v1.0 Subset)

The following C constructs are explicitly excluded from the v1.0 verified subset and must not be used in claims of semantic equivalence:

1. **Variable-Length Arrays (VLAs)**: Dynamic allocation of runtime-sized arrays on the stack (`int arr[n]`).
2. **Floating-Point Arithmetic (Extended)**: IEEE-754 `long double` (80-bit x87) or vector SIMD intrinsics beyond SSE2 basic scalar floats.
3. **Complex Designated Initializers**: Nested designated initializers with mixed struct/union punning and out-of-order sparse indices (`.a[3].b = 10`).
4. **Variadic Functions (arbitrary)**: Custom variadic argument implementations using `<stdarg.h>` macros beyond standard `printf`/`snprintf` calls.
5. **GNU Statement Expressions**: `({ int y = foo(); y + 1; })` syntax.
6. **Complex Packed Bitfield Merging**: Mixed-width adjacent bitfields crossing word boundaries without explicit alignment annotations.
7. **Thread Local Storage (TLS)**: `__thread` or `_Thread_local` storage classes.
8. **Setjmp / Longjmp**: Non-local jumps and exception unwinding.

---

## 4. Verification Protocol

Any program claiming conformance to `SPEC-C-SUBSET-v1` must pass:
1. **Compilation**: `zcc prog.c -o prog.s` exits with code 0 without compiler warnings or diagnostics.
2. **Assembly & Link**: `gcc -no-pie prog.s -o prog_zcc -lm` exits with code 0.
3. **Differential Execution Equivalence**:
   - `exit_code(prog_zcc) == exit_code(prog_gcc) == exit_code(prog_clang)`
   - `stdout(prog_zcc) == stdout(prog_gcc) == stdout(prog_clang)`
   - `stderr(prog_zcc) == stderr(prog_gcc) == stderr(prog_clang)`
