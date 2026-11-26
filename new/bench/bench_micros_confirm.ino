#include <Arduino.h>
#include "../src/time/Tick.h"
#include "dbg.h"

// ============================================================================
// bench_micros_confirm — скорость time::micros() vs core micros() + сверка.
// Платформа: ATmega328P/328, Nano. Контекст: main.
// Ожидание: ours_1k/core_1k — время 1000 вызовов (наши дешевле, без cli/sei
// на uint8_t-специализациях нет, но raw короче core); span_dif ~0 (в пределах
// разрешения 4 мкс + дрейф core).
// ============================================================================
__attribute__((noinline)) static uint32_t benchOurs(uint16_t n) {
    uint32_t acc = 0;
    uint32_t t0 = ::micros();
    for (uint16_t i = 0; i < n; ++i) acc += time::micros();
    uint32_t dt = ::micros() - t0;
    if (acc == 0xFFFFFFFFUL) dbg::byte('!');
    return dt;
}

__attribute__((noinline)) static uint32_t benchCore(uint16_t n) {
    uint32_t acc = 0;
    uint32_t t0 = ::micros();
    for (uint16_t i = 0; i < n; ++i) acc += ::micros();
    uint32_t dt = ::micros() - t0;
    if (acc == 0xFFFFFFFFUL) dbg::byte('!');
    return dt;
}

void setup() {
    dbg::begin(9600);
    time::init();

    const uint16_t N = 1000;
    uint32_t ours = benchOurs(N);
    uint32_t core = benchCore(N);

    uint32_t u0 = time::micros();
    uint32_t c0 = ::micros();
    delay(100);
    uint32_t u1 = time::micros();
    uint32_t c1 = ::micros();

    dbg::text("\r\n=== P0-3: micros confirm ===\r\n");
    dbg::row ("ours_1k =", ours);
    dbg::row ("core_1k =", core);
    dbg::row ("span_our=", u1 - u0);
    dbg::row ("span_cor=", c1 - c0);
    dbg::srow("span_dif=", (int32_t)(u1-u0) - (int32_t)(c1-c0));
    dbg::flush();
}

void loop() {}