#pragma once

// ============================================================================
// Платформа: только ATmega328P / ATmega328 (Arduino Nano). Точка правды.
// ============================================================================
// Клоны Nano бывают на ATmega328 (без P): регистровая карта идентична,
// отличие — picoPower/сигнатура, ничего из используемого не затронуто.
// Поэтому оба макроса; строгий __AVR_ATmega328P__ сломал бы сборку на
// физически том же кристалле.
#if !defined(__AVR_ATmega328P__) && !defined(__AVR_ATmega328__)
#  error "Fast_nano supports only ATmega328P/328 (Arduino Nano)."
#endif

// Отдельный #ifndef F_CPU — чтобы без Arduino core получить внятную
// диагностику, а не «F_CPU=0 не 8/16».
#if !defined(F_CPU)
#  error "F_CPU must be defined (Arduino core defines it)."
#endif

#if (F_CPU != 16000000UL) && (F_CPU != 8000000UL)
#  error "Fast_nano supports only 16 MHz (Nano 5V) or 8 MHz (Nano 3.3V)."
#endif

// ============================================================================
// Mcu — порты ATmega328P и принудительный инлайн для горячих методов.
// ----------------------------------------------------------------------------
// Платформа: ATmega328P/328, Arduino Nano (guard выше).
// Ресурсы:   PORTB/C/D, DDRB/C/D, PINB/C/D — весь GPIO.
// Предусл.:  нет (только определения; побочных эффектов не имеет).
// Контекст:  set/clr/flip/dirOut/dirIn/pullUp/read — main+ISR
//            (см. ограничения: RMW не атомарен, flip — атомарный toggle бита).
// Огранич.:  Только 328P/Nano: имена регистров совпадают с 328P; на других
//            чипах пины PortC/PortD могут быть другими — не поддерживается.
//            set/clr — read-modify-write, не атомарно относительно ISR;
//            для ISR — Guard снаружи или flip (аппаратный toggle одного бита).
//            flip(m) через PINx=m: на выходе — переключение уровня, на входе —
//            переключение pull-up (не «уровня входа»). Контракт: Pin::toggle()
//            только для Dir::Out.
// Зависит:   <avr/io.h>, <stdint.h>.
// ============================================================================
#include <avr/io.h>
#include <stdint.h>
// Принудительный инлайн для горячих методов. Обычный inline при -Os
// не гарантирует инлайна, если функция вызывается из нескольких мест.
#define HW_INLINE inline __attribute__((always_inline))

namespace hw {

// ---- Частоты -------------------------------------------------------------
// F_CPU_HZ/CYC_PER_US убраны до появления Delay: единственный источник
// частоты — F_CPU тулчейна. Вернуть, когда появится потребитель (Delay::us).
// static constexpr uint32_t F_CPU_HZ   = F_CPU;
// static constexpr uint32_t CYC_PER_US = F_CPU / 1000000UL;

// ---- Порты ---------------------------------------------------------------
// Осознанно не шаблон: читаемость и простота диагностики важнее; это не
// ограничение компилятора (gnu++11 шаблон с volatile uint8_t& возможен).
// Не «оптимизировать» обратно в шаблон без отдельной проверки на avr-gcc.
struct PortB {
    static inline void set(uint8_t m)   { PORTB |=  m; }
    static inline void clr(uint8_t m)   { PORTB &= (uint8_t)~m; }
    static inline void flip(uint8_t m)  { PINB  =   m; }   // аппаратный toggle
    static inline void dirOut(uint8_t m){ DDRB  |=  m; }
    static inline void dirIn(uint8_t m) { DDRB  &= (uint8_t)~m; }
    static inline void pullUp(uint8_t m){ PORTB |=  m; }
    static inline uint8_t read()        { return PINB; }
};

struct PortC {
    static inline void set(uint8_t m)   { PORTC |=  m; }
    static inline void clr(uint8_t m)   { PORTC &= (uint8_t)~m; }
    static inline void flip(uint8_t m)  { PINC  =   m; }
    static inline void dirOut(uint8_t m){ DDRC  |=  m; }
    static inline void dirIn(uint8_t m) { DDRC  &= (uint8_t)~m; }
    static inline void pullUp(uint8_t m){ PORTC |=  m; }
    static inline uint8_t read()        { return PINC; }
};

struct PortD {
    static inline void set(uint8_t m)   { PORTD |=  m; }
    static inline void clr(uint8_t m)   { PORTD &= (uint8_t)~m; }
    static inline void flip(uint8_t m)  { PIND  =   m; }
    static inline void dirOut(uint8_t m){ DDRD  |=  m; }
    static inline void dirIn(uint8_t m) { DDRD  &= (uint8_t)~m; }
    static inline void pullUp(uint8_t m){ PORTD |=  m; }
    static inline uint8_t read()        { return PIND; }
};

} // namespace hw