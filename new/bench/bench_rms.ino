#include <Arduino.h>
#include "src/time/Tick.h"
#include "src/peripheral/AdcStream.h"
#include "src/logic/Rms.h"
#include "dbg.h"

// ============================================================================
// bench_rms — isqrt32, synth const/alt, real RMS за 0.5 с (bandgap).
// Платформа: ATmega328P/328, Nano. Контекст: main.
// Ожидание: isqrt_65000=254, isqrt_max=65535, synth_const=239, synth_alt=254,
// real_rms ~239 (bandgap DC ~1.1 В).
// ============================================================================
using AdcPro = adc::AdcStream<adc::Ref::AVcc, adc::Clk::Div128, adc::BANDGAP>;

__attribute__((noinline)) static uint16_t synthConst(uint16_t v) {
    logic::Rms<8, 10> r;
    uint16_t out = 0;
    for (uint16_t i = 0; i < 256; ++i) {
        base::Result<uint16_t> x = r.feed(v);
        if (x.ok()) out = x.value;
    }
    return out;
}

__attribute__((noinline)) static uint16_t synthAlt() {
    logic::Rms<8, 10> r;
    uint16_t out = 0;
    for (uint16_t i = 0; i < 256; ++i) {
        base::Result<uint16_t> x = r.feed((i & 1) ? 300 : 200);
        if (x.ok()) out = x.value;
    }
    return out;   // sqrt((200^2+300^2)/2) = sqrt(65000) = 254
}

__attribute__((noinline)) static uint16_t realRms(uint32_t windowUs) {
    logic::Rms<8, 10> r;
    uint16_t out = 0;
    uint32_t t0 = time::micros();
    while ((uint32_t)(time::micros() - t0) < windowUs) {
        if (AdcPro::available()) {
            base::Result<uint16_t> s = AdcPro::take();
            if (s.ok()) {
                base::Result<uint16_t> x = r.feed(s.value);
                if (x.ok()) out = x.value;
            }
        }
    }
    return out;
}

void setup() {
    dbg::begin(9600);
    time::init();
    AdcPro::init();
    AdcPro::start();

    uint16_t c  = synthConst(239);
    uint16_t a  = synthAlt();
    uint16_t rr = realRms(500000UL);
    AdcPro::stop();

    dbg::text("\r\n=== S2: RMS DC+AC ===\r\n");
    dbg::row("isqrt_65000 =", logic::isqrt32(65000UL));      // 254
    dbg::row("isqrt_max   =", logic::isqrt32(0xFFFFFFFFUL));  // 65535
    dbg::row("synth_const =", c);    // 239
    dbg::row("synth_alt   =", a);    // 254
    dbg::row("real_rms    =", rr);   // ~239
    dbg::flush();
}

void loop() {}