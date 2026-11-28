// ============================================================================
// Tick.cpp — ISR Timer2 и состояние счётчика; определение detail::counter.
// ----------------------------------------------------------------------------
// Платформа: ATmega328P/328, Arduino Nano (guard: hw/Mcu.h через Tick.h).
// Ресурсы:   TIMER2_COMPA_vect; volatile uint32_t counter (4 байта .bss).
// Предусл.:  time::init() до любого чтения времени; собирается вместе с Tick.h.
// Контекст:  ISR — прерывания запрещены на входе (Guard не нужен);
//            counter читается только через hw::load / raw().
// Огранич.:  ISR без ISR_NOBLOCK (короткая, ++counter); состояние модуля —
//            только в этом TU, снаружи — через API Tick.h.
// Зависит:   time/Tick.h, <avr/interrupt.h> (через Tick.h).
// ============================================================================
#include "Tick.h"

namespace time { namespace detail {
    volatile uint32_t counter = 0;
}}

// Timer2 COMPA. ISR уже критическая секция (прерывания выключены на входе),
// Guard не нужен. Атомарность относительно main обеспечивает hw::load()
// на стороне читателя. ISR короткая (++counter), ISR_NOBLOCK не нужен.
ISR(TIMER2_COMPA_vect) {
    ++time::detail::counter;
}