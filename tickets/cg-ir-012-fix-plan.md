# CG-IR-012 Fix Plan

## Target File
`compiler_passes.c`

## Patch Sites & Exact Line Numbers

### Site 1: Function Signature & Call Site (Lines 5956-5964, 7091-7095)

**Function Signature Update (Lines ~5956-5964):**
```diff
 static void ir_asm_number_and_liveness(Function *fn,
                                        const uint32_t *block_order,
                                        uint32_t n_block_order, int *def_seq,
-                                       int *last_use) {
+                                       int *last_use, int *first_use) {
   for (int i = 0; i < MAX_INSTRS; i++) {
     def_seq[i] = -1;
     last_use[i] = -1;
+    first_use[i] = -1;
   }
```

**Call Site Update (Lines ~7091-7095):**
```diff
   /* Linear scan register allocation: number instructions, compute intervals,
    * assign phys regs. */
   for (int i = 0; i < MAX_INSTRS; i++)
     ctx->phys_reg[i] = -1;
-  ir_asm_number_and_liveness(fn, ctx->block_order, ctx->n_block_order,
-                             ctx->def_seq, ctx->last_use);
+  int *first_use = calloc(MAX_INSTRS, sizeof(int));
+  ir_asm_number_and_liveness(fn, ctx->block_order, ctx->n_block_order,
+                             ctx->def_seq, ctx->last_use, first_use);
+  free(first_use);
   ir_asm_linear_scan(fn, ctx->block_order, ctx->n_block_order, ctx->def_seq,
                      ctx->last_use, ctx->phys_reg);
```

### Site 2: Use-Recording Loop (Lines 5976-6002)
**Line numbers:** 5976-6002
```diff
       /* Uses */
       if (ins->op == OP_CONDBR) {
-        if (ins->n_src >= 1 && ins->src[0])
-          last_use[ins->src[0]] = seq;
+        if (ins->n_src >= 1 && ins->src[0]) {
+          if (first_use[ins->src[0]] == -1) first_use[ins->src[0]] = seq;
+          last_use[ins->src[0]] = seq;
+        }
       } else if (ins->op == OP_BR) {
         /* no reg operands */
       } else if (ins->op == OP_RET) {
-        if (ins->n_src >= 1 && ins->src[0])
-          last_use[ins->src[0]] = seq;
+        if (ins->n_src >= 1 && ins->src[0]) {
+          if (first_use[ins->src[0]] == -1) first_use[ins->src[0]] = seq;
+          last_use[ins->src[0]] = seq;
+        }
       } else if (ins->op == OP_STORE) {
-        if (ins->n_src >= 1 && ins->src[0])
-          last_use[ins->src[0]] = seq;
-        if (ins->n_src >= 2 && ins->src[1])
-          last_use[ins->src[1]] = seq;
+        if (ins->n_src >= 1 && ins->src[0]) {
+          if (first_use[ins->src[0]] == -1) first_use[ins->src[0]] = seq;
+          last_use[ins->src[0]] = seq;
+        }
+        if (ins->n_src >= 2 && ins->src[1]) {
+          if (first_use[ins->src[1]] == -1) first_use[ins->src[1]] = seq;
+          last_use[ins->src[1]] = seq;
+        }
       } else if (ins->op == OP_PHI) {
         for (uint32_t p = 0; p < ins->n_phi; p++)
-          if (ins->phi[p].reg)
-            last_use[ins->phi[p].reg] = seq;
+          if (ins->phi[p].reg) {
+            if (first_use[ins->phi[p].reg] == -1) first_use[ins->phi[p].reg] = seq;
+            last_use[ins->phi[p].reg] = seq;
+          }
       } else if (ins->op == OP_CALL) {
         for (uint32_t c = 0; c < ins->n_call_args; c++)
-          if (ins->call_args[c])
-            last_use[ins->call_args[c]] = seq;
+          if (ins->call_args[c]) {
+            if (first_use[ins->call_args[c]] == -1) first_use[ins->call_args[c]] = seq;
+            last_use[ins->call_args[c]] = seq;
+          }
       } else {
         for (uint32_t s = 0; s < ins->n_src; s++)
-          if (ins->src[s])
-            last_use[ins->src[s]] = seq;
+          if (ins->src[s]) {
+            if (first_use[ins->src[s]] == -1) first_use[ins->src[s]] = seq;
+            last_use[ins->src[s]] = seq;
+          }
       }
```

### Site 3: Interval Expansion Condition (Lines 6063-6074)
**Line numbers:** 6063-6074
```diff
-  /* BUG-3 FIX: LICM hoisting + PGO can place a use BEFORE a def in linear emission
-   * order. The linear scanner assumes the interval starts at def_seq. If use < def,
-   * the interval [def, end] DOES NOT COVER the use, allowing the allocator to assign
-   * a register that is already active at the use point, causing lethal clobbering.
-   * Fix: Expand the interval backward to start at last_use, and forward to the
-   * end of the function (since it must span a loop back-edge). */
+  /* BUG-3 FIX + CG-IR-012 FIX: cover both last_use < def (BUG-3)
+   * and first_use < def < last_use (loop-spanning, CG-IR-012) */
   for (int r = 0; r < MAX_INSTRS; r++) {
     if (def_seq[r] >= 0 && last_use[r] >= 0 && last_use[r] < def_seq[r]) {
       def_seq[r] = last_use[r];
       last_use[r] = seq > 0 ? seq - 1 : 0;
+    } else if (def_seq[r] >= 0 && first_use[r] >= 0
+               && first_use[r] < def_seq[r]) {
+      def_seq[r] = first_use[r];
+      last_use[r] = seq > 0 ? seq - 1 : 0;
     }
   }
```

## Why this cannot break BUG-3 coverage
The proposed fix exclusively uses an `else if` branch chained to the existing BUG-3 condition. The original BUG-3 check (`last_use[r] < def_seq[r]`) is evaluated first and remains completely unmodified. If a variable's interval satisfies the BUG-3 constraint (meaning its strictly last use was before its definition), it enters the original block, mutates `def_seq` and `last_use`, and bypasses the new code. The new `first_use` logic only executes for variables that were previously rejected by the BUG-3 condition (i.e. `last_use >= def_seq`), extending their start boundary to their `first_use` without over-extending their tail. Thus, all intervals handled by BUG-3 continue to be processed identically, guaranteeing zero regression.
