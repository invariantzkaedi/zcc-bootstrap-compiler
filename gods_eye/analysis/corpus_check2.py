from huggingface_hub import HfApi
files = HfApi().list_repo_files(repo_id='zkaedi/zcc-compiler-bug-corpus', repo_type='dataset')
print(files)
