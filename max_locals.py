import sys

runs = []
current_run = []
for line in open('ra_dump.log'):
    if line.startswith('FUNC '):
        if '(run 0)' in line:
            if current_run: runs.append(current_run)
            current_run = []
        current_run.append(line.strip())
    else:
        current_run.append(line.strip())
if current_run: runs.append(current_run)

r = runs[2]
func_lens = []
curr_len = 0
for line in r:
    if line.startswith('FUNC '):
        if curr_len > 0: func_lens.append(curr_len)
        curr_len = 0
    else:
        curr_len += 1
if curr_len > 0: func_lens.append(curr_len)

print(f"Max locals: {max(func_lens)}")
