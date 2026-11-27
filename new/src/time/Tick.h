#pragma once

// ============================================================================
// Tick — системный тик 1 мс на Timer2 (CTC, prescaler 64).
// ----------------------------------------------------------------------------
// Платформа: ATmega328P/328, Arduino Nano (guard: hw/Mcu.h).
// Ресурсы:   Timer2 целиком (TCCR2A/B, OCR2A, TCNT2, OCIE2A), TIMER2_COMPA_vect.
//            OC2A/OC2B (D11/D3) как GPIO свободны (COM2Ax не установлены),
//            но analogWrite(3)/analogWrite(11) не работают, пока жив Tick.
// Предусл.:  time::init() до now()/micros()/subNow()/raw(); sei() — core/пользователь
//            (в Arduino уже есть; для bare-metal вызвать до init()).
// Контекст:  now/micros/subNow/raw — main+ISR; init — только main.
// Огранич.:  F_CPU 8/16 МГц (guard в Mcu.h + static_assert ниже); шаг subNow
//            4 мкс (16M) / 8 мкс (8M); без калибровки/компенсации.
//            raw() нельзя вызывать до time::init() (counter не двигается,
//            TIFR2 взводится — raw() вернёт завышенное m).
// Зависит:   hw/Mcu.h, hw/Atomic.h.
// ============================================================================
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

// Ограничение железа: OCR2A — 8 бит.
static_assert(OCR2A_VALUE <= 255, "OCR2A overflow: F_CPU too high for Timer2 presc 64");
// Политика Mcu.h уже запретила не-8/16 МГц; это второй слой — математика тика:
// целая миллисекунда. Если политика когда-то ослабнет, ограничение останется.
static_assert((F_CPU % 64000UL) == 0, "F_CPU not divisible: 1 ms tick has error");

// ---- Субмиллисекундная шкала --------------------------------------------
// TCNT2 считает такты prescaler'а внутри текущей миллисекунды: 0..249.
// Один отсчёт = 64 такта = 4 мкс @16 МГц (8 мкс @8 МГц).
constexpr uint8_t SUB_US = (uint8_t)(64000000UL / F_CPU);     // 4 @16M, 8 @8M

namespace detail { extern volatile uint32_t counter; }

inline void init() {
    // Глушим прерывания таймера до настройки: если init() вызван повторно
    // (или после чужой инициализации Timer2), ISR не должен выстрелить на
    // середине переустановки TCNT2/OCR2A.
    TIMSK2 = 0;
    TCCR2A = _BV(WGM21);            // CTC, TOP = OCR2A, выводы отключены
    TCCR2B = 0;                     // стоп на время настройки
    OCR2A  = OCR2A_VALUE;           // 249
    TCNT2  = 0;
    TIFR2  = _BV(OCF2A);            // сброс возможного висевшего флага
    TIMSK2 = _BV(OCIE2A);           // только COMPA, COMPB и OVF запрещены
    TCCR2B = _BV(CS22);             // prescaler 64 -> старт
    // sei() не вызываем: init() не меняет глобальное состояние. В Arduino
    // core sei() уже вызван; для bare-metal — ответственность вызывающего.
}

HW_INLINE uint32_t now() { return hw::load(detail::counter); }

// Атомарная пара (мс, отсчёт внутри мс). Предусловие: time::init() вызван.
// Если compare match совпал с чтением, но ISR ещё не инкрементировал counter,
// флаг OCF2A поднимет m на единицу. Условие t < OCR2A_VALUE отсекает
// случай, когда ISR уже отработал: в CTC при TCNT2 == OCR2A таймер
// сбрасывается в 0 в том же такте, поэтому t == OCR2A_VALUE гарантированно
// означает «ISR ещё не отработал» (флаг взведён или ISR в очереди).
HW_INLINE void raw(uint32_t& m, uint8_t& t) {
    uint8_t sreg = SREG;
    cli();
    m = detail::counter;
    t = TCNT2;
    if ((TIFR2 & _BV(OCF2A)) && (t < OCR2A_VALUE)) ++m;
    SREG = sreg;
}

// Микросекундный счётчик. Разрешение 4 мкс (16M) / 8 мкс (8M),
// переполнение ~1.19 часа (2^32 мкс, не 4.77 ч — 4.77 ч это шкала
// отсчётов по 4 мкс, subNow-подобная). Без умножений:
// ms×1000 = (ms<<10) − (ms<<4) − (ms<<3). t * SUB_US: компилятор свернёт
// умножение на константу в сдвиг.
HW_INLINE uint32_t micros() {
    uint32_t m; uint8_t t;
    raw(m, t);
    return (m << 10) - (m << 4) - (m << 3) + ((uint32_t)t * SUB_US);
}

// Короткая шкала для таймаутов периферии. Единица = SUB_US (4 мкс @16M).
// Осознанно младшие 16 бит от micros()/SUB_US без 32-битных операций:
// переполнение ~262 мс → годится для интервалов < 131 мс.
HW_INLINE uint16_t subNow() {
    uint32_t m; uint8_t t;
    raw(m, t);
    return (uint16_t)((uint16_t)m * (uint16_t)TICKS_PER_MS + t);
}

constexpr uint32_t ms(uint32_t v)  { return v; }
constexpr uint32_t sec(uint32_t v) { return v * 1000UL; }
constexpr uint16_t us(uint16_t v)  { return (uint16_t)(v / SUB_US); }

} // namespace time