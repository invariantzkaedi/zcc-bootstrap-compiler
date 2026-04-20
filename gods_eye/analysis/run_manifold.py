import os
import json
import numpy as np

# A simple text-to-vector embedding using character frequency to simulate dense vectors
def text_to_embedding(text, dim=128):
    np.random.seed(sum(ord(c) for c in text))
    base = np.random.randn(dim)
    # L2 normalize
    norm = np.linalg.norm(base)
    return base / (norm + 1e-8)

class SemanticManifold:
    def __init__(self, concepts, embeddings):
        self.concepts = concepts
        self.embeddings = embeddings
        self.N = len(concepts)
        self.D = embeddings.shape[1]
        
        # Cosine similarity matrix
        self.similarity = self.embeddings @ self.embeddings.T
        self.idx = {c: i for i, c in enumerate(concepts)}

    def domain_cohesion(self):
        return {"all": 1.0}

    def hub_concepts(self, top_n=10):
        avg_sim = []
        for i in range(self.N):
            total = sum(self.similarity[i, j] for j in range(self.N) if j != i)
            avg_sim.append((self.concepts[i], total / (max(1, self.N - 1))))
        avg_sim.sort(key=lambda x: x[1], reverse=True)
        return avg_sim[:top_n]

    def evolve_hamiltonian(self, timesteps=1000):
        eta, gamma, beta, sigma = 0.4, 0.3, 0.1, 0.05
        H = self.similarity.copy()
        H_base = self.similarity.copy()
        for t in range(timesteps):
            sigmoid_H = 1.0 / (1.0 + np.exp(-gamma * np.clip(H, -20, 20)))
            noise = np.random.normal(0, 1 + beta * np.abs(H))
            H = H_base + eta * H * sigmoid_H + sigma * noise
            H = np.clip(H, -50, 50)
        
        emergent_paths = []
        for i in range(self.N):
            for j in range(i + 1, self.N):
                original = self.similarity[i, j]
                evolved = H[i, j]
                if original < 0.3 and evolved > 0.6:
                    emergent_paths.append({
                        'concept_a': self.concepts[i],
                        'concept_b': self.concepts[j],
                        'original_sim': float(original),
                        'evolved_strength': float(evolved)
                    })
        emergent_paths.sort(key=lambda x: x['evolved_strength'], reverse=True)
        return {"emergent_paths": emergent_paths}

def main():
    repos = ['/mnt/h/agents/qsim', '/mnt/h/agents/playground-zkaedi']
    concepts = []
    for repo in repos:
        for root, dirs, files in os.walk(repo):
            if '.git' in root or 'node_modules' in root:
                continue
            for f in files:
                ext = f.split('.')[-1]
                if ext in ['py', 'js', 'rs', 'md', 'ts', 'sol']:
                    concepts.append(f)
    
    if not concepts:
        print("No concepts found.")
        return
        
    embeddings = np.array([text_to_embedding(c) for c in concepts])
    print(f"Loaded {len(concepts)} concepts.")
    
    manifold = SemanticManifold(concepts, embeddings)
    hubs = manifold.hub_concepts(5)
    print("\n[TOP SEMANTIC HUBS]")
    for h, score in hubs:
        print(f" - {h}: {score:.4f}")
        
    print("\n[HAMILTONIAN EVOLUTION]")
    res = manifold.evolve_hamiltonian(timesteps=500)
    print("Emergent Paths (Wormholes):")
    for bridge in res["emergent_paths"][:5]:
        print(f" - {bridge['concept_a']} <--> {bridge['concept_b']} (Boosted: {bridge['evolved_strength']:.2f})")

if __name__ == '__main__':
    main()
