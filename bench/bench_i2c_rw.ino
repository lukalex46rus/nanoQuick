#include <Arduino.h>
#include "src/time/Tick.h"
#include "src/bus/I2c.h"

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

using Bus = bus::I2c<100000, true>;

void setup() {
    dbg::begin(9600);
    time::init();
    Bus::init();

    uint8_t w0 = 0x00, wF = 0xFF, r0 = 0xAA, rF = 0xAA;
    base::Status sw0 = Bus::write(0x26, &w0, 1);
    base::Status sr0 = Bus::read(0x26, &r0, 1);
    base::Status swF = Bus::write(0x26, &wF, 1);
    base::Status srF = Bus::read(0x26, &rF, 1);

    dbg::text("\r\n=== V1: I2C RW (0x26 MCP23017) ===\r\n");
    dbg::row("wr00  =", sw0.ok() ? 1 : 0);   // 1
    dbg::row("rd00  =", sr0.ok() ? 1 : 0);   // 1
    dbg::row("val00 =", r0);                 // 0 (детерминированно)
    dbg::row("wrFF  =", swF.ok() ? 1 : 0);   // 1
    dbg::row("rdFF  =", srF.ok() ? 1 : 0);   // 1
    dbg::row("valFF =", rF);                 // информационно, может <255
    dbg::flush();
}

void loop() {}