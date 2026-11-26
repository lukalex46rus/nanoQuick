#include <Arduino.h>
#include "../src/time/Tick.h"
#include "../src/peripheral/Adc.h"
#include "dbg.h"

// ============================================================================
// bench_adc_base — время конверсии (Slow Div128 / Fast Div16), GND/bandgap.
// Платформа: ATmega328P/328, Nano. Контекст: main.
// Ожидание: us_slow ~104..110 (13 тактов @128), us_fast ~13..16 (13 тактов @16);
// gnd 0..3; bg ~200..250 (1.1 В при AVcc 5 В).
// ============================================================================
using AdcSlow = adc::Adc<adc::Ref::AVcc, adc::Clk::Div128>;
using AdcFast = adc::Adc<adc::Ref::AVcc, adc::Clk::Div16>;

// Конкретные хелперы (без шаблонов) — обход генератора прототипов Arduino.

__attribute__((noinline)) static uint32_t convTimeSlow(uint8_t ch, uint16_t n) {
    AdcSlow::start(ch); while (!AdcSlow::ready()) {} (void)AdcSlow::take(); // первая=25 тактов, отброс
    uint32_t t0 = time::micros();
    for (uint16_t i = 0; i < n; ++i) {
        AdcSlow::start(ch);
        while (!AdcSlow::ready()) {}
        (void)AdcSlow::take();
    }
    return (time::micros() - t0) / n;
}

__attribute__((noinline)) static uint32_t convTimeFast(uint8_t ch, uint16_t n) {
    AdcFast::start(ch); while (!AdcFast::ready()) {} (void)AdcFast::take();
    uint32_t t0 = time::micros();
    for (uint16_t i = 0; i < n; ++i) {
        AdcFast::start(ch);
        while (!AdcFast::ready()) {}
        (void)AdcFast::take();
    }
    return (time::micros() - t0) / n;
}

__attribute__((noinline)) static uint16_t readChSlow(uint8_t ch) {
    AdcSlow::start(ch); while (!AdcSlow::ready()) {}
    return AdcSlow::take().value;
}
__attribute__((noinline)) static uint16_t readChFast(uint8_t ch) {
    AdcFast::start(ch); while (!AdcFast::ready()) {}
    return AdcFast::take().value;
}

void setup() {
    dbg::begin(9600);
    time::init();

    AdcSlow::init();
    uint32_t tSlow = convTimeSlow(adc::GND_CH, 100);
    uint16_t gndS  = readChSlow(adc::GND_CH);
    uint16_t bgS   = readChSlow(adc::BANDGAP);
    AdcSlow::deinit();

    AdcFast::init();
    uint32_t tFast = convTimeFast(adc::GND_CH, 100);
    uint16_t gndF  = readChFast(adc::GND_CH);
    uint16_t bgF   = readChFast(adc::BANDGAP);
    AdcFast::deinit();

    dbg::text("\r\n=== N2: ADC base ===\r\n");
    dbg::row("us_slow =", tSlow);   // ~104..110
    dbg::row("us_fast =", tFast);   // ~13..16
    dbg::row("gnd_s   =", gndS);    // 0..3
    dbg::row("bg_s    =", bgS);     // ~200..250
    dbg::row("gnd_f   =", gndF);
    dbg::row("bg_f    =", bgF);
    dbg::flush();
}

void loop() {}