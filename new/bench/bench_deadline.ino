#include <Arduino.h>
#include "../src/time/Tick.h"
#include "../src/time/Deadline.h"
#include "dbg.h"

void setup() {
    dbg::begin(9600);
    time::init();

    // Deadline: 10 мс
    time::Deadline d; d.arm(10);
    uint32_t t0 = time::micros();
    while (!d.expired()) {}
    uint32_t dtExp = time::micros() - t0;

    // left() до истечения
    time::Deadline d2; d2.arm(100);
    delay(1);
    uint32_t left1 = d2.left();     // ~99

    // ShortDeadline: 100 мкс, разрешение SUB_US
    time::ShortDeadline sd; sd.armUs(100);
    uint32_t t1 = time::micros();
    while (!sd.expired()) {}
    uint32_t dtShort = time::micros() - t1;

    // armUs округление вверх: 5 мкс -> 2 отсчёта = 8 мкс (16M)
    time::ShortDeadline sd2; sd2.armUs(5);
    uint32_t t2 = time::micros();
    while (!sd2.expired()) {}
    uint32_t dtRound = time::micros() - t2;

    dbg::text("\r\n=== W1: Deadline ===\r\n");
    dbg::row("dl_10ms_us =", dtExp);      // ~10000
    dbg::row("left_1ms   =", left1);      // ~99
    dbg::row("sd_100us   =", dtShort);    // ~100..104
    dbg::row("sd_5us_up  =", dtRound);    // ~8 (округление вверх)
    dbg::flush();
}

void loop() {}