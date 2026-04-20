import sys
import trimesh
import numpy as np

def main():
    if len(sys.argv) < 3:
        print("Usage: python extract_mesh_to_h.py <input.glb> <output.h>")
        sys.exit(1)
    
    in_file = sys.argv[1]
    out_file = sys.argv[2]
    
    # Load GLB scene
    scene = trimesh.load(in_file, force="scene")
    
    # Flatten geometry
    # A single Trimesh object merging all geometries
    mesh = trimesh.util.concatenate([geom for geom in scene.geometry.values()])
    
    # We want around 8000-15000 faces for sanity if possible, but we'll take what we get.
    faces = mesh.faces
    if len(faces) > 8100:
        faces = faces[:8100]
    vertices = mesh.vertices
    
    # Scale and center the mesh into [-1, 1] bounds
    v_min = np.min(vertices, axis=0)
    v_max = np.max(vertices, axis=0)
    center = (v_min + v_max) / 2.0
    scale = np.max(v_max - v_min)
    if scale == 0: scale = 1
    
    vertices = (vertices - center) / (scale * 0.5)
    
    # Re-calculate bounds after scaling
    v_min = np.min(vertices, axis=0)
    v_max = np.max(vertices, axis=0)
    
    num_faces = len(faces)
    
    with open(out_file, 'w') as f:
        f.write("#ifndef MODELS_H\n#define MODELS_H\n\n")
        f.write("typedef struct { double x, y, z; } Vec3;\n")
        f.write("typedef struct { Vec3 v0, v1, v2; Vec3 color; double reflectivity; double shininess; } Triangle;\n\n")
        
        # AABB
        f.write(f"Vec3 aabb_min = {{{v_min[0]:.5f}, {v_min[1]:.5f}, {v_min[2]:.5f}}};\n")
        f.write(f"Vec3 aabb_max = {{{v_max[0]:.5f}, {v_max[1]:.5f}, {v_max[2]:.5f}}};\n\n")
        
        f.write(f"int MESH_SIZE = {num_faces};\n")
        f.write(f"Triangle mesh_triangles[{num_faces}] = {{\n")
        
        # The GLB might have colors per vertex. For now, gold default to look cool
        for i, face in enumerate(faces):
            v0 = vertices[face[0]]
            v1 = vertices[face[1]]
            v2 = vertices[face[2]]
            # Add a comma unless it's the last element
            end_comma = "," if i < num_faces - 1 else ""
            f.write(f"  {{ {{{v0[0]:.5f}, {v0[1]:.5f}, {v0[2]:.5f}}}, {{{v1[0]:.5f}, {v1[1]:.5f}, {v1[2]:.5f}}}, {{{v2[0]:.5f}, {v2[1]:.5f}, {v2[2]:.5f}}}, {{0.9, 0.7, 0.1}}, 0.1, 50.0 }}{end_comma}\n")
            
        f.write("};\n\n#endif\n")
        
    print(f"Successfully extracted {num_faces} faces to {out_file}")

if __name__ == "__main__":
    main()
