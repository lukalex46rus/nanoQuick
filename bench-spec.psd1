@{
    # Fqbn: выберите под ваш bootloader:
    #   New bootloader (большинство клонов, официальные Nano post-2018): arduino:avr:nano:cpu=atmega328
    #   Old bootloader (старые Nano, некоторые китайские):               arduino:avr:nano:cpu=atmega328old
    # При UPLOAD_FAIL проверьте соответствие. Bootloader old не прошивается с cpu=atmega328 и наоборот.
    
    Fqbn              = "arduino:avr:nano:cpu=atmega328old"
    Baud              = 9600
    DefaultHardCapSec = 20
    DefaultGraceSec   = 1

    Benches = @{
        bench_platform = @{
            MonitorMode = "WaitAll"; HardCapSec = 20; GraceSec = 1
            Auto = $true; Nm = $false
            Patterns = @{
                chip    = 'chip\s*=\s*(\d+)'
                f_cpu   = 'F_CPU\s*=\s*(\d+)'
                sz_stat = 'sz_stat\s*=\s*(\d+)'
                sz_res  = 'sz_res\s*=\s*(\d+)'
            }
            Expected = @{ chip = 328; sz_stat = 1; sz_res = 3 }
            Notes = "chip=328 для 328P; f_cpu=16000000 (5V) или 8000000 (3.3V)"
        }
        bench_status = @{
            MonitorMode = "WaitAll"; HardCapSec = 20; GraceSec = 1
            Auto = $true; Nm = $false
            Patterns = @{
                size_status = 'size_status\s*=\s*(\d+)'
                size_res16  = 'size_res16\s*=\s*(\d+)'
                ok_ok       = 'ok_ok\s*=\s*(\d+)'
                err_ok      = 'err_ok\s*=\s*(\d+)'
                r_ok_ok     = 'r_ok_ok\s*=\s*(\d+)'
                r_ok_val    = 'r_ok_val\s*=\s*(\d+)'
                r_er_ok     = 'r_er_ok\s*=\s*(\d+)'
            }
            Expected = @{ size_status = 1; size_res16 = 3; ok_ok = 1; err_ok = 0; r_ok_ok = 1; r_ok_val = 1234; r_er_ok = 0 }
            Notes = "static_assert на сборке — если не собралось, бенч не запустится"
        }
        bench_atomic = @{
            MonitorMode = "WaitAll"; HardCapSec = 20; GraceSec = 1
            Auto = $true; Nm = $false
            Patterns = @{
                r32     = 'r32\s*=\s*(\d+)'
                r8      = 'r8\s*=\s*(\d+)'
                enum_rt = 'enum_rt\s*=\s*(\d+)'
                mono    = 'mono\s*=\s*(\d+)'
            }
            Expected = @{ r32 = 3735928559; r8 = 165; enum_rt = 2; mono = 1 }
            Notes = "r32 = 0xDEADBEEF; static_assert trait — сборка = pass"
        }
        bench_pin_init = @{
            MonitorMode = "WaitAll"; HardCapSec = 20; GraceSec = 1
            Auto = $true; Nm = $false
            Patterns = @{
                portB_bef = 'portB_bef\s*=\s*(\d+)'
                ddrB_bef  = 'ddrB_bef\s*=\s*(\d+)'
                portB_aft = 'portB_aft\s*=\s*(\d+)'
                ddrB_aft  = 'ddrB_aft\s*=\s*(\d+)'
                pull_read = 'pull_read\s*=\s*(\d+)'
                out_high  = 'out_high\s*=\s*(\d+)'
                tog_read  = 'tog_read\s*=\s*(\d+)'
            }
            Expected = @{
                portB_bef = 2; ddrB_bef = 0
                portB_aft = 2          # PB1 pull-up жив, PB0 clr (маска 0x03 в бенче)
                ddrB_aft  = 1
                pull_read = 1; out_high = 1; tog_read = 0
            }
            Notes = "маски 0x03 в бенче: PB5/D13 трогает bootloader; PORTB-защёлка не PINB (синхронизатор)"
        }
        bench_mcu = @{
            MonitorMode = "WaitAll"; HardCapSec = 20; GraceSec = 1
            Auto = $true; Nm = $false
            Patterns = @{
                out_before  = 'out_before\s*=\s*(\d+)'
                out_after   = 'out_after\s*=\s*(\d+)'
                mask_keep   = 'mask_keep\s*=\s*(\d+)'
                pull_before = 'pull_before\s*=\s*(\d+)'
                pull_after  = 'pull_after\s*=\s*(\d+)'
            }
            Expected = @{ out_before = 0; out_after = 1; mask_keep = 2; pull_before = 1; pull_after = 0 }
            Notes = "читаем PORTB (защёлку), не PINB: синхронизатор PINx отстаёт 0.5..1.5 такта"
        }
        bench_tick_confirm = @{
            MonitorMode = "WaitSec"; WaitSec = 60
            Auto = $true; Nm = $false
            Patterns = @{
                TCCR2A  = 'TCCR2A\s*=\s*(\d+)'
                TCCR2B  = 'TCCR2B\s*=\s*(\d+)'
                OCR2A   = 'OCR2A\s*=\s*(\d+)'
                TIMSK2  = 'TIMSK2\s*=\s*(\d+)'
                counter = 'counter\s*=\s*(\d+)'
                core_ms = 'core_ms\s*=\s*(\d+)'
                diff    = 'diff\s*=\s*(-?\d+)'
            }
            Expected = @{ TCCR2A = 2; TCCR2B = 4; OCR2A = 249; TIMSK2 = 2 }
            Range = @{ diff = @{ Min = -2; Max = 2 } }
            Notes = "60 с — дрейф counter vs millis(); парсер берёт последний diff"
        }
        bench_micros_confirm = @{
            MonitorMode = "WaitAll"; HardCapSec = 10; GraceSec = 1
            Auto = $true; Nm = $false
            Patterns = @{
                ours_1k  = 'ours_1k\s*=\s*(\d+)'
                core_1k  = 'core_1k\s*=\s*(\d+)'
                span_our = 'span_our\s*=\s*(\d+)'
                span_cor = 'span_cor\s*=\s*(\d+)'
                span_dif = 'span_dif\s*=\s*(-?\d+)'
            }
            Range = @{ span_dif = @{ Min = -5; Max = 5 } }
            Notes = "span_dif ~0; ours_1k < core_1k"
        }
        bench_subnow = @{
            MonitorMode = "WaitAll"; HardCapSec = 20; GraceSec = 1
            Auto = $true; Nm = $false
            Patterns = @{
                step_1ms   = 'step_1ms\s*=\s*(\d+)'
                step_100us = 'step_100us\s*=\s*(\d+)'
                us_1000    = 'us\(1000\)\s*=\s*(\d+)'
            }
            Expected = @{ us_1000 = 250 }
            Range = @{
                step_1ms   = @{ Min = 248; Max = 260 }   # delay(1) квант Timer0 + overhead
                step_100us = @{ Min = 24;  Max = 30 }    # delayMicroseconds + 2×subNow overhead
            }
            Notes = "16 МГц; us(1000)=250 точно; шаги времени — Range (jitter). 8 МГц: править под плату"
        }
        bench_deadline = @{
            MonitorMode = "WaitAll"
            HardCapSec  = 20
            GraceSec    = 1
            Auto        = $true; Nm = $false
            Patterns = @{
                dl_10ms_us  = 'dl_10ms_us\s*=\s*(\d+)'
                left_1ms    = 'left_1ms\s*=\s*(\d+)'
                sd_100us    = 'sd_100us\s*=\s*(\d+)'
                sd_5us_up   = 'sd_5us_up\s*=\s*(\d+)'
                sd_65535us  = 'sd_65535us\s*=\s*(\d+)'
            }
            Range = @{
                dl_10ms_us  = @{ Min = 9900; Max = 10100 }
                sd_100us    = @{ Min = 96;   Max = 108 }
                sd_5us_up   = @{ Min = 4;    Max = 16 }
                sd_65535us  = @{ Min = 64000; Max = 67000 }   # B1: 32-bit round-up, не 0/8 баг 16-bit дал бы ~0 -> FAIL
            }
            Notes = "sd_5us_up ~8 (округление вверх); sd_65535us ~65536 — B1 защита от wrap us_+3 (16-bit баг -> ~0)"
        }
        bench_adc_base = @{
            MonitorMode = "WaitAll"; HardCapSec = 20; GraceSec = 1
            Auto = $true; Nm = $false
            Patterns = @{
                us_slow = 'us_slow\s*=\s*(\d+)'
                us_fast = 'us_fast\s*=\s*(\d+)'
                gnd_s   = 'gnd_s\s*=\s*(\d+)'
                bg_s    = 'bg_s\s*=\s*(\d+)'
                gnd_f   = 'gnd_f\s*=\s*(\d+)'
                bg_f    = 'bg_f\s*=\s*(\d+)'
            }
            Range = @{
                us_slow = @{ Min = 100; Max = 112 }
                us_fast = @{ Min = 12;  Max = 18 }
                gnd_s   = @{ Min = 0;   Max = 3 }
                gnd_f   = @{ Min = 0;   Max = 3 }
                bg_s    = @{ Min = 200; Max = 250 }
                bg_f    = @{ Min = 200; Max = 250 }
            }
            Notes = "us_slow ~104..110; us_fast ~13..16; bg ~225 при AVcc 5V"
        }
        bench_adc_stream = @{
            MonitorMode = "WaitAll"; HardCapSec = 20; GraceSec = 1
            Auto = $true; Nm = $false
            Patterns = @{
                samples = 'samples\s*=\s*(\d+)'
                expect  = 'expect\s*=\s*(\d+)'
                per_us  = 'per_us\s*=\s*(\d+)'
                missed  = 'missed\s*=\s*(\d+)'
                last    = 'last\s*=\s*(\d+)'
            }
            Range = @{
                samples = @{ Min = 8500; Max = 9300 }
                per_us  = @{ Min = 105;  Max = 120 }
                last    = @{ Min = 200;  Max = 250 }
            }
            Info = @("missed")
            Notes = "missed — информационно (0 идеально, >0 допустимо)"
        }
        bench_adc_decimate = @{
            MonitorMode = "WaitAll"; HardCapSec = 20; GraceSec = 1
            Auto = $true; Nm = $false
            Patterns = @{
                raw_min    = 'raw_min\s*=\s*(\d+)'
                raw_max    = 'raw_max\s*=\s*(\d+)'
                dec_min    = 'dec_min\s*=\s*(\d+)'
                dec_max    = 'dec_max\s*=\s*(\d+)'
                avg_min    = 'avg_min\s*=\s*(\d+)'
                avg_max    = 'avg_max\s*=\s*(\d+)'
                raw_spread = 'raw_spread\s*=\s*(\d+)'
                dec_spread = 'dec_spread\s*=\s*(\d+)'
                dec_count  = 'dec_count\s*=\s*(\d+)'
            }
            Range = @{
                raw_min = @{ Min = 230; Max = 245 }
                raw_max = @{ Min = 230; Max = 245 }
                dec_min = @{ Min = 920; Max = 985 }
                dec_max = @{ Min = 920; Max = 985 }
                avg_min = @{ Min = 230; Max = 245 }
                avg_max = @{ Min = 230; Max = 245 }
            }
            Info = @("raw_spread", "dec_spread", "dec_count")
            Notes = "dec_min ~955 (про-тир, 12-bit); avg_min ~238 (средний, 10-bit)"
        }
        bench_rms = @{
            MonitorMode = "WaitAll"; HardCapSec = 20; GraceSec = 1
            Auto = $true; Nm = $false
            Patterns = @{
                isqrt_65000 = 'isqrt_65000\s*=\s*(\d+)'
                isqrt_max   = 'isqrt_max\s*=\s*(\d+)'
                synth_const = 'synth_const\s*=\s*(\d+)'
                synth_alt   = 'synth_alt\s*=\s*(\d+)'
                real_rms    = 'real_rms\s*=\s*(\d+)'
            }
            Expected = @{ isqrt_65000 = 254; isqrt_max = 65535; synth_const = 239; synth_alt = 254 }
            Range = @{ real_rms = @{ Min = 200; Max = 250 } }
            Notes = "synth_const=239 (DC), synth_alt=254 (sqrt(65000)); real_rms ~239"
        }
        bench_spi = @{
            MonitorMode = "WaitAll"; HardCapSec = 20; GraceSec = 1
            Auto = $false; Nm = $true
            Requires = "jumper D11-D12 (MOSI-MISO loopback)"
            Patterns = @{
                SPCR4 = 'SPCR4\s*=\s*(\d+)'
                SPSR4 = 'SPSR4\s*=\s*(\d+)'
                us_4  = 'us_4\s*=\s*(\d+)'
                us_2  = 'us_2\s*=\s*(\d+)'
                rx4   = 'rx4\s*=\s*(\d+)'
                rx2   = 'rx2\s*=\s*(\d+)'
            }
            Expected = @{ SPCR4 = 80; SPSR4 = 0 }
            Info = @("us_4", "us_2", "rx4", "rx2")
            Notes = "rx4=165, rx2=60 — только с джампером D11–D12; без него — плавающий/0"
        }
        bench_uart = @{
            MonitorMode = "WaitAll"; HardCapSec = 20; GraceSec = 1
            Auto = $true; Nm = $false
            Patterns = @{
                ubrr     = 'ubrr\s*=\s*(\d+)'
                u2x      = 'u2x\s*=\s*(\d+)'
                err_p100 = 'err_p100\s*=\s*(\d+)'
                tx_us    = 'tx_us\s*=\s*(\d+)'
                lb_ok    = 'lb_ok\s*=\s*(\d+)'
                lb_val   = 'lb_val\s*=\s*(\d+)'
            }
            Expected = @{ u2x = 1 }
            Range = @{
                ubrr     = @{ Min = 16; Max = 17 }
                err_p100 = @{ Min = 0;  Max = 250 }
                tx_us    = @{ Min = 80; Max = 100 }
            }
            Info = @("lb_ok", "lb_val")
            Notes = "115200 @16M: UBRR=16 U2X=1 err~2.12%; TX без джампера PASS; lb_ok/lb_val=1/165 с джампером D0-D1 (СНЯТЬ перед прошивкой!)"
        }
        bench_softuart = @{
            MonitorMode = "WaitAll"
            HardCapSec  = 20
            GraceSec    = 1
            Auto        = $true; Nm = $false
            Patterns = @{
                tx_us  = 'tx_us\s*=\s*(\d+)'
                lb_ok  = 'lb_ok\s*=\s*(\d+)'
                lb_val = 'lb_val\s*=\s*(\d+)'
                pat_ok = 'pat_ok\s*=\s*(\d+)'
            }
            Range = @{
                tx_us = @{ Min = 950; Max = 1150 }   # ~1041 @9600/16M, допуск на overhead
            }
            Info = @("lb_ok", "lb_val", "pat_ok")
            Notes = "TX без джампера PASS (tx_us); lb/pat — только с джампером D2-D3; калибровка TX_OVERHEAD по tx_us"
        }
        bench_i2c = @{
            MonitorMode = "WaitAll"; HardCapSec = 20; GraceSec = 1
            Auto = $false; Nm = $false
            Requires = "MCP23017 @0x26 (A2/A1/A0=110) or other I2C device"
            Patterns = @{
                TWBR  = 'TWBR\s*=\s*(\d+)'
                ack   = 'ack\s*=\s*(\d+)'
                first = 'first\s*=\s*(\d+)'
                us    = 'us\s*=\s*(\d+)'
            }
            Expected = @{ TWBR = 72 }
            Info = @("ack", "first", "us")
            Notes = "ack>=1 при устройстве; ack=0 = MCP не подключён (стенд), не баг кода; first=0x26=38"
        }
        bench_i2c_reg = @{
            MonitorMode = "WaitAll"; HardCapSec = 20; GraceSec = 1
            Auto = $false; Nm = $false
            Requires = "MCP23017 @0x26 (A2/A1/A0=110)"
            Patterns = @{
                ping  = 'ping\s*=\s*(\d+)'
                wr00  = 'wr00\s*=\s*(\d+)'
                rd00  = 'rd00\s*=\s*(\d+)'
                val00 = 'val00\s*=\s*(\d+)'
                wrFF  = 'wrFF\s*=\s*(\d+)'
                rdFF  = 'rdFF\s*=\s*(\d+)'
                valFF = 'valFF\s*=\s*(\d+)'
            }
            Expected = @{ ping = 1; wr00 = 1; rd00 = 1; val00 = 0; wrFF = 1; rdFF = 1; valFF = 255 }
            Notes = "MCP23017 @0x26; valFF=255 (IODIRA=0xFF)"
        }
        bench_i2c_rw = @{
            MonitorMode = "WaitAll"; HardCapSec = 20; GraceSec = 1
            Auto = $false; Nm = $false
            Requires = "MCP23017 @0x26 (A2/A1/A0=110)"
            Patterns = @{
                wr00  = 'wr00\s*=\s*(\d+)'
                rd00  = 'rd00\s*=\s*(\d+)'
                val00 = 'val00\s*=\s*(\d+)'
                wrFF  = 'wrFF\s*=\s*(\d+)'
                rdFF  = 'rdFF\s*=\s*(\d+)'
                valFF = 'valFF\s*=\s*(\d+)'
            }
            Expected = @{ wr00 = 1; rd00 = 1; val00 = 0; wrFF = 1; rdFF = 1 }
            Info = @("valFF")
            Notes = "pointer-less read: val00=0; valFF — информационно"
        }
        bench_mcp23017 = @{
            MonitorMode = "WaitAll"
            HardCapSec  = 20
            GraceSec    = 1
            Auto        = $false
            Nm          = $false
            Requires    = "MCP23017 @0x26 (A2/A1/A0=110)"
            Patterns = @{
                init_ok  = 'init_ok\s*=\s*(\d+)'
                pu_read  = 'pu_read\s*=\s*(\d+)'
                dir_ok   = 'dir_ok\s*=\s*(\d+)'
                wr_ok    = 'wr_ok\s*=\s*(\d+)'
                olat_val = 'olat_val\s*=\s*(\d+)'
                gpio_val = 'gpio_val\s*=\s*(\d+)'
                zero_ok  = 'zero_ok\s*=\s*(\d+)'
                back_in  = 'back_in\s*=\s*(\d+)'
                back_dir = 'back_dir\s*=\s*(\d+)'
                back_pu  = 'back_pu\s*=\s*(\d+)'
            }
            Expected = @{
                init_ok  = 1
                dir_ok   = 1
                wr_ok    = 1
                olat_val = 43981     # 0xABCD — защёлка, не зависит от нагрузки
                zero_ok  = 1
                back_in  = 1
                back_dir = 255       # реальная проверка IODIRA
                back_pu  = 255       # реальная проверка GPPUA
            }
            Range = @{
                pu_read = @{ Min = 65000; Max = 65535 }   # обвес может тянуть 1-2 пина
        }
        Info = @("gpio_val")
        Notes = "100 кГц; pu_read Range (обвес); olat_val — защёлка (детерминированно); gpio_val=OLAT только без нагрузки; init не сбрасывает OLAT — бенч делает write16(0) перед финальным init"
        }
        bench_debounce = @{
            MonitorMode = "WaitAll"
            HardCapSec  = 20
            GraceSec    = 1
            Auto        = $true
            Nm          = $false
            Patterns = @{
                bounce_rose = 'bounce_rose\s*=\s*(\d+)'
                bounce_fell = 'bounce_fell\s*=\s*(\d+)'
                glitch_rose = 'glitch_rose\s*=\s*(\d+)'
                clean_rose  = 'clean_rose\s*=\s*(\d+)'
                clean_fell  = 'clean_fell\s*=\s*(\d+)'
                stable_end  = 'stable_end\s*=\s*(\d+)'
            }
            Expected = @{
                bounce_rose = 1
                bounce_fell = 1
                glitch_rose = 0
                clean_rose  = 1
                clean_fell  = 1
                stable_end  = 0
            }
            Notes = "синтетика, железо не нужно; дребезг 5×2мс подавлен MS=20; глюк 5мс не проходит"
        }
        bench_hd44780 = @{
            MonitorMode = "WaitAll"
            HardCapSec  = 20
            GraceSec    = 1
            Auto        = $false
            Nm          = $false
            Requires    = "LCD HD44780 20x4: RS=D2 EN=D3 D4=D4 D5=D5 D6=D6 D7=D7, RW=GND"
            Patterns = @{
                init_us  = 'init_us\s*=\s*(\d+)'
                cmd_us   = 'cmd_us\s*=\s*(\d+)'
                clear_us = 'clear_us\s*=\s*(\d+)'
                visual   = 'visual\s*=\s*(\d+)'
            }
            Range = @{
                init_us  = @{ Min = 45000; Max = 65000 }
                cmd_us   = @{ Min = 40;    Max = 120 }
                clear_us = @{ Min = 1900;  Max = 2400 }
            }
            Info = @("visual")
            Notes = "UART-часть проверяет тайминги; текст на 4 строках — визуально. RW=GND обязателен (write-only)"
        }
        bench_dfplayer = @{
            MonitorMode = "WaitAll"
            HardCapSec  = 20
            GraceSec    = 1
            Auto        = $true
            Nm          = $false
            Patterns = @{
                chk_vol  = 'chk_vol\s*=\s*(\d+)'
                bad_vol  = 'bad_vol\s*=\s*(\d+)'
                bad_trk  = 'bad_trk\s*=\s*(\d+)'
                send_ok  = 'send_ok\s*=\s*(\d+)'
                frame_us = 'frame_us\s*=\s*(\d+)'
            }
            Expected = @{
                chk_vol = 65254      # 0xFEE6: cmd=0x06 param=15
                bad_vol = 1
                bad_trk = 1
                send_ok = 1
            }
            Range = @{
                frame_us = @{ Min = 9000; Max = 13000 }   # 10 байт × ~1041 мкс @9600
            }
            Notes = "протокол+валидация без железа PASS; звук — на слух (DFPlayer TX=D8 через 1кОм, SD с MP3/0001.mp3); пауза 30мс между командами"
        }
        bench_probe = @{
            MonitorMode = "WaitSec"; WaitSec = 60
            Auto = $true; Nm = $false
            Patterns = @{
                TIMSK2  = 'TIMSK2\s*=\s*(\d+)'
                TCCR2A  = 'TCCR2A\s*=\s*(\d+)'
                TCCR2B  = 'TCCR2B\s*=\s*(\d+)'
                OCR2A   = 'OCR2A\s*=\s*(\d+)'
                OCR2B   = 'OCR2B\s*=\s*(\d+)'
                counter = 'counter\s*=\s*(\d+)'
                delta   = 'delta\s*=\s*(\d+)'
                core_ms = 'core_ms\s*=\s*(\d+)'
                TIFR2   = 'TIFR2\s*=\s*(\d+)'
            }
            Expected = @{ TIMSK2 = 2; TCCR2A = 2; TCCR2B = 4; OCR2A = 249; OCR2B = 0 }
            Range = @{
                delta = @{ Min = 990; Max = 1010 }
                TIFR2 = @{ Min = 0; Max = 4 }   # OCF2B=4 при OCR2B=0 (core) — норма, OCF2A не висит
            }
            Notes = "60 с; парсер берёт последний delta; TIFR2: OCF2B допустим (OCR2B=0 от core)"
        }
        bench_tick = @{
            MonitorMode = "Visual"; Auto = $true; Nm = $false
            Notes = "toggle D8 — меандр 500 Гц; результат только осциллографом"
        }
        bench_gpio = @{
            MonitorMode = "Visual"; Auto = $true; Nm = $false
            Notes = "LED D13 мигает; результат визуальный"
        }
    }
}