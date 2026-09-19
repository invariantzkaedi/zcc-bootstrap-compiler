<#
.SYNOPSIS
    ZCC XMM codegen stress validator — PowerShell native.

.DESCRIPTION
    PowerShell-native port of fractal_xmm_validate.sh. All analysis runs in PS;
    only the compile/link/run steps shell into WSL (because zcc is a Linux ELF).
    Validates SSE2 scalar FP opcodes, register pressure, SysV varargs convention,
    and byte-identical execution against GCC and golden MD5.

.PARAMETER Zcc
    Path to the zcc binary as seen from INSIDE WSL (relative or absolute).
    Default: ./zcc

.PARAMETER RefCc
    Reference compiler inside WSL. Default: gcc

.PARAMETER Src
    Source file. Default: tests/fractal.c

.PARAMETER UseIr
    Flag to enable IR backend pass (--ir --telemetry). Default: false

.PARAMETER WslDistro
    WSL distro name. Empty = default distro.

.EXAMPLE
    .\tests\fractal_xmm_validate.ps1
    .\tests\fractal_xmm_validate.ps1 -Zcc ./zcc_stage2
    .\tests\fractal_xmm_validate.ps1 -Zcc ./zcc -RefCc gcc -WslDistro Ubuntu

.NOTES
    Exit codes:
      0 = pass
      1 = ZCC compile failed
      2 = required FP opcode missing / assemble failed
      3 = output diverges from reference or golden MD5
#>
[CmdletBinding()]
param(
    [string]$Zcc = './zcc',
    [string]$RefCc = 'gcc',
    [string]$Src = 'tests/fractal.c',
    [switch]$UseIr,
    [string]$WslDistro = 'Ubuntu',
    [switch]$NoColor
)

$ErrorActionPreference = 'Stop'
$GoldenMd5 = '9fe81c3d00c986b2882e8973bb3c15a2'

# ─── helpers ───────────────────────────────────────────────────────────
function Write-C { param($Text, $Color = 'White')
    if ($NoColor) { Write-Host $Text } else { Write-Host $Text -ForegroundColor $Color }
}
function Write-Hdr { param($Text)
    Write-C ('─' * 60) Cyan
    Write-C "🔱 $Text" Cyan
    Write-C ('─' * 60) Cyan
}

function Invoke-Wsl { param([string]$BashCmd)
    if ($WslDistro) {
        wsl -d $WslDistro -e bash -c $BashCmd
    } else {
        wsl -e bash -c $BashCmd
    }
}

function ConvertTo-WslPath { param([string]$WinPath)
    $resolved = (Resolve-Path $WinPath -ErrorAction Stop).Path
    $drive = $resolved.Substring(0,1).ToLower()
    $rest  = $resolved.Substring(2).Replace('\','/')
    "/mnt/$drive$rest"
}

# ─── Phase 0: sanity ───────────────────────────────────────────────────
if (-not (Test-Path $Src)) {
    if (Test-Path "fractal.c") {
        $Src = "fractal.c"
    } elseif (Test-Path "tests/fractal.c") {
        $Src = "tests/fractal.c"
    } else {
        Write-C "ERROR: $Src not found in $(Get-Location)" Red
        exit 1
    }
}
try { $null = Get-Command wsl -ErrorAction Stop }
catch {
    Write-C 'ERROR: wsl command not found. zcc is a Linux ELF — WSL is required.' Red
    exit 1
}

$wslCwd = ConvertTo-WslPath (Get-Location).Path

# ─── Phase 1: ZCC compile ──────────────────────────────────────────────
Write-Hdr 'PHASE 1: ZCC compile → fractal.s'
Remove-Item -ErrorAction SilentlyContinue fractal.s, fractal.ir, zcc_ir.json, zcc.log, fractal_zcc, fractal_zcc.out, fractal_ref, fractal_ref.out

$extraFlags = if ($UseIr) { "--ir --telemetry " } else { "" }
$zccCmd = "cd '$wslCwd' && $Zcc $extraFlags-S $Src -o fractal.s 2>zcc.log"
Invoke-Wsl $zccCmd | Out-Null
if ($LASTEXITCODE -ne 0 -or -not (Test-Path fractal.s)) {
    Write-C '✗ ZCC compile failed. Last 30 lines of zcc.log:' Red
    if (Test-Path zcc.log) { Get-Content zcc.log -Tail 30 }
    exit 1
}
$sLines = (Get-Content fractal.s | Measure-Object -Line).Lines
Write-C "✓ fractal.s emitted ($sLines lines)" Green
if (Test-Path fractal.ir) {
    $irLines = (Get-Content fractal.ir | Measure-Object -Line).Lines
    Write-C "✓ fractal.ir     ($irLines lines)" Green
}
if (Test-Path zcc_ir.json) {
    $irBytes = (Get-Item zcc_ir.json).Length
    Write-C "✓ zcc_ir.json    ($irBytes bytes)" Green
}

# ─── Phase 2: XMM/FP opcode scan ───────────────────────────────────────
Write-Hdr 'PHASE 2: XMM register + FP opcode scan'
$asm = Get-Content fractal.s -Raw

$fpOps = [ordered]@{}
$opList = @('movsd','addsd','subsd','mulsd','divsd','ucomisd','comisd',
            'cvtsi2sd','cvttsd2si','cvtsd2ss','cvtss2sd','xorpd','andpd')
foreach ($op in $opList) {
    $pattern = "\b$op" + '[lq]?\b'
    $fpOps[$op] = ([regex]::Matches($asm, $pattern)).Count
}

$xmmRegs  = ([regex]::Matches($asm, '%xmm\d+') |
             ForEach-Object { $_.Value } | Sort-Object -Unique)
$xmmCount = $xmmRegs.Count

Write-Host "  XMM regs:    $($xmmRegs -join ' ')"
Write-Host "  distinct:    $xmmCount"
Write-Host '  FP opcodes:'
foreach ($op in $fpOps.Keys) {
    $c = $fpOps[$op]
    if ($c -gt 0) {
        Write-Host ("    ✓ {0,-10} {1,6}" -f $op, $c) -ForegroundColor Green
    } else {
        Write-Host ("    · {0,-10} {1,6}" -f $op, $c) -ForegroundColor DarkGray
    }
}

$required = @('movsd','addsd','mulsd','ucomisd','cvtsi2sd')
$missing  = $required | Where-Object { $fpOps[$_] -eq 0 }
if ($missing) {
    Write-C "✗ REQUIRED FP OPS MISSING: $($missing -join ', ')" Red
    Write-C '  ZCC is not emitting scalar SSE for doubles.' Red
    exit 2
}
Write-C '✓ All required FP opcodes present' Green

$spillCount = ([regex]::Matches($asm, 'movsd.*-\d+\(%rbp\)|movsd.*\(%rsp\)')).Count
Write-Host "  FP spills:   $spillCount  (stack-relative movsd)"

$alPattern = '(?m)^\s*mov[blq]?\s+\$\d+,\s*%(al|eax|rax)\b'
$alSets    = ([regex]::Matches($asm, $alPattern)).Count
Write-Host "  varargs al:  $alSets  (mov `$N, %al/%eax/%rax before varargs)"
if ($alSets -eq 0) {
    Write-C '⚠ No varargs count register set — SysV varargs rule may be violated' Yellow
}
if ($spillCount -lt 10) {
    Write-C "⚠ Fewer than 10 FP spills for 10 live doubles in mandel_iter" Yellow
}

# ─── Phase 3: assemble + link + run ZCC output ─────────────────────────
Write-Hdr 'PHASE 3: assemble + link + execute ZCC output'
$buildZcc = "cd '$wslCwd' && gcc -no-pie -o fractal_zcc fractal.s 2>asm.log && ./fractal_zcc > fractal_zcc.out"
Invoke-Wsl $buildZcc | Out-Null
if ($LASTEXITCODE -ne 0 -or -not (Test-Path fractal_zcc.out)) {
    Write-C '✗ assemble/link/run failed' Red
    if (Test-Path asm.log) { Get-Content asm.log }
    exit 2
}
$zccOutSize = (Get-Item fractal_zcc.out).Length
$zccHash = (Get-FileHash fractal_zcc.out -Algorithm MD5).Hash.ToLower()
Write-C "✓ fractal_zcc ran, $zccOutSize bytes output (md5: $zccHash)" Green

# ─── Phase 4: reference compile ────────────────────────────────────────
Write-Hdr "PHASE 4: reference compile with $RefCc"
$buildRef = "cd '$wslCwd' && $RefCc -O0 -no-pie -o fractal_ref $Src && ./fractal_ref > fractal_ref.out"
Invoke-Wsl $buildRef | Out-Null
if ($LASTEXITCODE -ne 0 -or -not (Test-Path fractal_ref.out)) {
    Write-C '✗ reference compile/run failed' Red
    exit 2
}
$refHash = (Get-FileHash fractal_ref.out -Algorithm MD5).Hash.ToLower()
Write-C "✓ fractal_ref ran (md5: $refHash)" Green

# ─── Phase 5: byte-for-byte diff ───────────────────────────────────────
Write-Hdr 'PHASE 5: output diff & golden checksum check'
$zccContent = Get-Content fractal_zcc.out -Raw
$refContent = Get-Content fractal_ref.out -Raw
if ($zccContent -ceq $refContent) {
    Write-C '✓ ✓ ✓  IDENTICAL OUTPUT TO GCC REFERENCE' Green
    Remove-Item -ErrorAction SilentlyContinue fractal.diff
} else {
    Write-C '✗ OUTPUT DIVERGES FROM GCC REFERENCE' Red
    $diff = Compare-Object (Get-Content fractal_ref.out) (Get-Content fractal_zcc.out) -SyncWindow 0
    $diff | Select-Object -First 30 | Format-Table SideIndicator, InputObject -AutoSize
    $diff | Out-File fractal.diff -Encoding utf8
    Write-C 'Full diff in fractal.diff' Yellow
    exit 3
}

if ($zccHash -ne $GoldenMd5) {
    Write-C "✗ OUTPUT MD5 ($zccHash) DOES NOT MATCH GOLDEN ($GoldenMd5)" Red
    exit 3
}
Write-C "✓ ✓ ✓  GOLDEN MD5 VERIFIED ($GoldenMd5)" Green

# ─── Cleanup temporary artifacts ───────────────────────────────────────
Remove-Item -ErrorAction SilentlyContinue fractal.s, fractal.ir, zcc_ir.json, zcc.log, fractal_zcc, fractal_zcc.out, fractal_ref, fractal_ref.out, asm.log

# ─── Summary ───────────────────────────────────────────────────────────
Write-Hdr 'SUMMARY'
Write-Host "  ZCC binary     : $Zcc"
Write-Host "  .s lines       : $sLines"
Write-Host "  XMM regs used  : $xmmCount  ($($xmmRegs -join ' '))"
Write-Host "  FP spills      : $spillCount"
Write-Host "  Varargs al sets: $alSets"
Write-C   '  Output match   : YES (bit-identical to GCC reference)' Green
Write-C   "  Golden MD5     : $GoldenMd5 (MATCH)" Green
Write-C   "`n🔱 ZCC XMM STRESS TEST: PASS" Green
exit 0
