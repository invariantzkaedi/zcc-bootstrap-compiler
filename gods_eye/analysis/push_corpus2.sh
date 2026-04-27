cd /mnt/h/__DOWNLOADS/selforglinux/zcc-compiler-bug-corpus
git config user.email "zkaedi@users.noreply.huggingface.co"
git config user.name "zkaedi"
git branch -m main
git add .
git commit -m "Initial: 12 real codegen bugs with ground-truth fixes"
pip install huggingface_hub --break-system-packages -q 2>/dev/null
python3 -m huggingface_hub.commands.huggingface_cli repo create zcc-compiler-bug-corpus --type dataset --yes 2>/dev/null || echo "repo may already exist"
git remote remove origin 2>/dev/null
git remote add origin https://huggingface.co/datasets/zkaedi/zcc-compiler-bug-corpus
git push -u origin main
