#pragma once

// ============================================================================
// Spi — мастер SPI на ATmega328P, polling, фиксированные пины.
// ----------------------------------------------------------------------------
// Платформа: ATmega328P/328, Arduino Nano (guard: hw/Mcu.h).
// Ресурсы:   SPI целиком (SPCR, SPSR, SPDR).
//            SS=PB2 (D10), MOSI=PB3 (D11), MISO=PB4 (D12), SCK=PB5 (D13) —
//            фиксированы (аппаратный SPI 328P). init() делает SS выходом и
//            ставит HIGH — иначе аппаратный SPI может сам уйти в slave.
// Предусл.:  SpiMaster::init() до первого transfer()/start().
// Контекст:  main; не из ISR.
// Огранич.:  Только мастер, mode 0..3, частоты из SpiClk. Управления SS нет:
//            пользователь дёргает SS сам (или через Pin).
//            transfer() без таймаута: при SPE=1 такты генерирует мастер,
//            зависания нет; при SPE=0 повиснет — не вызывать после deinit().
//            start()/take() не защищены от повторного start() до take():
//            WCOL -> IoFault в take(), но данные потеряны.
//            D10 (SS) после init — выход+HIGH, не годится как GPIO-вход.
//            D13 (SCK) мигает встроенным LED на Nano при обмене.
// Зависит:   hw/Mcu.h, base/Status.h.
// ============================================================================
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
// Div64x убран: даёт ту же частоту F/64, что и Div64, но другим кодом
// (SPR=11,SPI2X=1 vs SPR=10,SPI2X=0). Для выбора по частоте избыточен.
enum class SpiClk : uint8_t {
    Div4 = 0, Div16 = 1, Div64 = 2, Div128 = 3,
    Div2 = 4, Div8  = 5, Div32 = 6
};

constexpr uint8_t spiSpr(SpiClk c) { return (uint8_t)c & 3u; }
constexpr bool    spiX2 (SpiClk c) { return ((uint8_t)c & 4u) != 0; }

constexpr uint16_t spiDiv(SpiClk c) {
    return c == SpiClk::Div4   ? 4u   : c == SpiClk::Div16 ? 16u :
           c == SpiClk::Div64  ? 64u  : c == SpiClk::Div128 ? 128u :
           c == SpiClk::Div2   ? 2u   : c == SpiClk::Div8  ? 8u :
           32u;
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

        // Сброс SPIF/WCOL. (void)SPSR/(void)SPDR корректны: SPSR и SPDR —
        // volatile SFR, доступ к volatile — наблюдаемое поведение, компилятор
        // не выкинет чтение даже при -Os.
        (void)SPSR;
        (void)SPDR;
    }

    static void deinit() { SPCR = 0; }

    // Предусловие: init() выполнен и SPE не сброшен извне.
    // Повторный start до ready поднимет WCOL -> IoFault в take().
    static base::Status start(uint8_t b) { SPDR = b; return base::ok(); }
    static bool ready() { return (SPSR & _BV(SPIF)) != 0; }

    static base::Result<uint8_t> take() {
        uint8_t sr = SPSR;
        uint8_t b  = SPDR;                   // чтение SPDR сбрасывает SPIF
        // При WCOL читаем SPDR (иначе SPIF не сбросится), но байт
        // недействителен: слот освобождён, возвращаем IoFault.
        if (sr & _BV(WCOL)) return base::Result<uint8_t>{ 0, base::err(base::Err::IoFault) };
        return base::Result<uint8_t>{ b, base::ok() };
    }

    // Контракт: только после init(), до deinit(). При SPE=1 такты генерирует
    // мастер — зависания нет. При SPE=0 SPIF не взведётся, цикл повиснет.
    static uint8_t transfer(uint8_t b) {
        SPDR = b;
        while (!(SPSR & _BV(SPIF))) {}
        return SPDR;
    }

    // Best effort: у мастера ошибок кроме WCOL нет, Status не возвращаем.
    static void write(const uint8_t* p, uint16_t n) { while (n--) transfer(*p++); }
};

} // namespace bus