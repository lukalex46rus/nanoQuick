#include <Arduino.h>
#include "../src/time/Tick.h"
#include "../src/bus/SoftUart.h"
#include "dbg.h"

// ============================================================================
// bench_softuart — SoftUartTx<D2,9600> + SoftUartRx<D3,9600> loopback.
// Платформа: ATmega328P/328, Nano. Контекст: main.
// Джампер D2-D3 для RX-теста. Без джампера rx_ok=0 (Info, не FAIL).
// Ожидание: tx_us ~1041 (10 бит × 104.17 мкс @9600), lb_val=165 (0xA5),
//           pat_ok=1 (паттерн 0x55/0xAA/0x00/0xFF принят верно).
// Калибровка: если tx_us сильно отличается от 1041 — править TX_OVERHEAD.
// ============================================================================
using Tx = bus::SoftUartTx<hw::board::D2, 9600>;
using Rx = bus::SoftUartRx<hw::board::D3, 9600>;

__attribute__((noinline)) static uint32_t txTime(uint8_t b) {
    uint32_t t0 = time::micros();
    Tx::put(b);
    return time::micros() - t0;
}

void setup() {
    dbg::begin(9600);
    time::init();

    Tx::init();
    Rx::init();

    // Замер времени байта (0x55 = alternating bits, удобно для scope)
    uint32_t dt = txTime(0x55);

    // Loopback: отправить 0xA5, принять
    Tx::put(0xA5);
    base::Result<uint8_t> lb = Rx::get(10);

    // Паттерны: 0x55, 0xAA, 0x00, 0xFF
    uint8_t patterns[] = { 0x55, 0xAA, 0x00, 0xFF };
    uint8_t patOk = 1;
    for (uint8_t i = 0; i < 4; ++i) {
        Tx::put(patterns[i]);
        base::Result<uint8_t> r = Rx::get(10);
        if (!r.ok() || r.value != patterns[i]) patOk = 0;
    }

    dbg::text("\r\n=== Y2: SoftUART 9600 ===\r\n");
    dbg::row("tx_us  =", dt);              // ~1041
    dbg::row("lb_ok  =", lb.ok() ? 1 : 0); // 1 с джампером
    dbg::row("lb_val =", lb.value);         // 165 с джампером
    dbg::row("pat_ok =", patOk);            // 1 с джампером
    dbg::flush();
}

void loop() {}