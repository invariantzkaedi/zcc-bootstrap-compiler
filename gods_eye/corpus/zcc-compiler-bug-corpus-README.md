---
license: apache-2.0
task_categories:
  - text-classification
language:
  - en
tags:
  - compiler
  - codegen
  - bug-detection
  - static-analysis
  - x86-64
  - C
  - SSA
  - zkaedi-prime
size_categories:
  - n<1K
---

# ZCC Compiler Bug Corpus

A ground-truth dataset of 12 real codegen bugs discovered and fixed in a single debugging session on ZCC, a self-hosting C compiler with a dual-emission IR backend.

## Why this matters

Most compiler bug datasets are synthetic (Juliet, SARD) or extracted from bug trackers without reproduction artifacts. This corpus contains **real bugs in production code** — each one discovered through self-hosting (the compiler compiling itself), traced to root cause via GDB at the register level, fixed with a targeted patch, and verified through byte-identical bootstrap output.

Every entry includes the symptom, root cause, detection method, fix, CWE classification, PRIME energy score, and a regression test specification.

## The compiler

ZCC is a self-hosting C compiler targeting x86-64 (System V AMD64 ABI). It has two codegen paths:

- **AST-direct**: Traditional tree-walk code generation (proven correct via self-host)
- **IR backend**: 3-address SSA IR with optimization passes (DCE, Mem2Reg, escape analysis, constant folding, strength reduction, copy propagation, redundant load elimination, PGO block reordering)

The IR backend runs inside the AST's stack frame (hybrid architecture) — the AST owns the prologue/epilogue while the IR owns the function body. This hybrid design is the source of most bugs in the corpus: two systems sharing one stack frame with different assumptions about register ownership, slot layout, and instruction width.

## Bug categories

| Category | Count | CWE Classes |
|----------|-------|-------------|
| Stack frame conflicts | 4 | CWE-121, CWE-131, CWE-682, CWE-787 |
| Instruction selection | 2 | CWE-704 |
| SSA destruction (PHI) | 1 (3 sub-bugs) | CWE-682 |
| Bridge layer gaps | 3 | CWE-476, CWE-839 |
| ABI violations | 2 | CWE-682 |

## PRIME energy scoring

Each bug is scored on the ZKAEDI PRIME Recursively Coupled Hamiltonian energy scale:

- **9.5-10.0**: Critical bifurcation — system cannot converge (CG-IR-012b: hollow stubs, score 9.9)
- **8.5-9.4**: High energy — crashes on first use (CG-IR-005: PHI lost-copy, score 9.8)
- **7.0-8.4**: Moderate — manifests under specific conditions (CG-IR-007: call alignment, score 7.5)

The energy score reflects how deeply the bug disrupts the Hamiltonian convergence of the self-hosting bootstrap chain. A score of 9.9 means the compiler cannot compile itself at all; a score of 7.0 means it self-hosts but produces subtly wrong code for edge cases.

## Schema

```json
{
  "id": "CG-IR-005",
  "title": "PHI liveness back-edge inversion...",
  "cwe": "CWE-682",
  "cwe_name": "Incorrect Calculation",
  "severity": "critical",
  "domain": "compiler_codegen",
  "component": "compiler_passes.c:ir_asm_emit_phi_edge_copy",
  "symptom": "Functions with loops crash...",
  "root_cause": "Three interrelated bugs in PHI resolution...",
  "detection_method": "GDB on cc_alloc crash: r12=0xD8...",
  "fix_summary": "Fixed liveness back-edge direction...",
  "affected_functions": ["cc_alloc", "main", ...],
  "lines_changed": 45,
  "fix_type": "algorithm_fix",
  "prime_energy_score": 9.8,
  "prime_phase": "bifurcation_critical",
  "tags": ["PHI-resolution", "SSA-destruction", ...],
  "before_pattern": "/* code before fix */",
  "after_pattern": "/* code after fix */",
  "regression_test": "for (i=0; i<10; i++) sum += i; — must return 45"
}
```

## Usage

### Training a bug detector

```python
from datasets import load_dataset
ds = load_dataset("zkaedi/zcc-compiler-bug-corpus")

# Each entry has symptom → root_cause → fix mapping
for bug in ds["train"]:
    print(f"{bug['id']}: {bug['cwe']} — {bug['title']}")
```

### Evaluating static analyzers

Compare your tool's findings against ground truth:
- True positive: tool flags a pattern matching `before_pattern` with correct CWE
- False negative: tool misses a pattern in the corpus
- Use `regression_test` to verify fixes hold

### PRIME energy landscape analysis

```python
import json
bugs = json.load(open("zcc-compiler-bug-corpus.json"))
for b in sorted(bugs, key=lambda x: -x["prime_energy_score"]):
    print(f"{b['prime_energy_score']:.1f} {b['prime_phase']:25s} {b['id']}")
```

## Provenance

- **Compiler**: ZCC (github: zkaedi, HuggingFace: zkaedi)
- **Discovery date**: April 4, 2026
- **Discovery method**: Self-hosting bootstrap failure + GDB register-level debugging
- **Verification**: All 12 bugs fixed, `make selfhost` produces SELF-HOST VERIFIED (byte-identical zcc2.s == zcc3.s), 21/21 regression tests pass
- **Environment**: x86-64 Linux (WSL2 Ubuntu 24.04), GCC bootstrap, AMD Ryzen AI 7 350

## Citation

```bibtex
@dataset{zkaedi_zcc_bug_corpus_2026,
  title={ZCC Compiler Bug Corpus: 12 Real Codegen Bugs with Ground-Truth Fixes},
  author={ZKAEDI},
  year={2026},
  publisher={Hugging Face},
  url={https://huggingface.co/datasets/zkaedi/zcc-compiler-bug-corpus}
}
```

## License

Apache 2.0
