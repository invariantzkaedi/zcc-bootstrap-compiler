import trimesh, numpy as np

for name in ['Heavy_Metal_Apocalypse_UFO_LOD1',
             'Ancient_Overgrown_Jungle_UFO_LOD1',
             'Epic_Yeti_LOD1',
             'Kitsune_Anime_3D_LOD1']:
    path = f'/mnt/h/__DOWNLOADS/zcc_github_upload/lod_out/{name}.glb'
    scene = trimesh.load(path, force='scene')
    for gname, geom in scene.geometry.items():
        try:
            mat = geom.visual.material
            bcf = getattr(mat, 'baseColorFactor', None)
            bct = getattr(mat, 'baseColorTexture', None)
            main_color = getattr(mat, 'main_color', None)
            print(f'  {name}/{gname}: baseColorFactor={bcf} texture={bct is not None} main_color={main_color}')
        except Exception as e:
            print(f'  {name}/{gname}: {e}')
        break  # just first geom per mesh
