from pathlib import Path

p = Path(__file__).parent.resolve()
print('REPO_ROOT:', p)
print('zcc2 exists:', (p/'zcc2').exists())
