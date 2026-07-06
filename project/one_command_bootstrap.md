# One-Command Bootstrap

## Prereqs
- `PROJECTV2_TOKEN` secret present (PAT with `project` + `repo` scopes)
- ProjectV2 exists with fields from `project/projectv2-fields.md`
- `issues_import.csv` present

## Run
Trigger workflow:
- Actions → **Bootstrap Project System (manual)** → Run workflow
- Inputs:
  - owner: your org/user
  - repo: your repo
  - project_number: e.g. `1`
  - create_issues: `true`
  - validate_only: `false`

## What it does
1. Creates/updates labels
2. Creates milestones
3. Creates issues from CSV
4. Validates ProjectV2 fields/options
5. Exports field IDs to `.github/projectv2_field_ids.json`
6. Commits field-ID snapshot

## Post-bootstrap
- Enable:
  - `.github/workflows/projectv2-auto-add.yml`
  - `.github/workflows/projectv2-status-sync.yml`
- Open test issue + PR and verify routing.
