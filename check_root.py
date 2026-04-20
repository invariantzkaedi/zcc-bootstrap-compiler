from pathlib import Path
p = Path('/mnt/h/__DOWNLOADS/zcc_github_upload/zcc_oneirogenesis.py').parent.resolve()
print('REPO_ROOT:', p)
print('zcc2 exists:', (p/'zcc2').exists())
