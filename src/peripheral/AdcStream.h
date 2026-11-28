#pragma once

// ============================================================================
// AdcStream — непрерывный поток выборок АЦП (free-running) с кольцевым
// буфером на прерываниях.
// ----------------------------------------------------------------------------
// Платформа: ATmega328P/328, Arduino Nano (guard: hw/Mcu.h).
// Ресурсы:   ADC целиком + прерывание ADC_vect (__vector_21 на 328P).
//            Один канал, выбранный шаблонным параметром CH. Единовладелец ADC:
//            не совмещать с Adc без явного stop() и Adc::init().
// Предусл.:  AdcStream::init() до start(); time::init() не требуется.
//            Перед повторным init() после другого владельца ADC — stop().
// Контекст:  init/start/stop — main; take/available/read/missed — main;
//            ISR — ADC_vect (только запись выборки и инкремент head).
// Огранич.:  ОДИН ADC — ОДИН ISR — ОДИН активный поток. AdcStream<REF,CLK,CH>
//            конфигурирует, но не умножает потоки: два экземпляра с разными CH
//            разделят detail::buf/head/tail и будут ломать друг друга —
//            не поддерживается. Для многих каналов — сканирование внутри
//            одного потока.
//            Буфер фиксированного размера BUF_LOG2=6 (64 выборки). При
//            переполнении старые выборки теряются, missed() учитывает
//            потери на стороне consumer.
//            Torn-read при переполнении: ISR может перезаписать 16-битную
//            ячейку во время чтения consumer'ом (половина старого, половина
//            нового) — цена неблокирующего ISR. Потребитель читает быстрее
//            или принимает потери; для строгого чтения — Guard на чтение.
//            SAMPLE_RATE — верхняя оценка периода (14 тактов ADC вместо 13.5),
//            занижает реальную частоту на ~3.7%.
// Зависит:   hw/Mcu.h, base/Status.h, Adc.h, <avr/io.h>.
// ============================================================================
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
    extern uint32_t          missed; // пишет и читает только consumer (main)

    // Ресинхронизация при переполнении (consumer-side).
    // head читается в цикле до стабилизации: ISR может продвинуть head между
    // чтением и записью tail, и тогда tail = h - BUF_N окажется позади
    // реального head сильнее, чем надо -> буфер начнёт отдавать мусор.
    inline void sync() {
        for (;;) {
            uint8_t h1 = head;                       // 8-бит чтение атомарно
            uint8_t a  = (uint8_t)(h1 - tail);
            if (a <= BUF_N) return;                  // полон, но не переполнен
            missed += (uint8_t)(a - BUF_N);
            tail = (uint8_t)(h1 - BUF_N);
            if (head == h1) return;                  // за время записи ничего не пришло
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
    // Точное значение по даташиту — 13.5 такта; здесь 14 — верхняя оценка,
    // SAMPLE_RATE занижен на ~3.7%.
    static constexpr uint32_t SAMPLE_RATE = F_CPU / divOf(CLK) / 14u;

    // Предусловие: ADC не в активной конверсии.
    // ADCSRA = 0 первым: ADEN=0, ADIE=0 — ISR не выстрелит во время init,
    // гонка AS5 закрыта без Guard. Сброс указателей — до включения ADIE.
    // ADIF=1 в финальной записи — сброс висящего флага (write-1-clear):
    // иначе после ADIE=1 возможен мгновенный ложный ISR.
    static void init(uint8_t didrMask = 0) {
        ADCSRA = 0;                              // ADEN=0, ADIE=0, обрыв конверсии
        detail::head = 0; detail::tail = 0; detail::missed = 0;
        DIDR0  = didrMask;
        ADMUX  = (uint8_t)REF | (CH & 0x0F);
        ADCSRB = 0;                              // ADTS=000 -> free-running
        ADCSRA = _BV(ADEN) | _BV(ADIE) | _BV(ADIF) | (uint8_t)CLK;
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

    // missed пишет и читает только main (sync/take/available) — ISR не трогает,
    // 32-битное чтение в одном контексте безопасно, Guard не нужен.
    static uint32_t missed() { return detail::missed; }
};

} // namespace adc