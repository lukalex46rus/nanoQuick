#include <Arduino.h>
#include "src/time/Tick.h"
#include "src/peripheral/Adc.h"

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
    dbg::row("us_slow =", tSlow);   // ждём ~104..110
    dbg::row("us_fast =", tFast);   // ждём ~13..16
    dbg::row("gnd_s   =", gndS);    // ждём 0..3
    dbg::row("bg_s    =", bgS);     // ждём ~200..250
    dbg::row("gnd_f   =", gndF);
    dbg::row("bg_f    =", bgF);
    dbg::flush();
}

void loop() {}