# Gate Evidence — fix(svg,vir): close SVG DOM leaks and clarify cache lifecycle ownership

**Milestone**: `fix(svg,vir): close SVG DOM leaks and clarify cache lifecycle ownership`
**Commit baseline**: `dccf681a` (canonical fingerprint milestone — last green commit)

## Phase 0 Verdict
```
BASELINE:              GREEN
SYMPTOM-IN-HISTORY:    NO
FORENSIC-LATEST-SHA:   a60a8b89
PROCEED:               YES
```

---

## Changes Audited (diff verbatim)

### Memory defects fixed

**zcc_svg.c** — `render_layout_to_nodes`:
```c
} else {
    /* txt2 was allocated but there is no detail text for this
     * node — free it here rather than leaving a phantom. */
    svg_free_node_tree(txt2);
}
```

**zcc_svg.c** — `svg_render_ast`:
```c
/* Free the internally-constructed ZccSvgNode DOM; the serialised
 * string (svg_str) is what the caller owns and must free. */
svg_free_node_tree(svg);
```

**zcc_svg.h** — promoted `svg_free_node_tree` as public API:
```c
void svg_free_node_tree(ZccSvgNode* node);
```

**test_zcc_svg.c** — root tree freed:
```c
svg_free_node_tree(svg);
```

### Lifecycle hygiene

**test_zcc_svg_path_parser.c** — private duplicate destructor eliminated:
```c
/* Use the public destructor from zcc_svg.h — no local duplicate needed. */
#define free_svg_node_tree svg_free_node_tree
```

**test_zcc_vir.c** — redundant shutdown removed with explanatory comment:
```c
/* NOTE: vir_cache_shutdown() is NOT called here.
 * test_vir_compilation_caching() initialises the cache with
 * vir_cache_init() and owns the shutdown at the end of that test.
 * Calling shutdown a second time here would be a phantom lifecycle
 * violation — the guard in vir_cache_clear() makes it safe but
 * semantically wrong and misleading. */
```

---

## Gate 1 — Self-host byte-identical: `cmp zcc2.s zcc3.s`

```
$ make selfhost 2>&1 | tail -4
[Phase 5] Native C Peephole Optimization... OK (16089 elided)
[OK] ZCC Engine Compilation Terminated Successfully.
SELF-HOST VERIFIED (assembly identical)

$ cmp zcc2.s zcc3.s; echo CMP_EXIT:$?
CMP_EXIT:0
```

**Result: BYTE-IDENTICAL**

---

## Gate 2 — VIR Test Suite + LSan

```
$ gcc -O0 -g -fsanitize=address,leak -I. test_zcc_vir.c zcc_vir.c \
    zcc_svg_path_parser.c zcc_svg.c -o test_zcc_vir_lsan -lm \
    && ./test_zcc_vir_lsan
777JACKPOT777 — ALL VIR CORE TESTS GREEN.
LSAN_EXIT:0
```

**Result: 0 errors, 0 leaks**

---

## Gate 3 — SVG Test Suite + LSan

```
$ gcc -O0 -g -fsanitize=address,leak -I. test_zcc_svg.c zcc_svg.c \
    zcc_svg_path_parser.c zcc_vir.c -o test_zcc_svg_lsan -lm \
    && ./test_zcc_svg_lsan
SVG_LSAN_EXIT:0
```

**Result: 0 errors, 0 leaks**

---

## Gate 4 — SVG Path Parser Test Suite + LSan

```
$ gcc -O0 -g -fsanitize=address,leak -I. test_zcc_svg_path_parser.c \
    zcc_svg_path_parser.c zcc_svg.c zcc_vir.c -o test_path_parser_lsan -lm \
    && ./test_path_parser_lsan
=== ZCC SVG Path Ingestion Test Harness ===
[+] test_successful_parsing PASSED.
[+] test_malformed_paths PASSED.
[+] test_extreme_inputs PASSED.
777JACKPOT777 — ALL INGESTION TESTS GREEN.
PARSER_LSAN_EXIT:0
```

**Result: 0 errors, 0 leaks**

---

## Bugs caught mid-gate

None — gates ran clean on first attempt.

## Hygiene / deferred

`hybrid_omnified.svg` — unstaged, unrelated working-tree modification, excluded from this commit.
