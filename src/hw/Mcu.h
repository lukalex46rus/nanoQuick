#pragma once
#include <avr/io.h>
#include <stdint.h>
// Принудительный инлайн для горячих методов. Обычный inline при -Os
// не гарантирует инлайна, если функция вызывается из нескольких мест.
#define HW_INLINE inline __attribute__((always_inline))

namespace hw {

// ---- Частоты -------------------------------------------------------------
// constexpr, а не static const: гарантированно не занимает SRAM и
// usable в константных выражениях (делители таймеров).
constexpr uint32_t F_CPU_HZ   = F_CPU;
constexpr uint32_t CYC_PER_US = F_CPU / 1000000UL;

// ---- Порты ---------------------------------------------------------------
// Порт описывается тремя регистрами. volatile-ссылки допустимы как
// нетиповые параметры шаблона в C++11 (адрес — константа времени линковки).
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