import bpy
import numpy as np
import subprocess
import os
import shutil

# ==============================================================================
# Pipeline Functions under Test (Matching the Roadmap)
# ==============================================================================

def weight_crash_test(rig, mesh_obj, torture_poses=8):
    """Pose the rig through extreme rotations; measure worst vertex edge stretch."""
    # rest coords
    base = np.zeros(len(mesh_obj.data.vertices) * 3, dtype=np.float32)
    mesh_obj.data.vertices.foreach_get("co", base)
    base = base.reshape(-1, 3)
    
    # edge connections
    edges = np.zeros(len(mesh_obj.data.edges) * 2, dtype=np.int32)
    mesh_obj.data.edges.foreach_get("vertices", edges)
    edges = edges.reshape(-1, 2)
    
    rest_lens = np.linalg.norm(base[edges[:, 0]] - base[edges[:, 1]], axis=1)
    
    dg = bpy.context.evaluated_depsgraph_get()
    worst_stretch = 0.0
    limbs = [b for b in rig.pose.bones if "upper_arm" in b.name or "thigh" in b.name]
    
    for _ in range(torture_poses):
        for b in limbs:
            b.rotation_mode = 'XYZ'
            b.rotation_euler = tuple(np.random.uniform(-1.2, 1.2, 3))
        # force re-evaluation of pose changes in the view layer before fetching mesh
        bpy.context.view_layer.update()
        dg.update()
        
        ev = mesh_obj.evaluated_get(dg)
        deformed = np.zeros(len(ev.data.vertices) * 3, dtype=np.float32)
        ev.data.vertices.foreach_get("co", deformed)
        deformed = deformed.reshape(-1, 3)
        
        def_lens = np.linalg.norm(deformed[edges[:, 0]] - deformed[edges[:, 1]], axis=1)
        ratios = def_lens / (rest_lens + 1e-9)
        stretch = np.max(np.abs(ratios - 1.0))
        worst_stretch = max(worst_stretch, stretch)
        
        for b in limbs:
            b.rotation_euler = (0, 0, 0)
            
    return worst_stretch

def assemble(frames_dir, audio, out_mp4, fps=24, vertical=False, start_frame=1, use_start_number=True):
    vf = "scale=1080:1920:force_original_aspect_ratio=increase,crop=1080:1920" \
         if vertical else "scale=1920:-2"
    cmd = ["ffmpeg", "-y", "-framerate", str(fps)]
    if use_start_number:
        cmd += ["-start_number", str(start_frame)]
    cmd += ["-i", f"{frames_dir}/frame_%04d.png",
            "-i", audio,
            "-vf", vf,
            "-c:v", "libx264", "-preset", "slow", "-crf", "18",
            "-pix_fmt", "yuv420p",
            "-c:a", "aac", "-b:a", "192k", "-shortest", out_mp4]
    r = subprocess.run(cmd, capture_output=True, text=True)
    ok = r.returncode == 0 and os.path.exists(out_mp4) and os.path.getsize(out_mp4) > 0
    return {"ok": ok, "out": out_mp4, "returncode": r.returncode}

# ==============================================================================
# Gate Verification Suite
# ==============================================================================

def test_weight_crash_test():
    print("[GATE 2/4] Testing weight_crash_test...")
    bpy.ops.wm.read_factory_settings(use_empty=True)
    
    # Cylinder mesh (arm)
    bpy.ops.mesh.primitive_cylinder_add(radius=0.5, depth=4.0, location=(0, 0, 2))
    mesh_obj = bpy.context.active_object
    mesh_obj.name = "ArmMesh"
    
    # 2-bone armature
    bpy.ops.object.armature_add(enter_editmode=True, location=(0, 0, 0))
    rig = bpy.context.active_object
    rig.name = "ArmRig"
    
    eb = rig.data.edit_bones
    bone1 = eb[0]
    bone1.name = "thigh"
    bone1.head = (0, 0, 0)
    bone1.tail = (0, 0, 2)
    
    bone2 = eb.new("upper_arm")
    bone2.head = (0, 0, 2)
    bone2.tail = (0, 0, 4)
    bone2.parent = bone1
    
    bpy.ops.object.mode_set(mode='OBJECT')
    
    # Parent mesh to armature
    mesh_obj.select_set(True)
    rig.select_set(True)
    bpy.context.view_layer.objects.active = rig
    bpy.ops.object.parent_set(type='ARMATURE_AUTO')
    
    stretch = weight_crash_test(rig, mesh_obj, torture_poses=4)
    print(f" -> Deform stretch result: {stretch:.6f}")
    assert stretch > 0.0, "Stretch must be non-zero under deformation"
    print(" -> weight_crash_test PASS")

def test_ffmpeg_assembly():
    print("[GATE 5] Testing ffmpeg assembly...")
    frames_dir = "/tmp/test_frames"
    if os.path.exists(frames_dir):
        shutil.rmtree(frames_dir)
    os.makedirs(frames_dir, exist_ok=True)
    
    # Write frames starting at index 100
    for i in [100, 101, 102]:
        subprocess.run(["ffmpeg", "-y", "-f", "lavfi", "-i", "color=c=white:s=100x100", "-frames:v", "1", f"{frames_dir}/frame_{i:04d}.png"], capture_output=True)
        
    audio_path = "/tmp/silent.aac"
    subprocess.run(["ffmpeg", "-y", "-f", "lavfi", "-i", "anullsrc=r=44100:cl=mono", "-t", "1", "-c:a", "aac", audio_path], capture_output=True)
    
    # 1. Positive Control (with -start_number 100) -> expect success
    out_success = "/tmp/test_assembly_success.mp4"
    if os.path.exists(out_success):
        os.remove(out_success)
    res_pos = assemble(frames_dir, audio_path, out_success, start_frame=100, use_start_number=True)
    assert res_pos["ok"], f"Positive control failed: ffmpeg exited with code {res_pos['returncode']}"
    print(" -> Positive control: PASS (encoded successfully starting at frame 100)")
    
    # 2. Negative Control (without -start_number) -> expect failure
    out_fail = "/tmp/test_assembly_fail.mp4"
    if os.path.exists(out_fail):
        os.remove(out_fail)
    res_neg = assemble(frames_dir, audio_path, out_fail, start_frame=100, use_start_number=False)
    assert not res_neg["ok"], "Negative control failed: ffmpeg should have failed to find frames without start_number"
    print(" -> Negative control: PASS (correctly failed to find sequence starting at 100)")

def test_mesh_landmarks_parity():
    print("[GATE 6] Testing mesh_landmarks matrix parity...")
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.mesh.primitive_cube_add(size=2.0)
    obj = bpy.context.active_object
    
    # Apply non-uniform scale and rotation to challenge the coordinate transformation
    obj.scale = (1.5, 0.8, 2.2)
    obj.rotation_euler = (0.5, 1.2, -0.7)
    obj.location = (1.0, -2.0, 3.5)
    bpy.context.view_layer.update()
    
    # 1. Old list comprehension method
    old = np.array([obj.matrix_world @ v.co for v in obj.data.vertices])
    
    # 2. Optimized NumPy path
    coords = np.zeros(len(obj.data.vertices) * 3, dtype=np.float32)
    obj.data.vertices.foreach_get("co", coords)
    coords = coords.reshape(-1, 3)
    mat = np.array(obj.matrix_world)
    new = (coords @ mat[:3, :3].T) + mat[:3, 3]
    
    # Assert exact match
    parity = np.allclose(old, new, atol=1e-5)
    assert parity, "Landmark coordinates mismatch under transformation!"
    print(" -> Matrix parity: PASS (vectorized NumPy equivalent matches obj.matrix_world @ v.co)")

# ==============================================================================
# Main Runner
# ==============================================================================

if __name__ == "__main__":
    print("=== PIPELINE GATE RUNNER ===")
    test_weight_crash_test()
    test_ffmpeg_assembly()
    test_mesh_landmarks_parity()
    print("ALL GATES RESOLVED: 100% COMPLIANT")
