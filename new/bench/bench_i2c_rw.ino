#include <Arduino.h>
#include "../src/time/Tick.h"
#include "../src/bus/I2c.h"
#include "dbg.h"

// ============================================================================
// bench_i2c_rw — «сырой» write/read без writeRead: проверяет pointer-less read.
// Платформа: ATmega328P/328, Nano. Контекст: main.
// Ожидание: wr00/rd00 = 1, val00 = 0 (IODIRA=0); wrFF/rdFF = 1, valFF —
// информационно (после записи 0xFF в IODIRA читаем IODIRA с текущего указателя).
// Адрес 0x26 (MCP23017, A2/A1/A0=110) — при другом адресе поправить.
// ============================================================================
using Bus = bus::I2c<100000, true>;

void setup() {
    dbg::begin(9600);
    time::init();
    Bus::init();

    uint8_t w0 = 0x00, wF = 0xFF, r0 = 0xAA, rF = 0xAA;
    base::Status sw0 = Bus::write(0x26, &w0, 1);
    base::Status sr0 = Bus::read(0x26, &r0, 1);
    base::Status swF = Bus::write(0x26, &wF, 1);
    base::Status srF = Bus::read(0x26, &rF, 1);

    dbg::text("\r\n=== V1: I2C RW (0x26 MCP23017) ===\r\n");
    dbg::row("wr00  =", sw0.ok() ? 1 : 0);   // 1
    dbg::row("rd00  =", sr0.ok() ? 1 : 0);   // 1
    dbg::row("val00 =", r0);                 // 0
    dbg::row("wrFF  =", swF.ok() ? 1 : 0);   // 1
    dbg::row("rdFF  =", srF.ok() ? 1 : 0);   // 1
    dbg::row("valFF =", rF);                 // информационно
    dbg::flush();
}

void loop() {}