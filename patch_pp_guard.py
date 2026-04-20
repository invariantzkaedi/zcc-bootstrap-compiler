path = '/mnt/h/__DOWNLOADS/zcc_github_upload/zcc_oneirogenesis.py'
with open(path) as f:
    content = f.read()

old = (
    "            with open(zcc_pp_tmp) as fin, open(zcc_pp_c, 'w') as fout:\n"
    "                for line in fin:\n"
    "                    if line.startswith('#include \"zcc_ast_bridge.h\"'):"
)

new = (
    "            with open(zcc_pp_tmp) as fin, open(zcc_pp_c, 'w') as fout:\n"
    "                # Inject the bridge guard so ZCC's own preprocessor skips\n"
    "                # all five `#ifndef ZCC_AST_BRIDGE_H / #include \"part1.c\"`\n"
    "                # blocks. Without this fix, node_kind and every other\n"
    "                # accessor defined in part1.c is emitted multiple times in\n"
    "                # the mutant assembly, causing 'symbol already defined'.\n"
    "                fout.write('#define ZCC_AST_BRIDGE_H\\n')\n"
    "                for line in fin:\n"
    "                    if line.startswith('#include \"zcc_ast_bridge.h\"'):"
)

if old in content:
    content = content.replace(old, new, 1)
    with open(path, 'w') as f:
        f.write(content)
    print('PATCH APPLIED OK')
else:
    print('TARGET NOT FOUND — no changes made')
    print('First 200 chars of search:')
    print(repr(old[:200]))
