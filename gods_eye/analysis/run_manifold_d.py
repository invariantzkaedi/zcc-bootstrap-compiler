import json, numpy as np
class SemanticManifold:
    def __init__(self, concepts, embeddings, domains):
        self.concepts = concepts
        norms = np.linalg.norm(embeddings, axis=1, keepdims=True)
        self.embeddings = embeddings / np.maximum(norms, 1e-8)
        self.N = len(concepts)
        self.domains = domains
        self.similarity = self.embeddings @ self.embeddings.T

    def cross_domain_bridges(self, threshold=0.95):
        concept_to_domain = {}
        for d, c_list in self.domains.items():
            for c in c_list: concept_to_domain[c] = d
        bridges = []
        for i in range(self.N):
            for j in range(i + 1, self.N):
                di = concept_to_domain.get(self.concepts[i], '')
                dj = concept_to_domain.get(self.concepts[j], '')
                if di != dj and self.similarity[i, j] > threshold:
                    bridges.append((self.concepts[i], self.concepts[j], self.similarity[i, j], di, dj))
        bridges.sort(key=lambda x: x[2], reverse=True)
        return bridges

    def reduce_pca(self, n_components=2):
        mean = self.embeddings.mean(axis=0)
        centered = self.embeddings - mean
        cov = centered.T @ centered / (self.N - 1)
        positions = np.zeros((self.N, n_components))
        residual = centered.copy()
        for comp in range(n_components):
            v = np.random.randn(16)
            v = v / np.linalg.norm(v)
            for _ in range(100):
                v_new = cov @ v
                v_new = v_new / np.linalg.norm(v_new)
                if np.abs(np.dot(v, v_new)) > 0.9999:
                    break
                v = v_new
            positions[:, comp] = residual @ v
            eigenvalue = v @ cov @ v
            cov = cov - eigenvalue * np.outer(v, v)
        return positions

concepts = []
embeddings = []
domains = {}
with open('hft_harvest_pipeline.jsonl', 'r') as f:
    for i, line in enumerate(f):
        try:
            d = json.loads(line)
            c = f"{d['pool']}::{d['state']}::{i}"
            dom = d['pool']
            feats = d.get('16d_features', [])
            if len(feats) == 16:
                concepts.append(c)
                embeddings.append(feats)
                if dom not in domains: domains[dom] = []
                domains[dom].append(c)
        except:
            pass

manifold = SemanticManifold(concepts, np.array(embeddings), domains)
bridges = manifold.cross_domain_bridges(threshold=0.98)

print(f'Total concepts computed: {manifold.N}')
print('\n=== TOP 15 ARBITRAGE WORMHOLES (CROSS-DOMAIN BRIDGES) ===')
for b in bridges[:15]:
    print(f'{b[2]:.4f} Similarity | {b[3]} <=> {b[4]}\n   -- Bridge: {b[0]} <-> {b[1]}')

pca_pos = manifold.reduce_pca(2)
colors = {}
counter = 0
for d in domains:
    colors[d] = counter
    counter += 1
