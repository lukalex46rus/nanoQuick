# Контракты модулей nano_quick

Платформа: ATmega328P / ATmega328, Arduino Nano, F_CPU 16 МГц (5V) / 8 МГц (3.3V).
Guard: hw/Mcu.h (#error на чип и F_CPU). Другие чипы/платы не поддерживаются.
Тулчейн: avr-gcc 7.3.0, gnu++11. C++ std-заголовки (<type_traits> и др.) НЕ поставляются.
Каждый модуль несёт шапку: Платформа / Ресурсы / Предусл. / Контекст / Огранич. / Зависит.
Числа в контрактах — замерены на железе или выведены из datasheet (таблица указана).

## base/Status.h
- Err — enum class uint8_t: Ok, Busy, Timeout, NoDevice, BadParam, Overflow, NotReady, IoFault.
  Значения фиксированы, не меняются. Unknown/Internal — только при появлении реального источника.
- Status — struct{Err}, 1 байт. ok() const, explicit operator bool (контекстные проверки).
- Result<T> — value + status, sizeof(T)+1 (AVR alignof=1). value валиден только при ok().
- Фабрики constexpr ok()/err() — агрегатная инициализация; неявное Err→Status запрещено.
- Status — для операций, которые МОГУТ не дать результата (I/O, готовность блока).
  Чистые вычислительные функции с определённым контрактом (isqrt32) — без Status.
- Никаких строк ошибок в RAM. Тексты — отдельным слоем через PROGMEM.

## hw/Mcu.h
- Точка правды платформы: #error если не __AVR_ATmega328P__/__AVR_ATmega328__;
  #error если F_CPU не определён или не 16/8 МГц. Клоны Nano на 328 (без P) допустимы
  (регистровая карта идентична, отличие picoPower/сигнатура).
- PortB/C/D: все методы static inline, 0 байт состояния. Осознанно не шаблон (читаемость).
- set/clr — read-modify-write, НЕ атомарны относительно ISR. Для ISR: Guard снаружи
  или flip (аппаратный toggle одного бита через PINx=m, 1 такт).
- flip(m) на выходе — переключение уровня; на входе — переключение pull-up (не «уровня входа»).
- read() возвращает PINx. Сразу после записи PORTx/PINx может вернуть СТАРОЕ значение:
  синхронизатор PINx отстаёт на 0.5..1.5 такта. Для проверки только что установленного
  выхода читать PORTx или выждать ≥2 такта.
- HW_INLINE = inline __attribute__((always_inline)). Только горячий путь из нескольких мест.
- F_CPU_HZ/CYC_PER_US убраны до появления Delay (единственный источник частоты — F_CPU тулчейна).

## hw/Pin.h
- Pin<Port,Bit>: 0 байт RAM, все методы static. mask — constexpr при компиляции.
- init(Dir, Level=Low): для Out сначала уровень в PORTx, потом dirOut — без глитча HIGH
  при реините с pull-up. Не атомарно (несколько RMW), но безглитчево.
- Dir::InPullUp самодостаточен: Level игнорируется (pull-up всегда).
  In+High = pull-up; In+Low = tri-state.
- toggle() — аппаратный flip; контракт: только для Dir::Out (на входе меняет pull-up).
- read() — PINx, см. предупреждение о синхронизаторе в Mcu.h.
- board::D0..D13/A0..A5 — единственные допустимые имена пинов. Модули НЕ определяют свои typedef.
  D0/D1 — USART (RX/TX): не трогать при активном Uart/Serial. D13 — SCK/LED.

## hw/Atomic.h
- Guard: сохраняет/восстанавливает SREG (не sei безусловно). Вложенность корректна
  (внутренний восстановит «выключено», внешний — исходное), счётчик реентерабельности не нужен.
  Не копируем/не присваиваем (= delete).
- load/store — для целых/enum любого размера под Guard; структуры — осознанная копия под Guard.
  uint8_t — специализации без cli (1 байт атомарен сам).
- Свой detail::is_integral (специализации по фундаментальным типам, включая wchar_t/char16_t/char32_t)
  + builtin __is_enum. Причина: тулчейн без <type_traits>; __is_integral появился только в GCC 14.
  Внешние STL (ArduinoSTL/avr-libstdcpp) не тянем.
- load/store — основной способ доступа к volatile, разделяемым с ISR. Исключение: raw() в Tick
  читает counter под собственным cli/SREG (атомарная пара ms+sub, Guard не подходит).

## time/Tick.h
- Тик = 1000 мкс ровно. Timer2 CTC, prescaler 64 (datasheet табл. 17-9: CS22=1,CS21=0,CS20=0).
  OCR2A_VALUE = TICKS_PER_MS-1: 249 @16M, 124 @8M. static_assert(OCR2A_VALUE<=255).
- static_assert(F_CPU % 64000 == 0) — целая миллисекунда (второй слой под guard Mcu.h).
- SUB_US = 64000000/F_CPU: 4 мкс @16M, 8 мкс @8M. Разрешение субшкалы = SUB_US.
- init(): TIMSK2=0 → TCCR2A=WGM21 → TCCR2B=0 → OCR2A → TCNT2=0 → TIFR2=OCF2A(сброс) →
  TIMSK2=OCIE2A → TCCR2B=CS22. sei() НЕ вызывает (core/пользователь). Повторный init безопасен.
- now() — HW_INLINE, атомарное чтение counter через hw::load.
- raw(m,t) — атомарная пара под cli. Компенсация OCF2A при t<OCR2A_VALUE корректна
  (аппаратный сброс TCNT2 в CTC не зависит от ISR). Предусловие: time::init() вызван.
- micros() — разрешение SUB_US, переполнение ~1.19 ч (2^32 мкс). Формула m*1000+t*SUB_US
  сдвигами: (m<<10)-(m<<4)-(m<<3)+(t*SUB_US). Без умножений.
  (4.77 ч — это шкала 2^32 отсчётов по 4 мкс, не micros.)
- subNow() — младшие 16 бит отсчётов SUB_US, переполнение ~262 мс @16M / ~524 мс @8M.
  Для таймаутов периферии < 131 мс (@16M). Формула (uint16_t)m*TICKS_PER_MS+t.
- ms()/sec() — constexpr. us(v) — constexpr, округление ВВЕРХ в 32 битах (синхронно с armUs;
  защита от wrap при v>=65533). Единица интервалов API — миллисекунда.
- OC2A/OC2B как GPIO свободны (COM2Ax=0), но analogWrite(3)/analogWrite(11) не работают при живом Tick.

## time/Tick.cpp
- Содержит detail::counter (volatile uint32) и ISR(TIMER2_COMPA_vect){++counter}.
- ISR без Guard (прерывания запрещены на входе), без ISR_NOBLOCK (короткая).
- Не единственный .cpp: есть peripheral/AdcStream.cpp (ISR ADC_vect).
- Векторы TIMER2_COMPB/TIMER2_OVF свободны. Диагностических счётчиков нет.

## time/Deadline.h
- Размеры (AVR alignof=1): Deadline 9 Б, Period 8 Б, ShortDeadline 5 Б.
- Deadline: одноразовый срок, беззнаковая арифметика корректна при переполнении (~49.7 сут).
  toMs==0 — «сразу истёк». «Бесконечный» таймаут — не армировать (active()==false).
- Period: constexpr-конструктор (.bss без ctors). last_+=step_ без дрейфа;
  ресинхронизация после простоя (if n-last_>=step_ → last_=n), пропущенные периоды не догоняет.
  step_==0 — выключен.
- ShortDeadline: субшкала SUB_US, макс 65535 отсчётов (~262 мс @16M). arm/armUs/expired — HW_INLINE.
  armUs округляет ВВЕРХ в 32 битах (защита us_>=65533 от wrap → ложный Timeout).
- Все сроки в мс (Deadline/Period) или отсчётах subNow (ShortDeadline). Микросекунды — только armUs().
- Один контекст на объект (обычно main). main+ISR — Guard снаружи или разные объекты.

## peripheral/Adc.h
- Adc<REF,CLK>: 0 байт RAM, все static. Единовладелец ADC (не совмещать с AdcStream).
- init(didrMask): ADCSRA=0 ПЕРВЫМ (обрыв конверсии в полёте, ADEN=0) → DIDR0 → ADMUX=REF →
  ADCSRB=0 → ADCSRA=ADEN|CLK. polling, ADIE=0 (висящий ADIF не опасен).
- start(ch): BadParam первым (дёшево, без SFR), затем NotReady (ADEN=0), Busy (ADSC=1).
  ADMUX=REF|ch целиком (ADLAR=0, reserved=0, один out). Неблокирующий.
- validChannel: ch<=7 || TEMP_SENSOR(8) || BANDGAP(14) || GND_CH(15). Именованные константы.
- ready(): ADSC==0. take(): NotReady если конверсия; иначе uint16_t ADC (ADCL→ADCH аппаратно).
- Первая конверсия после init — 25 тактов ADC, далее 13. Right-aligned (ADLAR=0 всегда).
- Каналы 6/7 (A6/A7) — ADC-only, GPIO-типов нет. Смена канала при высокоомном источнике:
  первую выборку отбрасывать. Div16 снижает ENOB до ~7-8.

## peripheral/AdcStream.h / .cpp
- AdcStream<REF,CLK,CH>: один канал, free-running, единовладелец ADC. Один экземпляр на прошивку
  (detail::buf/head/tail/missed глобальные; два экземпляра разделят буфер — не поддерживается).
- Буфер: detail::BUF_LOG2=6 → 64 выборки (128 Б). ISR(ADC_vect) пишет только buf и head;
  tail/missed — только consumer (SPSC). missed() — plain read (один контекст main, ISR не трогает).
- init(): ADCSRA=0 → сброс head/tail/missed → DIDR0 → ADMUX=REF|CH → ADCSRB=0 →
  ADCSRA=ADEN|ADIE|ADIF|CLK. ADIF=1 в пусковой записи — сброс висящего флага (write-1-clear),
  иначе мгновенный ложный ISR после ADIE=1. Гонка init закрыта без Guard (ADEN=0/ADIE=0 первым).
- start(): ADATE|ADSC. stop(): снять ADATE|ADIE (ADSC может висеть — init следующего владельца обрывает).
- Переполнение: ISR перезаписывает старейшее (не блокируется), потери считает consumer в sync()
  (цикл стабилизации head). available/take/read — неблокирующие; take → NotReady если пусто.
- Torn-read при переполнении: ISR может перезаписать 16-битную ячейку во время чтения consumer —
  цена неблокирующего ISR. Для строгого чтения — Guard на чтение. Допустимо для lossy-потока.
- SAMPLE_RATE = F_CPU/divOf(CLK)/14 — верхняя оценка периода (free-running 13.5 такта по datasheet,
  замер ~14.01), частота занижена ~3.7%. @Div128/16M ≈ 8928 SPS.
- Oversampling/decimation — отдельный consumer (logic/Decimator).

## peripheral/Mcp23017.h
- Mcp23017<ADDR,BUS>: 16-битный GPIO расширитель по I2C. 0 Б RAM, все static.
  ADDR 0x20..0x27 (static_assert). BUS — тип bus::I2c<HZ,PULLUP>.
  Несколько чипов = несколько типов; массив состояний — у consumer.
- init(pullUp=false): форсирует IOCON=0 (BANK=0, SEQOP=0), IODIRA/B=0xFFFF (все INPUT),
  GPPUA/B=0xFFFF или 0. Fail-fast на первой ошибке I2C.
- read16(): Result<uint16_t>, атомарный снимок обоих портов (GPIOA защёлкивает A+B
  при SEQOP=0). low byte=A, high byte=B. Эквивалент Adafruit readGPIOAB().
- readA()/readB(): одиночный порт, экономит 1 байт обмена.
- write16(val): OLATA/B sequential write. Только для OUTPUT пинов.
- setDir(mask): бит=1 — INPUT (конвенция MCP, противоположно AVR DDR).
- setPullUp(mask): бит=1 — pull-up (~100к), только для INPUT.
- writeReg/readReg: низкоуровневый доступ к произвольному регистру.
- Прерывания INTA/INTB не используются (polling). HAEN не включается.
- Таймауты: BUS::msFor(n) на каждую операцию. Ошибки I2c пропагируются как есть.

## peripheral/Hd44780.h
- Hd44780<RS,EN,D4,D5,D6,D7>: LCD 4-bit write-only, 0 Б RAM. RW=GND (busy не читается).
  init(): 40мс power-on + 0x03×3 + 0x02 (4-bit) + 0x28/0x08/0x01/0x06/0x0C.
  command()/data(): два ниббла, EN pulse 1 мкс. clear/home 2 мс, прочие 50 мкс.
  Задержки cycle-counted (__builtin_avr_delay_cycles + delayMs<N> цикл) — Tick не нужен.
- setCursor(col,row): 20×4, адреса 0x00/0x40/0x14/0x54. Вне диапазона — игнор.
- print(const char*) / print(uint32_t): без snprintf. display(on,cursor,blink).
- Blocking: init ~50 мс однократно, команды ~50 мкс, clear ~2 мс. Не из ISR.

## peripheral/DfPlayer.h
- DfPlayer<TX_PIN,BAUD=9600>: DFPlayer Mini поверх SoftUartTx, TX-only, 0 Б RAM.
  Кадр 10 байт на стеке: 7E FF 06 CMD 00 PH PL CH CL EF.
  checksum = -(FF+06+CMD+00+PH+PL) big-endian. FEEDBACK=0 (без подтверждения).
- volume(0..30), playTrack(1..65535), playMp3(1..9999), stop(), reset(), sendCmd().
  BadParam при vol>30 или n==0. Возврат Status от SoftUartTx::write.
- Пауза >=30 мс между командами — ответственность вызывающего.
- Ответы модуля не читаются (fire-and-forget). IRQ off ~10 мс на кадр @9600.

## logic/Decimator.h
- Decimator<EXTRA>: N=4^EXTRA выборок, sum>>EXTRA → (10+EXTRA) бит. EXTRA 0..4. 6 Б RAM.
  constexpr-конструктор. feed() неблокирующий.
- Один feed-путь, два тира: feed().value — нормализованный (sum>>EXTRA, про-тир, суб-LSB разрешение);
  sum — сырая сумма последнего блока (поле), читается после ok() ДО первого feed() следующего блока.
  Обнуление sum — на первой выборке нового блока (cnt==0), не при выдаче ok.
- Средний тир: value>>EXTRA (10-bit, шумоподавление ~sqrt(N)). Это НЕ среднее sum>>2*EXTRA,
  а среднее, масштабированное в (10+EXTRA)-битную шкалу.
- Даёт РАЗРЕШЕНИЕ, не абсолютную точность (INL/offset остаются). Требует ~1 LSB шума (dither).
- Подтверждено O3/Q2: математика верна; bandgap слишком чист для замера ENOB.

## logic/Rms.h
- Rms<LOG2N,BITS>: полный RMS (DC+AC) = sqrt(mean(x^2)), без вычитания среднего.
  Окно N=2^LOG2N, среднее = сдвиг. BITS 1..14 (10 raw / 12 / 14 oversampled). 6 Б RAM.
- sumSq uint32; static_assert(MAXV^2*N <= 0xFFFFFFFF) — граница включительно (равенство помещается).
- Контракт: s<=MAXV (вызывающий гарантирует). Маска s&=MAXV не применяется (скрыла бы ошибку ввода).
- feed() неблокирующий: NotReady пока окно не полно; ok → floor(RMS). HW_INLINE (hot path).
- isqrt32: digit-by-digit, без деления/float, ~15 итераций, обычный inline (раз в окно, не hot).
- AC-only (вычитание среднего) — отдельный модуль, не здесь.

## logic/Debounce.h
- Debounce<MS=20>: антидребезг цифрового входа, 13 Б RAM (Deadline 9 + 4 bool).
  init(initial): стартовое состояние без ложного фронта. update(raw): периодически.
  Алгоритм: изменение сырого перезапускает срок; подтверждение после MS стабильности.
  stable(): подтверждённое состояние. rose()/fell(): фронты read-and-clear.
  Задержка реакции = MS. Дребезг короче MS подавляется. Не для энкодеров.
- Один контекст на объект. Требует time::init().

## bus/Spi.h
- SpiMaster<MODE,CLK>: аппаратный SPI master, 0 Б RAM, polling. MODE 0..3, CLK из SpiClk.
- Div64x убран (даёт ту же F/64, что Div64, другим кодом — избыточен для выбора по частоте).
- init(): SS(PB2) выход+HIGH (иначе потеря MSTR), MOSI(PB3)/SCK(PB5) выходы, MISO(PB4) вход.
  SPSR=SPI2X?, SPCR=SPE|MSTR|MODE|SPR. (void)SPSR/(void)SPDR — сброс SPIF/WCOL (volatile, не выкинется).
- SCK_HZ = F_CPU/spiDiv(CLK), табл. 19-2. Максимум 8 МГц @16M. Порядок SPSR до SPCR подтверждён
  замером (us_2=1 мкс/байт @Div2) — на 328P SPI2X применяется и при записи до SPE.
- transfer(b): bounded-blocking (максимум 8 SCK при SPE=1), возврат rx. При SPE=0 повиснет —
  контракт: только после init, до deinit.
- start/ready/take: неблокирующий конвейер. take при WCOL: байт прочитан (слот освобождён),
  но недействителен → IoFault. Повторный start до ready → WCOL.
- write(p,n): best effort, без Status (у мастера ошибок кроме WCOL нет).
- SCK=D13=LED: мерцание при передаче нормально. CS устройства — снаружи через hw::Pin.
- Блочная передача через ISR-буфер — отдельный модуль (под дисплей), позже.

## bus/I2c.h
- I2c<HZ,PULLUP>: TWI master, 0 Б RAM, polling bounded-with-timeout. 7-бит адресация (0x00..0x7F).
- SCL = F_CPU/(16+2*TWBR), TWPS=0. static_assert(F_CPU>=16*HZ, TWBR<=255).
  100к→TWBR=72, 400к→TWBR=12 @16M. PULLUP=true: внутренние ~30-50к на A4/A5 (bench);
  внешние 2.2-4.7к для 400к/длинных проводов.
- init(): TWSR=0, TWBR=TWBR_VAL, TWCR=TWINT|TWEN (сброс висящего TWINT write-1-clear), pull-up если PULLUP.
- Коды TWSR: START 0x08/repSTART 0x10; TX SLA+W 0x18/data 0x28; RX SLA+R 0x40/data ACK 0x50/NACK 0x58.
- write(addr,p,n,toMs=5): n==0 допустим как ping (p не разыменовывается). read n==0 → BadParam.
  Явный ping(addr,toMs) — предпочтительнее write(...,0).
- writeRead(addr,w,wn,r,rn,toMs): wn==0 или rn==0 → BadParam (pointer-less read — это read()).
  repStart после MT-data ACK ожидает ровно 0x10.
- read(addr,p,n,toMs): читает с ТЕКУЩЕГО указателя регистра устройства (pointer-less).
- toMs — бюджет на ВЕСЬ обмен. msFor(n)=9*(n+2)*1000/HZ+2 — калькулятор минимума (constexpr).
  Для длинных блоков увеличивать (default 5 мс — только короткие).
- stop() → Status: ждёт сброса TWSTO с лимитом STOP_US=2000000/HZ+20 (из HZ, не константа).
  Успешный хвост write/read/writeRead/ping: return stop(). fail(e): (void)stop(), return err(e) —
  первичная ошибка сохраняется. Bus recovery (залипшая SDA) не реализован.
- При ошибке содержимое r[] не определено. Неблокирующий state-machine — позже.

## bus/Uart.h
- Uart<BAUD=9600>: USART0, 0 Б RAM, все static, polling, 8N1 async (UMSEL0=0).
  D0=RX(PD0), D1=TX(PD1). Прерывания USART не использует.
- Конфликт: Arduino Serial и bootloader живут на USART0. После Serial.begin() наш init()
  перезапишет настройки; одновременно не использовать. Джампер D0-D1 (loopback) мешает прошивке —
  ставить ПОСЛЕ upload, снимать ПЕРЕД следующим.
- Автовыбор U2X: UBRR_N/UBRR_X округление к ближайшему; USE_X2 = ERR_X < ERR_N (при равенстве U2X=0).
  static_assert: UBRR<=4095, BAUD_ERR_PCT100<=250 (2.5%), F_CPU>=8*BAUD.
  115200@16M: UBRR=16, U2X=1, err≈2.12% (U2X=0 дал бы 3.55% — отрезается assert).
- init(rxEnable=true): UBRR, U2X, TXEN|RXEN?, UCSZ01|UCSZ00 (8N1), дренаж RXC чтением UDR0.
  deinit(): UCSR0B=0 (пины свободны).
- put(b,toMs=5): ждёт UDRE (transmit buffer empty) с Deadline → Timeout. Запись в буфер,
  НЕ гарантия физической передачи. При TXEN=0 UDRE=1 (буфер пуст) → put вернёт ok, байт не уйдёт
  (тихая потеря). Контракт: только после init (TXEN=1). Физическую передачу гарантирует flush().
- write(const char*,toMs): возвращает Status (общий Deadline на всю строку).
  Для консистентности с write(uint8_t*,n). Best-effort остановка на Timeout.
- flush(toMs): ждёт TXC (последний бит ушёл), сброс write-1-clear. Для сна/смены baud/deinit.
- available(): RXC без ожидания. get(toMs): ждёт RXC → Timeout; FE/DOR/UP → Overflow
  (байт прочитан, буфер освобождён, value недействителен). Флаги читаются ДО UDR0.
- read(dst,n,toMs): до n байт или первой ошибки, возвращает count. Повреждённый кадр останавливает набор.
- Один контекст (main), не из ISR. USART0 один — два Uart<> с разным baud конфликтуют.

## bus/SoftUart.h
- SoftUartTx<PIN,BAUD,TX_OVERHEAD=8>: bit-bang TX на любом GPIO. 0 Б RAM.
  PIN — hw::Pin<Port,Bit>. Несколько экземпляров на разных пинах допустимы.
  init(): Out+High (idle). put(b): Guard + 10 бит cycle-counted (__builtin_avr_delay_cycles).
  write(p,n)/write(s): цикл put. IRQ off ~1 мс/байт @9600. Всегда ok().
- SoftUartRx<PIN,BAUD,RX_OVERHEAD=6>: bit-bang RX на любом GPIO. 0 Б RAM.
  init(): InPullUp. available(): !read() (LOW=start). get(toMs=5): cycle-budget poll
  (НЕ Deadline! Timer2 ISR заморожен под Guard) + sample center + stop check.
  Timeout/IoFault/ok. IRQ off до toMs + ~1 байт. toMs=0: одна проверка.
- BAUD <= 250000. BIT_CYCLES = F_CPU/BAUD (truncation error < 0.7% @115200).
  TX_OVERHEAD/RX_OVERHEAD — калибруются бенчем (scope/logic analyzer).
- Не конфликтует с USART0/bootloader/Serial/dbg. Свободные пины: D2-D9, A0-A5.
  Избегать D0/D1 (hardware USART), D10-D13 при активном SPI.
- Trade-off vs hardware Uart: любой пин / несколько实例ций, но CPU-bound,
  IRQ off на.transfer, ниже max baud. Для production real-time — hardware Uart.