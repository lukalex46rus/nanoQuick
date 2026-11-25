#pragma once
#include <avr/io.h>
#include "../hw/Mcu.h"
#include "../hw/Pin.h"
#include "../base/Status.h"
#include "../time/Deadline.h"

namespace bus {

// Datasheet ATmega328P раздел 22 (TWI). SCL = F_CPU/(16 + 2*TWBR*4^TWPS), TWPS=0.
template<uint32_t HZ = 100000, bool PULLUP = true>
struct I2c {
    static constexpr uint8_t TWBR_VAL = (uint8_t)(((F_CPU / HZ) - 16UL) / 2UL);

    static void init() {
        TWSR = 0;                          // TWPS=0
        TWBR = TWBR_VAL;
        TWCR = _BV(TWEN);
        if (PULLUP) {                      // слабые внутренние для bench без внешних
            hw::PortC::dirIn(_BV(PC4) | _BV(PC5));
            hw::PortC::pullUp(_BV(PC4) | _BV(PC5));
        }
    }

    static base::Status write(uint8_t addr, const uint8_t* p, uint16_t n, uint32_t toMs = 5) {
        time::Deadline dl; dl.arm(toMs);
        TWCR = _BV(TWINT) | _BV(TWSTA) | _BV(TWEN);
        if (!waitInt(dl)) return fail(base::Err::Timeout);
        uint8_t st = status();
        if (st != 0x08 && st != 0x10) return fail(base::Err::IoFault);

        TWDR = (uint8_t)(addr << 1);
        TWCR = _BV(TWINT) | _BV(TWEN);
        if (!waitInt(dl)) return fail(base::Err::Timeout);
        if (status() != 0x18) return fail(base::Err::NoDevice);

        while (n--) {
            TWDR = *p++;
            TWCR = _BV(TWINT) | _BV(TWEN);
            if (!waitInt(dl)) return fail(base::Err::Timeout);
            if (status() != 0x28) return fail(base::Err::IoFault);
        }
        stop();
        return base::ok();
    }

    static base::Status read(uint8_t addr, uint8_t* p, uint16_t n, uint32_t toMs = 5) {
        if (n == 0) return base::err(base::Err::BadParam);
        time::Deadline dl; dl.arm(toMs);
        TWCR = _BV(TWINT) | _BV(TWSTA) | _BV(TWEN);
        if (!waitInt(dl)) return fail(base::Err::Timeout);
        uint8_t st = status();
        if (st != 0x08 && st != 0x10) return fail(base::Err::IoFault);

        TWDR = (uint8_t)((addr << 1) | 1);
        TWCR = _BV(TWINT) | _BV(TWEN);
        if (!waitInt(dl)) return fail(base::Err::Timeout);
        if (status() != 0x40) return fail(base::Err::NoDevice);

        for (uint16_t i = 0; i < n; ++i) {
            TWCR = _BV(TWINT) | _BV(TWEN) | ((i == n - 1) ? 0 : _BV(TWEA));
            if (!waitInt(dl)) return fail(base::Err::Timeout);
            uint8_t s = status();
            if (s != 0x50 && s != 0x58) return fail(base::Err::IoFault);
            p[i] = TWDR;
        }
        stop();
        return base::ok();
    }

private:
    static uint8_t status() { return TWSR & 0xF8; }
    static void stop() { TWCR = _BV(TWINT) | _BV(TWSTO) | _BV(TWEN); }
    static base::Status fail(base::Err e) { stop(); return base::err(e); }
    static bool waitInt(time::Deadline& dl) {
        while (!(TWCR & _BV(TWINT))) { if (dl.expired()) return false; }
        return true;
    }
};

} // namespace bus