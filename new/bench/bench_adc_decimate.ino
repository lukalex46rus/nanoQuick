#include <Arduino.h>
#include "../src/time/Tick.h"
#include "../src/peripheral/AdcStream.h"
#include "../src/logic/Decimator.h"
#include "dbg.h"

using AdcPro = adc::AdcStream<adc::Ref::AVcc, adc::Clk::Div128, adc::BANDGAP>;

// Сбор статистики raw и decimated за окно. last внутри цикла (правило O8).
__attribute__((noinline))
static void gather(uint32_t windowUs,
                   uint16_t* rmin, uint16_t* rmax,
                   uint16_t* dmin, uint16_t* dmax,
                   uint32_t* decCount) {
    logic::Decimator<2> dec;              // 16 выборок -> (10+2)-бит
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

    dbg::text("\r\n=== Q2: decimate (EXTRA=2) ===\r\n");
    dbg::row("raw_min =", rmin);            // ~238
    dbg::row("raw_max =", rmax);            // ~240
    dbg::row("dec_min =", dmin);            // ~955 (про-тир, 12-bit)
    dbg::row("dec_max =", dmax);            // ~957
    dbg::row("avg_min =", dmin >> 2);       // ~238 (средний тир, 10-bit)
    dbg::row("avg_max =", dmax >> 2);       // ~239
    dbg::row("raw_spread =", rmax - rmin);
    dbg::row("dec_spread =", dmax - dmin);  // в 12-bit шкале
    dbg::row("dec_count  =", dc);           // ~279
    dbg::flush();
}

void loop() {}