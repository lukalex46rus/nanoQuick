#include "Tick.h"

namespace time { namespace detail {
    volatile uint32_t counter = 0;
}}

// Диагностика: считаем входы в каждый вектор Timer2 отдельно.
volatile uint32_t isrCompA = 0;
volatile uint32_t isrCompB = 0;
volatile uint32_t isrOvf   = 0;

ISR(TIMER2_COMPA_vect) {
    ++isrCompA;
    ++time::detail::counter;
}

ISR(TIMER2_COMPB_vect) {
    ++isrCompB;
    ++time::detail::counter;      // если сюда заходим — вот источник удвоения
}

ISR(TIMER2_OVF_vect) {
    ++isrOvf;
    ++time::detail::counter;
}