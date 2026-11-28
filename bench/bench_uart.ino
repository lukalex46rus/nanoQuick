#include <Arduino.h>
#include "../src/time/Tick.h"
#include "../src/bus/Uart.h"
#include "dbg.h"

// ============================================================================
// bench_uart — Uart<115200> loopback джампером D0-D1 + скорость TX.
// Платформа: ATmega328P/328, Nano. Контекст: main.
// ВНИМАНИЕ: dbg печатает на USART0 (9600). Uart<115200> переинициализирует
// USART0 — после теста возвращаем dbg::begin(9600). Джампер D0-D1 ставить
// ТОЛЬКО на время теста (иначе мешает прошивке через bootloader).
// Ожидание: ubrr ~= 16 (U2X=1 @115200/16M), err_pct100 < 250,
// tx_us ~87/байт (10 бит / 115200), lb_ok=1 при джампере, lb_val=165.
// ============================================================================
using Test = bus::Uart<115200>;

__attribute__((noinline)) static uint32_t txBench(uint16_t n) {
    uint32_t t0 = time::micros();
    for (uint16_t i = 0; i < n; ++i) (void)Test::put((uint8_t)i, Test::msFor(1));
    (void)Test::flush(10);
    return time::micros() - t0;
}

void setup() {
    dbg::begin(9600);
    time::init();

    Test::init(true);                       // RX+TX, 115200
    uint16_t ubrr = UBRR0;
    uint8_t  u2x  = (UCSR0A & _BV(U2X0)) ? 1 : 0;

    uint32_t dt = txBench(100);             // 100 байт

    // Loopback: отправить 0xA5, принять (джампер D0-D1).
    (void)Test::put(0xA5, 5);
    base::Result<uint8_t> lb = Test::get(5);

    Test::deinit();
    dbg::begin(9600);                       // вернуть dbg на 9600

    dbg::text("\r\n=== Y1: UART 115200 ===\r\n");
    dbg::row("ubrr     =", ubrr);                    // 16 (U2X=1)
    dbg::row("u2x      =", u2x);                     // 1
    dbg::row("err_p100 =", Test::BAUD_ERR_PCT100);   // ~212 (2.12%)
    dbg::row("tx_us    =", dt / 100);                // ~87
    dbg::row("lb_ok    =", lb.ok() ? 1 : 0);         // 1 с джампером
    dbg::row("lb_val   =", lb.value);                // 165 с джампером
    dbg::flush();
}

void loop() {}