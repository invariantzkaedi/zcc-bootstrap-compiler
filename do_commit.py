import subprocess, os
os.chdir('/mnt/h/__DOWNLOADS/zcc_github_upload')
msg = (
    "fix: parts-as-SoT enforcement + AST bridge guard in mutation pipeline\n\n"
    "- Makefile: re-land tripwire (POSIX sh, temp-file diff) blocking hand-edits\n"
    "  to zcc.c; sandbox escape via ZCC_MUTATION_SANDBOX=1 or .mutation_sandbox\n"
    "- zcc_oneirogenesis.py: inject #define ZCC_AST_BRIDGE_H as first line of\n"
    "  generated zcc_pp.c. Prevents 5x conditional re-include of part1.c\n"
    "  (node_kind et al.) causing 'symbol already defined' on every mutant asm.\n"
    "  Root cause of the 100% rejection rate in Oneirogenesis.\n"
    "- zcc_dream_mutations.py: restore strength_reduction / cmpq_zero_to_testq /\n"
    "  branch_straighten sweeps disabled during diagnostic isolation\n"
    "- part2.c part3.c part5.c: 262bd08 C99/POSIX graduation breadcrumbs\n"
    "- .gitignore: exclude zcc.c zcc_pp.c build artifacts\n\n"
    "Validation: 3 cycles post-fix: G1 EVOLVED cmpq->testq peephole,\n"
    "G2+G3 EVOLVED LEA fusion. 23% evo rate. Blacklist: genuine regressions only.\n"
    "Baseline: QAlgo-Dream-G1/G2/G3 banked."
)
r = subprocess.run(['git', 'commit', '-m', msg], capture_output=True, text=True)
print(r.stdout)
print(r.stderr)
