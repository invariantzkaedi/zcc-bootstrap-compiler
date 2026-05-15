import json
import time
import argparse
import traceback
from pathlib import Path
import numpy as np
import torch
import trimesh
import sys

from zkaedi_full_orchestrator import ZkaediOrchestrator
import logging

logging.basicConfig(level=logging.INFO, format='%(asctime)s | %(levelname)s | %(message)s')
logger = logging.getLogger("ZKAEDI_BATCH")

def fix_wsl_path(path_str: str) -> str:
    """Safely bridges Windows drive letters to WSL mount points."""
    if sys.platform != "win32":
        path_str = path_str.replace("\\", "/")
        if len(path_str) >= 3 and path_str[1:3] == ":/":
            drive = path_str[0].lower()
            path_str = f"/mnt/{drive}/" + path_str[3:]
    return path_str

def load_dataset(jsonl_path: str, max_items: int = 5):
    items = []
    with open(jsonl_path, 'r') as f:
        for line in f:
            if not line.strip(): continue
            items.append(json.loads(line))
            if len(items) >= max_items:
                break
    return items

def generate_proxy_mesh(mesh: trimesh.Trimesh, target_faces=2000):
    """
    Fallback proxy mesh generator.
    In production, this is replaced by the C-native Mesh Warden.
    For this batch test, we use Trimesh simplification/convex hull to create a Ghost Mesh.
    """
    try:
        # Simplify mesh (requires manifold, might fail on raw datasets)
        ghost = mesh.convex_hull
        # Inflate slightly to wrap the high poly (proxy inflation)
        ghost.vertices += ghost.vertex_normals * (mesh.scale * 0.01)
        return ghost
    except Exception as e:
        logger.warning(f"Proxy generation failed: {e}")
        return mesh # Worst case fallback

def build_adjacency(faces, num_verts, max_degree):
    import collections
    adj = collections.defaultdict(set)
    for f in faces:
        for i in range(3):
            adj[f[i]].add(f[(i+1)%3])
            adj[f[i]].add(f[(i+2)%3])
    neighs = np.full((num_verts, max_degree), -1, dtype=np.int32)
    counts = np.zeros(num_verts, dtype=np.int32)
    for v in range(num_verts):
        nlist = list(adj[v])[:max_degree]
        counts[v] = len(nlist)
        if len(nlist) > 0:
            neighs[v, :len(nlist)] = nlist
    return {'neighbors': neighs, 'counts': counts}

def run_batch():
    parser = argparse.ArgumentParser(description="ZKAEDI Phase 1 Controlled Scale-Up Batch Runner")
    parser.add_argument("--manifest", default=r"d:\meshy_3d\glb_dataset.jsonl")
    parser.add_argument("--limit", type=int, default=5, help="Number of assets to process")
    parser.add_argument("--out-dir", default="batch_output")
    args = parser.parse_args()

    manifest_path = fix_wsl_path(args.manifest)
    out_dir = Path(args.out_dir)
    out_dir.mkdir(exist_ok=True)
    
    items = load_dataset(manifest_path, args.limit)
    logger.info(f"Loaded {len(items)} assets for Phase 1 Controlled Scale-Up.")
    
    orchestrator = ZkaediOrchestrator()
    summary_report = []
    
    for i, item in enumerate(items, 1):
        asset_id = item["id"]
        glb_path = fix_wsl_path(item["absolute_path"])
        size_mb = item["size_bytes"] / 1e6
        logger.info(f"\n=======================================================")
        logger.info(f"[{i}/{len(items)}] Processing {asset_id} ({size_mb:.1f} MB)")
        logger.info(f"=======================================================")
        
        try:
            t0 = time.time()
            # 1. Load High Poly Mesh
            logger.info("Loading GLB from disk...")
            scene = trimesh.load(glb_path, force="mesh")
            if isinstance(scene, trimesh.Scene):
                # Flatten scene into single mesh
                geometries = [trimesh.Trimesh(vertices=g.vertices, faces=g.faces) for g in scene.geometry.values() if hasattr(g, 'vertices')]
                if not geometries:
                    raise ValueError("No valid geometry found in GLB")
                mesh = trimesh.util.concatenate(geometries)
            else:
                mesh = scene
                
            high_pos = np.array(mesh.vertices, dtype=np.float32)
            high_idx = np.array(mesh.faces, dtype=np.uint32)
            
            # 2. Generate Ghost Mesh (Proxy)
            logger.info("Generating spatial proxy mesh...")
            ghost_mesh = generate_proxy_mesh(mesh, target_faces=2000)
            ghost_verts = np.array(ghost_mesh.vertices, dtype=np.float32)
            ghost_faces = np.array(ghost_mesh.faces, dtype=np.uint32)
            
            logger.info(f"High-Poly geometry: {len(high_pos)} Vertices, {len(high_idx)} Faces")
            logger.info(f"Ghost-Mesh proxy: {len(ghost_verts)} Vertices, {len(ghost_faces)} Faces")
            
            # 3. Setup Orchestrator state
            orchestrator.fusion.verts_cuda = torch.from_numpy(ghost_verts).cuda()
            orchestrator.fusion.loaded = True
            
            # Build dummy adjacency for Triton fusion solver
            adj_dict = build_adjacency(ghost_faces, len(ghost_verts), 12)
            orchestrator.fusion.adjacency_cuda = torch.tensor(adj_dict['neighbors'], device="cuda", dtype=torch.int32)
            orchestrator.fusion.neigh_count_cuda = torch.tensor(adj_dict['counts'], device="cuda", dtype=torch.int32)
            orchestrator.fusion.max_degree = 12
            
            # Synthetic bone sources for testing weight solving
            K = 4
            bone_sources = torch.zeros((len(ghost_verts), K), dtype=torch.float32, device="cuda")
            bone_sources[:, 0] = 1.0  # Just a dummy weight to test pipeline
            
            # 4. Execute Full Pipeline
            out_glb = str(out_dir / f"{asset_id}_rigged.glb")
            weights = orchestrator.process_mesh(
                high_pos, high_idx.flatten(), ghost_verts, ghost_faces, bone_sources, out_glb
            )
            
            t1 = time.time()
            
            # Report Success
            logger.info(f"✅ Success! {asset_id} processed in {t1-t0:.2f}s")
            summary_report.append({
                "id": asset_id,
                "status": "success",
                "time_sec": round(t1-t0, 2),
                "high_verts": len(high_pos),
                "ghost_verts": len(ghost_verts),
                "file_mb": round(size_mb, 2)
            })
            
        except Exception as e:
            logger.error(f"❌ Failed to process {asset_id}: {str(e)}")
            traceback.print_exc()
            summary_report.append({
                "id": asset_id,
                "status": "error",
                "error": str(e),
                "file_mb": round(size_mb, 2)
            })

    # Save summary
    summary_path = out_dir / "phase1_summary.json"
    with open(summary_path, "w") as f:
        json.dump(summary_report, f, indent=2)
    
    success_count = sum(1 for x in summary_report if x["status"] == "success")
    logger.info(f"\n=======================================================")
    logger.info(f"Phase 1 Batch run complete. {success_count}/{len(items)} successful.")
    logger.info(f"Summary telemetry saved to: {summary_path}")
    logger.info(f"=======================================================")

if __name__ == "__main__":
    run_batch()
