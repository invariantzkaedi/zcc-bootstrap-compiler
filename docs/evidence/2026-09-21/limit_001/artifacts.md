# LIMIT-001 Artifacts & Lineage Ledger (Hardened)

## Verified Source Artifacts

| Artifact | Path | Description |
|---|---|---|
| `zcc_inline_asm.h` | `zcc_inline_asm.h` | Modular extended inline asm parser & codegen engine (Hardened: named operands, immediate literals, callee-saved preservation, formatting cleanup) |
| `part1.c` | `part1.c` | `AsmOperand` structure and `Node` operand table fields |
| `part3.c` | `part3.c` | Colon-delimited operand section parser integration (38 lines diff, `< 50` lines constraint) |
| `part4.c` | `part4.c` | Codegen invocation and `variable_is_read` DCE liveness fix (48 lines diff, `< 50` lines constraint) |
| `tests/probe_inline_asm.c` | `tests/probe_inline_asm.c` | Rule EF-3 extended inline assembly test gauntlet (13 test cases) |
| `BOOTSTRAP_BASELINES.tsv` | `BOOTSTRAP_BASELINES.tsv` | Registered hardened selfhost baseline hash `4202b5e2ca1046c73f547946b766e84b` |

## Checksums (MD5)

```text
4202b5e2ca1046c73f547946b766e84b  zcc2.s
4202b5e2ca1046c73f547946b766e84b  zcc3.s
```

## Assembly Excerpts (Rule EF-2 Compliance)

Excerpts from `/tmp/probe_asm.s`:
```asm
    # Case 6 & 7: Named operands %[sum], %[delta] and %k[src], %[dst]
    movq %rax, %rdx
    movl %edx, %eax
    movl %eax, -112(%rbp)

    # Case 8: Immediate operand ("i")
    imull $10, %eax
    movl %eax, -120(%rbp)

    # Case 9: Direct memory operand ("m")
    movl -128(%rbp), %eax
    movl %eax, -136(%rbp)

    # Case 10: Callee-saved register push/pop preservation
    pushq %rbx
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    mov $0x111, %rbx
    mov $0x222, %r12
    mov $0x333, %r13
    mov $0x444, %r14
    mov $0x555, %r15
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbx

    # Case 11: Trailing semicolon and whitespace stripped
    add $25, %eax
    movl %eax, -144(%rbp)

    # Case 12: Hardware primitive: rdtsc
    rdtsc
    movl %eax, -152(%rbp)
    movl %edx, -160(%rbp)

    # Case 13: Hardware primitive: 4-register cpuid with preserved rbx
    pushq %rbx
    xorl %eax, %eax
    cpuid
    movl %eax, -168(%rbp)
    movl %ebx, -176(%rbp)
    movl %ecx, -184(%rbp)
    movl %edx, -192(%rbp)
    popq %rbx
```
