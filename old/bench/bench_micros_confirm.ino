#include <Arduino.h>
#include "src/time/Tick.h"

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
__attribute__((noinline)) inline void srow(const char* tag, int32_t v) {
    text(tag); if (v < 0) { byte('-'); v = -v; } num((uint32_t)v); text("\r\n");
}
inline void flush() { while (!(UCSR0A & _BV(TXC0))) {} UCSR0A |= _BV(TXC0); }
}

__attribute__((noinline)) static uint32_t benchOurs(uint16_t n) {
    uint32_t acc = 0;
    uint32_t t0 = ::micros();
    for (uint16_t i = 0; i < n; ++i) acc += time::micros();
    uint32_t dt = ::micros() - t0;
    if (acc == 0xFFFFFFFFUL) dbg::byte('!');
    return dt;
}

__attribute__((noinline)) static uint32_t benchCore(uint16_t n) {
    uint32_t acc = 0;
    uint32_t t0 = ::micros();
    for (uint16_t i = 0; i < n; ++i) acc += ::micros();
    uint32_t dt = ::micros() - t0;
    if (acc == 0xFFFFFFFFUL) dbg::byte('!');
    return dt;
}

void setup() {
    dbg::begin(9600);
    time::init();

    const uint16_t N = 1000;
    uint32_t ours = benchOurs(N);
    uint32_t core = benchCore(N);

    // Короткое плечо: сверка значений
    uint32_t u0 = time::micros();
    uint32_t c0 = ::micros();
    delay(100);
    uint32_t u1 = time::micros();
    uint32_t c1 = ::micros();

    dbg::text("\r\n=== P0-3: micros confirm ===\r\n");
    dbg::row ("ours_1k =", ours);
    dbg::row ("core_1k =", core);
    dbg::row ("span_our=", u1 - u0);
    dbg::row ("span_cor=", c1 - c0);
    dbg::srow("span_dif=", (int32_t)(u1-u0) - (int32_t)(c1-c0));
    dbg::flush();
}

void loop() {}