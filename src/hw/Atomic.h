#pragma once

// ============================================================================
// Atomic — RAII-критическая секция и атомарный доступ к volatile-переменным,
// разделяемым с ISR.
// ----------------------------------------------------------------------------
// Платформа: ATmega328P/328, Arduino Nano (guard: hw/Mcu.h).
// Ресурсы:   глобальный I-бит SREG (cli/sei).
// Предусл.:  нет.
// Контекст:  main+ISR (короткие секции); не долгоживущий lock.
// Огранич.:  Guard — сохранить/восстановить SREG: вложенность корректна
//            (внутренний восстановит «выключено», внешний — исходное),
//            счётчик реентерабельности не нужен.
//            load/store — только целые/enum (любой размер под Guard);
//            структуры — осознанная копия под Guard, не load(struct).
//            uint8_t — специализации без cli (1 байт атомарен сам).
//            Свой is_integral: тулчейн Arduino AVR не поставляет C++
//            std-заголовки (<type_traits> нет); builtin __is_integral
//            в avr-gcc 7.3 нет (появился в GCC 14); __is_enum — есть.
//            Внешние STL (ArduinoSTL/avr-libstdcpp) не тянем.
// Зависит:   <avr/interrupt.h>, <stdint.h>, <stddef.h>.
// ============================================================================
#include <avr/interrupt.h>
#include <stdint.h>
#include <stddef.h>    // wchar_t

namespace hw {

// ---- Минимальный is_integral (замена std::is_integral) --------------------
// Специализации по ФУНДАМЕНТАЛЬНЫМ типам: uint8_t..int64_t — их typedef,
// покрываются автоматически. char / signed char / unsigned char — три разных
// типа; bool — отдельный. wchar_t/char16_t/char32_t — тоже целые (как в std).
// float/double сознательно исключены.
namespace detail {
template<typename T> struct is_integral { static constexpr bool value = false; };
#define HW_INTEGRAL(T) template<> struct is_integral<T> { static constexpr bool value = true; }
HW_INTEGRAL(bool);
HW_INTEGRAL(char); HW_INTEGRAL(signed char); HW_INTEGRAL(unsigned char);
HW_INTEGRAL(short); HW_INTEGRAL(unsigned short);
HW_INTEGRAL(int); HW_INTEGRAL(unsigned int);
HW_INTEGRAL(long); HW_INTEGRAL(unsigned long);
HW_INTEGRAL(long long); HW_INTEGRAL(unsigned long long);
HW_INTEGRAL(wchar_t); HW_INTEGRAL(char16_t); HW_INTEGRAL(char32_t);
#undef HW_INTEGRAL
} // namespace detail

// RAII-критическая секция. Восстанавливает SREG, а не включает безусловно.
class Guard {
public:
    inline Guard() : sreg_(SREG) { cli(); }
    inline ~Guard()              { SREG = sreg_; }
private:
    uint8_t sreg_;
    Guard(const Guard&) = delete;
    Guard& operator=(const Guard&) = delete;
};

// Чтение многобайтовой переменной, разделяемой с ISR.
template<typename T>
inline T load(const volatile T& src) {
    static_assert(detail::is_integral<T>::value || __is_enum(T),
                  "load: only integral/enum; use Guard+copy for structs");
    Guard g;
    return (T)src;
}

template<typename T>
inline void store(volatile T& dst, T v) {
    static_assert(detail::is_integral<T>::value || __is_enum(T),
                  "store: only integral/enum; use Guard+copy for structs");
    Guard g;
    dst = v;
}

// 8-битные чтения/записи атомарны на AVR: специализации без Guard.
template<> inline uint8_t load<uint8_t>(const volatile uint8_t& src) { return src; }
template<> inline void    store<uint8_t>(volatile uint8_t& dst, uint8_t v) { dst = v; }

} // namespace hw