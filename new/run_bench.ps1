<#
.SYNOPSIS
  Сборка/прошивка/мониторинг бенчей nanoQuick с записью результатов.
  Рабочие копии — в .\tmp\<bench>\ (скетч + dbg.h + src + build + logs).
  Результаты — в .\results\<timestamp>\<bench>\ (monitor.txt, meta.json).
.PARAMETER Bench
  Имя бенча (BaseName .ino) или All.
.PARAMETER Upload
  Прошить после сборки (требует -Port).
.PARAMETER Port
  COM-порт (например, COM3).
.PARAMETER Results
  Каталог результатов. По умолчанию <root>/results/<timestamp>.
.PARAMETER MonitorSec
  Переопределить время монитора (иначе из bench-spec.psd1).
.PARAMETER NoSize
  Не печатать avr-size.
.PARAMETER NoNm
  Не печатать avr-nm.
.PARAMETER Search
  Шаблон поиска по avr-nm (Select-String). По умолчанию — без поиска.
.PARAMETER Force
  Пересоздать tmp/<bench>/ (иначе переиспользовать).
.PARAMETER SkipMonitor
  Только собрать (не прошивать/не слушать).
.EXAMPLE
  .\run_bench.ps1 -Bench All -Upload -Port COM3
.EXAMPLE
  .\run_bench.ps1 -Bench bench_atomic -Upload -Port COM3 -MonitorSec 5 -Search counter
#>
param(
    [string]$Bench = "All",
    [switch]$Upload,
    [string]$Port,
    [string]$Results,
    [int]$MonitorSec = -1,
    [switch]$NoSize,
    [switch]$NoNm,
    [string]$Search,
    [switch]$Force,
    [switch]$SkipMonitor
)

# ==== ФИКС КОДИРОВОК (как в старом) ====
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$OutputEncoding           = [System.Text.Encoding]::UTF8
try { chcp 65001 | Out-Null } catch {}
$env:LANG     = "en_US.UTF-8"
$env:LC_ALL   = "en_US.UTF-8"
$env:LANGUAGE = "en"
# ======================================

$ErrorActionPreference = "Stop"
$root      = $PSScriptRoot
$benchRoot = Join-Path $root "bench"
$srcRoot   = Join-Path $root "src"
$tmpRoot   = Join-Path $root "tmp"
$specPath  = Join-Path $root "bench-spec.psd1"
$fqbn      = "arduino:avr:nano:cpu=atmega328"
$mcu       = "atmega328p"

if (-not (Test-Path $benchRoot)) { throw "bench/ not found: $benchRoot" }
if (-not (Test-Path $srcRoot))   { throw "src/ not found: $srcRoot" }
if (-not (Test-Path $tmpRoot))   { New-Item -ItemType Directory -Path $tmpRoot | Out-Null }
if (-not (Test-Path $specPath))  { throw "bench-spec.psd1 not found: $specPath" }
$spec = Import-PowerShellDataFile $specPath
if ($spec.Fqbn) { $fqbn = $spec.Fqbn }

# --- Результаты -------------------------------------------------------------
if (-not $Results) {
    $stamp   = Get-Date -Format "yyyy-MM-dd_HHmmss"
    $Results = Join-Path $root "results\$stamp"
}
New-Item -ItemType Directory -Force -Path $Results | Out-Null
Write-Host "Results: $Results"

# --- Git commit -------------------------------------------------------------
$commit = "unknown"
try { $commit = (git -C $root rev-parse --short HEAD 2>$null) } catch {}
if (-not $commit) { $commit = "nogit" }

# --- Helpers ----------------------------------------------------------------
function Write-Step([string]$msg) {
    Write-Host ""
    Write-Host "=== $msg ===" -ForegroundColor Cyan
}

function Prepare-WorkDir([string]$inoPath, [string]$benchName) {
    $workDir  = Join-Path $tmpRoot $benchName
    $buildDir = Join-Path $workDir "build"
    $logDir   = Join-Path $workDir "logs"
    $inoTarget = Join-Path $workDir (Split-Path $inoPath -Leaf)
    $srcTarget = Join-Path $workDir "src"

    if ($Force -and (Test-Path $workDir)) { Remove-Item $workDir -Recurse -Force }
    New-Item -ItemType Directory -Force -Path $workDir  | Out-Null
    New-Item -ItemType Directory -Force -Path $buildDir | Out-Null
    New-Item -ItemType Directory -Force -Path $logDir   | Out-Null

    # скетч
    Copy-Item -Path $inoPath -Destination $inoTarget -Force
    # dbg.h (рядом с bench/)
    $dbgSrc = Join-Path $benchRoot "dbg.h"
    if (Test-Path $dbgSrc) { Copy-Item -Path $dbgSrc -Destination (Join-Path $workDir "dbg.h") -Force }
    # src/
    if (Test-Path $srcTarget) { Remove-Item $srcTarget -Recurse -Force }
    Copy-Item -Path $srcRoot -Destination $srcTarget -Recurse -Force

    return @{ workDir = $workDir; buildDir = $buildDir; logDir = $logDir; ino = $inoTarget }
}

function Get-SizeInfo([string]$elf, [string]$logDir, [string]$benchName) {
    if ($NoSize -or -not (Test-Path $elf)) { return @{ flash = -1; sram = -1 } }
    $sizeLog = Join-Path $logDir "size_$benchName.txt"
    $out = & avr-size --format=avr --mcu=$mcu $elf 2>&1
    $out | Set-Content $sizeLog -Encoding UTF8
    # Program:    N bytes (X% Full)  /  Data:    M bytes (Y% Full)
    $flash = -1; $sram = -1
    foreach ($line in $out) {
        if ($line -match 'Program:\s+(\d+)\s+bytes') { $flash = [int]$Matches[1] }
        if ($line -match 'Data:\s+(\d+)\s+bytes')    { $sram  = [int]$Matches[1] }
    }
    return @{ flash = $flash; sram = $sram }
}

function Invoke-Nm([string]$elf, [string]$logDir, [string]$benchName) {
    if ($NoNm -or -not (Test-Path $elf)) { return }
    $nmLog = Join-Path $logDir "nm_$benchName.txt"
    & avr-nm -S --size-sort -td $elf 2>&1 | Set-Content $nmLog -Encoding UTF8
    if ($Search) {
        $filterLog = Join-Path $logDir "nmfilter_$benchName.txt"
        $m = & avr-nm -S --size-sort -td $elf | Select-String $Search
        if ($m) { $m | ForEach-Object { $_.Line } | Set-Content $filterLog -Encoding UTF8 }
        else    { "# No matches for '$Search'" | Set-Content $filterLog -Encoding UTF8 }
    }
}

function Parse-Monitor([string]$text, $patterns) {
    $vals = @{}
    foreach ($k in $patterns.Keys) {
        $m = [regex]::Match($text, $patterns[$k])
        if ($m.Success) { $vals[$k] = [int64]$m.Groups[1].Value } else { $vals[$k] = $null }
    }
    return $vals
}

function Check-Expected($actual, $expected, $range, $info) {
    $pass = $true; $details = @()
    foreach ($k in $expected.Keys) {
        $exp = [int64]$expected[$k]; $act = $actual[$k]
        if ($null -eq $act) { $pass = $false; $details += "$k=MISSING(exp=$exp)"; continue }
        if ($act -ne $exp)  { $pass = $false; $details += "$k=$act(exp=$exp)" } else { $details += "$k=$act" }
    }
    if ($range) {
        foreach ($k in $range.Keys) {
            $act = $actual[$k]; $min = [int64]$range[$k].Min; $max = [int64]$range[$k].Max
            if ($null -eq $act) { $pass = $false; $details += "$k=MISSING(range=$min..$max)"; continue }
            if ($act -lt $min -or $act -gt $max) { $pass = $false; $details += "$k=$act(out $min..$max)" }
            else { $details += "$k=$act" }
        }
    }
    if ($info) {
        foreach ($k in $info) {
            $act = $actual[$k]
            if ($null -ne $act) { $details += "$k=$act(info)" } else { $details += "$k=MISSING(info)" }
        }
    }
    return @{ pass = $pass; details = ($details -join ", ") }
}

function Run-Monitor([string]$port, [int]$seconds, [string]$logPath) {
    $cliHelp = & arduino-cli monitor --help 2>&1 | Out-String
    $supportsDuration = $cliHelp -match '--duration'

    $monArgs = @("monitor", "-p", $port, "--config", "baudrate=$($spec.Baud)", "--timestamp")
    $useDuration = $false
    if ($supportsDuration -and $seconds -gt 0) {
        $monArgs += @("--duration", "${seconds}s")
        $useDuration = $true
    }

    $monErr = "$logPath.err"
    $p = Start-Process -FilePath "arduino-cli" -ArgumentList $monArgs `
        -RedirectStandardOutput $logPath -RedirectStandardError $monErr `
        -PassThru -NoNewWindow

    if ($useDuration) {
        $deadline = (Get-Date).AddSeconds($seconds + 10)
    } else {
        $deadline = (Get-Date).AddSeconds($seconds)
    }
    while (((Get-Date) -lt $deadline) -and (-not $p.HasExited)) { Start-Sleep -Milliseconds 200 }
    if (-not $p.HasExited) { try { $p.Kill() } catch {} }
    $p.WaitForExit()

    # stderr: штатные сообщения при kill — не ошибка
    if (Test-Path $monErr) {
        $stderr = Get-Content $monErr -Raw
        Remove-Item $monErr -Force -ErrorAction SilentlyContinue
        if ($stderr -and $stderr.Trim()) {
            $expected = $stderr -match 'I/O operation has been aborted|serial port closed|operation was canceled|broken pipe|EOF|Access is denied'
            if (-not $expected) { Add-Content "$logPath.stderr.txt" $stderr -Encoding UTF8 }
        }
    }
}

# --- Основной цикл ----------------------------------------------------------
$benches = if ($Bench -eq "All") {
    Get-ChildItem $benchRoot -Filter "bench_*.ino" | Sort-Object Name | ForEach-Object { $_.BaseName }
} else { @($Bench) }

$summary = @()

foreach ($b in $benches) {
    $inoPath = Join-Path $benchRoot "$b.ino"
    if (-not (Test-Path $inoPath)) { Write-Warning "skip: $inoPath"; continue }
    $bSpec = $spec.Benches[$b]
    if (-not $bSpec) { Write-Warning "no spec for $b — skip"; continue }

    Write-Step "$b"
    $w = Prepare-WorkDir $inoPath $b
    Write-Host "  work: $($w.workDir)"

    $benchResults = Join-Path $Results $b
    New-Item -ItemType Directory -Force -Path $benchResults | Out-Null

    # --- Компиляция ---
    $compileLog = Join-Path $w.logDir "compile_$b.txt"
    & arduino-cli compile --fqbn $fqbn --build-path $w.buildDir $w.workDir 2>&1 |
        Tee-Object -FilePath $compileLog | Out-Null
    if ($LASTEXITCODE -ne 0) {
        Write-Host "  COMPILE FAILED (log: $compileLog)" -ForegroundColor Red
        $summary += [pscustomobject]@{ bench=$b; status="COMPILE_FAIL"; flash=-1; sram=-1; details="see $compileLog"; notes=$bSpec.Notes }
        continue
    }

    # --- size / nm / search ---
    $elf = Join-Path $w.buildDir "$b.ino.elf"
    $size = Get-SizeInfo $elf $w.logDir $b
    Invoke-Nm $elf $w.logDir $b
    Write-Host ("  flash={0} sram={1}" -f $size.flash, $size.sram)

    # --- upload / monitor ---
    $actual = @{}
    $check  = @{ pass = $null; details = "visual" }
    $visual = [bool]$bSpec.Visual

    if (-not $SkipMonitor -and -not $visual) {
        if (-not $Upload) { throw "-Upload required for monitor (bench=$b)" }
        if (-not $Port)   { throw "-Port required for monitor (bench=$b)" }

        $hex = Join-Path $w.buildDir "$b.ino.hex"
        if (-not (Test-Path $hex)) {
            Write-Host "  HEX not found: $hex" -ForegroundColor Red
            $summary += [pscustomobject]@{ bench=$b; status="NO_HEX"; flash=$size.flash; sram=$size.sram; details=""; notes=$bSpec.Notes }
            continue
        }
        & arduino-cli upload --fqbn $fqbn -p $Port --input-file $hex 2>&1 | Out-Null
        if ($LASTEXITCODE -ne 0) {
            Write-Host "  UPLOAD FAILED" -ForegroundColor Red
            $summary += [pscustomobject]@{ bench=$b; status="UPLOAD_FAIL"; flash=$size.flash; sram=$size.sram; details=""; notes=$bSpec.Notes }
            continue
        }

        $sec = if ($MonitorSec -gt 0) { $MonitorSec } else { [int]$bSpec.MonitorSec }
        if ($sec -le 0) { $sec = $spec.DefaultMonitorSec }
        $monitorLog = Join-Path $benchResults "monitor.txt"
        Write-Host "  monitor $sec s on $Port"
        Run-Monitor -port $Port -seconds $sec -logPath $monitorLog

        if (Test-Path $monitorLog) {
            $text   = Get-Content $monitorLog -Raw
            $actual = Parse-Monitor $text $bSpec.Patterns
            $check  = Check-Expected $actual $bSpec.Expected $bSpec.Range $bSpec.Info
        } else {
            $check = @{ pass = $false; details = "no monitor log" }
        }
    }

    # --- meta.json ---
    $meta = [ordered]@{
        bench     = $b
        timestamp = (Get-Date).ToString("o")
        commit    = $commit
        fqbn      = $fqbn
        flash     = $size.flash
        sram      = $size.sram
        expected  = $bSpec.Expected
        range     = $bSpec.Range
        info      = $bSpec.Info
        actual    = $actual
        pass      = $check.pass
        details   = $check.details
        notes     = $bSpec.Notes
    }
    $meta | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $benchResults "meta.json") -Encoding UTF8

    $status = if ($null -eq $check.pass) { "VISUAL" } elseif ($check.pass) { "PASS" } else { "FAIL" }
    Write-Host ("  {0}: {1}" -f $status, $check.details)
    $summary += [pscustomobject]@{
        bench=$b; status=$status; flash=$size.flash; sram=$size.sram
        details=$check.details; notes=$bSpec.Notes
    }
}

# --- summary.md -------------------------------------------------------------
$summaryPath = Join-Path $Results "summary.md"
$lines = @()
$lines += "# ResultsBench — $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
$lines += ""
$lines += "- commit: ``$commit``"
$lines += "- fqbn: ``$fqbn``"
$lines += "- port: ``$Port``"
$lines += ""
$lines += "| Bench | Status | Flash | SRAM | Details | Notes |"
$lines += "|-------|--------|-------|------|---------|-------|"
foreach ($s in $summary) {
    $lines += ("| {0} | {1} | {2} | {3} | {4} | {5} |" -f `
        $s.bench, $s.status, $s.flash, $s.sram, $s.details, $s.notes)
}
$lines | Set-Content $summaryPath -Encoding UTF8
Write-Host ""
Write-Host "Summary: $summaryPath"