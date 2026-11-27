#include <Arduino.h>
#include "src/hw/Mcu.h"
#include "src/base/Status.h"
#include "dbg.h"

void setup() {
    dbg::begin(9600);
    dbg::text("\r\n=== X0: platform ===\r\n");
#if defined(__AVR_ATmega328P__)
    dbg::row("chip   =", 328);            // 328P
#elif defined(__AVR_ATmega328__)
    dbg::row("chip   =", 3280);           // 328 (без P), маркер
#endif
    dbg::row("F_CPU  =", F_CPU);
    dbg::row("sz_stat=", sizeof(base::Status));
    dbg::row("sz_res =", sizeof(base::Result<uint16_t>));
    dbg::flush();
}

void loop() {}