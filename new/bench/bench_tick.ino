#include "src/hw/Pin.h"
#include "src/time/Tick.h"
#include "src/time/Deadline.h"

// ============================================================================
// bench_tick — Period(1 мс) на D8: toggle каждую миллисекунду.
// Платформа: ATmega328P/328, Nano. Контекст: main.
// Ожидание: на D8 меандр 500 Гц (toggle каждую 1 мс). Осциллограф/частота.
// ============================================================================
using Probe = hw::board::D8;
time::Period p;

void setup() {
    Probe::init(hw::Dir::Out);
    time::init();
    p.start(time::ms(1));
}

void loop() {
    if (p.due()) Probe::toggle();
}