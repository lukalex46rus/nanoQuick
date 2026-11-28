#include <Arduino.h>
#include "../src/time/Tick.h"
#include "../src/bus/I2c.h"
#include "../src/peripheral/Mcp23017.h"
#include "dbg.h"

// ============================================================================
// bench_mcp23017 — драйвер MCP23017 @0x26: init/read16/setDir/write16/read-back.
// Платформа: ATmega328P/328, Nano. Контекст: main.
// Requires: MCP23017 @0x26 (A2/A1/A0=110), I2C на A4/A5.
// Шина 100 кГц (надёжность на макете; скорость — отдельный бенч).
// Ожидание: init_ok=1, pu_read≈65535 (Range: обвес может тянуть пины),
//           dir_ok=1, wr_ok=1, olat_val=43981 (0xABCD, защёлка — не зависит от нагрузки),
//           gpio_val информационно (GPIO=OLAT только без внешней нагрузки),
//           back_dir=255, back_pu=255 (реальная проверка безопасного состояния).
// OLAT сбрасывается в 0 перед финальным init — иначе при следующем setDir(0)
// пины примут последнюю защёлку (init не трогает OLAT).
// ============================================================================
using Bus  = bus::I2c<100000, true>;
using Chip = peripheral::Mcp23017<0x26, Bus>;
namespace R = peripheral::mcp_reg;

void setup() {
    dbg::begin(9600);
    time::init();
    Bus::init();

    // 1) init с pull-up: все входы подтянуты → read16 ≈ 0xFFFF
    base::Status si = Chip::init(true);
    base::Result<uint16_t> pu = Chip::read16();

    // 2) все выходы, записать 0xABCD
    base::Status sd = Chip::setDir(0x0000);
    base::Status sw = Chip::write16(0xABCD);

    // Читаем ЗАЩЁЛКУ (OLAT) — детерминированно, не зависит от нагрузки на пинах.
    base::Result<uint8_t> olatA = Chip::readReg(R::OLATA);
    base::Result<uint8_t> olatB = Chip::readReg(R::OLATB);
    uint16_t olatVal = (uint16_t)olatA.value | ((uint16_t)olatB.value << 8);

    // GPIO для выходов = OLAT только без внешней нагрузки — информационно.
    base::Result<uint16_t> gp = Chip::read16();

    // 3) безопасное состояние: сбросить защёлку, затем все входы с pull-up.
    //    init() НЕ трогает OLAT — без write16(0) следующий setDir(0) дал бы 0xABCD.
    base::Status sz = Chip::write16(0x0000);
    base::Status sb = Chip::init(true);

    // Реальная проверка (не только Status): IODIRA и GPPUA должны быть 0xFF.
    base::Result<uint8_t> backDir = Chip::readReg(R::IODIRA);
    base::Result<uint8_t> backPu  = Chip::readReg(R::GPPUA);

    dbg::text("\r\n=== Z1: MCP23017 driver ===\r\n");
    dbg::row("init_ok  =", si.ok() ? 1 : 0);        // 1
    dbg::row("pu_read  =", pu.value);               // ~65535 (Range)
    dbg::row("dir_ok   =", sd.ok() ? 1 : 0);        // 1
    dbg::row("wr_ok    =", sw.ok() ? 1 : 0);        // 1
    dbg::row("olat_val =", olatVal);                // 43981 (0xABCD)
    dbg::row("gpio_val =", gp.value);               // info: =OLAT без нагрузки
    dbg::row("zero_ok  =", sz.ok() ? 1 : 0);        // 1
    dbg::row("back_in  =", sb.ok() ? 1 : 0);        // 1
    dbg::row("back_dir =", backDir.value);          // 255
    dbg::row("back_pu  =", backPu.value);           // 255
    dbg::flush();
}

void loop() {}