#include "Tick.h"

namespace time { namespace detail {
    volatile uint32_t counter = 0;
}}

ISR(TIMER2_COMPA_vect) {
    ++time::detail::counter;
}