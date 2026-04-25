# Agent Discovery Template

Before editing, fill this out.

## Repository architecture

### Build system

- Build tool:
- Main build command:
- Test command:
- Compiler binary output path:
- Existing CI files:

### Parser

- Parser files:
- Keyword handling:
- `sizeof` handling:
- Declaration parsing:
- Existing `_Static_assert` handling:

### Type system

- Type definition files:
- Primitive type representation:
- Pointer type representation:
- Array type representation:
- Struct type representation:
- Union type representation:
- Existing size/alignment fields:
- Existing member offset fields:

### Constant evaluation

- Const-eval files:
- Integer constant expression support:
- Existing diagnostics for non-constant expressions:

### Diagnostics

- Diagnostic files:
- Error severity model:
- Existing error code model:
- Source location type:

### Layout

- Existing layout files:
- Existing struct layout logic:
- Existing union layout logic:
- Existing ABI table:
- Existing target triple handling:

### Codegen

- Codegen files:
- Where type size is consumed:
- Where type align is consumed:
- Where member offsets are consumed:

### Tests

- Test framework:
- Negative test style:
- Expected diagnostic style:
- CI test entry point:

## Integration plan

- Files to modify:
- Files to add:
- Names adapted from requested design:
- Risks:
- First small commit: