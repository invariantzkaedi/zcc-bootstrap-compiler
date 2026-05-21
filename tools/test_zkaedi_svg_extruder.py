import unittest
import numpy as np
import os
import struct
import tempfile
from zkaedi_svg_extruder_v2 import (
    parse_svg_to_polygons,
    earcut_triangulate,
    extrude_mesh_clean,
    generate_zkaedi_proxy_textures,
    unknowncode_seed
)

class TestZkaediSvgExtruder(unittest.TestCase):
    def setUp(self):
        self.temp_dir = tempfile.TemporaryDirectory()
        self.pos_out = os.path.join(self.temp_dir.name, 'proxy_verts.bin')
        self.faces_out = os.path.join(self.temp_dir.name, 'proxy_faces.bin')

    def tearDown(self):
        # Remove read-only lock before cleanup
        for f in [self.pos_out, self.faces_out]:
            if os.path.exists(f):
                os.chmod(f, 0o666)
        self.temp_dir.cleanup()

    def test_parse_svg_to_polygons(self):
        # Simple square path
        path_d = "M 0 0 L 10 0 L 10 10 L 0 10 Z"
        polygons = parse_svg_to_polygons(path_d)
        self.assertTrue(len(polygons) > 0, "Failed to parse simple SVG path")
        self.assertEqual(len(polygons[0]), 5, "Square should be segmented correctly")

    def test_empty_geometry_fallback(self):
        # Empty polygon list
        verts_2d, faces_2d, polys = earcut_triangulate([])
        self.assertEqual(len(verts_2d), 0, "Empty verts expected")
        self.assertEqual(len(faces_2d), 0, "Empty faces expected")
        
        # Test extrude on empty
        verts_3d, faces_3d = extrude_mesh_clean(verts_2d, faces_2d, polys)
        self.assertEqual(len(verts_3d), 0)
        self.assertEqual(len(faces_3d), 0)

    def test_NaN_inf_purging(self):
        # Corrupted vertex data with NaN and Inf
        corrupted_verts_2d = np.array([
            [0.0, 0.0],
            [np.nan, 10.0],
            [10.0, np.inf]
        ], dtype=np.float32)
        
        faces_2d = np.array([[0, 1, 2]], dtype=np.int32)
        polygons = [[[0.0, 0.0], [np.nan, 10.0], [10.0, np.inf]]]
        
        # The extruder should intercept and zero out NaNs and Infs
        verts_3d, faces_3d = extrude_mesh_clean(corrupted_verts_2d, faces_2d, polygons, depth=20.0)
        
        # Check that no NaNs or Infs leaked through
        self.assertFalse(np.isnan(verts_3d).any(), "NaN values leaked into 3D geometry")
        self.assertFalse(np.isinf(verts_3d).any(), "Inf values leaked into 3D geometry")
        
        # Original NaN should be 0.0
        self.assertEqual(verts_3d[1][0], 0.0, "NaN should be zeroed")
        # Original Inf should be 0.0
        self.assertEqual(verts_3d[2][1], 0.0, "Inf should be zeroed")

    def test_generate_textures_and_locking(self):
        # Mock simple geometry
        verts_3d = np.array([
            [0.0, 0.0, 10.0],
            [10.0, 0.0, 10.0],
            [5.0, 10.0, 10.0],
            [0.0, 0.0, -10.0],
            [10.0, 0.0, -10.0],
            [5.0, 10.0, -10.0]
        ], dtype=np.float32)
        
        faces_3d = np.array([
            [0, 1, 2],
            [3, 4, 5]
        ], dtype=np.int32)
        
        generate_zkaedi_proxy_textures(verts_3d, faces_3d, self.pos_out, self.faces_out)
        
        self.assertTrue(os.path.exists(self.pos_out))
        self.assertTrue(os.path.exists(self.faces_out))
        
        # Check file sizes
        # 6 verts * 3 floats * 4 bytes = 72 bytes
        self.assertEqual(os.path.getsize(self.pos_out), 72)
        # 2 faces * 3 ints * 4 bytes = 24 bytes
        self.assertEqual(os.path.getsize(self.faces_out), 24)
        
        # Check read-only bit (fortification check)
        mode_pos = os.stat(self.pos_out).st_mode
        self.assertFalse(mode_pos & 0o200, "Pos bin file should be read-only")

if __name__ == '__main__':
    unittest.main()
