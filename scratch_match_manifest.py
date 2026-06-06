import json
import hashlib
from pathlib import Path

manifest_path = Path("MATRIX_MANIFEST.json")
studio_path = Path("/mnt/h/_studio_tripo3d")
remote_path = Path("/mnt/h/__DOWNLOADS/selforglinux/zcc_remote")

def hash_file(file_path):
    h = hashlib.sha512()
    try:
        with open(file_path, "rb") as f:
            while chunk := f.read(256 * 1024):
                h.update(chunk)
        return h.hexdigest()
    except Exception as e:
        print(f"Error hashing {file_path}: {e}")
        return None

def main():
    if not manifest_path.exists():
        print(f"Manifest not found at {manifest_path}")
        exit(1)

    with open(manifest_path) as f:
        manifest = json.load(f)

    signatures = manifest.get("signatures", {})
    # Invert signatures map: hash -> clean name
    hash_to_name = {sig: name for name, sig in signatures.items() if name.endswith(".glb")}
    print(f"Loaded {len(hash_to_name)} GLB signatures from manifest.")

    # Map target size -> list of (clean_name, hash)
    size_to_target = {}
    for hash_val, clean_name in hash_to_name.items():
        clean_file = remote_path / clean_name
        if clean_file.exists():
            size = clean_file.stat().st_size
            size_to_target.setdefault(size, []).append((clean_name, hash_val))
        else:
            # Check in zcc root as fallback
            clean_file_fallback = Path(".") / clean_name
            if clean_file_fallback.exists():
                size = clean_file_fallback.stat().st_size
                size_to_target.setdefault(size, []).append((clean_name, hash_val))

    print(f"Mapped {len(size_to_target)} unique file sizes from clean assets.")

    # Now scan studio_path
    glb_files = list(studio_path.glob("*.glb"))
    print(f"Scanning {len(glb_files)} files in studio path...")

    found_mappings = {}
    candidates = []
    
    for file in glb_files:
        size = file.stat().st_size
        if size in size_to_target:
            candidates.append((file, size))

    print(f"Found {len(candidates)} file size candidates to hash (out of {len(glb_files)} total files).")

    for i, (file, size) in enumerate(candidates, 1):
        file_hash = hash_file(file)
        if not file_hash:
            continue
            
        # Check against target hashes for this size
        for clean_name, target_hash in size_to_target[size]:
            if file_hash == target_hash:
                found_mappings[file.name] = clean_name
                print(f"  -> SUCCESS: {file.name} matches {clean_name}")
                break

    print(f"\nTotal matches found: {len(found_mappings)}")
    
    expected_assets = set(hash_to_name.values())
    found_assets = set(found_mappings.values())
    missing = expected_assets - found_assets
    
    print(f"Matches found: {len(found_assets)} / {len(expected_assets)} expected clean assets")
    if missing:
        print("Missing clean assets:")
        for m in sorted(missing):
            print(f"  - {m}")
    else:
        print("All expected clean assets matched successfully!")

    # Write output JSON
    output_mapping = studio_path / "detected_mappings.json"
    with open(output_mapping, "w") as f:
        json.dump(found_mappings, f, indent=2)
    print(f"Wrote detected mappings to {output_mapping}")

if __name__ == "__main__":
    main()
