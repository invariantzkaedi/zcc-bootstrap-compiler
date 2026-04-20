path = '/mnt/h/__DOWNLOADS/zcc_github_upload/Makefile'
with open(path) as f:
    content = f.read()

old = 'zcc: zcc.c $(PASSES)\n\t$(CC) $(CFLAGS) -o zcc zcc.c $(PASSES) $(LDFLAGS)'

new = (
    'zcc: zcc.c $(PASSES)\n'
    '\t# Tripwire: reject hand-edited zcc.c — parts are the source of truth.\n'
    '\t# Bypass with: ZCC_MUTATION_SANDBOX=1 make zcc  (Oneirogenesis daemon)\n'
    '\t# or:          touch .mutation_sandbox && make zcc\n'
    '\t@if [ -z "$$ZCC_MUTATION_SANDBOX" ] && [ ! -f .mutation_sandbox ]; then \\\n'
    '\t  if ! diff -q <(cat $(PARTS)) zcc.c > /dev/null 2>&1; then \\\n'
    '\t    echo "ERROR: zcc.c does not match cat($(PARTS)). Edit the parts, not zcc.c."; \\\n'
    '\t    echo "       To suppress (mutation sandbox): export ZCC_MUTATION_SANDBOX=1"; \\\n'
    '\t    exit 1; \\\n'
    '\t  fi; \\\n'
    '\tfi\n'
    '\t$(CC) $(CFLAGS) -o zcc zcc.c $(PASSES) $(LDFLAGS)'
)

if old in content:
    content = content.replace(old, new, 1)
    with open(path, 'w') as f:
        f.write(content)
    print('TRIPWIRE APPLIED')
else:
    print('TARGET NOT FOUND')
