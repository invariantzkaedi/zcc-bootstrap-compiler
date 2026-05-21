# IR Whitelist Batch 8 Plan

## Un-whitelisted Inventory Count
Total remaining: 74 functions.

## Proposed Batch 8 Additions
The following functions are simple math primitives or memory safety/validation predicates with no complex IR structure or tight inner loops:

1. `"log2_of"` — Pure math helper (already known safe).
2. `"guard_node"` — Memory safety guard predicate.
3. `"is_bad_ptr"` — Pointer bounds check predicate.
4. `"ptr_in_fault_range"` — Simple bitwise bounds check.
5. `"validate_token_bounds"` — Token boundary predicate.
6. `"validate_node"` — AST structural validation predicate.
7. `"validate_type"` — Type structure validation predicate.
8. `"bad_node_cutoff"` — Simple threshold check.

## Excluded Functions
- **CG-IR-012 Known Bad (DO NOT ADD)**: `"next_token"`, `"read_char"`, `"read_escape"`, `"node_name"`.
- **High Complexity / Side-Effects**: `codegen_*` family, `parse_const_expr_*` family, `rust_*` family, and `zcc_*` initialization routines. These emit extensive IR/Assembly, parse heavy logic, or involve system state, making them high-risk for IR translation bugs.

## Estimated Line Diff
- **`part4.c`**: +3 lines (1 comment, 2 strings)
```diff
       /* Batch 7: Token & Lexing Core + Missed AST Helpers (bisect-safe) */
       "peek_token", "peek_char", "is_type_token", "lookup_keyword",
       "lookup_keyword_fallback", "expect",
       "node_ptr_elem_size", "ptr_elem_size", "get_member_size",
+      /* Batch 8: Memory Safety & Math Primitives */
+      "log2_of", "guard_node", "is_bad_ptr", "ptr_in_fault_range",
+      "validate_token_bounds", "validate_node", "validate_type", "bad_node_cutoff",
       NULL
   };
```
