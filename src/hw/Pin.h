#pragma once

// ============================================================================
// Pin — типизированный пин (порт + бит) с нулевым состоянием.
// ----------------------------------------------------------------------------
// Платформа: ATmega328P/328, Arduino Nano (guard: hw/Mcu.h).
// Ресурсы:   один бит GPIO (DDRx/PORTx/PINx соответствующего порта).
// Предусл.:  нет; init() задаёт направление и начальный уровень.
// Контекст:  main; из ISR — только toggle (аппаратный) или если порт не
//            трогается в main / под Guard (set/clr — RMW, см. Mcu.h).
// Огранич.:  Только пины 0..7 одного порта. Прерываний (INTx/PCINTx) нет —
//            для них отдельный модуль. board:: — карта Nano (D0..A5), не
//            «Arduino вообще»: на других платах пины другие.
//            D0/D1 — USART (RX/TX): не трогать при активном Uart.
// Зависит:   hw/Mcu.h.
// ============================================================================
#include "Mcu.h"

namespace hw {

enum class Dir : uint8_t { In, InPullUp, Out };
enum class Level : uint8_t { Low = 0, High = 1 };

// Пин = порт + бит. Ноль байт состояния, все методы static.
template<typename Port, uint8_t Bit>
struct Pin {
    static_assert(Bit < 8, "Pin bit must be 0..7");
    static constexpr uint8_t mask = (uint8_t)(1u << Bit);

    // Порядок для Out: сначала уровень в PORTx (пин ещё вход, Hi-Z/pull-up),
    // потом DDR->Out: выход сразу драйвит уже установленный уровень, глитча
    // HIGH нет. Порядок dirOut->clr дал бы импульс HIGH, если PORTx был 1
    // (пин был входом с pull-up).
    // Для входа порядок dirIn->set/clr безопасен: выхода нет, переключается
    // только pull-up (High) или tri-state (Low).
    static inline void init(Dir d, Level lv = Level::Low) {
        if (d == Dir::Out) {
            if (lv == Level::High) Port::set(mask); else Port::clr(mask);
            Port::dirOut(mask);
        } else {
            Port::dirIn(mask);
            if (d == Dir::InPullUp || lv == Level::High) Port::set(mask);
            else                                          Port::clr(mask);
        }
    }

    static inline void high()          { Port::set(mask); }
    static inline void low()           { Port::clr(mask); }
    // toggle() на входе переключает pull-up, а не уровень (см. Mcu.h).
    static inline void toggle()        { Port::flip(mask); }
    static inline void write(Level v)  { if (v == Level::High) high(); else low(); }
    static inline void write(bool v)   { if (v) high(); else low(); }
    static inline bool read()          { return (Port::read() & mask) != 0; }
};

// ---- Именованные пины платы Nano ----------------------------------------
// Карта Nano (D0..A5). Определяются здесь один раз, чтобы модули не плодили
// свои D9/PB1. На других платах пины другие — не поддерживается.
namespace board {
    typedef Pin<PortD, 0> D0;   // RX  — USART, не трогать при активном Uart
    typedef Pin<PortD, 1> D1;   // TX  — USART, не трогать при активном Uart
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
    typedef Pin<PortB, 5> D13;  // SCK / LED (LED на Nano)
    typedef Pin<PortC, 0> A0;
    typedef Pin<PortC, 1> A1;
    typedef Pin<PortC, 2> A2;
    typedef Pin<PortC, 3> A3;
    typedef Pin<PortC, 4> A4;   // SDA
    typedef Pin<PortC, 5> A5;   // SCL
}

} // namespace hw