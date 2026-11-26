#include <Arduino.h>
#include "../src/time/Tick.h"
#include "../src/peripheral/AdcStream.h"
#include "dbg.h"

// ============================================================================
// bench_adc_stream — 1 с free-running: samples, missed, last.
// Платформа: ATmega328P/328, Nano. Контекст: main.
// Ожидание: samples ~8928 (16M / 128 / 14), missed=0 (если main успевает
// вычитывать; иначе >0 — допустимо), per_us ~112, last ~238 (bandgap).
// ============================================================================
using AdcPro = adc::AdcStream<adc::Ref::AVcc, adc::Clk::Div128, adc::BANDGAP>;

__attribute__((noinline)) static uint32_t countSamples(uint32_t windowUs, uint16_t* last) {
    uint32_t n = 0;
    uint32_t t0 = time::micros();
    while ((uint32_t)(time::micros() - t0) < windowUs) {
        if (AdcPro::available()) {
            base::Result<uint16_t> r = AdcPro::take();
            if (r.ok()) { *last = r.value; ++n; }
        }
    }
    return n;
}

void setup() {
    dbg::begin(9600);
    time::init();

    AdcPro::init();
    AdcPro::start();
    uint16_t last = 0;
    uint32_t c = countSamples(1000000UL, &last);   // окно 1 с, last внутри цикла
    uint32_t miss = AdcPro::missed();
    AdcPro::stop();

    dbg::text("\r\n=== O3c: ADC stream 1s ===\r\n");
    dbg::row("samples =", c);                 // ~8928
    dbg::row("expect  =", AdcPro::SAMPLE_RATE);
    dbg::row("per_us  =", 1000000UL / c);     // ~112
    dbg::row("missed  =", miss);              // 0
    dbg::row("last    =", last);              // ~238
    dbg::flush();
}

void loop() {}