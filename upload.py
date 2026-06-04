import os
from huggingface_hub import HfApi

_dir = os.path.dirname(os.path.abspath(__file__))

api = HfApi()
api.upload_file(
    path_or_fileobj=os.path.join(_dir, 'zcc-compiler-bug-corpus.json'),
    path_in_repo='zcc-compiler-bug-corpus.json',
    repo_id='zkaedi/zcc-compiler-bug-corpus',
    repo_type='dataset'
)
