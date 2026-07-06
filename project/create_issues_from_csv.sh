#!/usr/bin/env bash
set -euo pipefail
OWNER_REPO="${1:?usage: create_issues_from_csv.sh owner/repo}"
CSV="${2:-project/issues_import.csv}"

python3 - "$OWNER_REPO" "$CSV" << 'PY'
import csv, subprocess, sys, tempfile, os
repo=sys.argv[1]
csv_path=sys.argv[2]

def run(cmd):
    subprocess.run(cmd, check=True)

with open(csv_path, newline='', encoding='utf-8') as f:
    for row in csv.DictReader(f):
        title=row["Title"]
        body=row["Body"]
        labels=[x.strip() for x in row["Labels"].split(";") if x.strip()]
        milestone=row["Milestone"].strip()

        with tempfile.NamedTemporaryFile("w", delete=False, suffix=".md", encoding="utf-8") as tf:
            tf.write(body)
            tmp=tf.name
        try:
            cmd=["gh","issue","create","--repo",repo,"--title",title,"--body-file",tmp]
            for lb in labels:
                cmd += ["--label", lb]
            if milestone:
                cmd += ["--milestone", milestone]
            run(cmd)
        finally:
            os.unlink(tmp)
PY
