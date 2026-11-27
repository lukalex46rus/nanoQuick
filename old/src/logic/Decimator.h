#pragma once
#include <stdint.h>
#include "../base/Status.h"

namespace logic {

// Oversampling decimator: сумма 4^EXTRA выборок, >> EXTRA -> (10+EXTRA) бит.
// EXTRA 0..4 (N=1..256). Даёт разрешение, не точность; нужен ~1 LSB шума.
template<uint8_t EXTRA>
struct Decimator {
    static_assert(EXTRA <= 4, "EXTRA 0..4");
    static constexpr uint16_t N = (uint16_t)(1u << (2 * EXTRA));

    uint32_t sum;
    uint16_t cnt;

    constexpr Decimator() : sum(0), cnt(0) {}

    // Подать выборку; ok когда блок из N выборок децимирован.
    base::Result<uint16_t> feed(uint16_t s) {
        sum += s;
        if (++cnt < N)
            return base::Result<uint16_t>{ 0, base::err(base::Err::NotReady) };
        uint16_t v = (uint16_t)(sum >> EXTRA);
        sum = 0; cnt = 0;
        return base::Result<uint16_t>{ v, base::ok() };
    }

    void reset() { sum = 0; cnt = 0; }
};

} // namespace logic