#pragma once
#include "Tick.h"

namespace time {

// Одноразовый срок. Хранит момент старта и длительность.
// Арифметика беззнаковая: корректна при любом переполнении счётчика
// и при любой длительности < 2^32 тиков (~49.7 суток).
class Deadline {
public:
    constexpr Deadline() : start_(0), span_(0), armed_(false) {}

    HW_INLINE void arm(uint32_t ms_) {
        start_ = time::now();
        span_  = ms_;
        armed_ = true;
    }

    HW_INLINE bool expired() const {
        return armed_ && (uint32_t)(time::now() - start_) >= span_;
    }

    inline bool active() const { return armed_; }
    inline void cancel()       { armed_ = false; }

    HW_INLINE uint32_t left() const {
        if (!armed_) return 0;
        uint32_t e = time::now() - start_;
        return e >= span_ ? 0u : (span_ - e);
    }

private:
    uint32_t start_;
    uint32_t span_;
    bool     armed_;
};

// Периодический триггер. constexpr-конструктор -> глобальный объект
// уходит в .bss без _GLOBAL__sub_I_ и без вызова ctor из main.
class Period {
public:
    constexpr Period() : last_(0), step_(0) {}

    HW_INLINE void start(uint32_t ms_) {
        step_ = ms_;
        last_ = time::now();
    }

    HW_INLINE bool due() {
        if (step_ == 0) return false;
        uint32_t n = time::now();
        if ((uint32_t)(n - last_) < step_) return false;
        last_ += step_;                                 // без накопления дрейфа
        if ((uint32_t)(n - last_) >= step_) last_ = n;   // ресинхронизация после простоя
        return true;
    }

    inline void stop() { step_ = 0; }

private:
    uint32_t last_;
    uint32_t step_;
};
// Короткий срок на субмиллисекундной шкале. Разрешение 8 мкс.
// Максимальный интервал — 65535 отсчётов = 524 мс.
// Арифметика uint16_t: корректна при переполнении.
class ShortDeadline {
public:
    constexpr ShortDeadline() : start_(0), span_(0), armed_(false) {}

    inline void arm(uint16_t subs) {
        start_ = time::subNow();
        span_  = subs;
        armed_ = true;
    }

    inline void armUs(uint16_t us_) { arm((uint16_t)(us_ / time::SUB_US)); }

    inline bool expired() const {
        return armed_ && (uint16_t)(time::subNow() - start_) >= span_;
    }

    inline bool active() const { return armed_; }
    inline void cancel()       { armed_ = false; }

private:
    uint16_t start_;
    uint16_t span_;
    bool     armed_;
};
} // namespace time