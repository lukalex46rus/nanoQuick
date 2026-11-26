#include <Arduino.h>
#include "../src/base/Status.h"
#include "dbg.h"

// constexpr-проверки на этапе компиляции
static_assert(base::ok().ok(), "ok() must be ok");
static_assert(!base::err(base::Err::Timeout).ok(), "err() must not be ok");
static_assert(sizeof(base::Status) == 1, "Status must be 1 byte");

void setup() {
    dbg::begin(9600);

    base::Status s_ok  = base::ok();
    base::Status s_err = base::err(base::Err::Timeout);
    base::Result<uint16_t> r_ok { 1234, base::ok() };
    base::Result<uint16_t> r_er { 0,    base::err(base::Err::NotReady) };

    dbg::text("\r\n=== X4: Status ===\r\n");
    dbg::row("size_status =", sizeof(base::Status));              // 1
    dbg::row("size_res16  =", sizeof(base::Result<uint16_t>));    // 3
    dbg::row("ok_ok       =", s_ok.ok() ? 1 : 0);                 // 1
    dbg::row("err_ok      =", s_err.ok() ? 1 : 0);                // 0
    dbg::row("r_ok_ok     =", r_ok.ok() ? 1 : 0);                 // 1
    dbg::row("r_ok_val    =", r_ok.value);                        // 1234
    dbg::row("r_er_ok     =", r_er.ok() ? 1 : 0);                 // 0
    dbg::flush();
}

void loop() {}