#include <Arduino.h>
#include "../src/time/Tick.h"
#include "dbg.h"

void setup() {
    dbg::begin(9600);
    time::init();

    // Шаг subNow за 1 мс: TICKS_PER_MS (250 @16M)
    uint16_t s0 = time::subNow();
    delay(1);
    uint16_t s1 = time::subNow();
    uint16_t d1 = (uint16_t)(s1 - s0);

    // Шаг subNow за 100 мкс: ~25 отсчётов @16M
    uint16_t s2 = time::subNow();
    delayMicroseconds(100);
    uint16_t s3 = time::subNow();
    uint16_t d2 = (uint16_t)(s3 - s2);

    // time::us(1000) = 250 @16M (1000/SUB_US)
    uint16_t u = time::us(1000);

    dbg::text("\r\n=== W2: subNow ===\r\n");
    dbg::row("step_1ms  =", d1);     // ~250 @16M, ~125 @8M
    dbg::row("step_100us=", d2);     // ~25 @16M, ~12 @8M
    dbg::row("us(1000)  =", u);      // 250 @16M, 125 @8M
    dbg::flush();
}

void loop() {}