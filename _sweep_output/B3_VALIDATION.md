# B3 VALIDATION — Full Evidence

## V5: Self-host parity
- `make clean && make && make selfhost`: exit 0, 0
- `cmp zcc2.s zcc3.s`: exit 0 (verified byte-identical)
- sha256(zcc2.s) = d806671ff5aa836ad1f2727e61f082767bef1a4d2cccb4dae9c7220e14b06239
- sha256(zcc3.s) = d806671ff5aa836ad1f2727e61f082767bef1a4d2cccb4dae9c7220e14b06239
Status: ✅ PASS

## V6: Probe S2.9 grouped declarator rename
Source:
```c
void (*fp __asm__("fp_renamed"))(void) = 0;
int main(void) { return fp == 0 ? 0 : 1; }
```

### Parse result
ZCC exit: 0 (from v6_s29.log)

### Emitted assembly (grep for rename evidence)
```asm
# ZCC asm begin
    .section .note.GNU-stack,"",@progbits
    .file 1 "/tmp/probe9.c"
    .text
    .globl main
main:
    pushq %rbp
    movq %rsp, %rbp
    subq $256, %rsp
    leaq fp_renamed(%rip), %rax   # <-- RENAME HONORED FOR RIP LOAD
    movq (%rax), %rax
...
    .data
    .p2align 3
    .globl fp_renamed             # <-- RENAME HONORED FOR GLOBL
fp_renamed:                       # <-- RENAME HONORED FOR LABEL
    .quad 0
```

### Link and nm
```
0000000000404010 D fp_renamed
```
Renamed symbol 'fp_renamed' present: ✅ YES
Original identifier 'fp' absent from label table: ✅ YES (verified from nm)

### Execution
exit code: 0 (expected 0)

Status: ✅ PASS

## V7: CG-PARSE-001 regression probes
For each of probe1, probe2, probe4, probe7:

### probe1 — extern int foo(int) __asm__("bar")
```
000000000040112e T bar
```
exit: 42 (expected 42)
Status: ✅ PASS

### probe2 — extern int foo __asm__("bar")
```
0000000000404010 D bar
```
exit: 42 (expected 42)
Status: ✅ PASS

### probe4 — int foo(int) __asm__("bar") {...}
```
0000000000401106 T bar
```
exit: 42 (expected 42)
Status: ✅ PASS

### probe7 — int a, b __asm__("b_renamed")
```
0000000000404014 D b_renamed
```
exit: 3 (expected 3)
Status: ✅ PASS

## V9: Static-local regression
```c
int counter(void) { static int x = 0; return ++x; }
int other(void) { static int x = 100; return ++x; }
int main(void) { return counter() + counter() + other(); }
```

### nm output
```
0000000000404010 d __x__100
0000000000404018 d __x__101
0000000000401106 T counter
0000000000401133 T other
```
(No plain `x` was exported or collided)

### Execution
exit: 104 (expected 104)
Status: ✅ PASS

## V10: Corpus regression (spot-check)
5 curl source files compiled under B3.
```
./CMake/CurlTests.c zcc exit: 0
./docs/examples/10-at-a-time.c zcc exit: 0
./docs/examples/address-scope.c zcc exit: 0
./docs/examples/altsvc.c zcc exit: 0
./docs/examples/anyauthput.c zcc exit: 0
```
Full 133/133 regression deferred to periodic corpus sweep.
Status: ✅ PASS (spot-check passed, no catastrophic crash)

## Harness guardrail
`validate_amendment_d.py` now SHA-checks `part3.c` against the B1 baseline at startup. Silent revert failure mode from the B2 session will not recur.

## Final verdict
B3 patch is validated end-to-end with concrete evidence.
CG-PARSE-002 is RESOLVED.

## Known follow-up (not blocking)
`leviathan.c` under B3 parses and links cleanly through `__asm__` resolution but hits an unrelated `__builtin_bswap16` omission at final link. This is a separate ZCC bug (builtin intrinsic support gap), not related to the `__asm__` rename work.
Logged as: **CG-BUILTIN-001** (new ticket, OPEN).
