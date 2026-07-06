# ProjectV2 Setup Checklist

- [ ] Create ProjectV2 and note project number
- [ ] Add fields from `project/projectv2-fields.md`
- [ ] Create PAT in bot/service account with scopes:
  - `project`
  - `repo` (if private repo)
- [ ] Add repository secret: `PROJECTV2_TOKEN`
- [ ] Update workflow/script constants:
  - `OWNER`
  - `PROJECT_NUMBER`
- [ ] Enable workflows:
  - `.github/workflows/projectv2-auto-add.yml`
  - `.github/workflows/projectv2-status-sync.yml`
- [ ] Open test issue + PR and verify:
  - auto-added to project
  - fields routed (Status/Track/Milestone/Priority/Size/Type)
  - PR status transitions to Done on merge
