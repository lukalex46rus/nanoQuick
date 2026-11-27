#include <Arduino.h>
#include "src/time/Tick.h"
#include "src/bus/Spi.h"

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

using Spi4 = bus::SpiMaster<bus::SpiMode::M0, bus::SpiClk::Div4>;   // 4 МГц
using Spi2 = bus::SpiMaster<bus::SpiMode::M0, bus::SpiClk::Div2>;   // 8 МГц

__attribute__((noinline)) static uint32_t bench4(uint16_t n) {
    uint32_t t0 = time::micros();
    for (uint16_t i = 0; i < n; ++i) (void)Spi4::transfer((uint8_t)i);
    return time::micros() - t0;
}
__attribute__((noinline)) static uint32_t bench2(uint16_t n) {
    uint32_t t0 = time::micros();
    for (uint16_t i = 0; i < n; ++i) (void)Spi2::transfer((uint8_t)i);
    return time::micros() - t0;
}

void setup() {
    dbg::begin(9600);
    time::init();

    Spi4::init();
    uint8_t  spcr4 = SPCR, spsr4 = SPSR;
    uint32_t d4 = bench4(1000);
    uint8_t  rx4 = Spi4::transfer(0xA5);
    Spi4::deinit();

    Spi2::init();
    uint32_t d2 = bench2(1000);
    uint8_t  rx2 = Spi2::transfer(0x3C);
    Spi2::deinit();

    dbg::text("\r\n=== T2: SPI master ===\r\n");
    dbg::row("SPCR4 =", spcr4);        // ждём 0x50 = SPE|MSTR
    dbg::row("SPSR4 =", spsr4);        // ждём 0
    dbg::row("us_4  =", d4 / 1000);    // ~2 мкс/байт (+overhead)
    dbg::row("us_2  =", d2 / 1000);    // ~1 мкс/байт
    dbg::row("rx4   =", rx4);          // 0xA5=165 при джампере D11-D12
    dbg::row("rx2   =", rx2);          // 0x3C=60 при джампере
    dbg::flush();
}

void loop() {}