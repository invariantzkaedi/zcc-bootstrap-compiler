import numpy as np
import scipy.linalg

# Nodes:
# 0: Idea 1 (ZCC-Opt-DPO)
# 1: Idea 2 (Forensic Agent DPO)
# 2: Idea 3 (Flipper Zero Micro-DPO)
# 3: Idea 4 (PoUW Verifiable DPO)
# 4: Idea 5 (Fuzzing Oracle DPO)

# Asymmetric transition weights based on causal dependencies:
# Need foundational correctness (Node 1) & test oracles (Node 4) -> compiler opt (Node 0) -> PoUW (Node 3) -> Flipper (Node 2)
W = np.array([
    [0.0, 1.5, 0.8, 1.2, 1.8],  # 0
    [1.5, 0.0, 0.5, 0.9, 2.0],  # 1 (Strong coupling to Fuzzing & Opt)
    [0.8, 0.5, 0.0, 1.6, 0.7],  # 2
    [1.2, 0.9, 1.6, 0.0, 1.4],  # 3
    [1.8, 2.0, 0.7, 1.4, 0.0]   # 4
])

D = np.diag(np.sum(W, axis=1))
H = D - W

# Multi-step continuous evolution with Hamiltonian phase dampening
dt = 0.08
steps = 85
U = scipy.linalg.expm(-1j * H * (dt * steps))

# Seeded state starting at Foundation (Node 1: Forensic Invariant Engine)
psi_0 = np.zeros(5, dtype=complex)
psi_0[1] = 1.0

psi_t = U @ psi_0
probs = np.abs(psi_t)**2
probs /= np.sum(probs)

names = [
    "Idea 1: Compiler Pass Phase-Ordering DPO (ZCC-Opt-DPO)",
    "Idea 2: Forensic Self-Host Agent Alignment DPO",
    "Idea 3: Flipper Zero Micro-DPO Firmware Policy",
    "Idea 4: PoUW On-Chain Verifiable DPO Training",
    "Idea 5: Synthetic Negative-Control Fuzzing DPO"
]

print("=== CAUSALLY CONSTRAINED QUANTUM WALK COLLAPSE ===")
ranked = np.argsort(probs)[::-1]
for rank, idx in enumerate(ranked, 1):
    print(f"Phase {rank}: [Probability Resonance: {probs[idx]*100:5.2f}%] -> {names[idx]}")
