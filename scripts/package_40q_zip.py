import os
import zipfile
import shutil

zip_path = r"E:\__GROUPED_IMAGES\ABSTRACT\zkaedi_prime_40qubit_hypercube_artifacts.zip"

files_to_add = [
    r"h:\__DOWNLOADS\zcc_github_upload\artifacts\40QUBIT_EXPLORATION_REPORT.md",
    r"h:\__DOWNLOADS\zcc_github_upload\artifacts\quantum_40qubit_metrics.json",
    r"h:\__DOWNLOADS\zcc_github_upload\artifacts\quantum_40qubit_hypercube_observatory.html",
    r"h:\__DOWNLOADS\zcc_github_upload\artifacts\QUANTUM_CHEMISTRY_VQE_REPORT.md",
    r"h:\__DOWNLOADS\zcc_github_upload\artifacts\quantum_chemistry_metrics.json",
    r"h:\__DOWNLOADS\zcc_github_upload\artifacts\quantum_chemistry_observatory.html",
    r"h:\__DOWNLOADS\zcc_github_upload\artifacts\quantum_sonification_40qubit.wav",
    r"h:\__DOWNLOADS\zcc_github_upload\artifacts\quantum_chemistry_vqe_sonification.wav",
    r"h:\__DOWNLOADS\zcc_github_upload\notebooks\zkaedi_prime_40qubit_hypercube.ipynb",
    r"h:\__DOWNLOADS\zcc_github_upload\notebooks\zkaedi_prime_quantum_chemistry_vqe.ipynb",
    r"h:\__DOWNLOADS\zcc_github_upload\tools\quantum_40qubit_hypercube_engine.py",
    r"h:\__DOWNLOADS\zcc_github_upload\tools\quantum_chemistry_hyperslab_engine.py"
]

temp_zip = zip_path + ".tmp"
added_names = set()

# Read existing entries, omitting ones we are updating
if os.path.exists(zip_path):
    with zipfile.ZipFile(zip_path, 'r') as zin, zipfile.ZipFile(temp_zip, 'w', compression=zipfile.ZIP_DEFLATED) as zout:
        targets_to_replace = {os.path.basename(f) for f in files_to_add if os.path.exists(f)}
        for item in zin.infolist():
            if item.filename not in targets_to_replace:
                zout.writestr(item, zin.read(item.filename))
    shutil.move(temp_zip, zip_path)

with zipfile.ZipFile(zip_path, 'a', compression=zipfile.ZIP_DEFLATED) as zout:
    for f in files_to_add:
        if os.path.exists(f):
            arcname = os.path.basename(f)
            zout.write(f, arcname)
            print(f"Added/Updated in ZIP: {arcname} ({os.path.getsize(f)} bytes)")

print(f"Successfully finalized {zip_path}, total size: {os.path.getsize(zip_path)} bytes")
