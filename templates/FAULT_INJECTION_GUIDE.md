# 🔥 ZCC Fault-Injection Guide (Template)

## Purpose
Prove verification gates are sensitive to the target defect class and not producing false confidence.

## Target Defect Class
- [ABI | stack drift | register clobber | FP precision | bounds/lifetime | parser/IR/codegen]
- Specific invariant to violate: [ ]

---

## Safety Rules
- [ ] Inject only on isolated branch
- [ ] Keep injection minimal and reversible
- [ ] Record exact injected diff
- [ ] Restore immediately after RED proof

---

## Injection Plan
1. **Injection point file/function:** [ ]
2. **Minimal intentional break:** [ ]
3. **Expected failing gate(s):** [ ]
4. **Expected symptom signature:** [stderr text / exit code / diff mismatch]

---

## Execute RED
- Command(s):
```bash
[ ]
```
- Raw output:
```text
[ ]
```
- Exit code(s): [ ]
- Did expected gate fail? [Yes/No]
- If no, explain why gate is insensitive: [ ]

---

## Restore & Re-verify GREEN
- Restore command(s):
```bash
[ ]
```
- Re-run gate command(s):
```bash
[ ]
```
- Raw output:
```text
[ ]
```
- Exit code(s): [ ]
- Returned to GREEN? [Yes/No]

---

## Sensitivity Verdict
- **Gate sensitivity:** [Proven | Not proven]
- **Evidence quality:** [High | Medium | Low]
- **Action if not proven:** [Add/adjust gate, add regression, refine probe]

---

## Appendix
### Injected Diff (exact)
```diff
[ ]
```

### Restored Diff (exact)
```diff
[ ]
```
