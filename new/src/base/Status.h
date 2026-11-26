#pragma once

// ============================================================================
// Status — коды ошибок и результат без исключений и динамики.
// ----------------------------------------------------------------------------
// Платформа: не зависит (нет регистров; чистая логика).
// Ресурсы:   — (только типы; Status — 1 байт, Result<T> — sizeof(T)+1).
// Предусл.:  нет.
// Контекст:  — (тип; usable в main+ISR, если сам объект не разделяется).
// Огранич.:  Status — для операций, которые МОГУТ не дать результата
//            (I/O, готовность блока). Чистые вычислительные функции с
//            определённым контрактом (isqrt32) — без Status.
//            Unknown/Internal добавим, когда появится реальный источник.
//            Неявное Err->Status запрещено: единая точка создания ok()/err().
// Зависит:   <stdint.h>.
// ============================================================================
#include <stdint.h>

namespace base {

// Err — код ошибки. Status — результат операции (успешно/ошибка).
// Обёртка ради семантики и единых проверок, не «на вырост байта».
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

    // constexpr: позволяет использовать в static_assert и в constexpr-контексте.
    constexpr bool ok() const { return code == Err::Ok; }
    // explicit: if (st) можно, int x = st — нельзя (не просачивается в арифметику).
    constexpr explicit operator bool() const { return ok(); }
};

constexpr Status ok()       { return Status{ Err::Ok }; }
constexpr Status err(Err e) { return Status{ e }; }

// Значение + статус для операций чтения. value имеет смысл только при ok().
template<typename T>
struct Result {
    T      value;
    Status status;
    constexpr bool ok() const { return status.ok(); }
};

} // namespace base