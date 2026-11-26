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