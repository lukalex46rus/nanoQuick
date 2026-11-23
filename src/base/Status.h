#pragma once
#include <stdint.h>

namespace base {

enum class Err : uint8_t {
    Ok = 0,
    Busy,        // ресурс занят, операция не начата
    Timeout,     // истёк заданный срок
    NoDevice,    // нет ответа на адресе
    BadParam,    // аргумент вне диапазона
    Overflow,    // буфер переполнен
    NotReady,    // данных ещё нет
    IoFault      // ошибка обмена на шине
};

// Результат без исключений и без динамики. 1 байт.
struct Status {
    Err code;

    inline bool ok() const { return code == Err::Ok; }
    inline explicit operator bool() const { return ok(); }
};

inline Status ok()       { return Status{ Err::Ok }; }
inline Status err(Err e) { return Status{ e }; }

// Значение + статус для операций чтения.
template<typename T>
struct Result {
    T      value;
    Status status;
    inline bool ok() const { return status.ok(); }
};

} // namespace base