#pragma once

// ============================================================================
// Uart — USART0 ATmega328P, polling, 8N1, TX/RX с таймаутами.
// ----------------------------------------------------------------------------
// Платформа: ATmega328P/328, Arduino Nano (guard: hw/Mcu.h).
// Ресурсы:   USART0 целиком (UCSR0A/B/C, UBRR0, UDR0), D0=RX (PD0), D1=TX (PD1).
//            Прерывания USART не использует. Конфликт: Arduino Serial и
//            bootloader живут на USART0 — после Serial.begin() наш init()
//            перезапишет настройки; одновременно не использовать.
// Предусл.:  init() до put/get/write/read; time::init() до операций с
//            таймаутом (toMs > 0). toMs == 0 — одна попытка без ожидания.
// Контекст:  main (polling); не из ISR. Один контекст на экземпляр настроек
//            (USART0 один — два Uart<> с разным baud конфликтуют, как AdcStream).
// Огранич.:  Только 8N1 (формат фиксирован, как Spi mode — осознанно минимально).
//            Только асинхронный режим (UMSEL0=0). baud: UBRR должен влезть
//            в 12 бит (static_assert); ошибка baud — см. BAUD_ERR_PCT.

//            put() ждёт UDRE (буфер передатчика пуст) и пишет в буфер — НЕ гарантия
//            физической передачи. При TXEN=0 UDRE=1 (буфер пуст) → put вернёт ok,
//            байт не уйдёт (тихая потеря). Контракт: только после init (TXEN=1).
//            Физическую передачу гарантирует flush() (TXC). get() с таймаутом:
//            Timeout — данных не пришло; Overflow — FE/DOR/UP повреждены.
//            (value недействителен). read() — best effort до n байт или первого
//            Timeout, возвращает count. 
//            flush() ждёт TXC (последний бит ушёл) — для сна/смены baud.
// Зависит:   hw/Mcu.h, base/Status.h, time/Deadline.h.
// ============================================================================
#include <avr/io.h>
#include "../hw/Mcu.h"
#include "../base/Status.h"
#include "../time/Deadline.h"

namespace bus {

// Datasheet ATmega328P раздел 20 (USART0). Async normal mode (U2X0=0):
//   UBRR = F_CPU/(16*baud) - 1.  U2X0=1: UBRR = F_CPU/(8*baud) - 1.
// Выбираем U2X автоматически: вариант с меньшей ошибкой округления.
// Ошибка baud > ~2% на 8N1 даёт сбои приёма (суммарная по обеим сторонам < ~4.5%).

namespace detail {
// Реальный baud при данном UBRR и U2X.
constexpr uint32_t actualBaud(uint32_t ubrr, bool u2x) {
    return F_CPU / ((u2x ? 8UL : 16UL) * (ubrr + 1UL));
}
// Ошибка в сотых процента относительно запрошенного (без знака).
constexpr uint32_t errPct100(uint32_t want, uint32_t got) {
    return want > got ? ((want - got) * 10000UL) / want
                      : ((got - want) * 10000UL) / want;
}
} // namespace detail

template<uint32_t BAUD = 9600>
struct Uart {
    // Кандидаты UBRR для U2X=0 и U2X=1 (округление к ближайшему).
    static constexpr uint32_t UBRR_N = (F_CPU + 8UL * BAUD) / (16UL * BAUD) - 1UL;
    static constexpr uint32_t UBRR_X = (F_CPU + 4UL * BAUD) / (8UL  * BAUD) - 1UL;

    static constexpr uint32_t ERR_N = detail::errPct100(BAUD, detail::actualBaud(UBRR_N, false));
    static constexpr uint32_t ERR_X = detail::errPct100(BAUD, detail::actualBaud(UBRR_X, true));

    // Выбор варианта с меньшей ошибкой; при равенстве — U2X=0 (меньше помех).
    static constexpr bool     USE_X2   = ERR_X < ERR_N;
    static constexpr uint16_t UBRR_VAL = (uint16_t)(USE_X2 ? UBRR_X : UBRR_N);
    static constexpr uint32_t BAUD_ERR_PCT100 = USE_X2 ? ERR_X : ERR_N; // сотые %

    static_assert(UBRR_VAL <= 4095, "UBRR overflow: baud too low for F_CPU");
    static_assert(BAUD_ERR_PCT100 <= 250, "baud error > 2.5%: pick another baud/F_CPU");
    // Нижний предел: UBRR=0 при U2X=1 -> F_CPU/8 максимум. BAUD выше — UBRR ушёл бы в -1.
    static_assert(F_CPU >= 8UL * BAUD, "baud too high for F_CPU");

    // ---- Режимы -----------------------------------------------------------
    // TX-only: RX не включается (PD0 свободен как GPIO). RX+TX: оба.
    static void init(bool rxEnable = true) {
        UBRR0H = (uint8_t)(UBRR_VAL >> 8);
        UBRR0L = (uint8_t)UBRR_VAL;
        UCSR0A = USE_X2 ? _BV(U2X0) : 0;
        UCSR0B = _BV(TXEN0) | (rxEnable ? _BV(RXEN0) : 0);
        UCSR0C = _BV(UCSZ01) | _BV(UCSZ00);          // 8N1, async
        // Сброс висящих флагов приёма: чтение UDR0 до пустого RXC.
        if (rxEnable) { while (UCSR0A & _BV(RXC0)) (void)UDR0; }
    }

    static void deinit() { UCSR0B = 0; }             // TXEN/RXEN off, пины свободны

    // ---- TX ---------------------------------------------------------------
    // put: ждёт UDRE (буфер передатчика пуст), пишет в UDR0. Запись в буфер,
    // не физическая передача. При TXEN=0 UDRE=1 → ok без передачи (тихая потеря).
    // Контракт: TXEN=1 (после init, до deinit). Физическую передачу — flush().
    static base::Status put(uint8_t b, uint32_t toMs = 5) {
        time::Deadline dl; dl.arm(toMs);
        while (!(UCSR0A & _BV(UDRE0)))
            if (dl.expired()) return base::err(base::Err::Timeout);
        UDR0 = b;
        return base::ok();
    }

    // write: n байт, toMs — бюджет на ВЕСЬ блок (как I2c). Для длинных блоков
    // увеличивать: байт 8N1 = 10 бит, msFor(n) — калькулятор.
    static base::Status write(const uint8_t* p, uint16_t n, uint32_t toMs = 5) {
        time::Deadline dl; dl.arm(toMs);
        while (n--) {
            while (!(UCSR0A & _BV(UDRE0)))
                if (dl.expired()) return base::err(base::Err::Timeout);
            UDR0 = *p++;
        }
        return base::ok();
    }

    static void write(const char* s, uint32_t toMs = 5) {
        while (*s) {
            if (!put((uint8_t)*s++, toMs).ok()) return;
        }
    }

    // flush: ждёт TXC — последний бит физически ушёл. Для сна/смены baud/deinit.
    static base::Status flush(uint32_t toMs = 5) {
        time::Deadline dl; dl.arm(toMs);
        while (!(UCSR0A & _BV(TXC0)))
            if (dl.expired()) return base::err(base::Err::Timeout);
        UCSR0A |= _BV(TXC0);                          // сброс write-1-clear
        return base::ok();
    }

    // Минимальный toMs для n байт: 10 бит/байт + запас 2 мс. Ноль рантайм-стоимости.
    static constexpr uint16_t msFor(uint16_t n) {
        return (uint16_t)((10UL * n * 1000UL) / BAUD + 2u);
    }

    // ---- RX ---------------------------------------------------------------
    // available: байт в приёмном буфере есть (RXC). Без ожидания.
    static bool available() { return (UCSR0A & _BV(RXC0)) != 0; }

    // get: ждёт байт до toMs. Timeout — не пришло. Overflow — FE/DOR/UP:
    // байт прочитан (буфер освобождён), но повреждён, value недействителен.
    // Флаги читаются ДО UDR0 (чтение UDR сбрасывает их).
    static base::Result<uint8_t> get(uint32_t toMs = 5) {
        time::Deadline dl; dl.arm(toMs);
        while (!(UCSR0A & _BV(RXC0)))
            if (dl.expired()) return base::Result<uint8_t>{ 0, base::err(base::Err::Timeout) };
        uint8_t sr = UCSR0A;
        uint8_t b  = UDR0;
        if (sr & (_BV(FE0) | _BV(DOR0) | _BV(UPE0)))
            return base::Result<uint8_t>{ 0, base::err(base::Err::Overflow) };
        return base::Result<uint8_t>{ b, base::ok() };
    }

    // read: до n байт, каждый с таймаутом toMs. Возвращает число принятых.
    // Повреждённый байт (FE/DOR/UP) пропускается, но останавливает набор —
    // потеря синхронизации кадра важнее количества.
    static uint8_t read(uint8_t* dst, uint8_t n, uint32_t toMs = 5) {
        uint8_t k = 0;
        while (k < n) {
            base::Result<uint8_t> r = get(toMs);
            if (!r.ok()) break;
            dst[k++] = r.value;
        }
        return k;
    }
};

} // namespace bus