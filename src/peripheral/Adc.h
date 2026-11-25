#pragma once
#include <avr/io.h>
#include "../hw/Mcu.h"
#include "../base/Status.h"

namespace adc {

// Datasheet ATmega328P, раздел 24 (ADC).
// ADPS2:0: 0,1->2  2->4  3->8  4->16  5->32  6->64  7->128
// Конверсия: 13 тактов ADC; первая после ADEN — 25.
// Полный 10-бит: F_ADC 50..200 кГц.

enum class Ref : uint8_t {
    External = 0,                              // REFS=00, вывод AREF
    AVcc     = _BV(REFS0),                     // REFS=01
    Int1V1   = _BV(REFS1) | _BV(REFS0)         // REFS=11
};

enum class Clk : uint8_t {
    Div2 = 0, Div4 = 2, Div8 = 3, Div16 = 4,
    Div32 = 5, Div64 = 6, Div128 = 7
};

constexpr uint8_t TEMP_SENSOR = 8;
constexpr uint8_t BANDGAP     = 14;   // 1.1 В
constexpr uint8_t GND_CH      = 15;

template<Ref REF = Ref::AVcc, Clk CLK = Clk::Div128>
struct Adc {
    // didrMask: биты ADC7D..ADC0D — отключить цифровые буферы используемых пинов.
    static void init(uint8_t didrMask = 0) {
        ADMUX  = static_cast<uint8_t>(REF);
        ADCSRB = 0;                              // автотриггер выключен
        DIDR0  = didrMask;
        ADCSRA = _BV(ADEN) | static_cast<uint8_t>(CLK);
    }

    static void deinit() { ADCSRA = 0; }         // ADEN=0, питание ADC off

    static base::Status start(uint8_t ch) {
        if (ADCSRA & _BV(ADSC)) return base::err(base::Err::Busy);
        if (!validChannel(ch))  return base::err(base::Err::BadParam);
        ADMUX = (ADMUX & 0xF0) | (ch & 0x0F);    // сохранить REFS/ADLAR
        ADCSRA |= _BV(ADSC);
        return base::ok();
    }

    static bool ready() { return !(ADCSRA & _BV(ADSC)); }

    static base::Result<uint16_t> take() {
        if (!ready()) return base::Result<uint16_t>{ 0, base::err(base::Err::NotReady) };
        uint16_t v = ADC;                        // 16-бит SFR: ADCL затем ADCH
        return base::Result<uint16_t>{ v, base::ok() };
    }

private:
    static bool validChannel(uint8_t ch) {
        return ch <= 8 || ch == BANDGAP || ch == GND_CH;
    }
};

} // namespace adc