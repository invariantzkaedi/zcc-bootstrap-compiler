# MAX Ops Runbook

## One-time setup
1. Ensure scripts are executable:
```bash
chmod +x scripts/max/*.sh scripts/ci/pre_push_guard.sh scripts/legendary/run_all_the_things.sh
```

2. Bootstrap project system:
```bash
bash scripts/max/bootstrap_full_safe.sh <owner/repo> <project_number>
```

## Daily operator flow
```bash
make max-day1 OWNER_REPO=<owner/repo>
make max-quality
```

## Pre-PR flow
```bash
bash scripts/max/pr_ready_check.sh
```

## Full weekly flow
```bash
make max-all OWNER_REPO=<owner/repo>
bash scripts/max/weekly_ops_report.sh out/ops/weekly_report.md
```

## Emergency validation
```bash
bash scripts/max/ci_smoke_matrix.sh
```
