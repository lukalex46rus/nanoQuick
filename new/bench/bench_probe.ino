#include <Arduino.h>
#include "src/hw/Pin.h"
#include "src/hw/Atomic.h"
#include "src/time/Tick.h"
#include "src/time/Deadline.h"
#include "dbg.h"

// ============================================================================
// bench_probe — ISR census: регистры Timer2 + counter/delta/TIFR2 раз в 1 с.
// Платформа: ATmega328P/328, Nano. Контекст: main.
// Ожидание: TIMSK2=2, TCCR2A=2, TCCR2B=4, OCR2A=249, OCR2B=0;
// delta ~1000 за 1 с; TIFR2=0 в норме.
// ============================================================================
using Led = hw::board::D13;

static time::Period report;

void setup() {
    Led::init(hw::Dir::Out);
    dbg::begin(9600);
    time::init();

    dbg::text("\r\n--- D9: ISR census (fixed) ---\r\n");
    dbg::row("TIMSK2 =", TIMSK2);   // 2
    dbg::row("TCCR2A =", TCCR2A);   // 2
    dbg::row("TCCR2B =", TCCR2B);   // 4
    dbg::row("OCR2A  =", OCR2A);    // 249
    dbg::row("OCR2B  =", OCR2B);    // 0

    report.start(time::ms(1000));
}

void loop() {
    if (report.due()) {
        // counter инкрементируется ровно раз в 1 мс. За 1 с между
        // предыдущим и текущим due() должно пройти ~1000 инкрементов.
        static uint32_t prev = 0;
        uint32_t m  = time::now();
        uint32_t ms = ::millis();
        uint32_t delta = m - prev;
        prev = m;

        dbg::row("counter =", m);
        dbg::row("delta   =", delta);      // ~1000
        dbg::row("core_ms =", ms);
        dbg::row("TIFR2   =", TIFR2);      // 0 в норме
        dbg::text("\r\n");
        dbg::flush();
    }
}