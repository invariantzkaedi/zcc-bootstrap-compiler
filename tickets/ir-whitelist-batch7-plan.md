# IR Whitelist Batch 7 Plan

## Missing Functions Inventory
The following functions are defined in `part1.c`, `part2.c`, `part3.c`, and `part4.c` but are NOT yet in the `ir_whitelisted` array:

- abi_join
- allocate_registers
- ast_quarantine_lift_phase
- bad_node_cutoff
- classify_aggregate
- classify_field
- close
- codegen_addr
- codegen_addr_checked
- codegen_expr
- codegen_expr_checked
- codegen_func
- codegen_load
- codegen_program
- codegen_stmt
- codegen_store
- compute_liveness
- emit_bb_telem
- emit_global_init_list
- emit_global_var
- emit_label_fmt
- emit_local_initializer
- emit_strings
- eval_const_expr
- eval_const_expr_p4
- expect
- fold_constants
- get_member_size
- guard_node
- is_bad_ptr
- is_type_token
- log2_of
- lookup_keyword
- lookup_keyword_fallback
- new_label
- next_token
- node_name
- node_ptr_elem_size
- parse_const_expr
- parse_const_expr_add
- parse_const_expr_band
- parse_const_expr_bor
- parse_const_expr_bxor
- parse_const_expr_eq
- parse_const_expr_land
- parse_const_expr_lor
- parse_const_expr_mul
- parse_const_expr_primary
- parse_const_expr_rel
- parse_const_expr_shift
- parse_const_expr_ternary
- parse_const_expr_unary
- parse_value_from_wire
- peek_char
- peek_token
- pop_reg
- ptr_elem_size
- ptr_in_fault_range
- push_reg
- ra_add_local
- read_char
- read_escape
- register_struct
- rust_backend_bridge_compile_file
- rust_frontend_compile_file
- rust_lower_to_ir
- rust_resolve_names
- rust_typecheck
- setenv
- shm_open
- unsetenv
- validate_node
- validate_token_bounds
- validate_type
- zcc_build_from_wire
- zcc_build_init_internal
- zcc_build_initializer
- zcc_init_list_append
- zcc_init_list_begin
- zcc_init_list_end
- zcc_init_value
- zcc_run_passes_emit_body_pgo
- zcc_shm_ring_open
- zcc_shm_ring_read

## Proposed Batch 7 Additions
I propose "Batch 7: Token & Lexing Core + Missed AST Helpers" to continue whitelisting high-frequency parser primitives that do not emit IR or require symbolic execution lifting.

1. `"next_token"` — Core lexer state transition, high frequency.
2. `"peek_token"` — Core lexer lookahead, pure state query.
3. `"peek_char"` — Character lookahead helper.
4. `"read_char"` — Character advance helper.
5. `"read_escape"` — String parsing helper, high frequency during string lexing.
6. `"is_type_token"` — Pure predicate for token classification.
7. `"lookup_keyword"` — Pure symbol lookup.
8. `"lookup_keyword_fallback"` — Pure symbol lookup.
9. `"expect"` — Simple token matching and error raising.
10. `"node_name"` — AST read-only helper (missed in Batch 4).
11. `"node_ptr_elem_size"` — AST read-only helper (missed in Batch 4).
12. `"ptr_elem_size"` — AST read-only helper (missed in Batch 4).
13. `"get_member_size"` — AST read-only helper (missed in Batch 4).

## Estimated Line Diff
- **`part4.c`**: +3 lines (1 line for comment, 2 lines for new strings)
```diff
       "node_new", "node_num", "node_flit",
       "scope_push", "scope_pop", "scope_find", "scope_find_local", "scope_add", "scope_add_local",
+      /* Batch 7: Token & Lexing Core + Missed AST Helpers */
+      "next_token", "peek_token", "peek_char", "read_char", "read_escape", "is_type_token", "lookup_keyword", "lookup_keyword_fallback", "expect",
+      "node_name", "node_ptr_elem_size", "ptr_elem_size", "get_member_size",
       NULL
   };
```
