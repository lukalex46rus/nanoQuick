#include <Arduino.h>
#include "src/time/Tick.h"
#include "src/bus/I2c.h"
#include "dbg.h"

using Bus = bus::I2c<100000, true>;

// MCP23017 @ 0x26 (A2/A1/A0=110). Регистр 0x00 = IODIRA.
static constexpr uint8_t ADDR = 0x26;
static constexpr uint8_t REG  = 0x00;

__attribute__((noinline)) static base::Status wr(uint8_t reg, uint8_t val) {
    uint8_t buf[2] = { reg, val };
    return Bus::write(ADDR, buf, 2, Bus::msFor(2));
}
__attribute__((noinline)) static base::Result<uint8_t> rd(uint8_t reg) {
    uint8_t v = 0;
    base::Status st = Bus::writeRead(ADDR, &reg, 1, &v, 1, Bus::msFor(2));
    if (!st.ok()) return base::Result<uint8_t>{ 0, st };
    return base::Result<uint8_t>{ v, base::ok() };
}

void setup() {
    dbg::begin(9600);
    time::init();
    Bus::init();

    uint8_t ping_ok = Bus::ping(ADDR).ok() ? 1 : 0;
    base::Status w0 = wr(REG, 0x00);
    base::Result<uint8_t> r0 = rd(REG);
    base::Status wF = wr(REG, 0xFF);
    base::Result<uint8_t> rF = rd(REG);

    dbg::text("\r\n=== V2: I2C writeRead (MCP @0x26) ===\r\n");
    dbg::row("ping =", ping_ok);           // 1
    dbg::row("wr00 =", w0.ok() ? 1 : 0);   // 1
    dbg::row("rd00 =", r0.ok() ? 1 : 0);   // 1
    dbg::row("val00=", r0.value);          // 0 (IODIRA=0)
    dbg::row("wrFF =", wF.ok() ? 1 : 0);   // 1
    dbg::row("rdFF =", rF.ok() ? 1 : 0);   // 1
    dbg::row("valFF=", rF.value);          // 255 (IODIRA=0xFF)
    dbg::flush();
}

void loop() {}