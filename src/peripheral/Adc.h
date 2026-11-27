#pragma once

// ============================================================================
// Adc — одиночные конверсии АЦП ATmega328P, polling.
// ----------------------------------------------------------------------------
// Платформа: ATmega328P/328, Arduino Nano (guard: hw/Mcu.h).
// Ресурсы:   ADC целиком (ADMUX, ADCSRA, ADCSRB, DIDR0, ADC). Каналы 328P:
//            ADC0..7 (A0..A7), TEMP_SENSOR=8, BANDGAP=14, GND=15.
//            Для каналов ADC0..5 затронуты PC0..PC5 (те же пины A0..A5) —
//            цифровые буферы отключаются через DIDR0.
// Предусл.:  Adc::init() до start(). Перед take() был start() и ready()==true.
// Контекст:  main (polling); не из ISR.
// Огранич.:  Только polling, без прерываний и free-running (для потока —
//            AdcStream). Right-aligned (ADLAR=0) — start() принудительно
//            сбрасывает ADLAR. Единовладелец ADC: не совмещать с AdcStream
//            без явного stop()/init(). При ошибке take() (NotReady) value
//            не определено.
// Зависит:   hw/Mcu.h, base/Status.h, <avr/io.h>.
// ============================================================================
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
    // Предусловие: ADC не в активной конверсии. ADCSRA = 0 первым снимает
    // ADEN и обрывает конверсию в полёте; иначе init() после start() дал бы
    // Busy/ISR. DIDR0 независим от ADCSRA, пишется после остановки.
    // didrMask: биты ADC7D..ADC0D — отключить цифровые буферы используемых пинов.
    static void init(uint8_t didrMask = 0) {
        ADCSRA = 0;                              // стоп/отключение, обрыв конверсии
        DIDR0  = didrMask;
        ADMUX  = static_cast<uint8_t>(REF);
        ADCSRB = 0;                              // автотриггер выключен
        ADCSRA = _BV(ADEN) | static_cast<uint8_t>(CLK);
        // polling, ADIE=0 — висящий ADIF не опасен, _BV(ADIF) не нужен.
    }

    static void deinit() { ADCSRA = 0; }         // ADEN=0, питание ADC off

    // ADMUX = REF | ch: детерминированно (ADLAR=0, reserved=0) и дешевле —
    // один out вместо RMW in/andi/or/out. REF — шаблонный, свернётся в константу.
    // NotReady при ADEN=0 (ADC не инициализирован), Busy — конверсия в полёте.
    static base::Status start(uint8_t ch) {
        if (!(ADCSRA & _BV(ADEN))) return base::err(base::Err::NotReady);
        if (ADCSRA & _BV(ADSC))    return base::err(base::Err::Busy);
        if (!validChannel(ch))     return base::err(base::Err::BadParam);
        ADMUX = (uint8_t)REF | (ch & 0x0F);      // REFS из шаблона, ADLAR=0, MUX=ch
        ADCSRA |= _BV(ADSC);
        return base::ok();
    }

    static bool ready() { return !(ADCSRA & _BV(ADSC)); }

    // ADC как 16-бит SFR: чтение ADCL->ADCH с аппаратной блокировкой обновления.
    static base::Result<uint16_t> take() {
        if (!ready()) return base::Result<uint16_t>{ 0, base::err(base::Err::NotReady) };
        uint16_t v = ADC;
        return base::Result<uint16_t>{ v, base::ok() };
    }

private:
    // Каналы 328P. Именованные константы, не ch <= 8: ch <= 8 неявно
    // включает TEMP_SENSOR=8 и хрупко при смене чипа.
    static bool validChannel(uint8_t ch) {
        return ch <= 7 || ch == TEMP_SENSOR || ch == BANDGAP || ch == GND_CH;
    }
};

} // namespace adc