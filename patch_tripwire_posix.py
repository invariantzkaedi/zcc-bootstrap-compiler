path = '/mnt/h/__DOWNLOADS/zcc_github_upload/Makefile'
with open(path) as f:
    content = f.read()

old = (
    '\t@if [ -z "$$ZCC_MUTATION_SANDBOX" ] && [ ! -f .mutation_sandbox ]; then \\\n'
    '\t  if ! diff -q <(cat $(PARTS)) zcc.c > /dev/null 2>&1; then \\\n'
    '\t    echo "ERROR: zcc.c does not match cat($(PARTS)). Edit the parts, not zcc.c."; \\\n'
    '\t    echo "       To suppress (mutation sandbox): export ZCC_MUTATION_SANDBOX=1"; \\\n'
    '\t    exit 1; \\\n'
    '\t  fi; \\\n'
    '\tfi\n'
)

new = (
    '\t@if [ -z "$$ZCC_MUTATION_SANDBOX" ] && [ ! -f .mutation_sandbox ]; then \\\n'
    '\t  cat $(PARTS) > .zcc_parts_check.tmp; \\\n'
    '\t  if ! diff -q .zcc_parts_check.tmp zcc.c > /dev/null 2>&1; then \\\n'
    '\t    rm -f .zcc_parts_check.tmp; \\\n'
    '\t    echo "ERROR: zcc.c does not match cat($(PARTS)). Edit the parts, not zcc.c."; \\\n'
    '\t    echo "       To suppress (mutation sandbox): export ZCC_MUTATION_SANDBOX=1"; \\\n'
    '\t    exit 1; \\\n'
    '\t  fi; \\\n'
    '\t  rm -f .zcc_parts_check.tmp; \\\n'
    '\tfi\n'
)

if old in content:
    content = content.replace(old, new, 1)
    with open(path, 'w') as f:
        f.write(content)
    print('TRIPWIRE POSIX-FIXED')
else:
    print('TARGET NOT FOUND')
