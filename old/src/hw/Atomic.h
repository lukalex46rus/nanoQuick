#pragma once
#include <avr/interrupt.h>
#include <stdint.h>

namespace hw {

// RAII-критическая секция. Восстанавливает I-бит, а не включает безусловно.
class Guard {
public:
    inline Guard() : sreg_(SREG) { cli(); }
    inline ~Guard()              { SREG = sreg_; }
private:
    uint8_t sreg_;
    Guard(const Guard&);
    Guard& operator=(const Guard&);
};

// Чтение многобайтовой переменной, разделяемой с ISR.
template<typename T>
inline T load(const volatile T& src) {
    Guard g;
    return (T)src;
}

template<typename T>
inline void store(volatile T& dst, T v) {
    Guard g;
    dst = v;
}

} // namespace hw