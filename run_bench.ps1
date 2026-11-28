<#
.SYNOPSIS
  Сборка/прошивка/мониторинг бенчей nanoQuick.
  Без -Bench — интерактивный выбор. -Bench All — все Auto + подтверждение Requires.
  Результаты: results/<stamp>/<bench>/ (meta.json, monitor.txt, compile.txt, upload.txt[, nm.txt])
              results/<stamp>/summary.md (глаза), run.json (машина, сводный).
.PARAMETER Bench
  Имя бенча или All. Без параметра — интерактив.
.PARAMETER Upload
  Прошить (требует -Port).
.PARAMETER Port
  COM-порт.
.PARAMETER Results
  Каталог результатов. По умолчанию <root>/results/<stamp>.
.PARAMETER MonitorSec
  Переопределить WaitSec (режим WaitSec).
.PARAMETER UploadTimeoutSec
  Таймаут прошивки (default 30). Bootloader может молчать — не виснем вечно.
.PARAMETER Search
  Шаблон поиска по avr-nm.
.PARAMETER Nm
  Принудительный avr-nm (иначе по флагу Nm в спеке).
.PARAMETER Force
  Пересоздать tmp/<bench>/.
.PARAMETER SkipMonitor
  Только собрать (BUILD_ONLY).
.PARAMETER Yes
  Не спрашивать Requires в -Bench All.
.PARAMETER List
  Показать список бенчей и выйти.
.EXAMPLE
  .\run_bench.ps1 -List
  .\run_bench.ps1 -Bench All -Upload -Port COM3 -Yes
  .\run_bench.ps1 -Bench bench_spi -Upload -Port COM3 -Nm
.NOTES
  Fqbn: arduino:avr:nano:cpu=atmega328 (new bootloader) или cpu=atmega328old (old).
  Если UPLOAD_FAIL — проверь соответствие bootloader платы и cpu= в спеке.
  Спека: bench-spec.psd1 (Import-PowerShellDataFile требует .psd1).
#>
param(
    [string]$Bench,
    [switch]$Upload,
    [string]$Port,
    [string]$Results,
    [int]$MonitorSec = -1,
    [int]$UploadTimeoutSec = 30,
    [string]$Search,
    [switch]$Nm,
    [switch]$Force,
    [switch]$SkipMonitor,
    [switch]$Yes,
    [switch]$List
)

# ==== Кодировки ====
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$OutputEncoding           = [System.Text.Encoding]::UTF8
try { chcp 65001 | Out-Null } catch {}
$ErrorActionPreference = "Stop"

$root      = $PSScriptRoot
$benchRoot = Join-Path $root "bench"
$srcRoot   = Join-Path $root "src"
$tmpRoot   = Join-Path $root "tmp"
$specPath  = Join-Path $root "bench-spec.psd1"
$mcu       = "atmega328p"

# ==== Проверка окружения ====
function Assert-Tools {
    foreach ($t in @("arduino-cli", "avr-size", "avr-nm")) {
        if (-not (Get-Command $t -ErrorAction SilentlyContinue)) {
            throw "$t not found in PATH. Install Arduino AVR toolchain / add to PATH."
        }
    }
}
Assert-Tools

if (-not (Test-Path $benchRoot)) { throw "bench/ not found: $benchRoot" }
if (-not (Test-Path $srcRoot))   { throw "src/ not found: $srcRoot" }
if (-not (Test-Path $tmpRoot))   { New-Item -ItemType Directory -Path $tmpRoot | Out-Null }

# ==== Спека ====
if (-not (Test-Path $specPath)) {
    $alt = Join-Path $root "bench-spec.ps1"
    if (Test-Path $alt) {
        throw "bench-spec.psd1 not found, but bench-spec.ps1 exists. Rename to .psd1 (Import-PowerShellDataFile requires .psd1)."
    }
    throw "bench-spec.psd1 not found: $specPath"
}
$spec = Import-PowerShellDataFile $specPath
$fqbn = if ($spec.Fqbn) { $spec.Fqbn } else { "arduino:avr:nano:cpu=atmega328" }

# ==== Результаты ====
if (-not $Results) {
    $stamp   = Get-Date -Format "yyyy-MM-dd_HHmmss"
    $Results = Join-Path $root (Join-Path "results" $stamp)
}
New-Item -ItemType Directory -Force -Path $Results | Out-Null

$commit = "nogit"
try {
    $c = git -C $root rev-parse --short HEAD 2>$null
    if ($c) { $commit = $c }
} catch {}

Write-Host "Results: $Results"
Write-Host "Fqbn:    $fqbn"
Write-Host "Commit:  $commit"

# ==== Helpers ====
function Write-Step([string]$msg) {
    Write-Host ""; Write-Host "=== $msg ===" -ForegroundColor Cyan
}

function Prepare-WorkDir([string]$inoPath, [string]$benchName) {
    $workDir   = Join-Path $tmpRoot $benchName
    $buildDir  = Join-Path $workDir "build"
    $inoTarget = Join-Path $workDir (Split-Path $inoPath -Leaf)
    if ($Force -and (Test-Path $workDir)) { Remove-Item $workDir -Recurse -Force }
    New-Item -ItemType Directory -Force -Path $workDir  | Out-Null
    New-Item -ItemType Directory -Force -Path $buildDir | Out-Null
    Copy-Item -Path $inoPath -Destination $inoTarget -Force
    $dbgSrc = Join-Path $benchRoot "dbg.h"
    if (Test-Path $dbgSrc) { Copy-Item $dbgSrc (Join-Path $workDir "dbg.h") -Force }
    $srcTarget = Join-Path $workDir "src"
    if (Test-Path $srcTarget) { Remove-Item $srcTarget -Recurse -Force }
    Copy-Item -Path $srcRoot -Destination $srcTarget -Recurse -Force
    return @{ workDir=$workDir; buildDir=$buildDir; ino=$inoTarget }
}

function Invoke-Compile([string]$workDir, [string]$buildDir, [string]$logPath) {
    & arduino-cli compile --fqbn $fqbn --build-path $buildDir $workDir 2>&1 |
        Tee-Object -FilePath $logPath | Out-Null
    $ok = ($LASTEXITCODE -eq 0)
    $flash = -1; $sram = -1
    if ($ok) {
        $elf = Join-Path $buildDir ((Split-Path $workDir -Leaf) + ".ino.elf")
        if (Test-Path $elf) {
            $out = & avr-size --format=avr --mcu=$mcu $elf 2>&1
            foreach ($line in $out) {
                if ($line -match 'Program:\s+(\d+)\s+bytes') { $flash = [int]$Matches[1] }
                if ($line -match 'Data:\s+(\d+)\s+bytes')    { $sram  = [int]$Matches[1] }
            }
        }
    }
    return @{ ok=$ok; flash=$flash; sram=$sram }
}

function Invoke-Upload([string]$hex, [string]$logPath, [int]$timeoutSec) {
    $errPath = "$logPath.err"
    $p = Start-Process -FilePath "arduino-cli" `
        -ArgumentList @("upload", "--fqbn", $fqbn, "-p", $Port, "--input-file", $hex) `
        -RedirectStandardOutput $logPath -RedirectStandardError $errPath `
        -PassThru -NoNewWindow
    $deadline = (Get-Date).AddSeconds($timeoutSec)
    while (((Get-Date) -lt $deadline) -and (-not $p.HasExited)) { Start-Sleep -Milliseconds 200 }
    $timedOut = -not $p.HasExited
    if ($timedOut) { try { $p.Kill() } catch {} }
    $p.WaitForExit()
    if (Test-Path $errPath) {
        Get-Content $errPath -Raw | Add-Content $logPath -Encoding UTF8
        Remove-Item $errPath -Force -ErrorAction SilentlyContinue
    }
    $ok = (-not $timedOut) -and ($p.ExitCode -eq 0)
    return @{ ok=$ok; timedOut=$timedOut }
}

function Invoke-Nm([string]$elf, [string]$outDir, [string]$benchName, [bool]$enabled) {
    if (-not $enabled -or -not (Test-Path $elf)) { return $null }
    $nmLog = Join-Path $outDir "nm_$benchName.txt"
    & avr-nm -S --size-sort -td $elf 2>&1 | Set-Content $nmLog -Encoding UTF8
    if ($Search) {
        $filterLog = Join-Path $outDir "nmfilter_$benchName.txt"
        $m = & avr-nm -S --size-sort -td $elf | Select-String $Search
        if ($m) { $m | ForEach-Object { $_.Line } | Set-Content $filterLog -Encoding UTF8 }
        else    { "# No matches for '$Search'" | Set-Content $filterLog -Encoding UTF8 }
    }
    return $nmLog
}

function Invoke-Monitor {
    param([string]$logPath, [string]$mode, [int]$hardCapSec, [int]$waitSec, [int]$graceSec, $patterns)
    if ($mode -eq "Visual") { return @{ mode="Visual"; seconds=0 } }
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    $cliHelp = & arduino-cli monitor --help 2>&1 | Out-String
    $supportsDuration = $cliHelp -match '--duration'
    $monArgs = @("monitor", "-p", $Port, "--config", "baudrate=$($spec.Baud)", "--timestamp")
    $useDuration = $false
    if ($mode -eq "WaitSec" -and $supportsDuration -and $waitSec -gt 0) {
        $monArgs += @("--duration", "${waitSec}s"); $useDuration = $true
    }
    $errPath = "$logPath.err"
    $p = Start-Process -FilePath "arduino-cli" -ArgumentList $monArgs `
        -RedirectStandardOutput $logPath -RedirectStandardError $errPath `
        -PassThru -NoNewWindow
    if ($mode -eq "WaitSec") {
        $deadline = if ($useDuration) { (Get-Date).AddSeconds($waitSec + 10) } else { (Get-Date).AddSeconds($waitSec) }
        while (((Get-Date) -lt $deadline) -and (-not $p.HasExited)) { Start-Sleep -Milliseconds 200 }
    } elseif ($mode -eq "WaitAll") {
        $deadline = (Get-Date).AddSeconds($hardCapSec)
        $keys = @($patterns.Keys)
        while (((Get-Date) -lt $deadline) -and (-not $p.HasExited)) {
            Start-Sleep -Milliseconds 200
            if (Test-Path $logPath) {
                $txt = Get-Content $logPath -Raw -ErrorAction SilentlyContinue
                if ($txt) {
                    $all = $true
                    foreach ($k in $keys) { if ($txt -notmatch $patterns[$k]) { $all = $false; break } }
                    if ($all) { break }
                }
            }
        }
        if ($graceSec -gt 0) { Start-Sleep -Seconds $graceSec }
    }
    if (-not $p.HasExited) { try { $p.Kill() } catch {} }
    $p.WaitForExit()
    Start-Sleep -Milliseconds 150   # сброс буфера ФС перед чтением
    if (Test-Path $errPath) {
        $stderr = Get-Content $errPath -Raw
        Remove-Item $errPath -Force -ErrorAction SilentlyContinue
        if ($stderr -and $stderr.Trim()) {
            $expected = $stderr -match 'I/O operation has been aborted|serial port closed|operation was canceled|broken pipe|EOF|Access is denied'
            if (-not $expected) { Add-Content "$logPath.stderr.txt" $stderr -Encoding UTF8 }
        }
    }
    $sw.Stop()
    return @{ mode=$mode; seconds=[math]::Round($sw.Elapsed.TotalSeconds, 1) }
}

function Parse-Monitor([string]$text, $patterns) {
    $vals = @{}
    foreach ($k in $patterns.Keys) {
        # ПОСЛЕДНЕЕ совпадение: периодические бенчи печатают серию,
        # первая итерация может включать время setup.
        $ms = [regex]::Matches($text, $patterns[$k])
        if ($ms.Count -gt 0) { $vals[$k] = [int64]$ms[$ms.Count - 1].Groups[1].Value }
        else                 { $vals[$k] = $null }
    }
    return $vals
}

function Test-Expected($actual, $expected, $range, $info) {
    $pass = $true; $details = @()
    if ($expected) {
        foreach ($k in $expected.Keys) {
            $exp = [int64]$expected[$k]; $act = $actual[$k]
            if ($null -eq $act) { $pass = $false; $details += "$k=MISSING(exp=$exp)"; continue }
            if ($act -ne $exp)  { $pass = $false; $details += "$k=$act(exp=$exp)" } else { $details += "$k=$act" }
        }
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
    return @{ pass=$pass; details=($details -join ", ") }
}

# ==== Список бенчей ====
$allBenches = Get-ChildItem $benchRoot -Filter "bench_*.ino" | Sort-Object Name | ForEach-Object { $_.BaseName }

function Show-BenchList {
    Write-Host ""
    Write-Host "Доступные бенчи:" -ForegroundColor Yellow
    for ($i = 0; $i -lt $allBenches.Count; $i++) {
        $b  = $allBenches[$i]
        $bs = $spec.Benches[$b]
        $tags = @()
        if ($bs.Requires) { $tags += "req: $($bs.Requires)" }
        if ($bs.MonitorMode -eq "Visual") { $tags += "visual" }
        if ($bs.Auto) { $tags += "auto" }
        $tag = if ($tags) { " [" + ($tags -join "; ") + "]" } else { "" }
        "{0,2}. {1}{2}" -f ($i + 1), $b, $tag | Write-Host
    }
}

if ($List) { Show-BenchList; exit 0 }

# ==== Интерактивный выбор ====
if (-not $Bench) {
    Show-BenchList
    do {
        $ans = (Read-Host "`nКакой бенч? (номер, имя, all)").Trim()
        if ($ans -match '^(all|все)$') { $Bench = "All"; break }
        if ($ans -match '^\d+$') {
            $idx = [int]$ans - 1
            if ($idx -ge 0 -and $idx -lt $allBenches.Count) { $Bench = $allBenches[$idx]; break }
        } else {
            $m = $allBenches | Where-Object { $_ -eq $ans -or $_ -eq "bench_$ans" }
            if ($m) { $Bench = $m; break }
        }
        Write-Host "Некорректный выбор." -ForegroundColor Red
    } while ($true)
}

if (-not $Search) {
    $ans = (Read-Host "Шаблон поиска по nm (Enter = пропустить)").Trim()
    if ($ans) { $Search = $ans }
}

$benches = if ($Bench -eq "All") { $allBenches } else { @($Bench) }

# ==== Основной цикл ====
$results = @()

foreach ($b in $benches) {
    $inoPath = Join-Path $benchRoot "$b.ino"
    if (-not (Test-Path $inoPath)) { Write-Warning "skip: $inoPath"; continue }
    $bs = $spec.Benches[$b]
    if (-not $bs) { Write-Warning "no spec for $b — skip"; continue }

    # Requires в -Bench All
    if ($Bench -eq "All" -and $bs.Requires -and -not $bs.Auto -and -not $Yes) {
        Write-Host ""
        Write-Host "  $b requires: $($bs.Requires)" -ForegroundColor Yellow
        $ans = (Read-Host "  Условие выполнено? (y/N/s=skip)").Trim().ToLower()
        if ($ans -eq "s") {
            $results += [pscustomobject]@{ bench=$b; status="SKIP"; flash=-1; sram=-1; mode="-"; seconds=0; details="manual skip"; notes=$bs.Notes; requires=$bs.Requires }
            continue
        }
        if ($ans -ne "y") {
            $results += [pscustomobject]@{ bench=$b; status="SKIP"; flash=-1; sram=-1; mode="-"; seconds=0; details="condition not met"; notes=$bs.Notes; requires=$bs.Requires }
            continue
        }
    }

    Write-Step $b
    $w = Prepare-WorkDir $inoPath $b
    $benchResults = Join-Path $Results $b
    New-Item -ItemType Directory -Force -Path $benchResults | Out-Null

    # Дефолты результата (все поля определены ВСЕГДА)
    $r = [ordered]@{
        bench     = $b
        timestamp = (Get-Date).ToString("o")
        commit    = $commit
        fqbn      = $fqbn
        port      = $Port
        status    = "UNKNOWN"
        flash     = -1
        sram      = -1
        monitor   = @{ mode="-"; seconds=0; hardCapSec=0; waitSec=0; graceSec=0 }
        expected  = $bs.Expected
        range     = $bs.Range
        info      = $bs.Info
        actual    = @{}
        pass      = $null
        details   = ""
        requires  = $bs.Requires
        notes     = $bs.Notes
        logs      = @{}
    }

    # --- Компиляция (лог ВСЕГДА в results) ---
    $compileLog = Join-Path $benchResults "compile.txt"
    $cmp = Invoke-Compile $w.workDir $w.buildDir $compileLog
    $r.flash = $cmp.flash; $r.sram = $cmp.sram
    $r.logs.compile = "compile.txt"
    if (-not $cmp.ok) {
        $r.status = "COMPILE_FAIL"; $r.details = "see $compileLog"
        Write-Host "  COMPILE FAILED (log: $compileLog)" -ForegroundColor Red
        $r | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $benchResults "meta.json") -Encoding UTF8
        $results += [pscustomobject]@{ bench=$b; status=$r.status; flash=$r.flash; sram=$r.sram; mode="-"; seconds=0; details=$r.details; notes=$bs.Notes; requires=$bs.Requires }
        continue
    }
    Write-Host ("  flash={0} sram={1}" -f $r.flash, $r.sram)

    # --- nm ---
    $elf = Join-Path $w.buildDir "$b.ino.elf"
    $nmOn = if ($Nm) { $true } else { [bool]$bs.Nm }
    $nmPath = Invoke-Nm $elf $benchResults $b $nmOn
    if ($nmPath) { $r.logs.nm = (Split-Path $nmPath -Leaf) }

    # --- Монитор/прошивка ---
    $mode = if ($bs.MonitorMode) { $bs.MonitorMode } else { "WaitAll" }
    $hardCap = if ($bs.HardCapSec) { [int]$bs.HardCapSec } else { [int]$spec.DefaultHardCapSec }
    $grace   = if ($bs.GraceSec)   { [int]$bs.GraceSec }   else { [int]$spec.DefaultGraceSec }
    $waitSec = if ($bs.WaitSec)    { [int]$bs.WaitSec }    else { 0 }
    if ($MonitorSec -gt 0) { $mode = "WaitSec"; $waitSec = $MonitorSec }
    $r.monitor = @{ mode=$mode; seconds=0; hardCapSec=$hardCap; waitSec=$waitSec; graceSec=$grace }

    if ($SkipMonitor) {
        $r.status = "BUILD_ONLY"; $r.monitor.mode = "Skip"; $r.details = "compiled, no upload/monitor"
    } elseif ($mode -eq "Visual") {
        $r.status = "VISUAL"; $r.details = "visual check"
        # Visual тоже можно прошить если -Upload
        if ($Upload -and $Port) {
            $hex = Join-Path $w.buildDir "$b.ino.hex"
            $uploadLog = Join-Path $benchResults "upload.txt"
            $up = Invoke-Upload $hex $uploadLog $UploadTimeoutSec
            $r.logs.upload = "upload.txt"
            if (-not $up.ok) { $r.status = "UPLOAD_FAIL"; $r.details = if ($up.timedOut) { "upload timeout ${UploadTimeoutSec}s" } else { "see $uploadLog" } }
        }
    } else {
        if (-not $Upload) { throw "-Upload required for monitor (bench=$b)" }
        if (-not $Port)   { throw "-Port required for monitor (bench=$b)" }
        $hex = Join-Path $w.buildDir "$b.ino.hex"
        if (-not (Test-Path $hex)) {
            $r.status = "NO_HEX"; $r.details = "hex not found"
        } else {
            $uploadLog = Join-Path $benchResults "upload.txt"
            Write-Host "  upload -> $Port (timeout ${UploadTimeoutSec}s)"
            $up = Invoke-Upload $hex $uploadLog $UploadTimeoutSec
            $r.logs.upload = "upload.txt"
            if (-not $up.ok) {
                $r.status = "UPLOAD_FAIL"
                $r.details = if ($up.timedOut) { "upload timeout ${UploadTimeoutSec}s (bootloader silent? check cpu= in spec)" } else { "see $uploadLog" }
                Write-Host "  UPLOAD FAILED: $($r.details)" -ForegroundColor Red
            } else {
                $monitorLog = Join-Path $benchResults "monitor.txt"
                Write-Host "  monitor mode=$mode cap=$hardCap sec=$waitSec grace=$grace"
                $mon = Invoke-Monitor -logPath $monitorLog -mode $mode -hardCapSec $hardCap -waitSec $waitSec -graceSec $grace -patterns $bs.Patterns
                $r.monitor.seconds = $mon.seconds
                $r.logs.monitor = "monitor.txt"
                if (Test-Path $monitorLog) {
                    $text = Get-Content $monitorLog -Raw
                    $r.actual = Parse-Monitor $text $bs.Patterns
                    $chk = Test-Expected $r.actual $bs.Expected $bs.Range $bs.Info
                    $r.pass = $chk.pass; $r.details = $chk.details
                    $r.status = if ($chk.pass) { "PASS" } else { "FAIL" }
                } else {
                    $r.status = "FAIL"; $r.pass = $false; $r.details = "no monitor log"
                }
            }
        }
    }

    # --- meta.json ---
    $r | ConvertTo-Json -Depth 6 | Set-Content (Join-Path $benchResults "meta.json") -Encoding UTF8
    $color = switch ($r.status) { "PASS" { "Green" } "FAIL" { "Red" } "COMPILE_FAIL" { "Red" } "UPLOAD_FAIL" { "Red" } default { "Yellow" } }
    Write-Host ("  {0}: {1}" -f $r.status, $r.details) -ForegroundColor $color
    $results += [pscustomobject]@{ bench=$b; status=$r.status; flash=$r.flash; sram=$r.sram; mode=$r.monitor.mode; seconds=$r.monitor.seconds; details=$r.details; notes=$bs.Notes; requires=$bs.Requires }
}

# ==== summary.md ====
$lines = @()
$lines += "# ResultsBench — $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')"
$lines += ""
$lines += "- commit: ``$commit``"
$lines += "- fqbn: ``$fqbn``"
$lines += "- port: ``$Port``"
$lines += ""
$lines += "| Bench | Status | Flash | SRAM | Mode | Sec | Details | Notes |"
$lines += "|-------|--------|-------|------|------|-----|---------|-------|"
foreach ($s in $results) {
    $lines += ("| {0} | {1} | {2} | {3} | {4} | {5} | {6} | {7} |" -f $s.bench, $s.status, $s.flash, $s.sram, $s.mode, $s.seconds, $s.details, $s.notes)
}
$summaryPath = Join-Path $Results "summary.md"
$lines | Set-Content $summaryPath -Encoding UTF8

# ==== run.json (сводный машинный) ====
$run = [ordered]@{
    timestamp = (Get-Date).ToString("o")
    commit    = $commit
    fqbn      = $fqbn
    port      = $Port
    spec_file = "bench-spec.psd1"
    benches   = @()
}
foreach ($b in $benches) {
    $metaPath = Join-Path (Join-Path $Results $b) "meta.json"
    if (Test-Path $metaPath) {
        $run.benches += (Get-Content $metaPath -Raw | ConvertFrom-Json)
    }
}
$run | ConvertTo-Json -Depth 8 | Set-Content (Join-Path $Results "run.json") -Encoding UTF8

Write-Host ""
Write-Host "Summary: $summaryPath"
Write-Host "Run JSON: $(Join-Path $Results 'run.json')"
$passCount = ($results | Where-Object { $_.status -eq "PASS" }).Count
$failCount = ($results | Where-Object { $_.status -in @("FAIL","COMPILE_FAIL","UPLOAD_FAIL","NO_HEX") }).Count
Write-Host ("PASS={0} FAIL={1} TOTAL={2}" -f $passCount, $failCount, $results.Count)