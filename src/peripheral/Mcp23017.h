#pragma once

// ============================================================================
// Mcp23017 — 16-битный расширитель GPIO по I2C (Microchip MCP23017).
// ----------------------------------------------------------------------------
// Платформа: ATmega328P/328, Arduino Nano (guard: hw/Mcu.h через I2c.h).
// Ресурсы:   шина I2C (BUS), адрес ADDR (0x20..0x27, задаётся A2/A1/A0).
//            0 байт RAM: все методы static, состояние — у consumer.
//            Несколько чипов = несколько типов Mcp23017<ADDR,BUS>.
// Предусл.:  BUS::init() до init(); time::init() до операций (таймауты I2c).
//            init() форсирует IOCON=0 (BANK=0, SEQOP=0) — карта регистров ниже.
// Контекст:  main (polling); не из ISR (I2c блокирующий).
// Огранич.:  Только BANK=0 (init форсирует). Sequential mode (SEQOP=0):
//            чтение GPIOA защёлкивает ОБА порта → read16() атомарный снимок.
//            setDir: бит=1 — INPUT (конвенция MCP, противоположно AVR DDR).
//            Прерывания INTA/INTB не используются (polling). Hardware address
//            enable (HAEN) не включается — адрес задаётся пинами физически.
//            init() НЕ сбрасывает OLAT (защёлку). После write16(X) + init(true)
//            пины остаются входами, но при следующем setDir(0) сразу драйвят X.
//            Для безопасного состояния: write16(0x0000) перед init().
//            read16() читает GPIO (состояние пинов). Для выходов GPIO=OLAT только
//            при отсутствии внешней нагрузки. Детерминированно — readReg(OLATA/B).
// Зависит:   bus/I2c.h, base/Status.h, hw/Mcu.h.
// ============================================================================
#include "../hw/Mcu.h"
#include "../base/Status.h"
#include "../bus/I2c.h"

namespace peripheral {

// Datasheet MCP23017 (DS20001952), таблица 3-5, BANK=0.
// Sequential mode (IOCON.SEQOP=0, default): адресный указатель авто-инкрементируется,
// чтение GPIOA защёлкивает GPIOA+GPIOB одновременно (раздел 3.5).
namespace mcp_reg {
    constexpr uint8_t IODIRA = 0x00;   // direction A: 1=input, 0=output
    constexpr uint8_t IODIRB = 0x01;
    constexpr uint8_t GPPUA  = 0x0C;   // pull-up A (100к типично)
    constexpr uint8_t GPPUB  = 0x0D;
    constexpr uint8_t GPIOA  = 0x12;   // port value A (read latches both)
    constexpr uint8_t GPIOB  = 0x13;
    constexpr uint8_t OLATA  = 0x14;   // output latch A
    constexpr uint8_t OLATB  = 0x15;
    constexpr uint8_t IOCON  = 0x0A;   // config: BANK/SEQOP/MIRROR/...
}

template<uint8_t ADDR, typename BUS>
struct Mcp23017 {
    static_assert(ADDR >= 0x20 && ADDR <= 0x27,
                  "MCP23017 address must be 0x20..0x27 (A2/A1/A0 pins)");

    // ---- Инициализация ----------------------------------------------------
    // Форсирует IOCON=0 (BANK=0, SEQOP=0), все пины INPUT, pull-up опционально.
    // Fail-fast: первая ошибка I2C возвращается немедленно.
    static base::Status init(bool pullUp = false) {
        base::Status s;

        // IOCON = 0: BANK=0, SEQOP=0 (sequential), остальное default.
        s = writeReg(mcp_reg::IOCON, 0x00);
        if (!s.ok()) return s;

        // Все пины INPUT (IODIRA/B = 0xFF).
        s = write16Reg(mcp_reg::IODIRA, 0xFFFF);
        if (!s.ok()) return s;

        // Pull-up на всех входах (или снять).
        s = write16Reg(mcp_reg::GPPUA, pullUp ? 0xFFFF : 0x0000);
        return s;
    }

    // ---- Чтение -----------------------------------------------------------
    // Атомарный 16-битный снимок: GPIOA защёлкивает оба порта (SEQOP=0).
    // low byte = port A (GP A0..A7), high byte = port B (GP B0..B7).
    static base::Result<uint16_t> read16() {
        uint8_t reg = mcp_reg::GPIOA;
        uint8_t buf[2] = { 0, 0 };
        base::Status s = BUS::writeRead(ADDR, &reg, 1, buf, 2, BUS::msFor(3));
        if (!s.ok()) return base::Result<uint16_t>{ 0, s };
        uint16_t v = (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
        return base::Result<uint16_t>{ v, base::ok() };
    }

    // Одиночный порт (экономит 1 байт обмена, если нужен только A или B).
    static base::Result<uint8_t> readA() { return readReg(mcp_reg::GPIOA); }
    static base::Result<uint8_t> readB() { return readReg(mcp_reg::GPIOB); }

    // ---- Запись -----------------------------------------------------------
    // Защёлка выхода OLATA/B. Смысл только для пинов, настроенных как OUTPUT.
    static base::Status write16(uint16_t val) {
        return write16Reg(mcp_reg::OLATA, val);
    }

    // Направление: бит=1 — INPUT, бит=0 — OUTPUT (конвенция MCP).
    // 0xFFFF = все входы, 0x0000 = все выходы.
    static base::Status setDir(uint16_t mask) {
        return write16Reg(mcp_reg::IODIRA, mask);
    }

    // Pull-up: бит=1 — включить (только для INPUT пинов).
    static base::Status setPullUp(uint16_t mask) {
        return write16Reg(mcp_reg::GPPUA, mask);
    }

    // ---- Низкоуровневый доступ (для редких регистров) ---------------------
    static base::Status writeReg(uint8_t reg, uint8_t val) {
        uint8_t buf[2] = { reg, val };
        return BUS::write(ADDR, buf, 2, BUS::msFor(2));
    }

    static base::Result<uint8_t> readReg(uint8_t reg) {
        uint8_t v = 0;
        base::Status s = BUS::writeRead(ADDR, &reg, 1, &v, 1, BUS::msFor(2));
        if (!s.ok()) return base::Result<uint8_t>{ 0, s };
        return base::Result<uint8_t>{ v, base::ok() };
    }

private:
    // Sequential write: {reg, lo, hi} → регистр и следующий за ним.
    // Работает для пар A/B (IODIRA→IODIRB, GPPUA→GPPUB, OLATA→OLATB).
    static base::Status write16Reg(uint8_t reg, uint16_t val) {
        uint8_t buf[3] = { reg, (uint8_t)(val & 0xFF), (uint8_t)(val >> 8) };
        return BUS::write(ADDR, buf, 3, BUS::msFor(3));
    }
};

} // namespace peripheral