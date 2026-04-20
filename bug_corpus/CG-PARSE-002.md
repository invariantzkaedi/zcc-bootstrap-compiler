# CG-PARSE-002: Grouped Declarator `__asm__` Parse Discard/Honor

**Status:** RESOLVED — B3 COMPLETE
**Validation:** `_sweep_output/B3_VALIDATION.md`

## 1. Discovery Context
During the completion of CG-PARSE-001, an extended probe (`S2.9`) matching `void (*fp __asm__("fp_renamed"))(void) = 0;` failed to parse. The grouped declarator (`(*fp)`) inherently isolates identifier parsing from standard `parse_declarator_inner()` flow when hit at the `parse_program` layer.

## 2. Root Cause
In `parse_program`, the grammar looks ahead for function definitions to avoid erroneously parsing the parameter list as a generic type. If it detects a grouped declarator, it explicitly parses `(*name)` manually. It expected the next token after the identifier to be either `)` or `[`, so `TK_ASM` caused a direct syntax error. 

## 3. Phase B3 Patch
This bug required extending the B2 stash-and-pickup architecture directly into the grouped declarator loop.
- **Capture Site:** Directly after `TK_IDENT` consumption (around line `2920`), we check for `TK_ASM`, parse it, and save the string into `cc->pending_asm_name`.
- **Pickup Site:** Because grouped declarators jump via `goto after_name;` to the standard global-variable parsing system, the stash is naturally picked up by the outer scope's `gvar` symbol table `scope_add` routines. 

A secondary bug was also identified and patched where trailing comma-separated global definitions (`int a = 1, b __asm__("b_renamed") = 2;`) were skipping `gvar->name` writebacks in B2. This was fully resolved. 

## 4. Validation 
As seen in `B3_VALIDATION.md`:
- Compilation does not syntax error.
- Linkage works natively against the renamed expected object files.
- `nm` correctly flags `D fp_renamed` for `.data` labels.
- Standard aliases and static local variable systems remain regression-free.
