#include "src/hw/Pin.h"
using Led = hw::board::D13;

void setup() { Led::init(hw::Dir::Out); }
void loop()  { Led::toggle(); }