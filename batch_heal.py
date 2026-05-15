import os
import glob
import subprocess

def main():
    src_dir = "mega_corpus"
    out_dir = "healed_payloads"
    os.makedirs(out_dir, exist_ok=True)
    
    # Use iglob to avoid loading all 50,000 into memory at once
    files = []
    for f in glob.iglob(os.path.join(src_dir, "*.bin")):
        files.append(f)
        if len(files) >= 50000:
            break
            
    print(f"[ZKAEDI] Healing {len(files)} payloads...")
    
    for i, f in enumerate(files, 1):
        basename = os.path.basename(f)
        out_name = basename.replace(".bin", "_healed.yul")
        out_path = os.path.join(out_dir, out_name)
        
        # ./zcc -fevm-emit mega_corpus/xyz.bin > healed_payloads/xyz_healed.yul
        with open(out_path, "w") as out_f:
            subprocess.run(["./zcc", "-fevm-emit", f], stdout=out_f)
            
        if i % 1000 == 0:
            print(f"  [{i}/{len(files)}] Sutured checkpoint...")
        
    print("\n[ZKAEDI] Batch heal complete.")

if __name__ == "__main__":
    main()
