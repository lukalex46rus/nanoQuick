#pragma once

// ============================================================================
// Debounce — антидребезг цифрового входа с детектом фронтов.
// ----------------------------------------------------------------------------
// Платформа: ATmega328P/328, Arduino Nano (guard: hw/Mcu.h через Tick.h).
// Ресурсы:   — (чистая логика поверх Tick; 13 байт на объект).
// Предусл.:  time::init() до update(); init(initial) задаёт стартовое состояние
//            без ложного фронта.
// Контекст:  один контекст на объект (обычно main). main+ISR — Guard снаружи.
// Огранич.:  Таймерный алгоритм: любое изменение сырого входа перезапускает срок;
//            подтверждение только после MS непрерывной стабильности.
//            Дребезг короче MS подавляется; задержка реакции = MS.
//            rose()/fell() — read-and-clear (фронт потребляется чтением).
//            Не для энкодеров (там нужна другая логика — квадратура).
// Зависит:   time/Tick.h, time/Deadline.h, <stdint.h>.
// ============================================================================
#include <stdint.h>
#include "../time/Tick.h"
#include "../time/Deadline.h"

namespace logic {

template<uint16_t MS = 20>
class Debounce {
public:
    constexpr Debounce()
        : dl_(), stable_(false), cand_(false), rose_(false), fell_(false) {}

    // Стартовое состояние без ложного фронта. Вызывать после time::init().
    void init(bool initial) {
        stable_ = initial;
        cand_   = initial;
        rose_   = false;
        fell_   = false;
        dl_.cancel();
    }

    // Подать сырое значение. Вызывать периодически (чаще чем MS/2).
    void update(bool raw) {
        if (raw != cand_) {
            cand_ = raw;
            dl_.arm(MS);                 // дребезг перезапускает срок
        } else if (dl_.active() && dl_.expired()) {
            if (stable_ != cand_) {
                if (cand_) rose_ = true; else fell_ = true;
                stable_ = cand_;
            }
            dl_.cancel();
        }
    }

    bool stable() const { return stable_; }

    // Фронты с момента последнего чтения (read-and-clear).
    bool rose() { bool r = rose_; rose_ = false; return r; }
    bool fell() { bool f = fell_; fell_ = false; return f; }

private:
    time::Deadline dl_;      // 9 байт
    bool stable_;            // подтверждённое состояние
    bool cand_;              // кандидат (последнее сырое)
    bool rose_;              // фронт 0→1 ожидает потребления
    bool fell_;              // фронт 1→0 ожидает потребления
};

} // namespace logic