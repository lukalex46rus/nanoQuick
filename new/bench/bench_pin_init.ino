#include <Arduino.h>
#include "src/hw/Pin.h"
#include "src/time/Tick.h"
#include "dbg.h"

using P  = hw::board::D8;      // PB0
using In = hw::board::D9;      // PB1

void setup() {
    dbg::begin(9600);
    time::init();

    // 1) init(Out) на пине, бывшем входом с pull-up: глитча быть не должно.
    In::init(hw::Dir::InPullUp);   // PORTB1=1, DDRB1=0
    uint8_t portBefore = PORTB;
    uint8_t ddrBefore  = DDRB;
    P::init(hw::Dir::Out, hw::Level::Low);   // уровень -> dirOut
    uint8_t portAfter  = PORTB;
    uint8_t ddrAfter   = DDRB;

    // 2) read() на пине с pull-up
    In::init(hw::Dir::InPullUp);
    bool pull = In::read();        // 1, если пин не притянут к GND

    // 3) init(Out, High) — сразу высокий уровень
    P::init(hw::Dir::Out, hw::Level::High);
    bool outHigh = P::read();      // 1

    // 4) toggle через PINx
    P::toggle();
    bool afterToggle = P::read();  // 0

    dbg::text("\r\n=== X1: Pin init ===\r\n");
    dbg::row("portB_bef =", portBefore);   // 2 (PB1 pull-up)
    dbg::row("ddrB_bef  =", ddrBefore);    // 0
    dbg::row("portB_aft =", portAfter);    // 0 (P=PB0 clr)
    dbg::row("ddrB_aft  =", ddrAfter);     // 1 (P=PB0 out)
    dbg::row("pull_read =", pull);         // 1
    dbg::row("out_high  =", outHigh);      // 1
    dbg::row("tog_read  =", afterToggle);  // 0
    dbg::flush();
}

void loop() {}