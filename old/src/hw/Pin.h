#pragma once
#include "Mcu.h"

namespace hw {

enum class Dir : uint8_t { In, InPullUp, Out };
enum class Level : uint8_t { Low = 0, High = 1 };

// Пин = порт + бит. Ноль байт состояния, все методы static.
template<typename Port, uint8_t Bit>
struct Pin {
    static_assert(Bit < 8, "Pin bit must be 0..7");
    static const uint8_t mask = (uint8_t)(1u << Bit);

    static inline void init(Dir d) {
        switch (d) {
            case Dir::Out:      Port::dirOut(mask); Port::clr(mask); break;
            case Dir::InPullUp: Port::dirIn(mask);  Port::pullUp(mask); break;
            default:            Port::dirIn(mask);  Port::clr(mask); break;
        }
    }

    static inline void high()          { Port::set(mask); }
    static inline void low()           { Port::clr(mask); }
    static inline void toggle()        { Port::flip(mask); }
    static inline void write(Level v)  { if (v == Level::High) high(); else low(); }
    static inline void write(bool v)   { if (v) high(); else low(); }
    static inline bool read()          { return (Port::read() & mask) != 0; }
};

// ---- Именованные пины платы Nano ----------------------------------------
// Определяются здесь один раз, чтобы модули не плодили свои D9/PB1.
namespace board {
    typedef Pin<PortD, 0> D0;   // RX
    typedef Pin<PortD, 1> D1;   // TX
    typedef Pin<PortD, 2> D2;   // INT0
    typedef Pin<PortD, 3> D3;   // INT1
    typedef Pin<PortD, 4> D4;
    typedef Pin<PortD, 5> D5;
    typedef Pin<PortD, 6> D6;
    typedef Pin<PortD, 7> D7;
    typedef Pin<PortB, 0> D8;   // ICP1
    typedef Pin<PortB, 1> D9;   // OC1A
    typedef Pin<PortB, 2> D10;  // OC1B / SS
    typedef Pin<PortB, 3> D11;  // MOSI
    typedef Pin<PortB, 4> D12;  // MISO
    typedef Pin<PortB, 5> D13;  // SCK / LED
    typedef Pin<PortC, 0> A0;
    typedef Pin<PortC, 1> A1;
    typedef Pin<PortC, 2> A2;
    typedef Pin<PortC, 3> A3;
    typedef Pin<PortC, 4> A4;   // SDA
    typedef Pin<PortC, 5> A5;   // SCL
}

} // namespace hw