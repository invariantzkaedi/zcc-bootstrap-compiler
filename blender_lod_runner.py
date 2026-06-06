#!/usr/bin/env python3
import json
import subprocess
import sys
from pathlib import Path

# Config
LOD_FACE_LIMITS = {1: 80_000, 2: 25_000, 3: 8_000}
FLEET_JSON = Path("fleet_ir.json")
GLB_DIR = Path("/mnt/h/__DOWNLOADS/selforglinux/zcc_remote")
OUT_DIR = Path("lod_out")
DECIMATE_SCRIPT = Path("scratch/blender_decimate.py")

def run_blender_decimate(input_path: Path, output_path: Path, target_faces: int):
    cmd = [
        "blender",
        "--background",
        "--python", str(DECIMATE_SCRIPT),
        "--",
        str(input_path),
        str(output_path),
        str(target_faces)
    ]
    
    # Run process headlessly
    res = subprocess.run(cmd, capture_output=True, text=True)
    if res.returncode != 0:
        print(f"ERROR: Blender returned non-zero exit code: {res.returncode}")
        print(f"STDOUT:\n{res.stdout}")
        print(f"STDERR:\n{res.stderr}")
        return False
    return True

def main():
    if not FLEET_JSON.exists():
        print(f"ERROR: {FLEET_JSON} not found.", file=sys.stderr)
        sys.exit(1)
        
    if not DECIMATE_SCRIPT.exists():
        print(f"ERROR: Decimate script {DECIMATE_SCRIPT} not found.", file=sys.stderr)
        sys.exit(1)
        
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    
    # Parse fleet
    data = json.loads(FLEET_JSON.read_text())
    plan = data.get("lod_plan_optimized_v2", {})
    picks = list(plan.get("guaranteed_picks", [])) + list(plan.get("greedy_picks", []))
    
    print(f"Loaded {len(picks)} picks from LOD plan.")
    
    succeeded = 0
    failed = 0
    skipped = 0
    
    for i, p in enumerate(picks, 1):
        asset = p["asset"]
        lod_level = p["lod"]
        target_faces = LOD_FACE_LIMITS[lod_level]
        
        input_file = GLB_DIR / f"{asset}.glb"
        # Fallback to local root if not found in remote downloads folder
        if not input_file.exists():
            input_file = Path(".") / f"{asset}.glb"
            
        output_file = OUT_DIR / f"{asset}_LOD{lod_level}.glb"
        
        print(f"\n[{i}/{len(picks)}] Processing {asset} LOD{lod_level} (target: {target_faces} faces)")
        
        # Check if already generated
        if output_file.exists():
            print(f"  -> SKIP: {output_file.name} already exists.")
            skipped += 1
            continue
            
        if not input_file.exists():
            print(f"  -> ERROR: Source file {input_file.name} not found.")
            failed += 1
            continue
            
        print(f"  -> Decimating {input_file.name} in Blender headlessly...")
        ok = run_blender_decimate(input_file, output_file, target_faces)
        
        if ok and output_file.exists():
            print(f"  -> SUCCESS: Generated {output_file.name} ({output_file.stat().st_size / (1024*1024):.2f} MB)")
            succeeded += 1
        else:
            print(f"  -> FAILED generating LOD for {asset}")
            failed += 1
            
    print(f"\n=== local Blender LOD runner complete ===")
    print(f"Succeeded: {succeeded}")
    print(f"Failed:    {failed}")
    print(f"Skipped:   {skipped}")

if __name__ == "__main__":
    main()
