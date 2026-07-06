#!/usr/bin/env bash
set -euo pipefail
OWNER_REPO="${1:?usage: bootstrap_all.sh owner/repo}"

bash project/create_labels.sh "$OWNER_REPO"
bash project/create_milestones.sh "$OWNER_REPO"
bash project/create_issues_from_csv.sh "$OWNER_REPO" project/issues_import.csv

echo "Done. Labels, milestones, and issues created for $OWNER_REPO"
