# Карта владения периферией ATmega328P

| Ресурс | Владелец | Статус |
|---|---|---|
| Timer0 | Arduino core (millis/micros/delay) | не трогаем |
| Timer1 | зарезервирован: ICP1 / PWM | свободен |
| Timer2 | time::Tick (1 мс) + subNow (4 мкс) | занят |
| ADC | adc::Adc ИЛИ adc::AdcStream | по выбору |
| ADC_vect (__vector_21) | adc::AdcStream (про) | занят AdcStream (про); свободен при базе |
| USART0 | отладка dbg / будущий Uart | по выбору |
| SPI | bus/Spi | занят SpiMaster (PB2 SS, PB3 MOSI, PB4 MISO, PB5 SCK); SCK=D13=LED |
| TWI | bus/I2c | занят I2c (PC4 SDA / PC5 SCL); подтяжка внутренняя или внешняя |
| INT0/INT1, PCINT | будущие кнопки/энкодер | свободны |