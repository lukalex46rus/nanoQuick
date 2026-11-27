#include "src/hw/Pin.h"
#include "src/time/Tick.h"
#include "src/time/Deadline.h"

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