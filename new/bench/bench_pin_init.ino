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

    // 4) toggle через PINx: проверяем PORTB-защёлку (PINB отстаёт на 0.5..1.5 такта)
    P::toggle();
    bool afterToggle = (PORTB & _BV(PB0)) != 0;  // 0

    dbg::text("\r\n=== X1: Pin init ===\r\n");

    dbg::row("portB_bef =", portBefore & 0x03);   // 2 (PB1 pull-up; PB5 не считаем)
    dbg::row("ddrB_bef  =", ddrBefore  & 0x03);   // 0 (PB0/PB1; PB5=bootloader LED)
    dbg::row("portB_aft =", portAfter  & 0x03);   // 2 (PB1 pull-up жив, PB0 clr)
    dbg::row("ddrB_aft  =", ddrAfter   & 0x03);   // 1 (PB0 out)
    
    dbg::row("pull_read =", pull);         // 1
    dbg::row("out_high  =", outHigh);      // 1
    dbg::row("tog_read  =", afterToggle);  // 0
    dbg::flush();
}

void loop() {}
