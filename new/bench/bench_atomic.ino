#include <Arduino.h>
#include "src/hw/Atomic.h"
#include "src/time/Tick.h"
#include "dbg.h"

void setup() {
    dbg::begin(9600);
    time::init();

    // 32-бит: load/store через Guard
    volatile uint32_t v32 = 0;
    hw::store(v32, 0xDEADBEEFUL);
    uint32_t r32 = hw::load(v32);

    // 8-бит: специализация без cli
    volatile uint8_t v8 = 0;
    hw::store(v8, 0xA5);
    uint8_t r8 = hw::load(v8);

    // load(now) не рвётся ISR: 1000 чтений, монотонность
    uint32_t prev = time::now();
    uint8_t  mono = 1;
    for (uint16_t i = 0; i < 1000; ++i) {
        uint32_t n = time::now();
        if ((int32_t)(n - prev) < 0) mono = 0;
        prev = n;
    }

    dbg::text("\r\n=== X2: Atomic ===\r\n");
    dbg::row("r32  =", r32);          // 0xDEADBEEF
    dbg::row("r8   =", r8);           // 0xA5
    dbg::row("mono =", mono);         // 1
    dbg::flush();
}

void loop() {}