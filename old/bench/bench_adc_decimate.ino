#include <Arduino.h>
#include "src/time/Tick.h"
#include "src/peripheral/AdcStream.h"
#include "src/logic/Decimator.h"

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

// Сбор статистики raw и decimated за окно. last внутри цикла (правило O8).
__attribute__((noinline))
static void gather(uint32_t windowUs,
                   uint16_t* rmin, uint16_t* rmax,
                   uint16_t* dmin, uint16_t* dmax,
                   uint32_t* decCount) {
    logic::Decimator<2> dec;              // 16 выборок -> 12 бит
    uint16_t rmn = 0xFFFF, rmx = 0, dmn = 0xFFFF, dmx = 0;
    uint32_t dc = 0;
    uint32_t t0 = time::micros();
    while ((uint32_t)(time::micros() - t0) < windowUs) {
        if (AdcPro::available()) {
            base::Result<uint16_t> r = AdcPro::take();
            if (r.ok()) {
                uint16_t s = r.value;
                if (s < rmn) rmn = s;
                if (s > rmx) rmx = s;
                base::Result<uint16_t> d = dec.feed(s);
                if (d.ok()) {
                    uint16_t v = d.value;
                    if (v < dmn) dmn = v;
                    if (v > dmx) dmx = v;
                    ++dc;
                }
            }
        }
    }
    *rmin = rmn; *rmax = rmx; *dmin = dmn; *dmax = dmx; *decCount = dc;
}

void setup() {
    dbg::begin(9600);
    time::init();
    AdcPro::init();
    AdcPro::start();

    uint16_t rmin, rmax, dmin, dmax;
    uint32_t dc;
    gather(500000UL, &rmin, &rmax, &dmin, &dmax, &dc);   // 0.5 с
    AdcPro::stop();

    dbg::text("\r\n=== Q2: decimate (EXTRA=2, 12-bit) ===\r\n");
    dbg::row("raw_min =", rmin);            // ~238
    dbg::row("raw_max =", rmax);            // ~240
    dbg::row("dec_min =", dmin);            // ~955 (12-bit)
    dbg::row("dec_max =", dmax);            // ~957
    dbg::row("raw_spread =", rmax - rmin);
    dbg::row("dec_spread =", dmax - dmin);  // в 12-bit шкале
    dbg::row("dec_count  =", dc);           // ~279
    dbg::flush();
}

void loop() {}