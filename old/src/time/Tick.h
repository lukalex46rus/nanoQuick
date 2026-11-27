#pragma once
#include <avr/io.h>
#include <avr/interrupt.h>
#include "../hw/Mcu.h"
#include "../hw/Atomic.h"

namespace time {

// ---- Миллисекундный тик --------------------------------------------------
// Timer2, CTC, TOP = OCR2A, prescaler 64.
// Период: (OCR2A + 1) × 64 = 250 × 64 = 16000 тактов = 1000 мкс @16 МГц.
// Выводы OC2A/OC2B свободны: биты COM2Ax не установлены.
//
// Datasheet ATmega328P, таблица 17-9 (Timer2 Prescaler):
//   CS22=1, CS21=0, CS20=0 → prescaler 64
//   (_BV(CS21)|_BV(CS20) = 0x03 даёт prescaler 32, НЕ 64)
constexpr uint16_t TICKS_PER_MS = F_CPU / 64UL / 1000UL;      // 250
constexpr uint8_t  OCR2A_VALUE  = (uint8_t)(TICKS_PER_MS - 1U); // 249

static_assert(TICKS_PER_MS <= 256UL, "F_CPU too high for 1 ms on Timer2 presc 64");
static_assert((F_CPU % 64000UL) == 0, "F_CPU not divisible: 1 ms tick has error");

// ---- Субмиллисекундная шкала --------------------------------------------
// TCNT2 считает такты prescaler'а внутри текущей миллисекунды: 0..249.
// Один отсчёт = 64 такта = 4 мкс @16 МГц.
constexpr uint8_t SUB_US = (uint8_t)(64000000UL / F_CPU);     // 4

namespace detail { extern volatile uint32_t counter; }

inline void init() {
    TCCR2A = _BV(WGM21);            // CTC, TOP = OCR2A, выводы отключены
    TCCR2B = 0;                     // стоп на время настройки
    OCR2A  = OCR2A_VALUE;           // 249
    TCNT2  = 0;
    TIFR2  = _BV(OCF2A);            // сброс возможного висевшего флага
    TIMSK2 = _BV(OCIE2A);           // только COMPA, COMPB и OVF запрещены
    TCCR2B = _BV(CS22);             // prescaler 64 -> старт
}

HW_INLINE uint32_t now() { return hw::load(detail::counter); }

// Атомарная пара (мс, отсчёт внутри мс).
// Если compare match совпал с чтением, но ISR ещё не инкрементировал counter,
// флаг OCF2A поднимет m на единицу. Условие t < OCR2A_VALUE отсекает
// случай, когда ISR уже отработал (TCNT2 ушёл вперёд, флаг сброшен аппаратно).
HW_INLINE void raw(uint32_t& m, uint8_t& t) {
    uint8_t sreg = SREG;
    cli();
    m = detail::counter;
    t = TCNT2;
    if ((TIFR2 & _BV(OCF2A)) && (t < OCR2A_VALUE)) ++m;
    SREG = sreg;
}

// Микросекундный счётчик. Разрешение 4 мкс, переполнение ~4.77 часа.
// Без умножений: ms×1000 = (ms<<10) − (ms<<4) − (ms<<3).
HW_INLINE uint32_t micros() {
    uint32_t m; uint8_t t;
    raw(m, t);
    return (m << 10) - (m << 4) - (m << 3) + ((uint32_t)t << 2);
}

// Короткая шкала для таймаутов периферии. Единица = 4 мкс.
// Переполнение через 262 мс → годится для интервалов < 131 мс.
HW_INLINE uint16_t subNow() {
    uint32_t m; uint8_t t;
    raw(m, t);
    return (uint16_t)((uint16_t)m * (uint16_t)TICKS_PER_MS + t);
}

constexpr uint32_t ms(uint32_t v)  { return v; }
constexpr uint32_t sec(uint32_t v) { return v * 1000UL; }
constexpr uint16_t us(uint16_t v)  { return (uint16_t)(v / SUB_US); }

} // namespace time