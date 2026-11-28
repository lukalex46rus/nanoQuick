#include <Arduino.h>
#include "../src/time/Tick.h"
#include "../src/peripheral/Hd44780.h"
#include "dbg.h"

// ============================================================================
// bench_hd44780 — LCD 20x4: init/clear/setCursor/print, замер времени команд.
// Платформа: ATmega328P/328, Nano. Контекст: main.
// Requires: LCD HD44780 20x4, подключение: RS=D2 EN=D3 D4=D4 D5=D5 D6=D6 D7=D7,
//           RW=GND, V0=контраст (потенциометр), подсветка по паспорту модуля.
// Ожидание: init_us ~50000..60000, cmd_us ~50..80, clear_us ~2000..2200,
//           visual=1 (текст виден на всех 4 строках — проверить глазами).
// UART-часть PASS автоматически; визуальная часть — глазами.
// ============================================================================
using Lcd = peripheral::Hd44780<
    hw::board::D2, hw::board::D3,
    hw::board::D4, hw::board::D5, hw::board::D6, hw::board::D7>;

void setup() {
    dbg::begin(9600);
    time::init();

    uint32_t t0 = time::micros();
    Lcd::init();
    uint32_t initUs = time::micros() - t0;

    t0 = time::micros();
    Lcd::setCursor(0, 0);
    uint32_t cmdUs = time::micros() - t0;

    t0 = time::micros();
    Lcd::clear();
    uint32_t clearUs = time::micros() - t0;

    Lcd::setCursor(0, 0); Lcd::print("nanoQuick HD44780");
    Lcd::setCursor(0, 1); Lcd::print("row1 test 12345");
    Lcd::setCursor(0, 2); Lcd::print("row2 ");  Lcd::print((uint32_t)43981);
    Lcd::setCursor(0, 3); Lcd::print("row3 OK");

    dbg::text("\r\n=== Z3: HD44780 20x4 ===\r\n");
    dbg::row("init_us  =", initUs);    // ~50000..60000
    dbg::row("cmd_us   =", cmdUs);     // ~50..80
    dbg::row("clear_us =", clearUs);   // ~2000..2200
    dbg::row("visual   =", 1);         // текст на 4 строках — глазами
    dbg::flush();
}

void loop() {}