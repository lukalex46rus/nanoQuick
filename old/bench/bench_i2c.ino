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

__attribute__((noinline)) static uint8_t scan(uint8_t* first) {
    uint8_t cnt = 0; *first = 0xFF;
    for (uint8_t a = 0x08; a <= 0x77; ++a) {
        if (Bus::write(a, nullptr, 0).ok()) {
            if (cnt == 0) *first = a;
            ++cnt;
        }
    }
    return cnt;
}

void setup() {
    dbg::begin(9600);
    time::init();
    Bus::init();

    uint8_t first;
    uint32_t t0 = time::micros();
    uint8_t cnt = scan(&first);
    uint32_t dt = time::micros() - t0;

    dbg::text("\r\n=== U2: I2C scan ===\r\n");
    dbg::row("TWBR  =", TWBR);          // 72 для 100 кГц
    dbg::row("ack   =", cnt);           // 0 без устройства, >=1 с MCP/дисплеем
    dbg::row("first =", first);         // 0x20=32 для MCP23017
    dbg::row("us    =", dt);            // время скана, нет зависания
    dbg::flush();
}

void loop() {}