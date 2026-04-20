cd /mnt/h/__DOWNLOADS/selforglinux
mkdir -p zcc-compiler-bug-corpus
cp zcc-compiler-bug-corpus.json zcc-compiler-bug-corpus/
cp zcc-compiler-bug-corpus-README.md zcc-compiler-bug-corpus/README.md
cd zcc-compiler-bug-corpus
git init
git add .
git commit -m "Initial: 12 real codegen bugs with ground-truth fixes"
huggingface-cli repo create zcc-compiler-bug-corpus --type dataset
git remote add origin https://huggingface.co/datasets/zkaedi/zcc-compiler-bug-corpus
git push -u origin main
