#include <Arduino.h>
#include "src/hw/Mcu.h"
#include "src/time/Tick.h"
#include "dbg.h"

using namespace hw;

void setup() {
    dbg::begin(9600);
    time::init();

    // flip на выходе: уровень меняется
    PortB::dirOut(_BV(PB0));
    PortB::clr(_BV(PB0));
    bool b0 = PortB::read() & _BV(PB0);
    PortB::flip(_BV(PB0));
    bool b1 = PortB::read() & _BV(PB0);

    // set/clr маской не трогает соседние биты
    PortB::dirOut(_BV(PB1) | _BV(PB2));
    PortB::clr(_BV(PB1) | _BV(PB2));
    PortB::set(_BV(PB1));
    uint8_t pb = PortB::read() & (_BV(PB1) | _BV(PB2));  // 0b10

    // flip на входе: переключается pull-up (PORTx)
    PortD::dirIn(_BV(PD0));
    PortD::pullUp(_BV(PD0));
    uint8_t portD_before = PORTD & _BV(PD0);
    PortD::flip(_BV(PD0));
    uint8_t portD_after  = PORTD & _BV(PD0);

    dbg::text("\r\n=== X3: Mcu ports ===\r\n");
    dbg::row("out_before =", b0);          // 0
    dbg::row("out_after  =", b1);          // 1
    dbg::row("mask_keep  =", pb);          // 2 (только PB1)
    dbg::row("pull_before=", portD_before); // 1
    dbg::row("pull_after =", portD_after);  // 0
    dbg::flush();
}

void loop() {}