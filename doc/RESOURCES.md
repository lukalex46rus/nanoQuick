# Карта владения периферией ATmega328P (Arduino Nano)

Платформа: ATmega328P/328, F_CPU 16 МГц (5V) / 8 МГц (3.3V). Guard: hw/Mcu.h.

| Ресурс | Владелец | Статус |
|---|---|---|
| Timer0 | Arduino core (millis/micros/delay) | не трогаем |
| Timer1 | зарезервирован: ICP1 / PWM | свободен |
| Timer2 | time::Tick (1 мс) + subNow (SUB_US=4 мкс @16M / 8 мкс @8M) | занят; OC2A/OC2B GPIO свободны, analogWrite(3/11) не работает |
| ADC | adc::Adc ИЛИ adc::AdcStream (единовладелец) | по выбору; второй init() обрывает конверсию первого |
| ADC_vect (__vector_21) | adc::AdcStream | занят при AdcStream; свободен при Adc (polling) |
| USART0 | dbg (бенчи) ИЛИ bus::Uart ИЛИ Arduino Serial | по выбору; конфликт с bootloader при прошивке |
| USART0 пины | D0=RX(PD0), D1=TX(PD1) | джампер D0-D1 (loopback) ставить ПОСЛЕ upload, снимать ПЕРЕД прошивкой |
| SPI | bus::SpiMaster (polling) | занят: PB2 SS, PB3 MOSI(D11), PB4 MISO(D12), PB5 SCK(D13); SCK=D13=LED мерцает |
| TWI | bus::I2c (polling) | занят: PC4 SDA(A4), PC5 SCL(A5); подтяжка внутренняя (~30-50к) или внешняя (2.2-4.7к для 400к) |
| INT0/INT1, PCINT | будущие кнопки/энкодер | свободны |
| GPIO PortB/C/D | hw::Port / hw::Pin | set/clr RMW не атомарны vs ISR; flip атомарен; read() PINx с задержкой синхронизатора |
| GPIO (soft UART) | bus::SoftUartTx/Rx (bit-bang) | свободен: любые пины кроме занятых периферией; несколько экземпляров |
| MCP23017 (внешний) | peripheral::Mcp23017<ADDR,BUS> | на шине TWI: 0x20..0x27, до 8 чипов × 16 GPIO = 128 линий |
| GPIO (LCD) | peripheral::Hd44780<RS,EN,D4..D7> | 6 пинов, write-only (RW=GND); свободен выбор пинов |
| GPIO (DFPlayer) | peripheral::DfPlayer<TX_PIN> | 1 пин TX через SoftUartTx; IRQ off ~10мс/кадр |

## Примечания
- USART0 один: bus::Uart<BAUD> и dbg и Serial не сосуществуют. init() перезаписывает настройки.
  Bootloader прошивается на своём baud (57600 old / 115200 new) — джампер D0-D1 мешает.
- ADC единовладелец: Adc::init() и AdcStream::init() оба делают ADCSRA=0 первым (обрыв конверсии).
  Переключение владельца — через stop()/deinit() предыдущего, затем init() нового.
- Timer2 занят Tick полностью (TCCR2A/B, OCR2A, TCNT2, OCIE2A). COMPB/OVF векторы свободны,
  но OCR2B=0 от Arduino core взводит OCF2B каждый оборот (норма, OCIE2B=0).
- SPI/TWI polling (без ISR): блокировка ограничена Deadline (I2c/Uart) или доказуема (Spi master SPE=1).
- Прерывания USART/SPI/TWI не используются — векторы свободны для будущих ISR-модулей.