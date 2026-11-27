#include "AdcStream.h"
#include <avr/interrupt.h>   // ISR()

namespace adc { namespace detail {
    volatile uint16_t buf[BUF_N];
    volatile uint8_t  head   = 0;
    uint8_t           tail   = 0;
    uint32_t          missed = 0;
}}

// ISR минимальна: только запись выборки и инкремент head.
ISR(ADC_vect) {
    uint16_t v = ADC;                       // ADCL->ADCH
    uint8_t  h = adc::detail::head;
    adc::detail::buf[h & adc::detail::BUF_MASK] = v;
    adc::detail::head = (uint8_t)(h + 1);
}