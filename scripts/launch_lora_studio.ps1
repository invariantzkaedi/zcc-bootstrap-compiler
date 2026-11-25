param(
    [string]$Model = "",
    [int]$Port = 8080,
    [int]$Ctx = 1024,
    [string]$KvType = "f16",
    [string]$LoraDir = "build_win64",
    [string]$LoadLora = "",
    [string]$LoraName = "",
    [string]$Route = "/",
    [switch]$NoPreload,
    [switch]$NoBrowser,
    [switch]$AllowShutdown
)

$ErrorActionPreference = "Stop"
if (-not $Route) { $Route = "/" }
if (-not $Route.StartsWith("/")) { $Route = "/" + $Route }
$RepoRoot = Split-Path -Parent $PSScriptRoot

function Convert-ToWslPath([string]$WinPath) {
    if (-not $WinPath) { return "" }
    $p = $WinPath.Trim()
    if ($p.StartsWith("/")) { return $p }
    if ($p -match '^([A-Za-z]):[\\/](.*)$') {
        $drv = $matches[1].ToLowerInvariant()
        $rest = $matches[2] -replace '\\', '/'
        return "/mnt/$drv/$rest"
    }
    return ($p -replace '\\', '/')
}

function Quote-WinArg([string]$s) {
    if ($s -match '[\s"]') {
        return '"' + ($s -replace '"', '\"') + '"'
    }
    return $s
}

$LinuxBin = Join-Path $RepoRoot "build_win64\zcc_gpu_lora_studio_linux"
if (-not (Test-Path $LinuxBin)) {
    Write-Error "Missing $LinuxBin. Build it first via: wsl -e bash -c `"cd $(Convert-ToWslPath $RepoRoot) && python3 tools/build_gpu_lora_studio_win64.py`""
    exit 1
}

if (-not $Model) {
    $Preferred = "H:\models\zkaedi-prime-dpo-q4_k_m.gguf"
    if (Test-Path $Preferred) {
        $Model = $Preferred
    } elseif (Test-Path "H:\models") {
        $FirstGguf = Get-ChildItem "H:\models" -Filter "*.gguf" -File -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($FirstGguf) { $Model = $FirstGguf.FullName }
    }
}
if (-not $Model) {
    Write-Error "No GGUF model found. Pass -Model <path-to-model.gguf>."
    exit 1
}

$PreloadPairs = @()
if ($LoadLora) {
    $PreloadPairs += @{ Path = $LoadLora; Name = $LoraName }
} elseif (-not $NoPreload -and ($Model -match 'zkaedi-prime-dpo-q4_k_m\.gguf$')) {
    $MarioEntry = @{ Rel = "build_win64/mario_architect_1_5b.zcc_lora"; Full = (Join-Path $RepoRoot "build_win64\mario_architect_1_5b.zcc_lora"); Name = "mario_architect" }
    $SvgEntry = @{ Rel = "build_win64/svg_forge_1_5b.zcc_lora"; Full = (Join-Path $RepoRoot "build_win64\svg_forge_1_5b.zcc_lora"); Name = "svg_forge" }
    $RunEntry = @{ Rel = "build_win64/runner_director_1_5b.zcc_lora"; Full = (Join-Path $RepoRoot "build_win64\runner_director_1_5b.zcc_lora"); Name = "runner_director" }
    $ObsEntry = @{ Rel = "build_win64/obsidian7_studio_1_5b.zcc_lora"; Full = (Join-Path $RepoRoot "build_win64\obsidian7_studio_1_5b.zcc_lora"); Name = "obsidian7" }
    if ($Route -eq "/mario") {
        $Ordered = @($MarioEntry, $SvgEntry, $RunEntry, $ObsEntry)
    } elseif ($Route -eq "/svg") {
        $Ordered = @($SvgEntry, $MarioEntry, $RunEntry, $ObsEntry)
    } elseif ($Route -eq "/runner") {
        $Ordered = @($RunEntry, $MarioEntry, $SvgEntry, $ObsEntry)
    } else {
        $Ordered = @($ObsEntry, $MarioEntry, $SvgEntry, $RunEntry)
    }
    foreach ($cand in $Ordered) {
        if (Test-Path $cand.Full) {
            $PreloadPairs += @{ Path = $cand.Rel; Name = $cand.Name }
        }
    }
}

$RepoWsl = Convert-ToWslPath $RepoRoot
$ModelWsl = Convert-ToWslPath $Model
$LoraDirWsl = Convert-ToWslPath $LoraDir

$WslArgs = @(
    "-e",
    "./build_win64/zcc_gpu_lora_studio_linux",
    "--model", $ModelWsl,
    "--host", "127.0.0.1",
    "--port", "$Port",
    "--kv-type", $KvType,
    "--ctx", "$Ctx",
    "--lora-dir", $LoraDirWsl
)
foreach ($pair in $PreloadPairs) {
    $WslArgs += @("--load-lora", (Convert-ToWslPath $pair.Path))
    if ($pair.Name) {
        $WslArgs += @("--lora-name", $pair.Name)
    }
}
if ($AllowShutdown) {
    $WslArgs += "--allow-shutdown"
}

$QuotedWslLine = ($WslArgs | ForEach-Object { Quote-WinArg $_ }) -join " "

Write-Host ""
Write-Host "  ================================================================" -ForegroundColor DarkYellow
Write-Host "   ZKAEDI ZCC // OBSIDIAN & CHAMPAGNE GOLD LORA STUDIO (SM 12.0)" -ForegroundColor Yellow
Write-Host "  ================================================================" -ForegroundColor DarkYellow
Write-Host "   Model   : $ModelWsl" -ForegroundColor Gray
if ($PreloadPairs.Count -gt 0) {
    $AdpList = ($PreloadPairs | ForEach-Object { if ($_.Name) { "$($_.Name) ($($_.Path))" } else { $_.Path } }) -join ", "
    Write-Host "   Adapters: $AdpList" -ForegroundColor Green
}
Write-Host "   URL     : http://127.0.0.1:$Port$Route" -ForegroundColor Cyan
Write-Host "   Booting native Linux ELF64 + Blackwell CUDA driver in WSL2..." -ForegroundColor DarkGray

$Proc = Start-Process -FilePath "wsl.exe" -WorkingDirectory $RepoRoot -ArgumentList $QuotedWslLine -PassThru -NoNewWindow
[void]$Proc.Handle
$HealthUrl = "http://127.0.0.1:$Port/health"
$Ready = $false
$Health = $null

try {
    for ($i = 0; $i -lt 180; $i++) {
        if ($Proc.HasExited) {
            throw "Studio backend exited early with code $($Proc.ExitCode)."
        }
        try {
            $Health = Invoke-RestMethod -Uri $HealthUrl -Method Get -TimeoutSec 2
            if ($Health -and $Health.status -eq "ok") {
                $Ready = $true
                break
            }
        } catch {
            Start-Sleep -Milliseconds 500
        }
    }

    if (-not $Ready) {
        throw "Timed out waiting for Studio backend at $HealthUrl"
    }

    Write-Host ""
    Write-Host "   [READY] $($Health.gpu_name) (SM $($Health.gpu_cc)) | VRAM $([math]::Round($Health.vram_used_mib)) / $([math]::Round($Health.vram_total_mib)) MiB" -ForegroundColor Green
    Write-Host "   [MODEL] $($Health.model) | arch=$($Health.arch) L$($Health.n_layers) | kv=$($Health.kv_type) ctx=$($Health.eff_max_seq) | active=$($Health.active_adapter)" -ForegroundColor Gray
    Write-Host "   Press Ctrl+C in this window to stop the server cleanly." -ForegroundColor DarkYellow
    Write-Host ""

    if (-not $NoBrowser) {
        Start-Process ("http://127.0.0.1:$Port" + $Route)
    }

    Wait-Process -Id $Proc.Id
} finally {
    if ($Proc -and -not $Proc.HasExited) {
        if ($AllowShutdown) {
            try {
                Invoke-RestMethod -Uri "http://127.0.0.1:$Port/shutdown" -Method Post -TimeoutSec 2 | Out-Null
            } catch {}
        }
        Start-Sleep -Milliseconds 300
        if (-not $Proc.HasExited) {
            Stop-Process -Id $Proc.Id -Force -ErrorAction SilentlyContinue
        }
    }
}
