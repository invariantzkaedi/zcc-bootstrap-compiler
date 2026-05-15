import os
import zipfile
import glob
import re

ZIP_FILES = [
    r"H:\_studio_tripo3d\Meshy_AI_assets_20260514_232550.zip",
    r"H:\_studio_tripo3d\Meshy_AI_assets_20260514_232507.zip",
    r"H:\_studio_tripo3d\Meshy_AI_assets_20260514_232257.zip",
    r"H:\_studio_tripo3d\Meshy_AI_assets_20260514_232225.zip"
]

DEST_DIR = r"d:\meshy_3d"
HTML_FILE = r"g:\zccMAIN\zcc\dashboard_hamiltonian_visualizer.html"

def main():
    print(f"[ZKAEDI] Extracting 40 assets to {DEST_DIR}...")
    os.makedirs(DEST_DIR, exist_ok=True)
    
    for zip_path in ZIP_FILES:
        if os.path.exists(zip_path):
            with zipfile.ZipFile(zip_path, 'r') as zf:
                zf.extractall(DEST_DIR)
                print(f"  -> Extracted: {os.path.basename(zip_path)}")
        else:
            print(f"  [!] Missing: {zip_path}")
            
    # Collect all GLBs
    glb_files = glob.glob(os.path.join(DEST_DIR, "*.glb"))
    print(f"[ZKAEDI] Found {len(glb_files)} .glb files in {DEST_DIR}.")
    
    # Patch HTML
    with open(HTML_FILE, "r", encoding="utf-8") as f:
        html = f.read()
        
    options_html = ""
    for glb in glb_files:
        filename = os.path.basename(glb)
        name = filename.replace("Meshy_AI_", "").replace("_texture.glb", "").replace(".glb", "")
        options_html += f'            <option value="/assets/{filename}">{name}</option>\n'
        
    # Replace the select block using regex
    pattern = re.compile(r'(<select id="asset-selector"[^>]*>).*?(</select>)', re.DOTALL)
    new_html = pattern.sub(rf'\1\n{options_html}        \2', html)
    
    with open(HTML_FILE, "w", encoding="utf-8") as f:
        f.write(new_html)
        
    print("[ZKAEDI] Asset Dropdown Patched! Refresh the Dashboard.")

if __name__ == "__main__":
    main()
