#!/usr/bin/env python3
import json
import io
import struct
from pathlib import Path
from PIL import Image
from pygltflib import GLTF2

fleet_json_path = Path("fleet_ir.json")
lod_dir = Path("lod_out")

def calculate_vram_mb(path: Path) -> float:
    try:
        g = GLTF2().load(str(path))
    except Exception as e:
        print(f"Error loading {path}: {e}")
        return 0.0
    
    # 1. Geometry Vertices
    verts = 0
    for mesh in g.meshes or []:
        for prim in mesh.primitives:
            if prim.attributes.POSITION is not None:
                verts += g.accessors[prim.attributes.POSITION].count or 0
    geo_bytes = verts * 48  # pos:12, norm:12, uv:8, tang:16
    
    # 2. Texture Bytes (RGBA + Mipmaps)
    tex_bytes = 0
    try:
        raw = path.read_bytes()
        if len(raw) >= 20:
            # GLB Header parsing
            magic, version, length = struct.unpack_from("<III", raw, 0)
            if magic == 0x46546C67:  # "glTF" binary
                jlen, jtype = struct.unpack_from("<II", raw, 12)
                bin_off = 20 + jlen
                if bin_off + 8 < len(raw):
                    blen, btype = struct.unpack_from("<II", raw, bin_off)
                    bin_data = raw[bin_off + 8 : bin_off + 8 + blen]
                    for img in g.images or []:
                        if img.bufferView is None:
                            continue
                        bv = g.bufferViews[img.bufferView]
                        img_bytes = bin_data[bv.byteOffset : bv.byteOffset + bv.byteLength]
                        try:
                            pil = Image.open(io.BytesIO(img_bytes))
                            w, h = pil.size
                            tex_bytes += w * h * 4 * 1.33
                        except Exception as e:
                            pass
    except Exception as e:
        print(f"Error reading texture bytes for {path.name}: {e}")
        
    total_bytes = geo_bytes + tex_bytes
    return round(total_bytes / (1024 * 1024), 2)

def get_typology(name: str) -> str:
    name_lower = name.lower()
    if any(k in name_lower for k in ("ufo", "mothership", "drone")):
        return "vessel"
    if any(k in name_lower for k in ("car", "locomotive", "vehicle", "sports_car")):
        return "vehicle"
    if any(k in name_lower for k in ("character", "kitsune", "lord", "nurse", "automaton")):
        return "character"
    if any(k in name_lower for k in ("dragon", "yeti", "leviathan", "seraphim", "void_watcher")):
        return "creature"
    if any(k in name_lower for k in ("temple", "citadel", "jungle", "environment", "atlantis")):
        return "environment"
    return "artifact"

def get_lod_info(path: Path):
    try:
        g = GLTF2().load(str(path))
        verts = 0
        tris = 0
        for mesh in g.meshes or []:
            for prim in mesh.primitives:
                if prim.attributes.POSITION is not None:
                    verts += g.accessors[prim.attributes.POSITION].count or 0
                if prim.indices is not None:
                    tris += (g.accessors[prim.indices].count or 0) // 3
        return verts, tris
    except Exception as e:
        print(f"Error getting LOD info for {path}: {e}")
        return 0, 0

def main():
    if not fleet_json_path.exists():
        print(f"Error: {fleet_json_path} not found.")
        return

    data = json.loads(fleet_json_path.read_text())
    fleet = data.get("fleet", [])
    
    print(f"Loaded {len(fleet)} assets from {fleet_json_path}")
    
    enriched_count = 0
    lods_found_count = 0
    
    for asset in fleet:
        name = asset.get("name")
        # Ensure typology
        if "typology" not in asset or not asset["typology"]:
            asset["typology"] = get_typology(name)
            
        # Ensure base VRAM calculation
        if "vram_mb" not in asset or asset["vram_mb"] == 0:
            # Try to load base asset from zcc_remote or local directory
            base_file = Path("h:/__DOWNLOADS/selforglinux/zcc_remote") / f"{name}.glb"
            if not base_file.exists():
                base_file = Path(".") / f"{name}.glb"
            if base_file.exists():
                asset["vram_mb"] = calculate_vram_mb(base_file)
            else:
                # Estimate VRAM roughly if file is missing
                verts = asset.get("geometry", {}).get("vertices", 0)
                asset["vram_mb"] = round((verts * 48) / (1024 * 1024), 2)
        
        # Scan for generated LODs
        lod_entries = []
        for level in (1, 2, 3):
            lod_file = lod_dir / f"{name}_LOD{level}.glb"
            if lod_file.exists():
                lods_found_count += 1
                verts, tris = get_lod_info(lod_file)
                vram = calculate_vram_mb(lod_file)
                vram_saved = round(max(0.0, asset.get("vram_mb", 0.0) - vram), 2)
                lod_entries.append({
                    "level": level,
                    "file": lod_file.name,
                    "vertices": verts,
                    "triangles": tris,
                    "vram_mb": vram,
                    "vram_saved_mb": vram_saved
                })
        
        if lod_entries:
            asset["lods"] = lod_entries
            
        enriched_count += 1

    # Write back enriched JSON
    fleet_json_path.write_text(json.dumps(data, indent=2))
    print(f"\n[ok] Enriched {enriched_count} assets in {fleet_json_path}")
    print(f"[ok] Found and processed {lods_found_count} generated LOD models in {lod_dir}")

if __name__ == "__main__":
    main()
