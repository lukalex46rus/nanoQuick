#pragma once
#include <avr/io.h>
#include "../hw/Mcu.h"
#include "../base/Status.h"

namespace bus {

// Datasheet ATmega328P раздел 19 (SPI), таблица 19-2 (такт).
enum class SpiMode : uint8_t {
    M0 = 0,
    M1 = _BV(CPHA),
    M2 = _BV(CPOL),
    M3 = _BV(CPOL) | _BV(CPHA)
};

// Значение кодирует SPI2X (бит 2) и SPR1:0 (биты 1:0).
enum class SpiClk : uint8_t {
    Div4 = 0, Div16 = 1, Div64 = 2, Div128 = 3,
    Div2 = 4, Div8  = 5, Div32 = 6, Div64x = 7
};

constexpr uint8_t spiSpr(SpiClk c) { return (uint8_t)c & 3u; }
constexpr bool    spiX2 (SpiClk c) { return ((uint8_t)c & 4u) != 0; }

constexpr uint16_t spiDiv(SpiClk c) {
    return c == SpiClk::Div4   ? 4u   : c == SpiClk::Div16 ? 16u :
           c == SpiClk::Div64  ? 64u  : c == SpiClk::Div128 ? 128u :
           c == SpiClk::Div2   ? 2u   : c == SpiClk::Div8  ? 8u :
           c == SpiClk::Div32  ? 32u  : 64u;
}

template<SpiMode MODE = SpiMode::M0, SpiClk CLK = SpiClk::Div4>
struct SpiMaster {
    static constexpr uint32_t SCK_HZ = F_CPU / spiDiv(CLK);

    static void init() {
        // SS(PB2) выход+HIGH, MOSI(PB3)/SCK(PB5) выходы, MISO(PB4) вход.
        hw::PortB::dirOut(_BV(PB2) | _BV(PB3) | _BV(PB5));
        hw::PortB::dirIn(_BV(PB4));
        hw::PortB::set(_BV(PB2));
        SPSR = spiX2(CLK) ? _BV(SPI2X) : 0;
        SPCR = _BV(SPE) | _BV(MSTR) | (uint8_t)MODE | spiSpr(CLK);
        (void)SPSR; (void)SPDR;              // сброс SPIF/WCOL
    }

    static void deinit() { SPCR = 0; }

    // Неблокирующий конвейер. Повторный start до ready поднимет WCOL -> IoFault в take.
    static base::Status start(uint8_t b) { SPDR = b; return base::ok(); }
    static bool ready() { return (SPSR & _BV(SPIF)) != 0; }

    static base::Result<uint8_t> take() {
        uint8_t sr = SPSR;
        uint8_t b  = SPDR;                   // чтение SPDR сбрасывает SPIF
        if (sr & _BV(WCOL)) return base::Result<uint8_t>{ 0, base::err(base::Err::IoFault) };
        return base::Result<uint8_t>{ b, base::ok() };
    }

    // Bounded-blocking: максимум 8 SCK. В master такт всегда идёт, зависания нет.
    static uint8_t transfer(uint8_t b) {
        SPDR = b;
        while (!(SPSR & _BV(SPIF))) {}
        return SPDR;
    }

    static void write(const uint8_t* p, uint16_t n) { while (n--) transfer(*p++); }
};

} // namespace bus