#include <Arduino.h>
#include "../src/time/Tick.h"
#include "../src/logic/Debounce.h"
#include "dbg.h"

// ============================================================================
// bench_debounce — синтетический дребезг + чистые фронты.
// Платформа: ATmega328P/328, Nano. Контекст: main. Железо не требуется.
// Ожидание: bounce_rose=1 (дребезг подавлен, один фронт),
//           bounce_fell=1, clean_rose=1, clean_fell=1,
//           glitch_rose=0 (импульс короче MS не проходит), stable_end=0.
// ============================================================================
using Db = logic::Debounce<20>;

static Db db;

// Прогон update() с заданным значением в течение us микросекунд.
__attribute__((noinline)) static void feedFor(bool level, uint32_t us) {
    uint32_t t0 = time::micros();
    while ((uint32_t)(time::micros() - t0) < us) {
        db.update(level);
    }
}

void setup() {
    dbg::begin(9600);
    time::init();

    db.init(false);

    // 1) Дребезг при нажатии: 5 переключений по 2 мс, затем стабильно HIGH 40 мс.
    for (uint8_t i = 0; i < 5; ++i) {
        feedFor(true,  2000);
        feedFor(false, 2000);
    }
    feedFor(true, 40000);
    uint8_t bRose = db.rose() ? 1 : 0;      // ровно 1, дребезг подавлен

    // 2) Дребезг при отпускании + стабильно LOW.
    for (uint8_t i = 0; i < 5; ++i) {
        feedFor(false, 2000);
        feedFor(true,  2000);
    }
    feedFor(false, 40000);
    uint8_t bFell = db.fell() ? 1 : 0;

    // 3) Короткий глюк (< MS) не должен пройти.
    feedFor(true, 5000);                    // 5 мс < 20 мс
    feedFor(false, 40000);
    uint8_t gRose = db.rose() ? 1 : 0;      // 0

    // 4) Чистые фронты без дребезга.
    feedFor(true, 40000);
    uint8_t cRose = db.rose() ? 1 : 0;      // 1
    feedFor(false, 40000);
    uint8_t cFell = db.fell() ? 1 : 0;      // 1

    dbg::text("\r\n=== Z2: Debounce ===\r\n");
    dbg::row("bounce_rose =", bRose);        // 1
    dbg::row("bounce_fell =", bFell);        // 1
    dbg::row("glitch_rose =", gRose);        // 0
    dbg::row("clean_rose  =", cRose);        // 1
    dbg::row("clean_fell  =", cFell);        // 1
    dbg::row("stable_end  =", db.stable() ? 1 : 0);  // 0
    dbg::flush();
}

void loop() {}