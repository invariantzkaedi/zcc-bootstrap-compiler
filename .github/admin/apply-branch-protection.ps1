param(
  [string]$Owner = "zkaedii",
  [string]$Repo = "zcc",
  [string]$Branch = "main",
  [string]$PolicyFile = ".github/admin/branch-protection-main.json"
)

$ErrorActionPreference = "Stop"

if (-not (Get-Command gh -ErrorAction SilentlyContinue)) {
  throw "GitHub CLI (gh) is required. Install gh and authenticate first."
}

if (-not (Test-Path $PolicyFile)) {
  throw "Policy file not found: $PolicyFile"
}

gh auth status | Out-Null

gh api `
  --method PUT `
  -H "Accept: application/vnd.github+json" `
  "/repos/$Owner/$Repo/branches/$Branch/protection" `
  --input $PolicyFile

Write-Host "Branch protection applied to ${Owner}/${Repo}:${Branch}"
