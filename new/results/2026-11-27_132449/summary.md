# ResultsBench — 2026-11-27 13:30:38

- commit: `nogit`
- fqbn: `arduino:avr:nano:cpu=atmega328old`
- port: `COM3`

| Bench | Status | Flash | SRAM | Details | Notes |
|-------|--------|-------|------|---------|-------|
| bench_adc_base | PASS | 1802 | 228 | us_fast=16, gnd_f=0, us_slow=104, gnd_s=0, bg_f=239, bg_s=238 | us_slow ~104..110; us_fast ~13..16; bg ~225 при AVcc 5V |
| bench_adc_decimate | PASS | 2202 | 281 | raw_min=238, avg_max=239, raw_max=239, avg_min=238, dec_max=956, dec_min=952, raw_spread=1(info), dec_spread=4(info), dec_count=279(info) | dec_min ~955 (про-тир, 12-bit); avg_min ~238 (средний, 10-bit) |
| bench_adc_stream | PASS | 1796 | 229 | samples=8928, last=238, per_us=112, missed=0(info) | missed — информационно (0 идеально, >0 допустимо) |
| bench_atomic | PASS | 1300 | 206 | enum_rt=2, r32=3735928559, r8=165, mono=1 | r32 — hex 0xDEADBEEF = 3735928559; static_assert trait — сборка = pass |
| bench_deadline | PASS | 2602 | 220 | sd_5us_up=8, dl_10ms_us=10000, sd_100us=100 | sd_5us_up ~8 (16M) — проверка B1 (округление вверх); 4..16 — допуск |
| bench_gpio | VISUAL | 604 | 142 | visual | LED D13 мигает; результат визуальный |
| bench_i2c_reg | FAIL | 2318 | 234 | rd00=0(exp=1), wrFF=0(exp=1), valFF=0(exp=255), val00=0, ping=0(exp=1), wr00=0(exp=1), rdFF=0(exp=1) | MCP23017 @0x26; valFF=255 (IODIRA=0xFF) |
| bench_i2c_rw | FAIL | 2066 | 230 | rd00=0(exp=1), wrFF=0(exp=1), wr00=0(exp=1), val00=170(exp=0), rdFF=0(exp=1), valFF=170(info) | pointer-less read: val00=0; valFF — информационно |
| bench_i2c | PASS | 2028 | 206 | TWBR=72, ack=0(info), first=255(info), us=13544(info) | ack >= 1 при устройстве на шине; first = адрес MCP (0x20..0x27) |
| bench_mcu | FAIL | 1054 | 234 | out_before=0, pull_after=0, out_after=0(exp=1), pull_before=1, mask_keep=0(exp=2) | flip на входе переключает pull-up (pull_before=1, pull_after=0) |
| bench_micros_confirm | PASS | 2004 | 226 | span_dif=4 | span_dif ~0; ours_1k < core_1k (наши дешевле) |
| bench_pin_init | FAIL | 1276 | 252 | ddrB_aft=33(exp=1), out_high=1, tog_read=1(exp=0), portB_bef=2, ddrB_bef=32(exp=0), portB_aft=2(exp=0), pull_read=1 | portB_bef=2 (PB1 pull-up); portB_aft=0 (P=PB0 clr), ddrB_aft=1 (PB0 out) |
| bench_platform | PASS | 1102 | 204 | sz_res=3, chip=328, sz_stat=1 | chip=328 для 328P; f_cpu=16000000 (5V) или 8000000 (3.3V) |
| bench_probe | FAIL | 1604 | 274 | TIFR2=4(exp=0), TCCR2A=2, TCCR2B=4, OCR2A=249, TIMSK2=2, OCR2B=0, delta=1092(out 990..1010) | delta ~1000 за 1 с; TIFR2=0 в норме |
| bench_rms | PASS | 2180 | 243 | synth_const=239, isqrt_65000=254, synth_alt=254, isqrt_max=65535, real_rms=238 | synth_const=239 (DC), synth_alt=254 (sqrt(65000)); real_rms ~239 |
| bench_spi | PASS | 1696 | 218 | SPCR4=80, SPSR4=0, us_4=3(info), us_2=1(info), rx4=0(info), rx2=0(info) | rx4=165, rx2=60 — только с джампером D11–D12; без него — плавающий/0 |
| bench_status | PASS | 1206 | 264 | size_status=1, err_ok=0, ok_ok=1, r_ok_ok=1, r_er_ok=0, size_res16=3, r_ok_val=1234 | static_assert на сборке — если не собралось, бенч не запустится |
| bench_subnow | FAIL | 1478 | 202 | step_100us=27(exp=25), us_1000=250, step_1ms=252(exp=250) | 16 МГц: 250/25/250; 8 МГц: 125/12/125 — править Expected под плату |
| bench_tick_confirm | PASS | 1812 | 260 | TCCR2A=2, TIMSK2=2, TCCR2B=4, OCR2A=249, diff=1 | diff в допуске ±2; counter монотонен |
| bench_tick | VISUAL | 868 | 150 | visual | toggle D8 — меандр 500 Гц; результат только осциллографом |
