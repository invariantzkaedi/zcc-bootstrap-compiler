from huggingface_hub import HfApi

api = HfApi()
api.upload_file(
    path_or_fileobj='/mnt/h/__DOWNLOADS/zcc_github_upload/zcc-compiler-bug-corpus.json',
    path_in_repo='zcc-compiler-bug-corpus.json',
    repo_id='zkaedi/zcc-compiler-bug-corpus',
    repo_type='dataset'
)
