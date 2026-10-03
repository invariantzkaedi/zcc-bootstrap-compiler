# Artifacts Inventory

| File | SHA-256 | Purpose |
|------|---------|---------|
| `part5.c` | b571a9c26f93a55328d69c54b1803619df4672706db5687d2ef5f455ece4bafc | Base-displacement local load folding (`movslq/movq disp(%rbp), reg`) & double `cltq` elision |
| `src/codegen.c` | c90f8ad208f3da43864b6ed702bed2f0bbe7e38b4200a2a56b15ebe8a85e4a42 | Direct assembler SIB instruction (`leaq (%rax,%rax,2), %rax`) and comma parenthesis isolation |
