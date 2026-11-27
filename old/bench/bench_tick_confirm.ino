#include <Arduino.h>
#include "src/hw/Pin.h"
#include "src/time/Tick.h"
#include "src/time/Deadline.h"

using Led = hw::board::D13;

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

static time::Period blink;
static time::Period report;

void setup() {
    Led::init(hw::Dir::Out);
    dbg::begin(9600);
    time::init();

    dbg::text("\r\n=== P0-2: tick confirm (presc 64) ===\r\n");
    dbg::row("TCCR2A=", TCCR2A);   // ждём 2
    dbg::row("TCCR2B=", TCCR2B);   // ждём 4 (_BV(CS22))
    dbg::row("OCR2A =", OCR2A);    // ждём 249
    dbg::row("TIMSK2=", TIMSK2);   // ждём 2

    blink.start(time::ms(500));
    report.start(time::ms(1000));
}

void loop() {
    if (blink.due()) Led::toggle();

    if (report.due()) {
        uint32_t m  = time::now();
        uint32_t ms = ::millis();
        int32_t  d  = (int32_t)m - (int32_t)ms;

        dbg::row ("counter=", m);
        dbg::row ("core_ms=", ms);
        dbg::srow("diff   =", d);
        dbg::flush();
    }
}