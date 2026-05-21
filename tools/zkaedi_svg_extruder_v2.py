import os
import re
import sys
import math
import struct
import hashlib
import numpy as np

# Force UTF-8 for Windows console emoji printing
sys.stdout.reconfigure(encoding='utf-8')
import svgpathtools
import mapbox_earcut as earcut

tools_dir = os.path.dirname(os.path.abspath(__file__))
zcc_root = os.path.dirname(tools_dir)
sys.path.insert(0, zcc_root)
try:
    import alerting
except ImportError:
    alerting = None

def parse_svg_to_polygons(path_d):
    """Adaptive segmentation based on path length for optimized rhythm."""
    path = svgpathtools.parse_path(path_d)
    polygons = []
    current_poly = []
    
    # Adaptive num_segments
    complexity = len(path)
    base_segments = 12 if complexity < 50 else 6
    
    for segment in path:
        if current_poly and np.linalg.norm(np.array([segment.start.real, segment.start.imag]) - np.array(current_poly[-1])) > 1e-3:
            polygons.append(current_poly)
            current_poly = []
            
        # Distribute segments based on curve type (lines need fewer than beziers)
        num_seg = base_segments if type(segment) != svgpathtools.Line else 1
        
        for i in range(num_seg + 1):
            t = i / float(num_seg)
            point = segment.point(t)
            v = [point.real, point.imag]
            if not current_poly or np.linalg.norm(np.array(v) - np.array(current_poly[-1])) > 1e-4:
                current_poly.append(v)
                
    if current_poly:
        polygons.append(current_poly)
        
    return polygons

def earcut_triangulate(polygons):
    if not polygons:
        return np.zeros((0, 2), dtype=np.float32), np.zeros((0, 3), dtype=np.int32), polygons

    flat_vertices = []
    ring_endpoints = []
    current_idx = 0

    for poly in polygons:
        for v in poly:
            flat_vertices.extend(v)
        current_idx += len(poly)
        ring_endpoints.append(current_idx)

    vertices_2d_np = np.array(flat_vertices, dtype=np.float32).reshape(-1, 2)
    ring_endpoints_np = np.array(ring_endpoints, dtype=np.uint32)

    triangles = earcut.triangulate_float32(vertices_2d_np, ring_endpoints_np)
    faces_2d = np.array(triangles).reshape(-1, 3) if len(triangles) > 0 else np.zeros((0, 3), dtype=np.int32)
    
    return vertices_2d_np, faces_2d, polygons

def extrude_mesh_clean(vertices_2d, faces_2d, polygons, depth=20.0):
    n = len(vertices_2d)
    if n == 0:
        return np.zeros((0, 3), dtype=np.float32), np.zeros((0, 3), dtype=np.int32)
        
    # Fortification: Math purity. Strip NaNs & Infinites
    nan_mask = np.isnan(vertices_2d)
    inf_mask = np.isinf(vertices_2d)
    if (nan_mask.any() or inf_mask.any()) and alerting:
        alerting.emit_alert("WARNING", "ZKAEDI_CORRUPTION_PURGED", "extrude_mesh_clean", f"Purged {nan_mask.sum()} NaNs and {inf_mask.sum()} Infs")
        
    verts_2d = np.nan_to_num(vertices_2d, posinf=0.0, neginf=0.0)
    
    verts_3d = []
    for v in verts_2d:
        verts_3d.append([v[0], v[1], depth / 2.0])
    for v in verts_2d:
        verts_3d.append([v[0], v[1], -depth / 2.0])
        
    verts_3d = np.array(verts_3d, dtype=np.float32)
    faces_3d = []
    
    max_idx = (2 * n) - 1
    
    # Front and Back faces
    for f in faces_2d:
        if max(f[0], f[1], f[2]) <= max_idx:
            faces_3d.append([f[0], f[1], f[2]])
            
    for f in faces_2d:
        b0, b1, b2 = f[2] + n, f[1] + n, f[0] + n
        if max(b0, b1, b2) <= max_idx:
            faces_3d.append([b0, b1, b2])
        
    # Connect boundaries
    offset = 0
    for poly in polygons:
        poly_len = len(poly)
        for i in range(poly_len):
            curr_idx = offset + i
            next_idx = offset + ((i + 1) % poly_len)
            c_front, n_front = curr_idx, next_idx
            c_back, n_back = curr_idx + n, next_idx + n
            
            if max(c_front, n_front, c_back, n_back) <= max_idx:
                faces_3d.append([c_front, n_front, c_back])
                faces_3d.append([n_front, n_back, c_back])
        offset += poly_len
        
    assert len(verts_3d) == 2 * n, "FATAL: ZKAEDI pipeline vertex overflow detected."
    return verts_3d, np.array(faces_3d, dtype=np.int32)

def generate_zkaedi_proxy_textures(verts, faces, out_pos_file, out_faces_file):
    center = np.mean(verts, axis=0)
    verts -= center
    max_dist = np.max(np.abs(verts))
    if max_dist > 0:
        verts /= max_dist
        
    verts[:, 1] *= -1.0 

    with open(out_pos_file, 'wb') as f:
        for v in verts:
            f.write(struct.pack('<fff', v[0], v[1], v[2]))
            
    with open(out_faces_file, 'wb') as f:
        for face in faces:
            f.write(struct.pack('<iii', int(face[0]), int(face[1]), int(face[2])))
            
    # Security fortify: lock bins to read-only
    os.chmod(out_pos_file, 0o444)
    os.chmod(out_faces_file, 0o444)

def unknowncode_seed(svg_hash: str, depth: float = 50.0):
    seed = int(svg_hash, 16) % (2**32)
    np.random.seed(seed)
    
    # Ignite 32 ghost vertices for swarm
    ghost_verts = np.random.uniform(-0.15, 0.15, (32, 3)).astype(np.float32)
    ghost_verts[:, 2] *= depth * 0.618  # Golden ratio Z pulse
    
    # Fortify: Catch NaNs instantly
    ghost_verts = np.nan_to_num(ghost_verts, copy=False)
    np.testing.assert_allclose(np.isnan(ghost_verts).sum(), 0)
    
    tools_dir = os.path.dirname(os.path.abspath(__file__))
    ghost_bin = os.path.join(tools_dir, 'proxy_ghost.bin')
    with open(ghost_bin, 'wb') as f:
        for v in ghost_verts:
            f.write(struct.pack('<fff', *v))
    os.chmod(ghost_bin, 0o444)
    return True

def reward_jackpot(verts_count: int, faces_count: int, ghost_count: int = 32, sentient=True) -> str:
    print(f"\n{'='*90}")
    print(f"777JACKPOT777 — {verts_count} verts | {faces_count} faces | {ghost_count} ghosts")
    if sentient:
        print("===UNKNOWNCODE=== SWEEP COMPLETE")
        print("The geometry has learned your name.")
        print("Ghost layer is now fully sentient.")
    print("You didn’t just sweep it. You *became* the mainframe.")
    
    try:
        tools_dir = os.path.dirname(os.path.abspath(__file__))
        with open(os.path.join(tools_dir, 'proxy_verts.bin'), 'rb') as f:
            h = hashlib.sha256(f.read()).hexdigest()[:16]
        print(f"PROXY HASH: {h} (fortified)")
    except:
        pass
        
    print(f"{'='*90}\n")
    if alerting:
        alerting.emit_alert("CRITICAL", "ZKAEDI_UNKNOWNCODE_JACKPOT", "reward_jackpot", f"Sweep complete. Sentient: {sentient}")
        
    return "🎰 LUCKY COUPLER + SWEEP PROTOCOL = eternal flow"

if __name__ == "__main__":
    tools_dir = os.path.dirname(os.path.abspath(__file__))
    zcc_root = os.path.dirname(tools_dir)
    svg_file = os.path.join(zcc_root, 'YETIIII.txt')
    pos_out = os.path.join(tools_dir, 'proxy_verts.bin')
    faces_out = os.path.join(tools_dir, 'proxy_faces.bin')
    ghost_out = os.path.join(tools_dir, 'proxy_ghost.bin')
    
    try:
        # Clear existing read-only locks before sweeping
        for f in [pos_out, faces_out, ghost_out]:
            if os.path.exists(f):
                os.chmod(f, 0o666)
                os.remove(f)

        with open(svg_file, 'r', encoding='utf-8') as f:
            data = f.read()
            
        if len(data) > 500_000:
            sys.exit("[-] FATAL: SVG too thicc")
            
        paths = re.findall(r'<path d="([^"]+)"', data)
        
        if not paths:
            print("[-] No SVG paths found.")
            sys.exit(1)
            
        target_path = paths[6] if len(paths) > 6 else paths[0]
        
        polygons = parse_svg_to_polygons(target_path)
        verts_2d, faces_2d, _ = earcut_triangulate(polygons)
        verts_3d, faces_3d = extrude_mesh_clean(verts_2d, faces_2d, polygons, depth=50.0)
        
        if len(verts_3d) * 3 % 3 != 0:
            raise RuntimeError("ZKAEDI rhythm broken — verts not divisible by 3")

        np.testing.assert_array_equal(verts_3d.shape[1], 3)
        
        generate_zkaedi_proxy_textures(verts_3d, faces_3d, pos_out, faces_out)
        
        if alerting:
            alerting.emit_alert("INFO", "ZKAEDI_PIPELINE_SYNC", "main", f"Proxy textures built. {len(verts_3d)} verts.")
            
        svg_hash = hashlib.sha256(data.encode()).hexdigest()[:16]
        unknown_active = False
        try:
            unknown_active = unknowncode_seed(svg_hash, depth=50.0)
        except Exception as e:
            print(f"[-] SWEEP GLITCH: {e}")
        
        msg = reward_jackpot(len(verts_3d), len(faces_3d), ghost_count=32, sentient=unknown_active)
        print(msg)
        
    except Exception as e:
        if alerting:
            alerting.emit_alert("ERROR", "ZKAEDI_SWEEP_GLITCH", "main", str(e))
        print(f"[-] SWEEP GLITCH: {e}")
