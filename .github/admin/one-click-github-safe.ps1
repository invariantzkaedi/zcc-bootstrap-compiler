param(
  [string]$Owner = "zkaedii",
  [string]$Repo = "zcc",
  [string]$Branch = "main",
  [switch]$OpenSettingsOnFallback = $true
)

$ErrorActionPreference = "Stop"

function Require-Gh {
  if (-not (Get-Command gh -ErrorAction SilentlyContinue)) {
    Write-Error "GitHub CLI (gh) is required. Install from https://cli.github.com/"
    throw "missing-gh"
  }
}

function Ensure-GhAuth {
  try {
    gh auth status | Out-Null
  } catch {
    Write-Host "GitHub CLI is not authenticated. Starting web login..."
    gh auth login --web
    gh auth status | Out-Null
  }
}

function Enable-SecretGuardrails {
  $tmp = Join-Path $env:TEMP "zcc-security-and-analysis.json"
  @"
{
  "security_and_analysis": {
    "secret_scanning": { "status": "enabled" },
    "secret_scanning_push_protection": { "status": "enabled" }
  }
}
"@ | Set-Content -Path $tmp -Encoding utf8

  gh api `
    --method PATCH `
    -H "Accept: application/vnd.github+json" `
    "/repos/$Owner/$Repo" `
    --input $tmp | Out-Null
}

Write-Host "== One-Click GitHub Safety Bootstrap =="
Write-Host "Repo: ${Owner}/${Repo}  Branch: ${Branch}"

Require-Gh
Ensure-GhAuth

Write-Host "[1/3] Applying branch protection..."
powershell -ExecutionPolicy Bypass -File ".github/admin/apply-branch-protection.ps1" -Owner $Owner -Repo $Repo -Branch $Branch

Write-Host "[2/3] Enabling secret scanning + push protection..."
try {
  Enable-SecretGuardrails
  Write-Host "Secret guardrails enabled via API."
} catch {
  Write-Warning "Could not enable secret guardrails automatically (missing permissions or plan limitations)."
  if ($OpenSettingsOnFallback) {
    $settingsUrl = "https://github.com/$Owner/$Repo/settings/security_analysis"
    Write-Host "Opening security settings: $settingsUrl"
    Start-Process $settingsUrl
  }
}

Write-Host "[3/3] Verifying branch protection API responds..."
gh api `
  -H "Accept: application/vnd.github+json" `
  "/repos/$Owner/$Repo/branches/$Branch/protection" | Out-Null

Write-Host ""
Write-Host "Done. Required status check should include 'rust-front-smoke'."
Write-Host "Use .github/admin/validate-protections.md for a quick pass/fail PR validation."
