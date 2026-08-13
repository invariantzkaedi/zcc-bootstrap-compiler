#!/usr/bin/env python3
import os
import json
import math

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUTPUT_DIR = os.path.join(REPO_ROOT, "output")
SECURITY_GRAPH_PATH = os.path.join(OUTPUT_DIR, "security_world_graph.json")
NEXUS_GRAPH_PATH = os.path.join(REPO_ROOT, "zkaedi_cinematic_sovereign_graph.json")
TOPOLOGY_3D_PATH = os.path.join(OUTPUT_DIR, "nexus_topology_3d.json")

def build_3d_topology():
    nodes = []
    links = []
    
    # Try reading from security_world_graph.json
    nodes_2d = []
    edges_2d = []
    
    if os.path.exists(SECURITY_GRAPH_PATH):
        try:
            with open(SECURITY_GRAPH_PATH, "r", encoding="utf-8") as f:
                sec_graph = json.load(f)
                nodes_2d = sec_graph.get("nodes", [])
                edges_2d = sec_graph.get("edges", [])
        except Exception as e:
            print(f"Failed to read security graph: {e}")
            
    # Default fallback data if empty or failed
    if not nodes_2d:
        # Build standard fallback matching mock data
        nodes_2d = [
            {"entity": "Universe", "type": "core", "status": "ACTIVE", "label": "Reality Universe"},
            {"entity": "fac_000", "type": "faction", "status": "ACTIVE", "label": "House Acheron"},
            {"entity": "fac_001", "type": "faction", "status": "COLLAPSED", "label": "Neo-Sovereign Union"},
            {"entity": "fac_002", "type": "faction", "status": "COLLAPSED", "label": "Order of Avalon"},
            {"entity": "fac_003", "type": "faction", "status": "ACTIVE", "label": "Star Syndicate"},
            {"entity": "fac_004", "type": "faction", "status": "ACTIVE", "label": "Kernel Resistance"},
            {"entity": "DirectorNode", "type": "security", "status": "ISOLATED", "label": "Director Swarm Node"},
            {"entity": "ResearchNode", "type": "security", "status": "ISOLATED", "label": "Research Agent Node"},
            {"entity": "AssetNode", "type": "security", "status": "ISOLATED", "label": "Asset Discovery Node"},
            {"entity": "RenderNode", "type": "security", "status": "ACTIVE", "label": "Render Cluster Node"},
            {"entity": "ValidationNode", "type": "security", "status": "ACTIVE", "label": "Validation Node"},
            {"entity": "DirectorNode_fork", "type": "security", "status": "ACTIVE", "label": "Director Node Replica"},
            {"entity": "AssetNode_fork_fork_fork", "type": "security", "status": "ACTIVE", "label": "Asset Node Replica v3"},
            {"entity": "prop_red_door", "type": "asset", "status": "ACTIVE", "label": "The Red Door"},
            {"entity": "vehicle_cruiser", "type": "asset", "status": "ACTIVE", "label": "Sovereign Cruiser"}
        ]
        edges_2d = [
            {"source": "Universe", "target": "fac_000"},
            {"source": "Universe", "target": "fac_001"},
            {"source": "Universe", "target": "fac_002"},
            {"source": "Universe", "target": "fac_003"},
            {"source": "Universe", "target": "fac_004"},
            {"source": "Universe", "target": "DirectorNode"},
            {"source": "Universe", "target": "ResearchNode"},
            {"source": "Universe", "target": "AssetNode"},
            {"source": "Universe", "target": "RenderNode"},
            {"source": "Universe", "target": "ValidationNode"},
            {"source": "DirectorNode", "target": "DirectorNode_fork"},
            {"source": "AssetNode", "target": "AssetNode_fork_fork_fork"},
            {"source": "Universe", "target": "prop_red_door"},
            {"source": "Universe", "target": "vehicle_cruiser"}
        ]

    # Map 2D nodes into structured 3D levels
    # Levels: Core (y=100), Faction (y=50), Security Swarm (y=0), Assets (y=-50)
    cores = [n for n in nodes_2d if n.get("type") == "core" or n.get("entity") == "Universe"]
    factions = [n for n in nodes_2d if n.get("type") == "faction"]
    securities = [n for n in nodes_2d if n.get("type") in ("security", "node") and n.get("entity") != "Universe"]
    assets = [n for n in nodes_2d if n.get("type") == "asset"]
    others = [n for n in nodes_2d if n not in cores and n not in factions and n not in securities and n not in assets]
    
    # 1. Apex Core (Universe)
    for c in cores:
        nodes.append({
            "id": c.get("entity", "Universe"),
            "label": c.get("label", c.get("entity", "Universe")),
            "type": c.get("type", "core"),
            "status": c.get("status", "ACTIVE"),
            "x": 0.0,
            "y": 100.0,
            "z": 0.0
        })
        
    # 2. Factions Level (y=50)
    for idx, f in enumerate(factions):
        theta = (2 * math.pi * idx) / max(len(factions), 1)
        radius = 80.0
        nodes.append({
            "id": f.get("entity"),
            "label": f.get("label", f.get("entity")),
            "type": "faction",
            "status": f.get("status", "ACTIVE"),
            "x": round(radius * math.cos(theta), 2),
            "y": 50.0,
            "z": round(radius * math.sin(theta), 2)
        })
        
    # 3. Swarm Nodes Level (y=0)
    for idx, s in enumerate(securities):
        theta = (2 * math.pi * idx) / max(len(securities), 1)
        radius = 120.0
        nodes.append({
            "id": s.get("entity"),
            "label": s.get("label", s.get("entity")),
            "type": "security",
            "status": s.get("status", "ACTIVE"),
            "x": round(radius * math.cos(theta), 2),
            "y": 0.0,
            "z": round(radius * math.sin(theta), 2)
        })
        
    # 4. Assets Level (y=-50)
    for idx, a in enumerate(assets):
        theta = (2 * math.pi * idx) / max(len(assets), 1)
        radius = 60.0
        nodes.append({
            "id": a.get("entity"),
            "label": a.get("label", a.get("entity")),
            "type": "asset",
            "status": a.get("status", "ACTIVE"),
            "x": round(radius * math.cos(theta), 2),
            "y": -50.0,
            "z": round(radius * math.sin(theta), 2)
        })
        
    # 5. Others Level (y=-100)
    for idx, o in enumerate(others):
        theta = (2 * math.pi * idx) / max(len(others), 1)
        radius = 100.0
        nodes.append({
            "id": o.get("entity"),
            "label": o.get("label", o.get("entity")),
            "type": o.get("type", "other"),
            "status": o.get("status", "ACTIVE"),
            "x": round(radius * math.cos(theta), 2),
            "y": -100.0,
            "z": round(radius * math.sin(theta), 2)
        })
        
    # Copy edges into links
    for e in edges_2d:
        links.append({
            "source": e.get("source"),
            "target": e.get("target")
        })
        
    topology = {
        "nodes": nodes,
        "links": links
    }
    
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    with open(TOPOLOGY_3D_PATH, "w", encoding="utf-8") as f:
        json.dump(topology, f, indent=2)
        
    print(f"3D Topology successfully compiled and saved to {TOPOLOGY_3D_PATH}")

if __name__ == "__main__":
    build_3d_topology()
