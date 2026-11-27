#include <Arduino.h>
#include "src/time/Tick.h"
#include "src/peripheral/AdcStream.h"
#include "src/logic/Rms.h"

namespace dbg {
inline void begin(uint32_t baud) {
    uint16_t ubrr = (uint16_t)((F_CPU / (16UL * baud)) - 1UL);
    UBRR0H = (uint8_t)(ubrr >> 8); UBRR0L = (uint8_t)ubrr;
    UCSR0A = 0; UCSR0B = _BV(TXEN0); UCSR0C = _BV(UCSZ01) | _BV(UCSZ00);
}
__attribute__((noinline)) inline void byte(uint8_t b) {
    while (!(UCSR0A & _BV(UDRE0))) {} UDR0 = b;
}
inline void text(const char* s) { while (*s) byte((uint8_t)*s++); }
__attribute__((noinline)) inline void num(uint32_t v) {
    char buf[11]; int8_t i = 10; buf[i] = 0;
    do { buf[--i] = (char)('0' + (v % 10u)); v /= 10u; } while (v && i > 0);
    text(&buf[i]);
}
__attribute__((noinline)) inline void row(const char* tag, uint32_t v) {
    text(tag); num(v); text("\r\n");
}
inline void flush() { while (!(UCSR0A & _BV(TXC0))) {} UCSR0A |= _BV(TXC0); }
}

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
    dbg::row("isqrt_65000 =", logic::isqrt32(65000UL));   // 254
    dbg::row("isqrt_max   =", logic::isqrt32(0xFFFFFFFFUL)); // 65535
    dbg::row("synth_const =", c);    // 239 (DC входит)
    dbg::row("synth_alt   =", a);    // 254
    dbg::row("real_rms    =", rr);   // ~239
    dbg::flush();
}

void loop() {}