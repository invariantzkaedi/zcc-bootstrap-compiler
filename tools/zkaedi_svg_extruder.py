import re
import math
import struct
import numpy as np
import svgpathtools

def parse_and_flatten_svg(path_d, num_segments=10):
    """
    Parses an SVG path 'd' string, flattens all bezier curves into discrete line segments,
    and returns a list of 2D vertices [x, y].
    """
    path = svgpathtools.parse_path(path_d)
    vertices = []
    
    for segment in path:
        for i in range(num_segments + 1):
            t = i / float(num_segments)
            point = segment.point(t)
            # svgpathtools returns complex numbers for 2D coords
            v = [point.real, point.imag]
            if not vertices or np.linalg.norm(np.array(v) - np.array(vertices[-1])) > 1e-4:
                vertices.append(v)
                
    return np.array(vertices)

def simple_triangulate_convex(vertices):
    """
    A minimal triangle fan approach for convex/simple shapes.
    (For complex SVG paths with holes, a proper Delaunay or Earcut is required).
    Returns a list of faces [v1_idx, v2_idx, v3_idx].
    """
    faces = []
    for i in range(1, len(vertices) - 1):
        faces.append([0, i, i + 1])
    return faces

def extrude_mesh(vertices, faces, depth=100.0):
    """
    Extrudes the 2D mesh into a 3D volume.
    Returns 3D vertices and 3D faces.
    """
    n = len(vertices)
    verts_3d = []
    
    # Front face (z = depth/2)
    for v in vertices:
        verts_3d.append([v[0], v[1], depth / 2.0])
        
    # Back face (z = -depth/2)
    for v in vertices:
        verts_3d.append([v[0], v[1], -depth / 2.0])
        
    verts_3d = np.array(verts_3d)
    faces_3d = []
    
    # Front faces (normal +Z)
    for f in faces:
        faces_3d.append([f[0], f[1], f[2]])
        
    # Back faces (normal -Z, inverted winding)
    for f in faces:
        faces_3d.append([f[2] + n, f[1] + n, f[0] + n])
        
    # Side walls (stitching the perimeter)
    # This assumes the vertices define the perimeter sequentially
    for i in range(n):
        next_i = (i + 1) % n
        # Triangle 1
        faces_3d.append([i, next_i, i + n])
        # Triangle 2
        faces_3d.append([next_i, next_i + n, i + n])
        
    return verts_3d, np.array(faces_3d)

def generate_zkaedi_proxy_textures(verts, faces, out_pos_file, out_faces_file):
    """
    Packs the 3D geometry into flat binary buffers for the ZKAEDI GLSL O(1) texture lookups.
    - proxyVertexPositions: RGB32F (12 bytes per vertex)
    - proxyFaces: RGB32I (12 bytes per face)
    """
    # 1. Pack Vertex Positions (Float32 x 3)
    with open(out_pos_file, 'wb') as f:
        for v in verts:
            f.write(struct.pack('<fff', v[0], v[1], v[2]))
            
    # 2. Pack Faces (Int32 x 3)
    with open(out_faces_file, 'wb') as f:
        for face in faces:
            f.write(struct.pack('<iii', int(face[0]), int(face[1]), int(face[2])))
            
    print(f"[+] Proxy Textures Baked:")
    print(f"    -> {out_pos_file} ({len(verts)} vertices)")
    print(f"    -> {out_faces_file} ({len(faces)} faces)")

if __name__ == "__main__":
    svg_file = r'g:\zccMAIN\zcc\YETIIII.txt'
    
    try:
        with open(svg_file, 'r', encoding='utf-8') as f:
            data = f.read()
            
        import re
        paths = re.findall(r'<path d="([^"]+)"', data)
        
        if not paths:
            print("[-] No SVG paths found.")
            sys.exit(1)
            
        # Target the 'cog' path (index 6) or fallback to first
        target_path = paths[6] if len(paths) > 6 else paths[0]
        
        print("[*] Flattening bezier curves...")
        flat_verts = parse_and_flatten_svg(target_path, num_segments=8)
        
        print("[*] Triangulating 2D mesh...")
        flat_faces = simple_triangulate_convex(flat_verts)
        
        print("[*] Extruding Z-axis volume...")
        verts_3d, faces_3d = extrude_mesh(flat_verts, flat_faces, depth=50.0)
        
        print("[*] Normalizing 3D coordinate space...")
        # Center the mesh at origin
        center = np.mean(verts_3d, axis=0)
        verts_3d -= center
        # Scale down to fit in a standard GLSL [-1, 1] unit box
        max_dist = np.max(np.abs(verts_3d))
        verts_3d /= max_dist
        
        print("[*] Generating ZKAEDI GPU Textures...")
        pos_out = r'g:\zccMAIN\zcc\tools\proxy_verts.bin'
        faces_out = r'g:\zccMAIN\zcc\tools\proxy_faces.bin'
        generate_zkaedi_proxy_textures(verts_3d, faces_3d, pos_out, faces_out)
        
        print("\n[+] ZKAEDI Pipeline Update Complete. The SVG is now a zero-copy 3D asset.")
        
    except Exception as e:
        print(f"[-] FATAL: {e}")
