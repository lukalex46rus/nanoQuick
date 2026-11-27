#pragma once

// ============================================================================
// I2c — мастер TWI (I²C) на ATmega328P, 7-битная адресация, polling.
// ----------------------------------------------------------------------------
// Платформа: ATmega328P/328, Arduino Nano (guard: hw/Mcu.h).
// Ресурсы:   TWI целиком (TWCR, TWSR, TWBR, TWDR).
//            SCL=PC5 (A5), SDA=PC4 (A4) — аппаратный TWI 328P, выбора пинов нет.
//            При PULLUP=true — внутренние pull-up (~20..50 кОм, слабые;
//            для 400 кГц / длинных проводов нужны внешние 4.7 кОм).
// Предусл.:  time::init() до первого обмена (Deadline). I2c::init() — один раз
//            до первого обмена.
// Контекст:  main (polling); не из ISR.
// Огранич.:  Только мастер. Только 7-битные адреса (0x00..0x7F); 10-битные
//            не поддерживаются. Только polling, без прерываний TWI.
//            Один обмен — один Deadline; при ошибке содержимое r[] не
//            определено. Bus recovery (залипшая SDA) не реализован:
//            если stop() не сформировался, шина останется занятой до
//            передёргивания питания или ручного recovery через GPIO.
//            writeRead с wn==0 — BadParam (нет указателя регистра);
//            pointer-less read — это read() (с текущего указателя).
//            read(...,0) — BadParam; write(...,0) допустим как ping (но
//            лучше явный ping()).
//            toMs — бюджет на ВЕСЬ обмен; для длинных блоков увеличивать
//            (см. msFor()).
// Зависит:   hw/Mcu.h, hw/Pin.h, base/Status.h, time/Deadline.h.
// ============================================================================
#include <avr/io.h>
#include "../hw/Mcu.h"
#include "../hw/Pin.h"
#include "../base/Status.h"
#include "../time/Deadline.h"

namespace bus {

// Datasheet ATmega328P раздел 22 (TWI). SCL = F_CPU/(16 + 2*TWBR*4^TWPS), TWPS=0.
// static_assert: TWBR должен влезть в 8 бит, F_CPU — хватить для HZ
// (иначе формула уходит в отрицательные значения и каст в uint8_t даёт мусор).
template<uint32_t HZ = 100000, bool PULLUP = true>
struct I2c {
    static_assert(F_CPU >= 16UL * HZ, "I2C freq too high for F_CPU");
    static_assert((((F_CPU / HZ) - 16UL) / 2UL) <= 255UL, "TWBR overflow: I2C rate too low for F_CPU");
    static constexpr uint8_t TWBR_VAL = (uint8_t)(((F_CPU / HZ) - 16UL) / 2UL);

    // STOP занимает ~1–2 такта SCL. Лимит ожидания сброса TWSTO — из HZ,
    // а не константа: на 10 кГц один такт = 100 мкс, фиксированные 100 мкс
    // дали бы ложный IoFault. На 100k → 40 мкс, 400k → 25 мкс, 10k → 220 мкс.
    static constexpr uint16_t STOP_US = (uint16_t)(2000000UL / HZ + 20u);

    // Оценка минимального toMs для обмена длиной n_total байт (wn+rn).
    // +2 байта — адреса; запас — START/STOP/stretch. Политика — у вызывающего:
    // Bus::write(a, p, n, Bus::msFor(n)). Ноль рантайм-стоимости.
    static constexpr uint16_t msFor(uint16_t n_total) {
        return (uint16_t)((9UL * (n_total + 2u) * 1000UL) / HZ + 2u);
    }

    static void init() {
        TWSR = 0;                          // TWPS=0
        TWBR = TWBR_VAL;
        // TWINT=1 — принудительный сброс возможного зависшего флага
        // (write-1-clear), иначе первая транзакция начнётся с висящего TWINT.
        TWCR = _BV(TWINT) | _BV(TWEN);
        if (PULLUP) {                      // слабые внутренние для bench без внешних
            hw::PortC::dirIn(_BV(PC4) | _BV(PC5));
            hw::PortC::pullUp(_BV(PC4) | _BV(PC5));
        }
    }

    // 7-битная адресация. 10-битные адреса не поддерживаются.
    // toMs — бюджет на весь обмен; при toMs == 0 Deadline истекает сразу.
    // При ошибке содержимое r[] не определено.
    static base::Status write(uint8_t addr, const uint8_t* p, uint16_t n, uint32_t toMs = 5) {
        if (addr > 0x7F) return base::err(base::Err::BadParam);
        time::Deadline dl; dl.arm(toMs);
        if (!start(dl)) return fail(base::Err::Timeout);
        if (!sendAddr(addr, 0, 0x18, dl)) return fail(base::Err::NoDevice);

        while (n--) {
            if (!sendByte(*p++, 0x28, dl)) return fail(base::Err::IoFault);
        }
        return stop();
    }

    // Читает с ТЕКУЩЕГО указателя регистра устройства. Для регистровых
    // устройств указатель задаётся через write/writeRead; pointer-less read.
    static base::Status read(uint8_t addr, uint8_t* p, uint16_t n, uint32_t toMs = 5) {
        if (addr > 0x7F)  return base::err(base::Err::BadParam);
        if (n == 0)       return base::err(base::Err::BadParam);
        time::Deadline dl; dl.arm(toMs);
        if (!start(dl)) return fail(base::Err::Timeout);
        if (!sendAddr(addr, 1, 0x40, dl)) return fail(base::Err::NoDevice);

        for (uint16_t i = 0; i < n; ++i) {
            if (!recvByte(&p[i], (i == n - 1), dl)) return fail(base::Err::IoFault);
        }
        return stop();
    }

    // Комбинированный обмен: SLA+W + записываемые байты (первый = указатель
    // регистра), repeated-START, SLA+R + чтение. Для регистровых устройств.
    // wn == 0 — BadParam: нет указателя регистра; pointer-less read — read().
    static base::Status writeRead(uint8_t addr,
                                  const uint8_t* w, uint16_t wn,
                                  uint8_t* r, uint16_t rn,
                                  uint32_t toMs = 5) {
        if (addr > 0x7F)          return base::err(base::Err::BadParam);
        if (wn == 0 || rn == 0)   return base::err(base::Err::BadParam);
        time::Deadline dl; dl.arm(toMs);
        if (!start(dl)) return fail(base::Err::Timeout);
        if (!sendAddr(addr, 0, 0x18, dl)) return fail(base::Err::NoDevice);

        for (uint16_t i = 0; i < wn; ++i) {
            if (!sendByte(w[i], 0x28, dl)) return fail(base::Err::IoFault);
        }

        // Repeated START: после успешного SLA+W+данные ожидаем только 0x10.
        // Состояние 0x08 здесь невозможно: шина уже занята нами.
        if (!repStart(dl)) return fail(base::Err::Timeout);
        if (status() != 0x10) return fail(base::Err::IoFault);

        if (!sendAddr(addr, 1, 0x40, dl)) return fail(base::Err::NoDevice);

        for (uint16_t i = 0; i < rn; ++i) {
            if (!recvByte(&r[i], (i == rn - 1), dl)) return fail(base::Err::IoFault);
        }
        return stop();
    }

    // Пробник: SLA+W + STOP без данных. ok — устройство ответило ACK.
    // Явный ping вместо write(...,0): не маскирует ping под запись.
    static base::Status ping(uint8_t addr, uint32_t toMs = 5) {
        if (addr > 0x7F) return base::err(base::Err::BadParam);
        time::Deadline dl; dl.arm(toMs);
        if (!start(dl)) return fail(base::Err::Timeout);
        if (!sendAddr(addr, 0, 0x18, dl)) return fail(base::Err::NoDevice);
        return stop();
    }

private:
    static uint8_t status() { return TWSR & 0xF8; }

    // START (или repeated START). Ожидаем 0x08 (первый) или 0x10 (repeated).
    static bool start(time::Deadline& dl) {
        TWCR = _BV(TWINT) | _BV(TWSTA) | _BV(TWEN);
        if (!waitInt(dl)) return false;
        uint8_t st = status();
        return st == 0x08 || st == 0x10;
    }

    // Repeated START без проверки статуса — проверку делает вызывающий
    // (для writeRead ожидается ровно 0x10).
    static bool repStart(time::Deadline& dl) {
        TWCR = _BV(TWINT) | _BV(TWSTA) | _BV(TWEN);
        return waitInt(dl);
    }

    // SLA+RW. expect — ожидаемое состояние TWSR (0x18 для W, 0x40 для R).
    static bool sendAddr(uint8_t addr, uint8_t rw, uint8_t expect, time::Deadline& dl) {
        TWDR = (uint8_t)((addr << 1) | (rw & 1));
        TWCR = _BV(TWINT) | _BV(TWEN);
        if (!waitInt(dl)) return false;
        return status() == expect;
    }

    // Байт данных. expect = 0x28 (data ACK) для записи.
    static bool sendByte(uint8_t b, uint8_t expect, time::Deadline& dl) {
        TWDR = b;
        TWCR = _BV(TWINT) | _BV(TWEN);
        if (!waitInt(dl)) return false;
        return status() == expect;
    }

    // Чтение байта. last -> NACK (0x58), иначе ACK (0x50).
    static bool recvByte(uint8_t* out, bool last, time::Deadline& dl) {
        TWCR = _BV(TWINT) | _BV(TWEN) | (last ? 0 : _BV(TWEA));
        if (!waitInt(dl)) return false;
        uint8_t s = status();
        if (s != 0x50 && s != 0x58) return false;
        *out = TWDR;
        return true;
    }

    // STOP + ожидание сброса TWSTO (ShortDeadline, лимит STOP_US из HZ).
    // Без ожидания следующий START может проигнорироваться (шина ещё занята
    // STOP'ом), и следующий вызов упадёт в Timeout после успешного предыдущего.
    // Возвращает Status: IoFault, если TWSTO не сбросился (шина залипла).
    static base::Status stop() {
        TWCR = _BV(TWINT) | _BV(TWSTO) | _BV(TWEN);
        time::ShortDeadline dl;
        dl.armUs(STOP_US);
        while (TWCR & _BV(TWSTO))
            if (dl.expired()) return base::err(base::Err::IoFault);
        return base::ok();
    }

    // Аварийный STOP. Первичная ошибка важнее: (void)stop(), return err(e).
    // Если stop() не сформировался (SDA удерживается slave'ом), шина останется
    // в «залипшем» состоянии до bus recovery (в облегчённой библиотеке нет).
    static base::Status fail(base::Err e) {
        (void)stop();
        return base::err(e);
    }

    static bool waitInt(time::Deadline& dl) {
        while (!(TWCR & _BV(TWINT))) { if (dl.expired()) return false; }
        return true;
    }
};

} // namespace bus