#pragma once

// ============================================================================
// Atomic — RAII-критическая секция и атомарный доступ к volatile-переменным,
// разделяемым с ISR.
// ----------------------------------------------------------------------------
// Платформа: AVR (SREG/cli/sei); для Nano — через hw/Mcu.h.
// Ресурсы:   глобальный I-бит SREG (cli/sei).
// Предусл.:  нет.
// Контекст:  main (короткие секции); не использовать как долгоживущий lock.
// Огранич.:  Guard не реентерабелен по смыслу «сохранить/восстановить»:
//            вложенность корректна (внутренний восстановит «выключено»,
//            внешний — исходное), счётчик не нужен. Два независимых Guard
//            в одном потоке потеряют исходное состояние внешнего.
//            load/store — только арифметика/enum; для структур — осознанные
//            копирующие функции под Guard (load(struct) не делает доступ к
//            полям атомарным по смыслу). Для uint8_t — специализации без cli.
// Зависит:   <avr/interrupt.h>, <stdint.h>.
// ============================================================================
#include <avr/interrupt.h>
#include <stdint.h>
#include <type_traits>

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
// Только арифметика/enum: для структур — копия под Guard, не load(struct).
template<typename T>
inline T load(const volatile T& src) {
    static_assert(std::is_integral<T>::value || std::is_enum<T>::value,
                  "load: only integral/enum; use Guard+copy for structs");
    Guard g;
    return (T)src;
}

template<typename T>
inline void store(volatile T& dst, T v) {
    static_assert(std::is_integral<T>::value || std::is_enum<T>::value,
                  "store: only integral/enum; use Guard+copy for structs");
    Guard g;
    dst = v;
}

// 8-битные чтения/записи атомарны на AVR: cli/sei только добавляют задержку
// ISR и съедают такты. Специализации убирают Guard для однобайтных типов.
template<> inline uint8_t load<uint8_t>(const volatile uint8_t& src) { return src; }
template<> inline void    store<uint8_t>(volatile uint8_t& dst, uint8_t v) { dst = v; }

} // namespace hw