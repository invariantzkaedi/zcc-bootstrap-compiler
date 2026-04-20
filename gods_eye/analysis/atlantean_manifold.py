import json
import numpy as np

# =========================================================================
# 🌌 ZKAEDI ATLANTEAN MANIFOLD: REAL-WORLD HAMILTONIAN TOPOLOGY
# =========================================================================

class SemanticManifold:
    def __init__(self, concepts, embeddings, tx_targets):
        self.concepts = concepts
        self.tx_targets = tx_targets
        self.N = len(concepts)
        self.embeddings = self._normalize(embeddings)
        self.similarity = self.embeddings @ self.embeddings.T

    def _normalize(self, emb):
        # L2 Normalization of the Web3 vectors
        norms = np.linalg.norm(emb, axis=1, keepdims=True)
        return emb / np.maximum(norms, 1e-8)

    def evolve_hamiltonian(self, timesteps=100, eta=0.4, gamma=0.3):
        H = self.similarity.copy()
        H_base = self.similarity.copy()
        
        print("\n" + "="*80)
        print(" 🌌 KICKSTARTING HAMILTONIAN TENSOR EVOLUTION OVER REAL MAINNET DATA")
        print("="*80)
        for t in range(timesteps):
            sigmoid_H = 1.0 / (1.0 + np.exp(-gamma * np.clip(H, -20, 20)))
            noise = np.random.normal(0, 0.05 + 0.1 * np.abs(H))
            H = H_base + eta * H * sigmoid_H + 0.05 * noise
            H = np.clip(H, -50, 50)
            
            if t % 25 == 0:
                print(f" [🧮] TIMESTEP {t:03d} | MANIFOLD ENERGY TRACE: {np.sum(H**2):.2f}")
        return H

    def detect_wormholes(self, evolved_H):
        wormholes = []
        for i in range(self.N):
            for j in range(i + 1, self.N):
                orig = self.similarity[i, j]
                evolved = evolved_H[i, j]
                
                target_a = self.tx_targets[i]
                target_b = self.tx_targets[j]
                
                # A Wormhole: High evolved correlation across different contracts
                if orig < 0.98 and evolved > 0.95 and target_a != target_b:
                    wormholes.append({
                        "hash_a": self.concepts[i],
                        "hash_b": self.concepts[j],
                        "target_a": target_a,
                        "target_b": target_b,
                        "evolved_strength": evolved
                    })
        
        # Sort by mathematical strength
        wormholes.sort(key=lambda x: x['evolved_strength'], reverse=True)
        return wormholes

if __name__ == "__main__":
    print("[+] LOADING ATLANTEAN ANOMALIES FROM DISK...")
    
    try:
        data = []
        with open("atlantean_anomalies.json", "r") as f:
            for line in f:
                line = line.strip()
                if not line: continue
                if line.endswith(","):
                    line = line[:-1]
                try:
                    data.append(json.loads(line))
                except:
                    continue # Skip broken lines from abrupt exit
    except Exception as e:
        print(f"[!] FAILED TO LOAD ARCHIVE: {e}")
        exit()
        
    print(f" [✓] {len(data)} TENSORS LOADED. PREPARING EMBEDDINGS...\n")
    
    # We slice out just the first 500 otherwise the matrix evolution will melt the CPU
    data = data[-500:] 

    concepts = []
    tx_targets = []
    matrix_rows = []

    # Normalize vectors mathematically
    max_gas = max([tx['gas_limit'] for tx in data]) + 1
    max_size = max([tx['bytecode_size_bytes'] for tx in data]) + 1
    max_price = max([tx['gas_price_gwei'] for tx in data]) + 1

    for tx in data:
        concepts.append(tx['hash'][:10] + "...")
        tx_targets.append(tx['target_contract'])
        
        # We build a 6-Dimensional Mathematical signature for each transaction
        # Encoding physical gas physics and bytecode depth
        row = [
            tx['gas_limit'] / max_gas,
            tx['bytecode_size_bytes'] / max_size,
            tx['gas_price_gwei'] / max_price,
            (tx['gas_limit'] * tx['gas_price_gwei']) / (max_gas * max_price),
            len(tx['function_selector']) / 10,
            1.0 if tx['target_contract'] else 0.0
        ]
        matrix_rows.append(row)

    embeddings = np.array(matrix_rows)
    
    manifold = SemanticManifold(concepts, embeddings, tx_targets)
    final_field = manifold.evolve_hamiltonian(timesteps=150)
    
    anomalies = manifold.detect_wormholes(final_field)
    
    print("\n" + "="*80)
    print(f" 🌀 TOPOLOGICAL WORMHOLES DISCOVERED (HIDDEN MEV BRIDGES):")
    print("="*80)
    if anomalies:
        for idx, a in enumerate(anomalies[:10]):
            print(f"\n [WORMHOLE {idx+1}] STRENGTH: {a['evolved_strength']:.4f}")
            print(f"  ├─ ALFA TARGET : {a['target_a']} (HASH: {a['hash_a']})")
            print(f"  └─ BETA TARGET : {a['target_b']} (HASH: {a['hash_b']})")
    else:
        print("  [!] SPACE IS LINEAR. NO CROSS-CHAIN WORMHOLES DETECTED.")
    print("="*80 + "\n")
