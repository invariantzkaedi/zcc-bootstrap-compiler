import json

import numpy as np
from sentence_transformers import SentenceTransformer


class ZKAEDIAutoFixer:
    def __init__(self, bug_corpus_path):
        self.bugs = json.load(open(bug_corpus_path, encoding="utf-8"))
        self.model = SentenceTransformer('all-MiniLM-L6-v2')
        
        # Auto-compute the manifold matrix instead of loading a .npy file
        raw_text = [f"[{b.get('cwe','UNK')}] {b.get('title','')} {b.get('root_cause','')}" for b in self.bugs]
        embeddings = self.model.encode(raw_text, show_progress_bar=False)
        norms = np.linalg.norm(embeddings, axis=1, keepdims=True)
        norms = np.maximum(norms, 1e-8)
        normed = embeddings / norms
        self.manifold = normed @ normed.T
        
    def analyze_fix_order(self):
        """Generate optimal fix sequence using topology"""
        hub = self.find_hub()
        wormholes = self.find_wormholes_from(hub)
        loops = self.find_loops()
        risk_scores = self.calculate_regression_risk()
        
        return {
            'hub': hub,
            'wormholes': wormholes,
            'loops': loops,
            'risks': risk_scores
        }
    
    def find_hub(self, threshold=0.5):
        """Find semantic hub (highest avg centrality)"""
        centralities = np.mean(self.manifold, axis=1)
        hub_idx = np.argmax(centralities)
        return {
            'bug': self.bugs[hub_idx],
            'centrality': float(centralities[hub_idx]),
            'reason': 'Semantic hub - affects most other bugs'
        }
    
    def find_wormholes_from(self, hub, min_amplification=1.5):
        """Find amplified connections from hub"""
        hub_idx = self.bugs.index(hub['bug'])
        base_similarities = self.manifold[hub_idx]
        
        # Simulate evolved manifold via non-linear thresholding
        evolved_similarities = 1.0 / (1.0 + np.exp(-10 * (base_similarities - 0.4)))
        amplifications = evolved_similarities / (base_similarities + 1e-6)
        
        wormholes = []
        for idx, amp in enumerate(amplifications):
            if amp > min_amplification and idx != hub_idx:
                wormholes.append({
                    'bug': self.bugs[idx],
                    'amplification': float(amp),
                    'base_sim': float(base_similarities[idx]),
                    'evolved_sim': float(evolved_similarities[idx])
                })
        
        return sorted(wormholes, key=lambda x: x['amplification'], reverse=True)

    def find_loops(self):
        # Placeholder for H1 topological feature loops
        return []

    def calculate_regression_risk(self):
        """Predict regression risk for each bug"""
        risks = []
        hub = self.find_hub()
        hub_idx = self.bugs.index(hub['bug'])

        for i, bug in enumerate(self.bugs):
            connectivity = np.sum(self.manifold[i] > 0.4)
            hub_distance = 1 - self.manifold[i][hub_idx]
            risk_score = (connectivity * 0.6) + ((1 - hub_distance) * 0.4)
            
            risks.append({
                'bug': bug,
                'risk_score': float(risk_score),
                'connectivity': int(connectivity),
                'hub_distance': float(hub_distance)
            })
        return sorted(risks, key=lambda x: x['risk_score'], reverse=True)


    def analyze_prefix_regression_risk(self, fixing_bug_id, verbose=True):
        """Comprehensive regression analysis BEFORE making any changes"""
        try:
            bug_idx = next(i for i, b in enumerate(self.bugs) if b['id'] == fixing_bug_id)
        except StopIteration:
            if verbose: print("Bug not found.")
            return None

        bug = self.bugs[bug_idx]
        connections = self.manifold[bug_idx]
        
        critical_risk = []  # sim > 0.6
        high_risk = []      # 0.4 < sim <= 0.6
        medium_risk = []    # 0.3 < sim <= 0.4
        
        for i, sim in enumerate(connections):
            if i == bug_idx: continue
            connected_bug = self.bugs[i]
            if sim > 0.6: critical_risk.append({'bug': connected_bug, 'sim': float(sim)})
            elif sim > 0.4: high_risk.append({'bug': connected_bug, 'sim': float(sim)})
            elif sim > 0.3: medium_risk.append({'bug': connected_bug, 'sim': float(sim)})
        
        total_connections = len(critical_risk) + len(high_risk) + len(medium_risk)
        regression_probability = min(total_connections * 0.15, 0.95)
        
        centrality = np.mean(connections)
        is_hub = centrality > 0.5
        
        loop_detected = False
        for i, sim in enumerate(connections):
            if sim > 0.5 and i != bug_idx:
                reverse_sim = self.manifold[i][bug_idx]
                if reverse_sim > 0.5:
                    loop_detected = True
                    break
        
        report = {
            'bug': bug,
            'regression_probability': regression_probability,
            'is_hub': is_hub,
            'loop_detected': loop_detected,
            'critical_risk': sorted(critical_risk, key=lambda x: x['sim'], reverse=True),
            'high_risk': sorted(high_risk, key=lambda x: x['sim'], reverse=True),
            'medium_risk': sorted(medium_risk, key=lambda x: x['sim'], reverse=True),
            'total_at_risk': total_connections
        }
        
        if verbose:
            print(f"\n{'='*70}")
            print(f"REGRESSION RISK ANALYSIS: {fixing_bug_id}")
            print(f"{'='*70}")
            print(f"\n📋 Bug: {bug['title']}")
            print(f"   Category: {bug.get('cwe', 'Unknown')}")
            print("\n📊 Overall Risk Assessment:")
            print(f"   Regression Probability: {regression_probability*100:.1f}%")
            print(f"   Hub Status: {'⚠️  YES - High cascade risk' if is_hub else '✓ No'}")
            print(f"   Loop Detected: {'⚠️  YES - Coordinated fix needed' if loop_detected else '✓ No'}")
            print(f"   Total Bugs at Risk: {total_connections}")
            
            if critical_risk:
                print(f"\n🔴 CRITICAL RISK (sim > 0.6) - {len(critical_risk)} bugs:")
                for r in critical_risk[:3]:
                    print(f"   • {r['bug']['id']}: {r['bug']['title']} (sim: {r['sim']:.3f})")
            if high_risk:
                print(f"\n🟡 HIGH RISK (0.4 < sim <= 0.6) - {len(high_risk)} bugs:")
                for r in high_risk[:3]:
                    print(f"   • {r['bug']['id']}: {r['bug']['title']} (sim: {r['sim']:.3f})")
            if medium_risk:
                print(f"\n🟢 MEDIUM RISK (0.3 < sim <= 0.4) - {len(medium_risk)} bugs:")
                for r in medium_risk[:2]:
                    print(f"   • {r['bug']['id']}: {r['bug']['title']} (sim: {r['sim']:.3f})")
            
            print("\n💡 RECOMMENDATIONS:")
            if is_hub: print("   1. Fix this bug FIRST (hub).")
            elif loop_detected: print("   1. Part of a loop, needs coordinated fixes.")
            elif critical_risk: print("   1. HIGH regression risk detected.")
            else: print("   1. LOW regression risk.")
            print(f"{'='*70}\n")
        
        return report

    def batch_fix_planner(self, target_count=5):
        print("\n🔱 BATCH FIX PLANNER")
        print("="*70)
        
        hub = self.find_hub()
        hub_idx = self.bugs.index(hub['bug'])
        
        plan = {
            'phases': [],
            'total_bugs': target_count,
            'estimated_time': 0,
            'risk_mitigation': []
        }
        
        phase1 = {
            'phase': 1,
            'name': 'Hub Fix (Foundation)',
            'bugs': [hub['bug']],
            'rationale': 'Semantic hub - cascades to other bugs',
            'estimated_time': 4,
            'dependencies': [],
            'cascade_effect': [w['bug']['id'] for w in self.find_wormholes_from(hub)[:3]]
        }
        plan['phases'].append(phase1)
        plan['estimated_time'] += 4
        
        fixed = {hub['bug']['id']}
        remaining = [b for b in self.bugs if b['id'] not in fixed]
        
        connectivity_scores = []
        for bug in remaining:
            b_idx = self.bugs.index(bug)
            connections = np.sum(self.manifold[b_idx] > 0.4)
            connectivity_scores.append({'bug': bug, 'connectivity': int(connections), 'idx': b_idx})
        
        connectivity_scores.sort(key=lambda x: x['connectivity'], reverse=True)
        phase2_bugs = connectivity_scores[:min(3, target_count-1)]
        
        if phase2_bugs:
            phase2 = {
                'phase': 2,
                'name': 'High-Connectivity Bugs',
                'bugs': [b['bug'] for b in phase2_bugs],
                'rationale': 'High connectivity - affect many other bugs',
                'estimated_time': len(phase2_bugs) * 3,
                'dependencies': [hub['bug']['id']],
                'cascade_effect': []
            }
            plan['phases'].append(phase2)
            plan['estimated_time'] += phase2['estimated_time']
        
        print("📋 OPTIMAL FIX SEQUENCE:\n")
        for phase in plan['phases']:
            print(f"{'='*70}\nPHASE {phase['phase']}: {phase['name']}\n{'='*70}")
            print(f"Estimated Time: {phase['estimated_time']} hours")
            print(f"Rationale: {phase['rationale']}")
            print("\nBugs to Fix:")
            for i, bug in enumerate(phase['bugs'], 1):
                print(f"  {i}. {bug['id']}: {bug['title']}")
        return plan

def main():
    print("🔱 ZKAEDI AUTO-FIXER - Topology-Guided Bug Prioritization")
    print("=" * 70)
    
    fixer = ZKAEDIAutoFixer('zcc-compiler-bug-corpus.json')
    
    analysis = fixer.analyze_fix_order()
    
    print("\n🎯 RECOMMENDED FIX ORDER:\n")
    
    print("1. SEMANTIC HUB (Fix First):")
    hub = analysis['hub']
    print(f"   {hub['bug']['id']}: {hub['bug']['title']}")
    print(f"   Centrality: {hub['centrality']:.3f}")
    print(f"   Reason: {hub['reason']}\n")
    
    print("2. WORMHOLE CASCADE (Fix Next):")
    for i, wh in enumerate(analysis['wormholes'][:3], 1):
        print(f"   {i}. {wh['bug']['id']}: {wh['bug']['title']}")
        print(f"      Amplification: {wh['amplification']:.2f}×")
        print(f"      Connection: {wh['base_sim']:.3f} -> {wh['evolved_sim']:.3f}\n")
    
    print("3. REGRESSION RISK SCORES (Top 5):")
    for i, risk in enumerate(analysis['risks'][:5], 1):
        print(f"   {i}. {risk['bug']['id']}: Risk={risk['risk_score']:.2f} (Conn: {risk['connectivity']}, HubDist: {risk['hub_distance']:.3f})")

    # Show prefix risk analysis for the Hub
    fixer.analyze_prefix_regression_risk(hub['bug']['id'])

    # Batch Plan
    fixer.batch_fix_planner()

if __name__ == '__main__':
    main()
