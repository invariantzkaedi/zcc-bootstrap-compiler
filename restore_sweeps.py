path = '/mnt/h/__DOWNLOADS/zcc_github_upload/zcc_dream_mutations.py'
with open(path) as f:
    content = f.read()

old = (
    "        if include_sweeps:\n"
    "            pass\n"
    "            # results.extend(self._sweep_zero_mov_to_xor(asm_lines))\n"
    "            # results.extend(self._sweep_strength_reduction(asm_lines))\n"
    "            # results.extend(self._sweep_cmpq_zero_to_testq(asm_lines))\n"
    "            # results.extend(self._sweep_branch_straighten(asm_lines))\n"
)

new = (
    "        if include_sweeps:\n"
    "            # results.extend(self._sweep_zero_mov_to_xor(asm_lines))  # re-enable after validation\n"
    "            results.extend(self._sweep_strength_reduction(asm_lines))\n"
    "            results.extend(self._sweep_cmpq_zero_to_testq(asm_lines))\n"
    "            results.extend(self._sweep_branch_straighten(asm_lines))\n"
)

if old in content:
    content = content.replace(old, new, 1)
    with open(path, 'w') as f:
        f.write(content)
    print('SWEEPS RESTORED')
else:
    print('TARGET NOT FOUND')
