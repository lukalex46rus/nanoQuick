#pragma once

// ============================================================================
// SoftUart — программный UART (bit-bang) на любом GPIO ATmega328P.
// ----------------------------------------------------------------------------
// Платформа: ATmega328P/328, Arduino Nano (guard: hw/Mcu.h).
// Ресурсы:   один GPIO на TX, один GPIO на RX (любые, кроме D0/D1 при активном
//            hardware Uart/Serial). Несколько экземпляров на разных пинах — можно.
//            CPU-bound: прерывания запрещены на время байта (~1 мс @9600).
// Предусл.:  init() до put/get/write; time::init() НЕ требуется (таймаут RX
//            cycle-counted, не зависит от Timer2 ISR).
// Контекст:  main (polling); не из ISR. IRQ off на весь байт TX/RX.
// Огранич.:  Только 8N1, LSB-first. BAUD <= 250000 (практический предел bit-bang).
//            Точность тайминга зависит от TX_OVERHEAD/RX_OVERHEAD (калибровка бенчем).
//            RX get(toMs): IRQ off до toMs + ~1 байт. При toMs=100 → 100 мс без
//            прерываний. Для real-time задач использовать hardware Uart или малый toMs.
//            Таймаут RX approximate (±10%), cycle-counted, не Deadline.
//            __builtin_avr_delay_cycles — compile-time constant, без call overhead.
// Зависит:   hw/Mcu.h, hw/Pin.h, hw/Atomic.h, base/Status.h.
// ============================================================================
#include "../hw/Mcu.h"
#include "../hw/Pin.h"
#include "../hw/Atomic.h"
#include "../base/Status.h"

namespace bus {

// ============================================================================
// SoftUartTx<PIN, BAUD, TX_OVERHEAD>
// PIN — hw::Pin<Port,Bit> (например hw::board::D2).
// TX_OVERHEAD — тактов на sbi/cbi + shift + branch + loop (default 8, калибровать).
// ============================================================================
template<typename PIN, uint32_t BAUD = 9600, uint8_t TX_OVERHEAD = 8>
struct SoftUartTx {
    static constexpr uint32_t BIT_CYCLES = F_CPU / BAUD;
    static constexpr uint32_t BIT_DELAY  = BIT_CYCLES - TX_OVERHEAD;

    static_assert(BAUD <= 250000UL, "SoftUartTx: baud too high for reliable bit-bang");
    static_assert(BIT_CYCLES >= 20UL, "SoftUartTx: BIT_CYCLES too small (overhead margin)");
    static_assert(BIT_DELAY > 0UL,    "SoftUartTx: TX_OVERHEAD >= BIT_CYCLES, reduce overhead");

    // Idle HIGH. Вызывать до первого put().
    static void init() { PIN::init(hw::Dir::Out, hw::Level::High); }

    // Передать байт: start(0) + 8 data(LSB-first) + stop(1).
    // IRQ off на весь байт (~10 бит × BIT_CYCLES). Всегда ok() после завершения.
    static base::Status put(uint8_t b) {
        hw::Guard g;

        // Start bit (LOW)
        PIN::low();
        __builtin_avr_delay_cycles(BIT_DELAY);

        // 8 data bits, LSB first
        for (uint8_t i = 0; i < 8; ++i) {
            if (b & 1) PIN::high(); else PIN::low();
            b >>= 1;
            __builtin_avr_delay_cycles(BIT_DELAY);
        }

        // Stop bit (HIGH)
        PIN::high();
        __builtin_avr_delay_cycles(BIT_DELAY);

        return base::ok();
    }

    // Блок байт. Каждый байт — отдельный Guard (IRQ briefly on between bytes).
    static base::Status write(const uint8_t* p, uint16_t n) {
        while (n--) { put(*p++); }
        return base::ok();
    }

    // Строка. Аналогично write(p,n).
    static base::Status write(const char* s) {
        while (*s) { put((uint8_t)*s++); }
        return base::ok();
    }
};

// ============================================================================
// SoftUartRx<PIN, BAUD, RX_OVERHEAD>
// PIN — hw::Pin<Port,Bit>. RX_OVERHEAD — тактов на read + shift + loop (default 6).
// ============================================================================
template<typename PIN, uint32_t BAUD = 9600, uint8_t RX_OVERHEAD = 6>
struct SoftUartRx {
    static constexpr uint32_t BIT_CYCLES   = F_CPU / BAUD;
    static constexpr uint32_t HALF_BIT     = BIT_CYCLES / 2;
    // После детекта start edge: ждём 1.5 бита до центра D0.
    static constexpr uint32_t FIRST_SAMPLE = BIT_CYCLES + HALF_BIT - RX_OVERHEAD;
    // Между последующими выборками: 1 бит минус overhead.
    static constexpr uint32_t SAMPLE_DELAY = BIT_CYCLES - RX_OVERHEAD;

    static_assert(BAUD <= 250000UL,   "SoftUartRx: baud too high for reliable bit-bang");
    static_assert(BIT_CYCLES >= 20UL, "SoftUartRx: BIT_CYCLES too small");
    static_assert(FIRST_SAMPLE > 0UL, "SoftUartRx: RX_OVERHEAD too large for first sample");
    static_assert(SAMPLE_DELAY > 0UL, "SoftUartRx: RX_OVERHEAD >= BIT_CYCLES");

    // Вход с pull-up (idle HIGH). Внешний pull-up рекомендуется для надёжности.
    static void init() { PIN::init(hw::Dir::InPullUp); }

    // Быстрая проверка: LOW = start bit присутствует. Без ожидания, без IRQ off.
    static bool available() { return !PIN::read(); }

    // Принять байт. toMs — бюджет ожидания start bit (approximate ±10%).
    // toMs=0: одна проверка пина, без polling.
    // IRQ off на всё время get() (ожидание + приём). Не использовать Deadline внутри!
    // Возврат: ok+byte, Timeout (нет start), IoFault (stop bit != HIGH).
    static base::Result<uint8_t> get(uint32_t toMs = 5) {
        // Циклический бюджет ДО Guard (не зависит от Timer2 ISR).
        constexpr uint32_t POLL_STEP = 16;
        uint32_t budget = toMs * (F_CPU / 1000UL);

        hw::Guard g;

        // Ожидание start bit (falling edge → LOW)
        if (toMs == 0) {
            if (PIN::read()) return base::Result<uint8_t>{ 0, base::err(base::Err::Timeout) };
        } else {
            while (PIN::read()) {
                if (budget < POLL_STEP)
                    return base::Result<uint8_t>{ 0, base::err(base::Err::Timeout) };
                budget -= POLL_STEP;
                __builtin_avr_delay_cycles(POLL_STEP - 8);
            }
        }

        // Start bit обнаружен. Ждём центр D0 (1.5 бита от начала start).
        __builtin_avr_delay_cycles(FIRST_SAMPLE);

        // 8 data bits, LSB first
        uint8_t b = 0;
        for (uint8_t i = 0; i < 8; ++i) {
            if (PIN::read()) b |= (1u << i);
            __builtin_avr_delay_cycles(SAMPLE_DELAY);
        }

        // Stop bit должен быть HIGH
        if (!PIN::read())
            return base::Result<uint8_t>{ 0, base::err(base::Err::IoFault) };

        return base::Result<uint8_t>{ b, base::ok() };
    }
};

} // namespace bus