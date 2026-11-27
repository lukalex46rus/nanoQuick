#include <Arduino.h>
#include "src/time/Tick.h"
#include "src/bus/I2c.h"
#include "dbg.h"

using Bus = bus::I2c<100000, true>;

__attribute__((noinline)) static uint8_t scan(uint8_t* first) {
    uint8_t cnt = 0; *first = 0xFF;
    for (uint8_t a = 0x08; a <= 0x77; ++a) {
        if (Bus::ping(a).ok()) {
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

    dbg::text("\r\n=== U2: I2C scan (ping) ===\r\n");
    dbg::row("TWBR  =", TWBR);          // 72
    dbg::row("ack   =", cnt);
    dbg::row("first =", first);         // 0x20=32 для MCP
    dbg::row("us    =", dt);
    dbg::flush();
}

void loop() {}