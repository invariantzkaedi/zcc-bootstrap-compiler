# Reproducible Validation Wrapper for Windows PowerShell
$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RepoRoot = Split-Path -Parent $ScriptDir

# Check for WSL
$wsl = Get-Command wsl.exe -ErrorAction SilentlyContinue
if (-not $wsl) {
    Write-Error "WSL is required on Windows to run the independent validation suite."
    exit 1
}

# Convert Windows path to WSL path
$wslRepoRoot = (wsl.exe -e bash -c "cd '$($RepoRoot.Replace('\', '/'))' 2>/dev/null && pwd").Trim()
if (-not $wslRepoRoot) {
    $drive = $RepoRoot.Substring(0, 1).ToLower()
    $wslRepoRoot = "/mnt/$drive" + $RepoRoot.Substring(2).Replace('\', '/')
}

$forwardArgs = $args -join " "

Write-Host "[*] Executing reproducible validation runner via WSL..." -ForegroundColor Cyan
& wsl.exe -e bash -c "cd '$wslRepoRoot' && ./scripts/reproducible_validate.sh $forwardArgs"
exit $LASTEXITCODE
