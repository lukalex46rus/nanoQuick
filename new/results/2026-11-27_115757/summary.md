# ResultsBench — 2026-11-27 12:02:01

- commit: `nogit`
- fqbn: `arduino:avr:nano:cpu=atmega328old`
- port: `COM3`

| Bench | Status | Flash | SRAM | Details | Notes |
|-------|--------|-------|------|---------|-------|
| bench_adc_base | FAIL | 1802 | 228 | us_fast=MISSING(range=12..18), gnd_f=MISSING(range=0..3), us_slow=MISSING(range=100..112), gnd_s=MISSING(range=0..3), bg_f=MISSING(range=200..250), bg_s=MISSING(range=200..250) | us_slow ~104..110; us_fast ~13..16; bg ~225 при AVcc 5V |
| bench_adc_decimate | FAIL | 2202 | 281 | raw_min=MISSING(range=230..245), avg_max=MISSING(range=230..245), raw_max=MISSING(range=230..245), avg_min=MISSING(range=230..245), dec_max=MISSING(range=920..985), dec_min=MISSING(range=920..985), raw_spread=MISSING(info), dec_spread=MISSING(info), dec_count=MISSING(info) | dec_min ~955 (про-тир, 12-bit); avg_min ~238 (средний, 10-bit) |
| bench_adc_stream | FAIL | 1796 | 229 | samples=MISSING(range=8500..9300), last=MISSING(range=200..250), per_us=MISSING(range=105..120), missed=MISSING(info) | missed — информационно (0 идеально, >0 допустимо) |
| bench_atomic | FAIL | 1300 | 206 | enum_rt=MISSING(exp=2), r32=MISSING(exp=3735928559), r8=MISSING(exp=165), mono=MISSING(exp=1) | r32 — hex 0xDEADBEEF = 3735928559; static_assert trait — сборка = pass |
| bench_deadline | FAIL | 2602 | 220 | sd_5us_up=MISSING(range=4..16), dl_10ms_us=MISSING(range=9900..10100), sd_100us=MISSING(range=96..108) | sd_5us_up ~8 (16M) — проверка B1 (округление вверх); 4..16 — допуск |
| bench_gpio | VISUAL | 604 | 142 | visual | LED D13 мигает; результат визуальный |
| bench_i2c_reg | FAIL | 2318 | 234 | rd00=MISSING(exp=1), wrFF=MISSING(exp=1), valFF=MISSING(exp=255), val00=MISSING(exp=0), ping=MISSING(exp=1), wr00=MISSING(exp=1), rdFF=MISSING(exp=1) | MCP23017 @0x26; valFF=255 (IODIRA=0xFF) |
| bench_i2c_rw | FAIL | 2066 | 230 | rd00=MISSING(exp=1), wrFF=MISSING(exp=1), wr00=MISSING(exp=1), val00=MISSING(exp=0), rdFF=MISSING(exp=1), valFF=MISSING(info) | pointer-less read: val00=0; valFF — информационно |
| bench_i2c | FAIL | 2028 | 206 | TWBR=MISSING(exp=72), ack=MISSING(info), first=MISSING(info), us=MISSING(info) | ack >= 1 при устройстве на шине; first = адрес MCP (0x20..0x27) |
| bench_mcu | FAIL | 1054 | 234 | out_before=MISSING(exp=0), pull_after=MISSING(exp=0), out_after=MISSING(exp=1), pull_before=MISSING(exp=1), mask_keep=MISSING(exp=2) | flip на входе переключает pull-up (pull_before=1, pull_after=0) |
| bench_micros_confirm | FAIL | 2004 | 226 | span_dif=MISSING(range=-5..5) | span_dif ~0; ours_1k < core_1k (наши дешевле) |
| bench_pin_init | FAIL | 1276 | 252 | ddrB_aft=MISSING(exp=1), out_high=MISSING(exp=1), tog_read=MISSING(exp=0), portB_bef=MISSING(exp=2), ddrB_bef=MISSING(exp=0), portB_aft=MISSING(exp=0), pull_read=MISSING(exp=1) | portB_bef=2 (PB1 pull-up); portB_aft=0 (P=PB0 clr), ddrB_aft=1 (PB0 out) |
| bench_platform | FAIL | 1102 | 204 | sz_res=MISSING(exp=3), chip=MISSING(exp=328), sz_stat=MISSING(exp=1) | chip=328 для 328P; f_cpu=16000000 (5V) или 8000000 (3.3V) |
| bench_probe | FAIL | 1604 | 274 | TIFR2=MISSING(exp=0), TCCR2A=2, TCCR2B=4, OCR2A=249, TIMSK2=2, OCR2B=MISSING(exp=0), delta=MISSING(range=990..1010) | delta ~1000 за 1 с; TIFR2=0 в норме |
| bench_rms | FAIL | 2180 | 243 | synth_const=MISSING(exp=239), isqrt_65000=MISSING(exp=254), synth_alt=MISSING(exp=254), isqrt_max=MISSING(exp=65535), real_rms=MISSING(range=200..250) | synth_const=239 (DC), synth_alt=254 (sqrt(65000)); real_rms ~239 |
| bench_spi | FAIL | 1696 | 218 | SPCR4=MISSING(exp=80), SPSR4=MISSING(exp=0), us_4=MISSING(info), us_2=MISSING(info), rx4=MISSING(info), rx2=MISSING(info) | rx4=165, rx2=60 — только с джампером D11–D12; без него — плавающий/0 |
| bench_status | FAIL | 1206 | 264 | size_status=MISSING(exp=1), err_ok=MISSING(exp=0), ok_ok=MISSING(exp=1), r_ok_ok=MISSING(exp=1), r_er_ok=MISSING(exp=0), size_res16=MISSING(exp=3), r_ok_val=MISSING(exp=1234) | static_assert на сборке — если не собралось, бенч не запустится |
| bench_subnow | FAIL | 1478 | 202 | step_100us=MISSING(exp=25), us_1000=MISSING(exp=250), step_1ms=MISSING(exp=250) | 16 МГц: 250/25/250; 8 МГц: 125/12/125 — править Expected под плату |
| bench_tick_confirm | FAIL | 1812 | 260 | TCCR2A=2, TIMSK2=2, TCCR2B=4, OCR2A=249, diff=MISSING(range=-2..2) | diff в допуске ±2; counter монотонен |
| bench_tick | VISUAL | 868 | 150 | visual | toggle D8 — меандр 500 Гц; результат только осциллографом |
