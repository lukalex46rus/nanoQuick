#include <Arduino.h>
#include "src/hw/Pin.h"
#include "src/time/Tick.h"
#include "src/time/Deadline.h"

using Led = hw::board::D13;

extern volatile uint32_t isrCompA;
extern volatile uint32_t isrCompB;
extern volatile uint32_t isrOvf;

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

static time::Period report;

void setup() {
    Led::init(hw::Dir::Out);
    dbg::begin(9600);
    time::init();

    dbg::text("\r\n--- D9: ISR census ---\r\n");
    dbg::row("TIMSK2 =", TIMSK2);
    dbg::row("TCCR2A =", TCCR2A);
    dbg::row("TCCR2B =", TCCR2B);
    dbg::row("OCR2A  =", OCR2A);
    dbg::row("OCR2B  =", OCR2B);

    report.start(time::ms(1000));
}

void loop() {
    if (report.due()) {
        uint32_t m  = time::now();
        uint32_t ms = ::millis();
        uint32_t a  = hw::load(isrCompA);
        uint32_t b  = hw::load(isrCompB);
        uint32_t o  = hw::load(isrOvf);

        dbg::row("counter=", m);
        dbg::row("compA  =", a);
        dbg::row("compB  =", b);
        dbg::row("ovf    =", o);
        dbg::row("sum    =", a + b + o);
        dbg::row("core_ms=", ms);
        dbg::row("TIFR2  =", TIFR2);
        dbg::text("\r\n");
        dbg::flush();
    }
}