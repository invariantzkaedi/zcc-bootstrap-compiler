#!/usr/bin/env bash
set -euo pipefail
OWNER_REPO="${1:?usage: full_dump_bootstrap.sh owner/repo}"
OWNER="${OWNER_REPO%/*}"

bash project/create_labels.sh "$OWNER_REPO"
bash project/create_milestones.sh "$OWNER_REPO"
bash project/create_issues_from_csv.sh "$OWNER_REPO" project/issues_import.csv
bash project/create_kickoff_issues.sh "$OWNER_REPO"

python3 project/validate_projectv2_setup.py --owner "$OWNER" --project-number 1
python3 project/export_projectv2_field_ids.py --owner "$OWNER" --project-number 1 --out .github/projectv2_field_ids.json

echo "FULL DUMP COMPLETE for $OWNER_REPO"
echo "Next: enable required checks + run M1 daily workflow."
