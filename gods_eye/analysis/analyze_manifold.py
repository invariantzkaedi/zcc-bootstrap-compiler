import json
import collections

bifurcation_counts = collections.defaultdict(int)
divergence_counts = collections.defaultdict(int)
max_energy = collections.defaultdict(float)
total_steps = collections.defaultdict(int)

# 0x4D07 = MF_TELEM_BIFURCATION
# 0x4D06 = MF_TELEM_DIVERGE
# 0x4D04 = MF_TELEM_EVOLVE_STEP

try:
    with open('/mnt/h/agents/selforglinux_build/sqlite_telemetry.log', 'r') as f:
        for line in f:
            line = line.strip()
            if not line.startswith('{'): continue
            try:
                data = json.loads(line)
                func = data.get('func', 'unknown')
                event = data.get('event')
                
                if event == '0x4D07':
                    bifurcation_counts[func] += 1
                elif event == '0x4D06':
                    divergence_counts[func] += 1
                elif event == '0x4D04':
                    energy = data.get('energy', 0.0)
                    total_steps[func] += 1
                    if energy > max_energy[func]:
                        max_energy[func] = energy
                        
            except json.JSONDecodeError:
                pass
except FileNotFoundError:
    print("Telemetry log not found yet.")
    exit(1)

print("Top 20 Functions by Bifurcation Count (Topological Chaos):")
print("-" * 60)
sorted_bifurcations = sorted(bifurcation_counts.items(), key=lambda x: x[1], reverse=True)
for i, (func, count) in enumerate(sorted_bifurcations[:20]):
    print(f"{i+1:2d}. {func:<35} | Bifurcations: {count:4d} | Max Energy: {max_energy[func]:.2E}")

print("\nTop 10 Functions by Total Hamiltonian Energy:")
print("-" * 60)
sorted_energy = sorted(max_energy.items(), key=lambda x: x[1], reverse=True)
for i, (func, energy) in enumerate(sorted_energy[:10]):
    print(f"{i+1:2d}. {func:<35} | Max Energy: {energy:.2E}")

if divergence_counts:
    print("\nFunctions that experienced Divergence (Clamp hit):")
    for func, count in divergence_counts.items():
        print(f" - {func}")
