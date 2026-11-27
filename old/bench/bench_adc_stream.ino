#include <Arduino.h>
#include "src/time/Tick.h"
#include "src/peripheral/AdcStream.h"

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

// AdcPro, не Stream: Stream — класс Arduino core (Stream.h).
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