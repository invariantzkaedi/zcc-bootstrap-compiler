#!/usr/bin/env python3
"""
fleet_extract.py — Walk a directory of GLBs and emit PRIME-scored IR JSON.
Usage:  python3 fleet_extract.py /path/to/glbs -o fleet_ir.json
Requires: pygltflib, pillow, numpy     pip install pygltflib pillow numpy
"""
from __future__ import annotations
import argparse, hashlib, io, json, math, struct, sys
from pathlib import Path
import numpy as np
from PIL import Image
from pygltflib import GLTF2

def analyze_textures(path: Path):
    g = GLTF2().load(str(path))
    raw = path.read_bytes()
    if len(raw) < 28: return []
    # GLB header: magic, version, length, jlen, jtype
    jlen, _ = struct.unpack_from("<II", raw, 12)
    bin_off = 12 + 8 + jlen
    if bin_off + 8 >= len(raw): return []
    blen, _ = struct.unpack_from("<II", raw, bin_off)
    bin_data = raw[bin_off+8 : bin_off+8+blen]

    results = []
    for i, img in enumerate(g.images or []):
        if img.bufferView is None: continue
        bv = g.bufferViews[img.bufferView]
        img_bytes = bin_data[bv.byteOffset : bv.byteOffset + bv.byteLength]
        try:
            pil = Image.open(io.BytesIO(img_bytes)).convert("RGB")
        except Exception:
            continue
        pil.thumbnail((64, 64))
        arr = np.array(pil)
        hsv = np.array(pil.convert("HSV"))
        rgb_std = arr.reshape(-1,3).std(axis=0)
        quantized = (arr // 32) * 32
        unique, counts = np.unique(quantized.reshape(-1,3), axis=0, return_counts=True)
        top5 = counts.argsort()[-5:][::-1]
        gx = np.abs(np.diff(arr.astype(int), axis=0)).mean()
        gy = np.abs(np.diff(arr.astype(int), axis=1)).mean()
        results.append({
            "image_idx": i,
            "hsv_mean": [float(hsv[:,:,c].mean()) for c in range(3)],
            "rgb_std":  [float(x) for x in rgb_std],
            "edge_density": float((gx+gy)/2),
            "palette_top5":    [[int(x) for x in unique[k]] for k in top5],
            "palette_weights": [int(counts[k]) for k in top5],
        })
    return results

def extract_ir(path: Path):
    try:
        g = GLTF2().load(str(path))
    except Exception as e:
        return {"file": path.name, "error": f"parse:{e}"}

    total_verts = total_tris = mesh_count = prim_count = 0
    total_vol = largest_dim = 0.0
    flags = {"has_normals":False, "has_tangents":False, "has_uvs":False}
    for mesh in g.meshes or []:
        mesh_count += 1
        for prim in mesh.primitives:
            prim_count += 1
            if prim.indices is not None:
                total_tris += (g.accessors[prim.indices].count or 0) // 3
            if prim.attributes.POSITION is not None:
                acc = g.accessors[prim.attributes.POSITION]
                total_verts += acc.count or 0
                if acc.max and acc.min:
                    dx,dy,dz = [acc.max[i]-acc.min[i] for i in range(3)]
                    total_vol += dx*dy*dz
                    largest_dim = max(largest_dim, max(dx,dy,dz))
            if prim.attributes.NORMAL:  flags["has_normals"]=True
            if prim.attributes.TANGENT: flags["has_tangents"]=True
            if prim.attributes.TEXCOORD_0: flags["has_uvs"]=True

    mats = g.materials or []
    tex = analyze_textures(path)
    first = tex[0] if tex else None
    hsv = first["hsv_mean"] if first else [0,0,128]
    ed  = first["edge_density"] if first else 10.0

    tri_norm  = min(total_tris / 500_000, 1.0)
    vert_norm = min(total_verts / 260_000, 1.0)
    edge_norm = min(ed / 25.0, 1.0)
    s_norm    = hsv[1] / 255.0
    v_norm    = hsv[2] / 255.0

    weights = first["palette_weights"] if first else [1]
    total_w = sum(weights) or 1
    richness = sum(1 for w in weights if w/total_w > 0.05) / 5.0

    H0 = (0.20*tri_norm + 0.10*vert_norm + 0.25*edge_norm
        + 0.15*s_norm + 0.10*v_norm + 0.20*richness)

    scores = {
        "organic":     (0.4 if 30<=hsv[0]<=90 else 0.1) + (0.3 if edge_norm>0.6 else 0.1)
                     + (0.2 if 0.3<v_norm<0.7 else 0.05) + (0.15 if s_norm>0.3 else 0),
        "mechanical":  (0.35 if s_norm<0.4 else 0.1) + (0.3 if edge_norm>0.6 else 0.1)
                     + (0.2 if 0.3<v_norm<0.7 else 0.05)
                     + (0.15 if hsv[0]<=30 or hsv[0]>=150 else 0),
        "crystalline": (0.3 if s_norm<0.3 else 0.05) + (0.3 if edge_norm<0.5 else 0.1)
                     + (0.4 if v_norm>0.5 else 0.1),
        "stellar":     (0.45 if s_norm>0.6 else 0.1) + (0.3 if v_norm>0.4 else 0.1)
                     + (0.25 if hsv[0]<60 or hsv[0]>140 else 0.05),
        "spectral":    (0.3 if 80<=hsv[0]<=140 else 0.05)
                     + (0.25 if 0.15<s_norm<0.5 else 0.05)
                     + (0.25 if edge_norm<0.5 else 0.05)
                     + (0.2 if v_norm<0.55 else 0.05),
    }
    dominant = max(scores, key=scores.get)
    if   H0 >= 0.75: tier = "LEGENDARY"
    elif H0 >= 0.60: tier = "MYTHIC"
    elif H0 >= 0.45: tier = "EPIC"
    elif H0 >= 0.30: tier = "RARE"
    else:            tier = "UNCOMMON"

    return {
        "file": path.name, "name": path.stem,
        "sha256": hashlib.sha256(path.read_bytes()).hexdigest()[:16],
        "size_mb": round(path.stat().st_size/(1024*1024), 2),
        "geometry": {
            "meshes": mesh_count, "primitives": prim_count,
            "vertices": total_verts, "triangles": total_tris,
            "bbox_volume": round(total_vol, 3),
            "largest_dim": round(largest_dim, 3),
            **flags,
        },
        "materials": {"count": len(mats)},
        "textures":  {"textures": len(g.textures or []), "images": len(g.images or [])},
        "palette":   first["palette_top5"] if first else [],
        "palette_weights": first["palette_weights"] if first else [],
        "hsv_mean":  hsv,
        "prime": {
            "H0_refined": round(H0, 4),
            "class_scores_refined": {k: round(v,3) for k,v in scores.items()},
            "dominant_class_refined": dominant,
            "tier_refined": tier,
            "richness": round(richness, 3),
            "edge_norm": round(edge_norm, 3),
        }
    }

def build_similarity(fleet):
    names = [a["name"] for a in fleet]
    vecs = np.array([
        [a["hsv_mean"][0]/180, a["hsv_mean"][1]/255, a["hsv_mean"][2]/255,
         a["prime"]["edge_norm"], a["prime"]["richness"]]
        for a in fleet
    ])
    norms = np.linalg.norm(vecs, axis=1, keepdims=True)
    normed = vecs / np.maximum(norms, 1e-9)
    return {"names": names, "matrix": (normed @ normed.T).tolist()}

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("glb_dir", help="Directory containing .glb files")
    ap.add_argument("-o", "--output", default="fleet_ir.json")
    args = ap.parse_args()

    d = Path(args.glb_dir)
    if not d.is_dir():
        print(f"ERROR: {d} is not a directory", file=sys.stderr); sys.exit(2)

    fleet = []
    for p in sorted(d.glob("*.glb")):
        ir = extract_ir(p)
        fleet.append(ir)
        pr = ir.get("prime", {})
        print(f"  {p.stem:<40} H0={pr.get('H0_refined',0):.3f}  "
              f"{pr.get('tier_refined',''):<10} {pr.get('dominant_class_refined','')}")

    out = {"fleet": fleet, "count": len(fleet),
           "similarity_matrix": build_similarity(fleet) if fleet else {}}
    Path(args.output).write_text(json.dumps(out, indent=2))
    print(f"\n[ok] {args.output} written with {len(fleet)} assets")

if __name__ == "__main__":
    main()
