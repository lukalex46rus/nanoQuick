#include <Arduino.h>
#include "src/hw/Atomic.h"
#include "src/time/Tick.h"
#include "src/base/Status.h"
#include "dbg.h"

// ============================================================================
// bench_atomic — load/store/Guard: компиляционные trait-проверки + рантайм.
// Платформа: ATmega328P/328, Nano. Контекст: main.
// Ожидание: все static_assert проходят на сборке; r32=0xDEADBEEF, r8=0xA5,
// enum_rt=2 (Timeout), mono=1 (now() монотонен под ISR).
// ============================================================================

// Компиляционные проверки trait (доказательство на этапе сборки):
static_assert(hw::detail::is_integral<uint8_t>::value,  "u8 integral");
static_assert(hw::detail::is_integral<uint32_t>::value, "u32 integral");
static_assert(hw::detail::is_integral<bool>::value,     "bool integral");
static_assert(hw::detail::is_integral<char>::value,     "char integral");
static_assert(!hw::detail::is_integral<float>::value,   "float excluded");
static_assert(__is_enum(base::Err),                     "enum builtin works");

void setup() {
    dbg::begin(9600);
    time::init();

    // 32-бит: load/store через Guard
    volatile uint32_t v32 = 0;
    hw::store(v32, 0xDEADBEEFUL);
    uint32_t r32 = hw::load(v32);

    // 8-бит: специализация без cli. Каст: 0xA5 — int, v8 — uint8_t;
    // store(volatile T&, T) требует единый T (строгий контракт, без сужения).
    volatile uint8_t v8 = 0;
    hw::store(v8, (uint8_t)0xA5);
    uint8_t r8 = hw::load(v8);

    // enum через load/store (под Guard, 1 байт — но корректно)
    volatile base::Err ve = base::Err::Ok;
    hw::store(ve, base::Err::Timeout);
    base::Err re = hw::load(ve);

    // load(now) не рвётся ISR: 1000 чтений, монотонность
    uint32_t prev = time::now();
    uint8_t  mono = 1;
    for (uint16_t i = 0; i < 1000; ++i) {
        uint32_t n = time::now();
        if ((int32_t)(n - prev) < 0) mono = 0;
        prev = n;
    }

    dbg::text("\r\n=== X2: Atomic ===\r\n");
    dbg::row("r32     =", r32);            // 0xDEADBEEF
    dbg::row("r8      =", r8);             // 0xA5
    dbg::row("enum_rt =", (uint8_t)re);    // 2 (Timeout)
    dbg::row("mono    =", mono);           // 1
    dbg::flush();
}

void loop() {}