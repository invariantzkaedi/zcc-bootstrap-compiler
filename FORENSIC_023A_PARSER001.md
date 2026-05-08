## Update: PP-HEADERS-023A closure + PARSER-001 scoping (2026-04-19)

PP-HEADERS-023A committed at `373afc5` with HONEST 8% partial result 
documented in commit body. LLONG_MAX/MIN/ULLONG_MAX pre-registered in 
PPState macro table (not just stddef stub — they fire before any 
#include reaches the stub injector).

Diagnostic probe for residual 22,398 errors isolated PARSER-001: 
parenthesized declarator form `type *(name)(params)` — patterns 1/3/4 
in `tools/battle/paren_decl_probe.c` fail with unexpected-token-86, 
patterns 2/5 pass. Every Lua API function in `lua.h` uses the failing 
form, cascading 130-500+ errors per Lua source file.

Next session: PARSER-001. Starting state is `373afc5`.

## Update: PARSER-001 Closure (2026-05-07)

**Status:** ✅ FIXED
**Resolution:** The logic inside `part3.c:parse_top_level()` that mistakenly consumed trailing `(params)` for grouped declarators (e.g., `int *(func)(int)`) has been surgically deferred. By allowing the parser to fall through to `after_name`, the native `parse_func_def()` correctly instantiates the AST function node and scope. 
**Verification:** Gate 4 target-specific verification passed by natively building `lua/lapi.c` (which imports `lua.h`) with ZCC. The 130-500+ cascading `unexpected-token-86` errors have been completely eliminated.
