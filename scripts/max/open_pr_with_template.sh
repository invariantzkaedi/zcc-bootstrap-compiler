#!/usr/bin/env bash
set -euo pipefail

OWNER_REPO="${1:?usage: open_pr_with_template.sh owner/repo}"
BASE_BRANCH="${2:-main}"
HEAD_BRANCH="${3:-$(git rev-parse --abbrev-ref HEAD)}"
TITLE="${4:-chore: kickoff M1 day-1 execution}"
BODY_FILE="${5:-.github/pull_request_template.md}"

gh pr create   --repo "$OWNER_REPO"   --base "$BASE_BRANCH"   --head "$HEAD_BRANCH"   --title "$TITLE"   --body-file "$BODY_FILE"
