#include <Arduino.h>
#include "../src/hw/Pin.h"
#include "../src/time/Tick.h"
#include "../src/time/Deadline.h"
#include "dbg.h"

// ============================================================================
// bench_tick_confirm — регистры Timer2 + сверка counter с core millis().
// Платформа: ATmega328P/328, Nano. Контекст: main.
// Ожидание: TCCR2A=2, TCCR2B=4, OCR2A=249, TIMSK2=2; diff ~0 каждую секунду.
// ============================================================================
using Led = hw::board::D13;

static time::Period blink;
static time::Period report;

void setup() {
    Led::init(hw::Dir::Out);
    dbg::begin(9600);
    time::init();

    dbg::text("\r\n=== P0-2: tick confirm (presc 64) ===\r\n");
    dbg::row("TCCR2A=", TCCR2A);   // 2
    dbg::row("TCCR2B=", TCCR2B);   // 4 (_BV(CS22))
    dbg::row("OCR2A =", OCR2A);    // 249
    dbg::row("TIMSK2=", TIMSK2);   // 2

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