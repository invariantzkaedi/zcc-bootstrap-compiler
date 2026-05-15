import sys

runs = []
current_run = []
for line in open('ra_dump.log'):
    if line.startswith('FUNC '):
        if '(run 0)' in line:
            if current_run: runs.append(current_run)
            current_run = []
    current_run.append(line.strip())
if current_run: runs.append(current_run)

print(f'Total runs found: {len(runs)}')
for idx, r in enumerate(runs):
    print(f'Run {idx+1} length: {len(r)}')

if len(runs) >= 4:
    r3 = runs[2]
    r4 = runs[3]
    if len(r3) != len(r4):
        print(f"DIFFERENT LENGTHS! R3={len(r3)}, R4={len(r4)}")
    for i in range(min(len(r3), len(r4))):
        if r3[i] != r4[i]:
            print(f'Diff at line {i}:')
            print(f'ZCC : {r3[i]}')
            print(f'ZCC2: {r4[i]}')
            break
