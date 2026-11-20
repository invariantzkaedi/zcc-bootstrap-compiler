# ZCC Native Microsoft x64 ABI setjmp / longjmp (`src/win64_setjmp.s`)
# ===================================================================
# Preserves Win64 GPR callee-saved registers (%rbx, %rbp, %rdi, %rsi,
# %r12-%r15), caller %rsp (post-ret), and return %rip in 80 bytes inside
# C jmp_buf[25] without invoking msvcrt RtlUnwindEx SEH frames.
.text
.globl setjmp
.globl _setjmp
setjmp:
_setjmp:
    movq %rbx, 0(%rcx)
    movq %rbp, 8(%rcx)
    movq %rdi, 16(%rcx)
    movq %rsi, 24(%rcx)
    movq %r12, 32(%rcx)
    movq %r13, 40(%rcx)
    movq %r14, 48(%rcx)
    movq %r15, 56(%rcx)
    leaq 8(%rsp), %rax
    movq %rax, 64(%rcx)
    movq 0(%rsp), %rax
    movq %rax, 72(%rcx)
    xorl %eax, %eax
    ret

.globl longjmp
.globl _longjmp
longjmp:
_longjmp:
    movl %edx, %eax
    testl %eax, %eax
    jne .Lwin64_lj_nonzero
    movl $1, %eax
.Lwin64_lj_nonzero:
    movq 0(%rcx), %rbx
    movq 8(%rcx), %rbp
    movq 16(%rcx), %rdi
    movq 24(%rcx), %rsi
    movq 32(%rcx), %r12
    movq 40(%rcx), %r13
    movq 48(%rcx), %r14
    movq 56(%rcx), %r15
    movq 72(%rcx), %rdx
    movq 64(%rcx), %rsp
    pushq %rdx
    ret
