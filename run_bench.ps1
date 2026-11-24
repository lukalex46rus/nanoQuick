<#
.SYNOPSIS
    Сборка bench_*.ino с Arduino CLI 1.6.0.
    Рабочие копии складываются в .\tmp\<bench_name>\.
#>

# ==== ФИКС КОДИРОВОК ====
[Console]::OutputEncoding = [System.Text.Encoding]::UTF8
$OutputEncoding           = [System.Text.Encoding]::UTF8
chcp 65001 | Out-Null
$env:LANG     = "en_US.UTF-8"
$env:LC_ALL   = "en_US.UTF-8"
$env:LANGUAGE = "en"
# ==========================

# ================== НАСТРОЙКИ ==================
$FQBN        = "arduino:avr:nano:cpu=atmega328old"
$MCU         = "atmega328p"
$PORT        = "COM3"
$BAUD        = 9600
$MONITOR_SEC = 60

$BenchDir = ".\bench"
$SrcDir   = ".\src"
$TmpRoot  = ".\tmp"

# ===============================================

$ErrorActionPreference = "Stop"
$script:HasError = $false

function Write-Step([string]$msg) {
    Write-Host ""
    Write-Host "=== $msg ===" -ForegroundColor Cyan
}

function Invoke-Safe([scriptblock]$action, [string]$errLog) {
    try {
        & $action
        if ($LASTEXITCODE -ne 0) { throw "exit code $LASTEXITCODE" }
    }
    catch {
        $script:HasError = $true
        $line = "[$([datetime]::Now)] ERROR: $_"
        Write-Host $line -ForegroundColor Red
        if ($errLog) { Add-Content -Path $errLog -Value $line -Encoding UTF8 }
    }
}

# ---------- 1. Проверка окружения ----------
if (-not (Test-Path $BenchDir)) { throw "Не найдена папка '$BenchDir'" }
if (-not (Test-Path $SrcDir))   { throw "Не найдена папка '$SrcDir'" }
if (-not (Test-Path $TmpRoot))  { New-Item -ItemType Directory -Path $TmpRoot | Out-Null }

$sketches = Get-ChildItem -Path $BenchDir -Filter "bench_*.ino" | Sort-Object Name
if ($sketches.Count -eq 0) { throw "В '$BenchDir' нет файлов bench_*.ino" }

# ---------- 2. Выбор скетча ----------
Write-Host "Доступные скетчи:" -ForegroundColor Yellow
for ($i = 0; $i -lt $sketches.Count; $i++) {
    "{0,2}. {1}" -f ($i + 1), $sketches[$i].Name | Write-Host
}

do {
    $answer = Read-Host "`nКакой bench_*.ino компилируем? (номер или имя)"
    $answer = $answer.Trim()

    if ($answer -match '^\d+$') {
        $idx = [int]$answer - 1
        if ($idx -ge 0 -and $idx -lt $sketches.Count) {
            $selected = $sketches[$idx]
            break
        }
    } else {
        $match = $sketches | Where-Object { $_.Name -eq $answer -or $_.BaseName -eq $answer }
        if ($match) { $selected = $match; break }
    }
    Write-Host "Некорректный выбор, повторите." -ForegroundColor Red
} while ($true)

$benchName = $selected.BaseName
$workDir   = Join-Path $TmpRoot $benchName
$buildDir  = Join-Path $workDir "build"
$logDir    = Join-Path $workDir "log"
$logoDir   = Join-Path $workDir "logo"
$inoTarget = Join-Path $workDir $selected.Name
$srcTarget = Join-Path $workDir "src"

Write-Host "`nВыбран: $($selected.Name)" -ForegroundColor Green
Write-Host "Рабочая папка: $workDir"        -ForegroundColor Green

# ---------- 3. Подготовка рабочей папки ----------
Write-Step "Подготовка '$workDir'"

if (Test-Path $workDir) {
    Write-Host "Удаляем старую рабочую копию..." -ForegroundColor Yellow
    Remove-Item $workDir -Recurse -Force
}
New-Item -ItemType Directory -Path $workDir   | Out-Null
New-Item -ItemType Directory -Path $buildDir  | Out-Null
New-Item -ItemType Directory -Path $logDir    | Out-Null
New-Item -ItemType Directory -Path $logoDir   | Out-Null

Copy-Item -Path $selected.FullName -Destination $inoTarget -Force
Copy-Item -Path $SrcDir -Destination $srcTarget -Recurse -Force

Write-Host "OK: скетч и src скопированы в tmp" -ForegroundColor Green

# ---------- 4. Пути к логам ----------
$errLog     = Join-Path $logDir "error_$benchName.txt"
$monLog     = Join-Path $logDir "monitor_$benchName.txt"
$sizeLog    = Join-Path $logDir "size_$benchName.txt"
$nmLog      = Join-Path $logDir "nm_$benchName.txt"
$nmFilterLog= Join-Path $logDir "nmfilter_$benchName.txt"
$compileLog = Join-Path $logDir "compile_$benchName.txt"

"" | Set-Content -Path $errLog -Encoding UTF8

# ---------- 5. Компиляция ----------
Write-Step "Компиляция ($FQBN)"
Invoke-Safe {
    arduino-cli compile --fqbn $FQBN --build-path $buildDir $workDir 2>&1 |
        Tee-Object -FilePath $compileLog
} $errLog

# ---------- 6. Размеры ----------
$elf = Join-Path $buildDir "$benchName.ino.elf"
if (Test-Path $elf) {
    Write-Step "Размер прошивки (avr-size)"
    Invoke-Safe {
        avr-size --format=avr --mcu=$MCU $elf 2>&1 | Tee-Object -FilePath $sizeLog
    } $errLog

    Write-Step "Символы (avr-nm)"
    Invoke-Safe {
        avr-nm -S --size-sort -td $elf 2>&1 | Tee-Object -FilePath $nmLog
    } $errLog

        # ---------- 6b. Поиск по символам ----------
    Write-Step "Поиск по символам (avr-nm | Select-String)"
    $pattern = Read-Host "Введите шаблон поиска (Enter = пропустить)"

    if ([string]::IsNullOrWhiteSpace($pattern)) {
        Write-Host "Поиск пропущен (пустой шаблон)." -ForegroundColor Yellow
    } else {
        Write-Host "Ищем: $pattern" -ForegroundColor Yellow
        Invoke-Safe {
            $matches = & avr-nm -S --size-sort -td $elf | Select-String $pattern
            if ($matches) {
                $matches | ForEach-Object {
                    Write-Host $_.Line -ForegroundColor Green
                }
                $matches | ForEach-Object { $_.Line } |
                    Set-Content -Path $nmFilterLog -Encoding UTF8
            } else {
                $emptyMsg = "# Совпадений по шаблону '$pattern' не найдено"
                Write-Host $emptyMsg -ForegroundColor Yellow
                Set-Content -Path $nmFilterLog -Value $emptyMsg -Encoding UTF8
            }
        } $errLog
    }

} else {
    $msg = "[$([datetime]::Now)] ERROR: ELF не найден: $elf"
    Write-Host $msg -ForegroundColor Red
    Add-Content -Path $errLog -Value $msg -Encoding UTF8
    $script:HasError = $true
}

# ---------- 7. Прошивка ----------
$hex = Join-Path $buildDir "$benchName.ino.hex"
if (Test-Path $hex) {
    Write-Step "Прошивка на $PORT"
    Invoke-Safe {
        arduino-cli upload --fqbn $FQBN -p $PORT --input-file $hex 2>&1
    } $errLog
} else {
    $msg = "[$([datetime]::Now)] ERROR: HEX не найден: $hex (прошивка пропущена)"
    Write-Host $msg -ForegroundColor Red
    Add-Content -Path $errLog -Value $msg -Encoding UTF8
    $script:HasError = $true
}

# ---------- 8. Монитор на 60 секунд ----------
if (-not $script:HasError) {
    Write-Step "Монитор порта $PORT ($BAUD бод, $MONITOR_SEC сек) -> $monLog"

    # ---- Проверяем поддержку --duration у текущего arduino-cli ----
    $cliHelp = & arduino-cli monitor --help 2>&1 | Out-String
    $supportsDuration = $cliHelp -match '--duration'

    $monArgs = @(
        "monitor", "-p", $PORT,
        "--config", "baudrate=$BAUD",
        "--timestamp"
    )
    if ($supportsDuration) {
        $monArgs += @("--duration", "${MONITOR_SEC}s")
        Write-Host "Используем --duration ${MONITOR_SEC}s" -ForegroundColor Yellow
    } else {
        Write-Host "Флаг --duration не поддерживается, остановим монитор по таймауту" -ForegroundColor Yellow
    }

    $monErrTmp = Join-Path $logDir "monitor_stderr.tmp"

    $proc = Start-Process -FilePath "arduino-cli" `
        -ArgumentList $monArgs `
        -RedirectStandardOutput $monLog `
        -RedirectStandardError  $monErrTmp `
        -PassThru -NoNewWindow

    $killedByUs = $false

    if ($supportsDuration) {
        # даём CLI завершиться самому (duration + запас)
        $deadline = (Get-Date).AddSeconds($MONITOR_SEC + 10)
    } else {
        # ждём ровно $MONITOR_SEC секунд, потом сами гасим
        $deadline = (Get-Date).AddSeconds($MONITOR_SEC)
    }

    while (((Get-Date) -lt $deadline) -and (-not $proc.HasExited)) {
        Start-Sleep -Milliseconds 250
    }

    if (-not $proc.HasExited) {
        Write-Host "Останавливаем монитор..." -ForegroundColor Yellow
        $killedByUs = $true
        try { $proc.Kill() } catch {}
    }
    $proc.WaitForExit()

    # ---- Разбираем stderr ----
    if (Test-Path $monErrTmp) {
        $stderr = Get-Content $monErrTmp -Raw
        Remove-Item $monErrTmp -Force

        if ($stderr -and $stderr.Trim()) {
            # ожидаемые при принудительном kill
            $expectedKill = $stderr -match 'I/O operation has been aborted' `
                        -or $stderr -match 'serial port closed' `
                        -or $stderr -match 'The operation was canceled' `
                        -or $stderr -match 'Access is denied' `
                        -or $stderr -match 'broken pipe' `
                        -or $stderr -match 'EOF'

            # ожидаемые при отсутствии платы/порта (не наша вина)
            $expectedMissing = $stderr -match 'No monitor available' `
                           -or $stderr -match 'not found' `
                           -or $stderr -match 'Port monitor'

            # всегда в отдельный лог для истории
            Add-Content -Path (Join-Path $logDir "monitor_stderr.txt") `
                        -Value $stderr -Encoding UTF8

            if ($killedByUs -and ($expectedKill -or -not $stderr.Trim())) {
                Write-Host "Монитор остановлен по таймауту (штатно)." -ForegroundColor Yellow
            }
            elseif ($expectedMissing) {
                # порт занят/монитора нет — тоже не считаем фатальной ошибкой всей сборки
                Write-Host "Монитор не смог стартовать (нет доступа к порту)." -ForegroundColor Yellow
                Add-Content -Path $errLog -Value "[$([datetime]::Now)] WARN monitor: $stderr" -Encoding UTF8
                $script:HasError = $true
            }
            else {
                # реальное падение CLI (например, exit=1 без kill)
                $script:HasError = $true
                $line = "[$([datetime]::Now)] MONITOR ERROR (exit=$($proc.ExitCode)): $stderr"
                Write-Host $line -ForegroundColor Red
                Add-Content -Path $errLog -Value $line -Encoding UTF8
            }
        }
    }

    if ($killedByUs) {
        Write-Host "Монитор завершён (таймаут $MONITOR_SEC сек). Лог: $monLog" -ForegroundColor Green
    } elseif ($proc.ExitCode -eq 0) {
        Write-Host "Монитор завершился штатно. Лог: $monLog" -ForegroundColor Green
    }
} else {
    Write-Host "`nИз-за ошибок монитор не запускался." -ForegroundColor Yellow
}

# ---------- 9. Итог ----------
Write-Step "Готово"
Write-Host "Проект : $workDir"
Write-Host "Логи   : $logDir"
if (Test-Path $errLog) {
    if ((Get-Item $errLog).Length -gt 0) {
        Write-Host "`n--- СОДЕРЖИМОЕ $errLog ---" -ForegroundColor Red
        Get-Content $errLog -Encoding UTF8 | ForEach-Object { Write-Host $_ -ForegroundColor Red }
        Write-Host "--- КОНЕЦ ---`n" -ForegroundColor Red
        exit 1
    } else {
        Write-Host "Ошибок нет." -ForegroundColor Green
    }
}
exit 0