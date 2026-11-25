<#
.SYNOPSIS
    Сборка bench_*.ino с Arduino CLI 1.6.0.
    Жёсткая остановка при первой ошибке:
      compile → (fail? stop) → size/nm/search → upload → (fail? stop) → monitor.
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
$MONITOR_SEC = 65

$BenchDir = ".\bench"
$SrcDir   = ".\src"
$TmpRoot  = ".\tmp"
# ===============================================

$ErrorActionPreference = "Stop"
$script:Stop     = $false   # взведён при любой фатальной ошибке
$script:Stage    = "init"   # текущий этап (для итогового отчёта)

function Write-Step([string]$msg) {
    Write-Host ""
    Write-Host "=== $msg ===" -ForegroundColor Cyan
}

function Fail([string]$msg, [string]$errLog) {
    $script:Stop = $true
    $line = "[$([datetime]::Now)] ERROR [$($script:Stage)]: $msg"
    Write-Host $line -ForegroundColor Red
    if ($errLog) { Add-Content -Path $errLog -Value $line -Encoding UTF8 }
}

function Invoke-Stage {
    param(
        [string]$Name,
        [scriptblock]$Action,
        [string]$ErrLog
    )
    $script:Stage = $Name
    Write-Step $Name
    if ($script:Stop) { return $false }

    try {
        & $Action
        if ($LASTEXITCODE -ne 0) { throw "exit code $LASTEXITCODE" }
        return $true
    }
    catch {
        Fail "$_" $ErrLog
        return $false
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

$selected = $null
do {
    $answer = Read-Host "`nКакой bench_*.ino компилируем? (номер или имя)"
    $answer = $answer.Trim()

    if ($answer -match '^\d+$') {
        $idx = [int]$answer - 1
        if ($idx -ge 0 -and $idx -lt $sketches.Count) {
            $selected = $sketches[$idx]; break
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
$logDir    = Join-Path $workDir "logs"
$inoTarget = Join-Path $workDir $selected.Name
$srcTarget = Join-Path $workDir "src"

Write-Host "`nВыбран: $($selected.Name)" -ForegroundColor Green
Write-Host "Рабочая папка: $workDir"        -ForegroundColor Green

# ---------- 3. Подготовка рабочей папки ----------
$ok = Invoke-Stage "Подготовка '$workDir'" {
    if (Test-Path $workDir) { Remove-Item $workDir -Recurse -Force }
    New-Item -ItemType Directory -Path $workDir  | Out-Null
    New-Item -ItemType Directory -Path $buildDir | Out-Null
    New-Item -ItemType Directory -Path $logDir   | Out-Null
    Copy-Item -Path $selected.FullName -Destination $inoTarget -Force
    Copy-Item -Path $SrcDir -Destination $srcTarget -Recurse -Force
    Write-Host "OK: скетч и src скопированы в tmp" -ForegroundColor Green
} $null

# ---------- 4. Пути к логам ----------
$errLog      = Join-Path $logDir "error_$benchName.txt"
$monLog      = Join-Path $logDir "monitor_$benchName.txt"
$sizeLog     = Join-Path $logDir "size_$benchName.txt"
$nmLog       = Join-Path $logDir "nm_$benchName.txt"
$nmFilterLog = Join-Path $logDir "nmfilter_$benchName.txt"
$compileLog  = Join-Path $logDir "compile_$benchName.txt"

if ($ok) { "" | Set-Content -Path $errLog -Encoding UTF8 }

# ---------- 5. Компиляция ----------
if (-not $script:Stop) {
    Invoke-Stage "Компиляция ($FQBN)" {
        arduino-cli compile --fqbn $FQBN --build-path $buildDir $workDir 2>&1 |
            Tee-Object -FilePath $compileLog
    } $errLog | Out-Null
}

$elf = Join-Path $buildDir "$benchName.ino.elf"

# ---------- 6. Размеры + nm + поиск (только если компиляция ок) ----------
if (-not $script:Stop) {
    if (Test-Path $elf) {
        Invoke-Stage "Размер прошивки (avr-size)" {
            avr-size --format=avr --mcu=$MCU $elf 2>&1 | Tee-Object -FilePath $sizeLog
        } $errLog | Out-Null

        Invoke-Stage "Символы (avr-nm)" {
            avr-nm -S --size-sort -td $elf 2>&1 | Tee-Object -FilePath $nmLog
        } $errLog | Out-Null

        if (-not $script:Stop) {
            $script:Stage = "Поиск по символам"
            Write-Step "Поиск по символам (avr-nm | Select-String)"
            $pattern = Read-Host "Введите шаблон поиска (Enter = пропустить)"

            if ([string]::IsNullOrWhiteSpace($pattern)) {
                Write-Host "Поиск пропущен (пустой шаблон)." -ForegroundColor Yellow
            } else {
                Write-Host "Ищем: $pattern" -ForegroundColor Yellow
                Invoke-Stage "Поиск по символам" {
                    $matches = & avr-nm -S --size-sort -td $elf | Select-String $pattern
                    if ($matches) {
                        $matches | ForEach-Object { Write-Host $_.Line -ForegroundColor Green }
                        $matches | ForEach-Object { $_.Line } |
                            Set-Content -Path $nmFilterLog -Encoding UTF8
                    } else {
                        $emptyMsg = "# Совпадений по шаблону '$pattern' не найдено"
                        Write-Host $emptyMsg -ForegroundColor Yellow
                        Set-Content -Path $nmFilterLog -Value $emptyMsg -Encoding UTF8
                    }
                } $errLog | Out-Null
            }
        }
    } else {
        $script:Stage = "Размеры"
        Fail "ELF не найден: $elf" $errLog
    }
}

# ---------- 7. Прошивка (только если всё выше ок) ----------
$hex = Join-Path $buildDir "$benchName.ino.hex"
if (-not $script:Stop) {
    if (Test-Path $hex) {
        Invoke-Stage "Прошивка на $PORT" {
            arduino-cli upload --fqbn $FQBN -p $PORT --input-file $hex 2>&1
        } $errLog | Out-Null
    } else {
        $script:Stage = "Прошивка"
        Fail "HEX не найден: $hex" $errLog
    }
}

# ---------- 8. Монитор (только если прошивка ок) ----------
# ---------- 8. Монитор ----------
if (-not $script:Stop) {
    $script:Stage = "Монитор"

    # --- запрос длительности ---
    $monitorSec = $null   # $null = без ограничений
    do {
        $ans = Read-Host "Сколько секунд работать монитору? (Enter = без ограничений)"
        $ans = $ans.Trim()
        if ([string]::IsNullOrEmpty($ans)) {
            $monitorSec = $null
            break
        }
        if ($ans -match '^\d+$' -and [int]$ans -gt 0) {
            $monitorSec = [int]$ans
            break
        }
        Write-Host "Введите положительное целое число или оставьте пустым." -ForegroundColor Red
    } while ($true)

    if ($null -eq $monitorSec) {
        Write-Host "`n=== Монитор порта $PORT ($BAUD бод, без ограничений) -> $monLog ===" -ForegroundColor Cyan
    } else {
        Write-Host "`n=== Монитор порта $PORT ($BAUD бод, $monitorSec сек) -> $monLog ===" -ForegroundColor Cyan
    }

    $cliHelp          = & arduino-cli monitor --help 2>&1 | Out-String
    $supportsDuration = $cliHelp -match '--duration'

    $monArgs = @("monitor", "-p", $PORT, "--config", "baudrate=$BAUD", "--timestamp")
    if ($monitorSec -ne $null -and $supportsDuration) {
        $monArgs += @("--duration", "${monitorSec}s")
        Write-Host "Используем --duration ${monitorSec}s" -ForegroundColor Yellow
    } elseif ($monitorSec -ne $null) {
        Write-Host "Флаг --duration не поддерживается — остановим монитор по таймауту" -ForegroundColor Yellow
    } else {
        Write-Host "Ограничение по времени не задано. Остановить: Ctrl+C." -ForegroundColor Yellow
    }

    $monErrTmp = Join-Path $logDir "monitor_stderr.tmp"

    $proc = Start-Process -FilePath "arduino-cli" `
        -ArgumentList $monArgs `
        -RedirectStandardOutput $monLog `
        -RedirectStandardError  $monErrTmp `
        -PassThru -NoNewWindow

    $killedByUs = $false

    if ($null -eq $monitorSec) {
        # --- без ограничения: ждём, пока пользователь нажмёт Ctrl+C или процесс умрёт сам ---
        Write-Host "Монитор работает... нажмите Ctrl+C для остановки." -ForegroundColor Yellow
        try {
            # Ждём вечно, пока процесс жив; если пользователь жмёт Ctrl+C —
            # PowerShell прерывает ожидание, и мы попадаем в finally/catch.
            while (-not $proc.HasExited) {
                Start-Sleep -Milliseconds 250
            }
        }
        catch [System.Management.Automation.Host.HostException] {
            Write-Host "`nПрервано пользователем (Ctrl+C)." -ForegroundColor Yellow
        }
        catch {
            Write-Host "`nПрерывание: $($_.Exception.Message)" -ForegroundColor Yellow
        }
        finally {
            if (-not $proc.HasExited) {
                Write-Host "Останавливаем монитор..." -ForegroundColor Yellow
                $killedByUs = $true
                try { $proc.Kill() } catch {}
            }
        }
    } else {
        # --- с ограничением: ждём N секунд (или duration+10, если поддерживается --duration) ---
        if ($supportsDuration) {
            $deadline = (Get-Date).AddSeconds($monitorSec + 10)
        } else {
            $deadline = (Get-Date).AddSeconds($monitorSec)
        }
        while (((Get-Date) -lt $deadline) -and (-not $proc.HasExited)) {
            Start-Sleep -Milliseconds 250
        }
        if (-not $proc.HasExited) {
            Write-Host "Останавливаем монитор..." -ForegroundColor Yellow
            $killedByUs = $true
            try { $proc.Kill() } catch {}
        }
    }

    $proc.WaitForExit()

    # --- разбор stderr ---
    if (Test-Path $monErrTmp) {
        $stderr = Get-Content $monErrTmp -Raw
        Remove-Item $monErrTmp -Force

        if ($stderr -and $stderr.Trim()) {
            Add-Content -Path (Join-Path $logDir "monitor_stderr.txt") `
                        -Value $stderr -Encoding UTF8

            $expectedKill = $stderr -match 'I/O operation has been aborted' `
                        -or $stderr -match 'serial port closed' `
                        -or $stderr -match 'The operation was canceled' `
                        -or $stderr -match 'Access is denied' `
                        -or $stderr -match 'broken pipe' `
                        -or $stderr -match 'EOF'
            $expectedMissing = $stderr -match 'No monitor available' `
                            -or $stderr -match 'not found'

            if ($killedByUs -and $expectedKill) {
                Write-Host "Монитор остановлен принудительно (штатно)." -ForegroundColor Yellow
            }
            elseif ($expectedMissing) {
                Fail "Монитор не смог стартовать: $stderr" $errLog
            }
            else {
                Fail "Монитор завершился с ошибкой (exit=$($proc.ExitCode)): $stderr" $errLog
            }
        }
    }

    if (-not $script:Stop) {
        if ($killedByUs) {
            if ($null -eq $monitorSec) {
                Write-Host "Монитор остановлен пользователем. Лог: $monLog" -ForegroundColor Green
            } else {
                Write-Host "Монитор завершён (таймаут $monitorSec сек). Лог: $monLog" -ForegroundColor Green
            }
        } elseif ($proc.ExitCode -eq 0) {
            Write-Host "Монитор завершился штатно. Лог: $monLog" -ForegroundColor Green
        } else {
            Fail "Монитор завершился с кодом $($proc.ExitCode)" $errLog
        }
    }
}

# ---------- 9. Итог ----------
Write-Step "Готово"
Write-Host "Проект : $workDir"
Write-Host "Логи   : $logDir"

if ($script:Stop) {
    Write-Host "`nСБОРКА ОСТАНОВЛЕНА на этапе: $($script:Stage)" -ForegroundColor Red
    if ((Test-Path $errLog) -and (Get-Item $errLog).Length -gt 0) {
        Write-Host "--- СОДЕРЖИМОЕ $errLog ---" -ForegroundColor Red
        Get-Content $errLog -Encoding UTF8 | ForEach-Object { Write-Host $_ -ForegroundColor Red }
        Write-Host "--- КОНЕЦ ---`n" -ForegroundColor Red
    }
    exit 1
}

Write-Host "Ошибок нет. Все этапы пройдены." -ForegroundColor Green
exit 0