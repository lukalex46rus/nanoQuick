#pragma once

// ============================================================================
// Rms — полный RMS (DC+AC) на целочисленной арифметике.
// ----------------------------------------------------------------------------
// Платформа: AVR (через hw/Mcu.h ради HW_INLINE; guard в Mcu.h).
// Ресурсы:   — (только типы; состояние — 6 байт на объект).
// Предусл.:  s <= MAXV (вызывающий не передаёт значение шире BITS).
//            Для оверсэмплированных входов BITS = 10 + EXTRA (см. Decimator).
// Контекст:  — (тип; usable в main+ISR, если сам объект не разделяется).
// Огранич.:  isqrt32: только сдвиги/вычитания, ~15 итераций, floor(sqrt).
//            Максимальная пара (LOG2N, BITS) ограничена static_assert:
//            sumSq = MAXV^2 * N должен влезать в uint32_t (<= 0xFFFFFFFF).
//            RMS считается без вычитания среднего (DC+AC), не AC-only.
//            Маска s &= MAXV не применяется: скрыла бы ошибку ввода.
// Зависит:   hw/Mcu.h, base/Status.h, <stdint.h>.
// ============================================================================
#include <stdint.h>
#include "../hw/Mcu.h"
#include "../base/Status.h"

namespace logic {

// Integer sqrt, digit-by-digit. Только сдвиги/вычитания, без деления.
// Возвращает floor(sqrt(x)). ~15 итераций. inline для ODR в заголовке;
// HW_INLINE не нужен: вызывается раз в блок, не на каждой выборке.
inline uint16_t isqrt32(uint32_t x) {
    uint32_t res = 0;
    uint32_t bit = 1UL << 30;
    while (bit > x) bit >>= 2;
    while (bit) {
        if (x >= res + bit) {
            x   -= res + bit;
            res  = (res >> 1) + bit;
        } else {
            res >>= 1;
        }
        bit >>= 2;
    }
    return (uint16_t)res;
}

// Полный RMS (DC+AC): sqrt(mean(x^2)), без вычитания среднего.
// LOG2N: окно = 2^LOG2N. BITS: разрядность входа (10 raw / 12 / 14 oversampled).
template<uint8_t LOG2N, uint8_t BITS = 10>
struct Rms {
    static_assert(LOG2N <= 12, "N up to 4096");
    static_assert(BITS >= 1 && BITS <= 14, "BITS 1..14");
    static constexpr uint16_t N    = (uint16_t)(1u << LOG2N);
    static constexpr uint32_t MAXV = (1UL << BITS) - 1UL;
    // <= корректен: при равенстве 0xFFFFFFFF сумма помещается точно,
    // переполнение начинается только при > 0xFFFFFFFF.
    static_assert((uint64_t)MAXV * MAXV * N <= 0xFFFFFFFFULL,
                  "sumSq overflow: reduce N or BITS");

    uint32_t sumSq;
    uint16_t cnt;

    constexpr Rms() : sumSq(0), cnt(0) {}

    // Контракт: s <= MAXV. Маска s &= MAXV не применяется — скрыла бы
    // ошибку ввода; проверка — на совести вызывающего.
    HW_INLINE base::Result<uint16_t> feed(uint16_t s) {
        sumSq += (uint32_t)s * s;
        if (++cnt < N)
            return base::Result<uint16_t>{ 0, base::err(base::Err::NotReady) };
        uint32_t ms = sumSq >> LOG2N;
        uint16_t r  = isqrt32(ms);
        sumSq = 0; cnt = 0;
        return base::Result<uint16_t>{ r, base::ok() };
    }

    void reset() { sumSq = 0; cnt = 0; }
};

} // namespace logic