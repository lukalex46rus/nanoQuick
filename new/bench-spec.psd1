@{
    # Общие настройки
    # Fqbn      = "arduino:avr:nano:cpu=atmega328"
		Fqbn      = "arduino:avr:nano:cpu=atmega328old"
		
    Baud      = 9600
    DefaultMonitorSec = 3

    Benches = @{
        bench_platform = @{
            MonitorSec = 2
            Patterns = @{
                chip     = 'chip\s*=\s*(\d+)'
                f_cpu    = 'F_CPU\s*=\s*(\d+)'
                sz_stat  = 'sz_stat\s*=\s*(\d+)'
                sz_res   = 'sz_res\s*=\s*(\d+)'
            }
            Expected = @{
                chip    = 328
                sz_stat = 1
                sz_res  = 3
            }
            Notes = "chip=328 для 328P; f_cpu=16000000 (5V) или 8000000 (3.3V)"
        }

        bench_status = @{
            MonitorSec = 2
            Patterns = @{
                size_status = 'size_status\s*=\s*(\d+)'
                size_res16  = 'size_res16\s*=\s*(\d+)'
                ok_ok       = 'ok_ok\s*=\s*(\d+)'
                err_ok      = 'err_ok\s*=\s*(\d+)'
                r_ok_ok     = 'r_ok_ok\s*=\s*(\d+)'
                r_ok_val    = 'r_ok_val\s*=\s*(\d+)'
                r_er_ok     = 'r_er_ok\s*=\s*(\d+)'
            }
            Expected = @{
                size_status = 1
                size_res16  = 3
                ok_ok       = 1
                err_ok      = 0
                r_ok_ok     = 1
                r_ok_val    = 1234
                r_er_ok     = 0
            }
            Notes = "static_assert на сборке — если не собралось, бенч не запустится"
        }

        bench_atomic = @{
            MonitorSec = 2
            Patterns = @{
                r32     = 'r32\s*=\s*(\d+)'
                r8      = 'r8\s*=\s*(\d+)'
                enum_rt = 'enum_rt\s*=\s*(\d+)'
                mono    = 'mono\s*=\s*(\d+)'
            }
            Expected = @{
                r32     = 3735928559   # 0xDEADBEEF
                r8      = 165          # 0xA5
                enum_rt = 2            # Timeout
                mono    = 1
            }
            Notes = "r32 — hex 0xDEADBEEF = 3735928559; static_assert trait — сборка = pass"
        }

        bench_pin_init = @{
            MonitorSec = 2
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
                portB_bef = 2
                ddrB_bef  = 0
                portB_aft = 0
                ddrB_aft  = 1
                pull_read = 1
                out_high  = 1
                tog_read  = 0
            }
            Notes = "portB_bef=2 (PB1 pull-up); portB_aft=0 (P=PB0 clr), ddrB_aft=1 (PB0 out)"
        }

        bench_mcu = @{
            MonitorSec = 2
            Patterns = @{
                out_before  = 'out_before\s*=\s*(\d+)'
                out_after   = 'out_after\s*=\s*(\d+)'
                mask_keep   = 'mask_keep\s*=\s*(\d+)'
                pull_before = 'pull_before\s*=\s*(\d+)'
                pull_after  = 'pull_after\s*=\s*(\d+)'
            }
            Expected = @{
                out_before  = 0
                out_after   = 1
                mask_keep   = 2
                pull_before = 1
                pull_after  = 0
            }
            Notes = "flip на входе переключает pull-up (pull_before=1, pull_after=0)"
        }

        bench_tick_confirm = @{
            MonitorSec = 5
            Patterns = @{
                TCCR2A  = 'TCCR2A\s*=\s*(\d+)'
                TCCR2B  = 'TCCR2B\s*=\s*(\d+)'
                OCR2A   = 'OCR2A\s*=\s*(\d+)'
                TIMSK2  = 'TIMSK2\s*=\s*(\d+)'
                counter = 'counter\s*=\s*(\d+)'
                core_ms = 'core_ms\s*=\s*(\d+)'
                diff    = 'diff\s*=\s*(-?\d+)'
            }
            Expected = @{
                TCCR2A = 2
                TCCR2B = 4
                OCR2A  = 249
                TIMSK2 = 2
            }
            Range = @{
                diff = @{ Min = -2; Max = 2 }
            }
            Notes = "diff в допуске ±2; counter монотонен"
        }

        bench_micros_confirm = @{
            MonitorSec = 3
            Patterns = @{
                ours_1k  = 'ours_1k\s*=\s*(\d+)'
                core_1k  = 'core_1k\s*=\s*(\d+)'
                span_our = 'span_our\s*=\s*(\d+)'
                span_cor = 'span_cor\s*=\s*(\d+)'
                span_dif = 'span_dif\s*=\s*(-?\d+)'
            }
            Range = @{
                span_dif = @{ Min = -5; Max = 5 }
            }
            Notes = "span_dif ~0; ours_1k < core_1k (наши дешевле)"
        }

        bench_subnow = @{
            MonitorSec = 2
            Patterns = @{
                step_1ms   = 'step_1ms\s*=\s*(\d+)'
                step_100us = 'step_100us\s*=\s*(\d+)'
                us_1000    = 'us\(1000\)\s*=\s*(\d+)'
            }
            Expected = @{
                step_1ms   = 250
                step_100us = 25
                us_1000    = 250
            }
            Notes = "16 МГц: 250/25/250; 8 МГц: 125/12/125 — править Expected под плату"
        }

        bench_deadline = @{
            MonitorSec = 3
            Patterns = @{
                dl_10ms_us = 'dl_10ms_us\s*=\s*(\d+)'
                left_1ms   = 'left_1ms\s*=\s*(\d+)'
                sd_100us   = 'sd_100us\s*=\s*(\d+)'
                sd_5us_up  = 'sd_5us_up\s*=\s*(\d+)'
            }
            Range = @{
                dl_10ms_us = @{ Min = 9900; Max = 10100 }
                sd_100us   = @{ Min = 96;   Max = 108 }
                sd_5us_up  = @{ Min = 4;    Max = 16 }
            }
            Notes = "sd_5us_up ~8 (16M) — проверка B1 (округление вверх); 4..16 — допуск"
        }

        bench_adc_base = @{
            MonitorSec = 3
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
            MonitorSec = 3
            Patterns = @{
                samples = 'samples\s*=\s*(\d+)'
                expect  = 'expect\s*=\s*(\d+)'
                per_us  = 'per_us\s*=\s*(\d+)'
                missed  = 'missed\s*=\s*(\d+)'
                last    = 'last\s*=\s*(\d+)'
            }
            Range = @{
                samples = @{ Min = 8500; Max = 9300 }   # ~8928 @16M
                per_us  = @{ Min = 105;  Max = 120 }
                last    = @{ Min = 200;  Max = 250 }
            }
            Info = @("missed")
            Notes = "missed — информационно (0 идеально, >0 допустимо)"
        }

        bench_adc_decimate = @{
            MonitorSec = 3
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
            MonitorSec = 3
            Patterns = @{
                isqrt_65000 = 'isqrt_65000\s*=\s*(\d+)'
                isqrt_max   = 'isqrt_max\s*=\s*(\d+)'
                synth_const = 'synth_const\s*=\s*(\d+)'
                synth_alt   = 'synth_alt\s*=\s*(\d+)'
                real_rms    = 'real_rms\s*=\s*(\d+)'
            }
            Expected = @{
                isqrt_65000 = 254
                isqrt_max   = 65535
                synth_const = 239
                synth_alt   = 254
            }
            Range = @{
                real_rms = @{ Min = 200; Max = 250 }
            }
            Notes = "synth_const=239 (DC), synth_alt=254 (sqrt(65000)); real_rms ~239"
        }

        bench_spi = @{
            MonitorSec = 3
            Patterns = @{
                SPCR4 = 'SPCR4\s*=\s*(\d+)'
                SPSR4 = 'SPSR4\s*=\s*(\d+)'
                us_4  = 'us_4\s*=\s*(\d+)'
                us_2  = 'us_2\s*=\s*(\d+)'
                rx4   = 'rx4\s*=\s*(\d+)'
                rx2   = 'rx2\s*=\s*(\d+)'
            }
            Expected = @{
                SPCR4 = 80   # 0x50
                SPSR4 = 0
            }
            Info = @("us_4", "us_2", "rx4", "rx2")
            Notes = "rx4=165, rx2=60 — только с джампером D11–D12; без него — плавающий/0"
        }

        bench_i2c = @{
            MonitorSec = 3
            Patterns = @{
                TWBR  = 'TWBR\s*=\s*(\d+)'
                ack   = 'ack\s*=\s*(\d+)'
                first = 'first\s*=\s*(\d+)'
                us    = 'us\s*=\s*(\d+)'
            }
            Expected = @{
                TWBR = 72
            }
            Info = @("ack", "first", "us")
            Notes = "ack >= 1 при устройстве на шине; first = адрес MCP (0x20..0x27)"
        }

        bench_i2c_reg = @{
            MonitorSec = 3
            Patterns = @{
                ping  = 'ping\s*=\s*(\d+)'
                wr00  = 'wr00\s*=\s*(\d+)'
                rd00  = 'rd00\s*=\s*(\d+)'
                val00 = 'val00\s*=\s*(\d+)'
                wrFF  = 'wrFF\s*=\s*(\d+)'
                rdFF  = 'rdFF\s*=\s*(\d+)'
                valFF = 'valFF\s*=\s*(\d+)'
            }
            Expected = @{
                ping  = 1
                wr00  = 1
                rd00  = 1
                val00 = 0
                wrFF  = 1
                rdFF  = 1
                valFF = 255
            }
            Notes = "MCP23017 @0x26; valFF=255 (IODIRA=0xFF)"
        }

        bench_i2c_rw = @{
            MonitorSec = 3
            Patterns = @{
                wr00  = 'wr00\s*=\s*(\d+)'
                rd00  = 'rd00\s*=\s*(\d+)'
                val00 = 'val00\s*=\s*(\d+)'
                wrFF  = 'wrFF\s*=\s*(\d+)'
                rdFF  = 'rdFF\s*=\s*(\d+)'
                valFF = 'valFF\s*=\s*(\d+)'
            }
            Expected = @{
                wr00  = 1
                rd00  = 1
                val00 = 0
                wrFF  = 1
                rdFF  = 1
            }
            Info = @("valFF")
            Notes = "pointer-less read: val00=0; valFF — информационно"
        }

        bench_probe = @{
            MonitorSec = 5
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
            Expected = @{
                TIMSK2 = 2
                TCCR2A = 2
                TCCR2B = 4
                OCR2A  = 249
                OCR2B  = 0
                TIFR2  = 0
            }
            Range = @{
                delta = @{ Min = 990; Max = 1010 }
            }
            Notes = "delta ~1000 за 1 с; TIFR2=0 в норме"
        }

        bench_tick = @{
            MonitorSec = 0     # без UART — визуально/осциллограф
            Patterns = @{}
            Notes = "toggle D8 — меандр 500 Гц; результат только осциллографом"
            Visual = $true
        }

        bench_gpio = @{
            MonitorSec = 0
            Patterns = @{}
            Notes = "LED D13 мигает; результат визуальный"
            Visual = $true
        }
    }
}