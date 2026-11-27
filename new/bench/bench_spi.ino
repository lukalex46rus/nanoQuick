#include <Arduino.h>
#include "src/time/Tick.h"
#include "src/bus/Spi.h"
#include "dbg.h"

// ============================================================================
// bench_spi — SPCR/SPSR, скорость 4/8 МГц, loopback через джампер D11-D12.
// Платформа: ATmega328P/328, Nano. Контекст: main.
// Ожидание: SPCR4=0x50 (SPE|MSTR), SPSR4=0; rx4=0xA5, rx2=0x3C при джампере
// MOSI->MISO. Без джампера rx — плавающий/0.
// ============================================================================
using Spi4 = bus::SpiMaster<bus::SpiMode::M0, bus::SpiClk::Div4>;   // 4 МГц
using Spi2 = bus::SpiMaster<bus::SpiMode::M0, bus::SpiClk::Div2>;   // 8 МГц

__attribute__((noinline)) static uint32_t bench4(uint16_t n) {
    uint32_t t0 = time::micros();
    for (uint16_t i = 0; i < n; ++i) (void)Spi4::transfer((uint8_t)i);
    return time::micros() - t0;
}
__attribute__((noinline)) static uint32_t bench2(uint16_t n) {
    uint32_t t0 = time::micros();
    for (uint16_t i = 0; i < n; ++i) (void)Spi2::transfer((uint8_t)i);
    return time::micros() - t0;
}

void setup() {
    dbg::begin(9600);
    time::init();

    Spi4::init();
    uint8_t  spcr4 = SPCR, spsr4 = SPSR;
    uint32_t d4 = bench4(1000);
    uint8_t  rx4 = Spi4::transfer(0xA5);
    Spi4::deinit();

    Spi2::init();
    uint32_t d2 = bench2(1000);
    uint8_t  rx2 = Spi2::transfer(0x3C);
    Spi2::deinit();

    dbg::text("\r\n=== T2: SPI master ===\r\n");
    dbg::row("SPCR4 =", spcr4);        // 0x50 = SPE|MSTR
    dbg::row("SPSR4 =", spsr4);        // 0
    dbg::row("us_4  =", d4 / 1000);    // ~2 мкс/байт (+overhead)
    dbg::row("us_2  =", d2 / 1000);    // ~1 мкс/байт
    dbg::row("rx4   =", rx4);          // 0xA5=165 при джампере D11-D12
    dbg::row("rx2   =", rx2);          // 0x3C=60 при джампере
    dbg::flush();
}

void loop() {}