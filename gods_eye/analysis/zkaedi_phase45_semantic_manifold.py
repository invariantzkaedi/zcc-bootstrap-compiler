import json
import numpy as np
from typing import List, Dict, Tuple, Optional
from sentence_transformers import SentenceTransformer

class SemanticManifold:
    def __init__(self, concepts: List[str], embeddings: np.ndarray, 
                 domains: Optional[Dict[str, List[str]]] = None):
        self.concepts = concepts
        self.embeddings = self._normalize(embeddings)
        self.N = len(concepts)
        self.D = embeddings.shape[1]
        self.similarity = self._cosine_similarity_matrix()
        self.domains = domains or self._auto_cluster()
        self.idx = {c: i for i, c in enumerate(concepts)}
    
    def _normalize(self, emb: np.ndarray) -> np.ndarray:
        norms = np.linalg.norm(emb, axis=1, keepdims=True)
        norms = np.maximum(norms, 1e-8)
        return emb / norms
    
    def _cosine_similarity_matrix(self) -> np.ndarray:
        return self.embeddings @ self.embeddings.T
    
    def _auto_cluster(self, n_clusters: int = 4) -> Dict[str, List[str]]:
        from sklearn.cluster import SpectralClustering
        affinity = (self.similarity + 1) / 2
        clustering = SpectralClustering(
            n_clusters=min(n_clusters, self.N), 
            affinity='precomputed',
            random_state=42
        ).fit(affinity)
        
        domains = {}
        for i, label in enumerate(clustering.labels_):
            domain = f"cluster_{label}"
            if domain not in domains:
                domains[domain] = []
            domains[domain].append(self.concepts[i])
        return domains
    
    @classmethod
    def from_huggingface(cls, concepts: List[str], domains: Dict[str, List[str]] = None,
                         model: str = "all-MiniLM-L6-v2") -> 'SemanticManifold':
        encoder = SentenceTransformer(model)
        embeddings = encoder.encode(concepts, show_progress_bar=False)
        return cls(concepts, np.array(embeddings), domains)
    
    def hub_concepts(self, top_n: int = 10) -> List[Tuple[str, float]]:
        avg_sim = []
        for i in range(self.N):
            total = sum(self.similarity[i, j] for j in range(self.N) if j != i)
            if self.N > 1:
                avg_sim.append((self.concepts[i], total / (self.N - 1)))
            else:
                avg_sim.append((self.concepts[i], 0.0))
        avg_sim.sort(key=lambda x: x[1], reverse=True)
        return avg_sim[:top_n]

    def cross_domain_bridges(self, threshold: float = 0.5) -> List[Tuple[str, str, float, str, str]]:
        concept_to_domain = {}
        for domain, concepts in self.domains.items():
            for c in concepts:
                concept_to_domain[c] = domain
        
        bridges = []
        for i in range(self.N):
            for j in range(i + 1, self.N):
                d_i = concept_to_domain.get(self.concepts[i], "unknown")
                d_j = concept_to_domain.get(self.concepts[j], "unknown")
                
                if d_i != d_j and self.similarity[i, j] > threshold:
                    bridges.append((
                        self.concepts[i], self.concepts[j],
                        float(self.similarity[i, j]),
                        d_i, d_j
                    ))
        
        bridges.sort(key=lambda x: x[2], reverse=True)
        return bridges
    
    def evolve_hamiltonian(self, timesteps: int = 2000, 
                           eta: float = 0.4, gamma: float = 0.3,
                           beta: float = 0.1, sigma: float = 0.05,
                           regimes: List[Tuple[float, float, float, float]] = None) -> Dict:
        if regimes is None:
            regimes = [
                (0.1, 0.05, 0.02, 0.01),  # Gentle: low feedback, low noise
                (0.4, 0.3, 0.1, 0.05),     # Standard: balanced dynamics
                (0.8, 0.5, 0.3, 0.1),      # Aggressive: high feedback
                (0.2, 0.8, 0.05, 0.02),    # Sharp: strong attractors
            ]
        steps_per_regime = timesteps // len(regimes)
        
        H = self.similarity.copy()
        H_base = self.similarity.copy()
        prev_energy = np.sum(H ** 2)
        emergent_paths = []
        
        for regime_idx, (r_eta, r_gamma, r_beta, r_sigma) in enumerate(regimes):
            for t in range(steps_per_regime):
                sigmoid_H = 1.0 / (1.0 + np.exp(-r_gamma * np.clip(H, -20, 20)))
                noise = np.random.normal(0, 1 + r_beta * np.abs(H))
                H = H_base + r_eta * H * sigmoid_H + r_sigma * noise
                H = np.clip(H, -50, 50)
                energy = np.sum(H ** 2)
                prev_energy = energy
                
                if t == steps_per_regime - 1:
                    for i in range(self.N):
                        for j in range(i + 1, self.N):
                            original = self.similarity[i, j]
                            evolved = H[i, j]
                            if original < 0.3 and evolved > 0.6:
                                emergent_paths.append({
                                    'concept_a': self.concepts[i],
                                    'concept_b': self.concepts[j],
                                    'original_sim': float(original),
                                    'evolved_strength': float(evolved),
                                    'regime': regime_idx
                                })
        return {'emergent_paths': emergent_paths}
    
    def persistent_homology(self, max_epsilon: float = 1.0, steps: int = 50) -> Dict:
        distance = 1.0 - self.similarity
        np.fill_diagonal(distance, 0)
        h0_deaths = [max_epsilon] * self.N
        components = list(range(self.N))
        h1_features = []
        
        edges = []
        for i in range(self.N):
            for j in range(i + 1, self.N):
                edges.append((distance[i, j], i, j))
        edges.sort()
        
        def find(x):
            while components[x] != x:
                components[x] = components[components[x]]
                x = components[x]
            return x
        
        def union(x, y, epsilon):
            rx, ry = find(x), find(y)
            if rx == ry: return False
            if rx > ry: rx, ry = ry, rx
            components[ry] = rx
            h0_deaths[ry] = epsilon
            return True
        
        for dist, i, j in edges:
            if dist > max_epsilon: break
            was_new = union(i, j, dist)
            if not was_new:
                h1_features.append({
                    'birth': float(dist),
                    'concepts': (self.concepts[i], self.concepts[j]),
                    'similarity': float(self.similarity[i, j])
                })
        
        return {'h1_features': h1_features}
    
    def wormhole_detection(self, min_distance: float = 0.6, 
                           min_similarity: float = 0.4) -> List[Dict]:
        wormholes = []
        for i in range(self.N):
            for j in range(i + 1, self.N):
                direct_sim = self.similarity[i, j]
                if direct_sim < min_similarity:
                    neighbors_i = set(k for k in range(self.N) if k != i and self.similarity[i, k] > 0.5)
                    neighbors_j = set(k for k in range(self.N) if k != j and self.similarity[j, k] > 0.5)
                    shared = neighbors_i & neighbors_j
                    
                    if shared:
                        max_transitive = 0
                        best_bridge = None
                        for k in shared:
                            transitive = min(self.similarity[i, k], self.similarity[j, k])
                            if transitive > max_transitive:
                                max_transitive = transitive
                                best_bridge = self.concepts[k]
                        
                        if max_transitive > min_distance:
                            wormholes.append({
                                'concept_a': self.concepts[i],
                                'concept_b': self.concepts[j],
                                'direct_similarity': float(direct_sim),
                                'transitive_similarity': float(max_transitive),
                                'bridge_concept': best_bridge,
                                'shared_neighbors': [self.concepts[k] for k in shared],
                                'amplification': float(max_transitive / (direct_sim + 1e-8))
                            })
        wormholes.sort(key=lambda x: x['amplification'], reverse=True)
        return wormholes

if __name__ == "__main__":
    import sys
    print("--- ZKAEDI PHASE 4.5: SEMANTIC MANIFOLD PROCESSOR ---")
    
    try:
        with open("zcc-compiler-bug-corpus.json", "r") as f:
            bugs = json.load(f)
    except Exception as e:
        print(f"Failed to load corpus: {e}")
        sys.exit(1)
        
    concepts = []
    domains = {}
    
    for bug in bugs:
        concept_text = f"[{bug['cwe']}] {bug['title']}: {bug['root_cause']} -> {bug['fix_summary']}"
        concepts.append(concept_text)
        
        domain = bug.get('domain', 'general')
        if domain not in domains:
            domains[domain] = []
        domains[domain].append(concept_text)
        
    print(f"Loaded {len(concepts)} bug topological nodes across {len(domains)} domains.")
    print("Embedding bug geometry with all-MiniLM-L6-v2...")
    
    manifold = SemanticManifold.from_huggingface(concepts, domains=domains)
    
    print("\n[Topology Result] SEMANTIC HUBS:")
    for hub, score in manifold.hub_concepts(5):
        print(f"  [{score:.3f}] {hub[:100]}...")
        
    print("\n[Topology Result] CROSS-DOMAIN BRIDGES:")
    bridges = manifold.cross_domain_bridges(threshold=0.5)
    for b in bridges[:5]:
        print(f"  [{b[2]:.3f}] ({b[3]}) {b[0][:40]}... <---> ({b[4]}) {b[1][:40]}...")
        
    print("\n[Topology Result] WORMHOLE DETECTION (Hidden Paths):")
    wormholes = manifold.wormhole_detection(min_distance=0.3, min_similarity=0.1)
    for w in wormholes[:5]:
        print(f"  *** WORMHOLE DISCOVERED ***")
        print(f"      Node A: {w['concept_a'][:80]}...")
        print(f"      Node B: {w['concept_b'][:80]}...")
        print(f"      Bridge: {w['bridge_concept'][:80]}...")
        print(f"      Direct Sim: {w['direct_similarity']:.3f} | Transitive Sim: {w['transitive_similarity']:.3f} | Amplification: {w['amplification']:.1f}x")
        
    print("\n[Topology Result] HAMILTONIAN EMERGENT PATHS:")
    evolved = manifold.evolve_hamiltonian(timesteps=500)
    for ep in evolved['emergent_paths'][:5]:
        print(f"  Emergent Link (Regime {ep['regime']}):")
        print(f"    {ep['concept_a'][:50]}... <-> {ep['concept_b'][:50]}...")
        print(f"    Original: {ep['original_sim']:.3f} -> Evolved: {ep['evolved_strength']:.3f}")

    print("\n[Topology Result] PERSISTENT HOMOLOGY (H1 FEATURES - Loops):")
    homology = manifold.persistent_homology()
    for h in homology['h1_features'][:5]:
        print(f"  Loop formed at e {h['birth']:.3f} between: {h['concepts'][0][:40]}... AND {h['concepts'][1][:40]}...")

    with open("phase45_synthesis_report.txt", "w", encoding="utf-8") as f:
        f.write("ZKAEDI PRIME PHASE 4.5 SEMANTIC MANIFOLD SYNTHESIS\n")
        f.write(f"Analyzed {len(concepts)} elements in {len(domains)} domains.\n")
        f.write("\nWORMHOLES DETECTED:\n")
        json.dump(wormholes, f, indent=2)
    print("\nPhase 4.5 execution finalized. Output written to 'phase45_synthesis_report.txt'.")
