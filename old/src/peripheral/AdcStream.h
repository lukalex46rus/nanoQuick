#pragma once
#include <avr/io.h>
#include "../hw/Mcu.h"
#include "../base/Status.h"
#include "Adc.h"   // Ref, Clk, каналы

namespace adc {

namespace detail {
    // Единственное место смены размера буфера.
    constexpr uint8_t BUF_LOG2 = 6;
    constexpr uint8_t BUF_N    = (uint8_t)(1u << BUF_LOG2);
    constexpr uint8_t BUF_MASK = (uint8_t)(BUF_N - 1u);

    extern volatile uint16_t buf[BUF_N];
    extern volatile uint8_t  head;   // пишет ISR
    extern uint8_t           tail;   // пишет consumer
    extern uint32_t          missed; // пишет consumer

    // Ресинхронизация при переполнении (consumer-side).
    inline void sync() {
        uint8_t h = head;                       // 8-бит чтение атомарно
        uint8_t a = (uint8_t)(h - tail);
        if (a > BUF_N) {
            missed += (uint8_t)(a - BUF_N);
            tail = (uint8_t)(h - BUF_N);
        }
    }
}

constexpr uint16_t divOf(Clk c) {
    return c == Clk::Div2  ? 2u  : c == Clk::Div4  ? 4u  :
           c == Clk::Div8  ? 8u  : c == Clk::Div16 ? 16u :
           c == Clk::Div32 ? 32u : c == Clk::Div64 ? 64u : 128u;
}

// Один канал, free-running. Единовладелец ADC (не совмещать с Adc).
template<Ref REF, Clk CLK, uint8_t CH>
struct AdcStream {
    // Free-running период = 14 тактов ADC (замер O3: 125000/8920≈14.01),
    // в отличие от single-shot = 13. Авто-триггер добавляет один такт.
    static constexpr uint32_t SAMPLE_RATE = F_CPU / divOf(CLK) / 14u;

    static void init(uint8_t didrMask = 0) {
        ADMUX  = (uint8_t)REF | (CH & 0x0F);
        ADCSRB = 0;                              // ADTS=000 -> free-running
        DIDR0  = didrMask;
        ADCSRA = _BV(ADEN) | _BV(ADIE) | (uint8_t)CLK;
        detail::head = 0; detail::tail = 0; detail::missed = 0;
    }

    static void start() { ADCSRA |= _BV(ADATE) | _BV(ADSC); }
    static void stop()  { ADCSRA &= (uint8_t)~(_BV(ADATE) | _BV(ADIE)); }

    static uint8_t available() {
        detail::sync();
        return (uint8_t)(detail::head - detail::tail);
    }

    static base::Result<uint16_t> take() {
        detail::sync();
        if (detail::head == detail::tail)
            return base::Result<uint16_t>{ 0, base::err(base::Err::NotReady) };
        uint16_t v = detail::buf[detail::tail & detail::BUF_MASK];
        detail::tail = (uint8_t)(detail::tail + 1);
        return base::Result<uint16_t>{ v, base::ok() };
    }

    static uint8_t read(uint16_t* dst, uint8_t n) {
        uint8_t k = 0;
        while (k < n) {
            base::Result<uint16_t> r = take();
            if (!r.ok()) break;
            dst[k++] = r.value;
        }
        return k;
    }

    static uint32_t missed() { return detail::missed; }
};

} // namespace adc