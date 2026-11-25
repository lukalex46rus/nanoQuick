# Контракты модулей nano_quick

## hw/Mcu.h
- F_CPU_HZ и CYC_PER_US — constexpr, не занимают SRAM.
- PortB/C/D: все методы static inline, 0 байт состояния.
- flip() использует запись в PINx (аппаратный toggle, 1 такт).
- HW_INLINE = __attribute__((always_inline)). Применять только к методам,
  вызываемым в горячем пути из нескольких мест.

## hw/Pin.h
- Pin<Port, Bit>: 0 байт RAM, все методы static.
- mask вычисляется при компиляции.
- init() устанавливает направление и начальное состояние атомарно.
- board::D0..D13/A0..A5 — единственные допустимые имена пинов.
  Модули НЕ определяют свои typedef пинов.

## hw/Atomic.h
- Guard восстанавливает SREG, а не включает прерывания безусловно.
- load()/store() — единственный способ доступа к volatile-переменным,
  разделяемым с ISR. Прямое чтение многобайтовых переменных запрещено.
- Guard не копируем и не присваиваем.

## base/Status.h
- Err — enum class uint8_t, значения фиксированы, не меняются.
- Status — 1 байт, ok() и explicit operator bool.
- Result<T> — значение + статус, для операций чтения.
- Фабрики ok()/err() — агрегатная инициализация, без temporaries.
- Никаких строк ошибок в RAM. Тексты — отдельным слоем через PROGMEM.

## time/Tick.h
- Тик = 1000 мкс ровно. Timer2 CTC, prescaler 64, OCR2A = 249.
- Datasheet ATmega328P таблица 17-9: CS22=1,CS21=0,CS20=0 → presc 64.
- SUB_US = 4 мкс. Разрешение субшкалы = 4 мкс.
- now() — HW_INLINE, атомарное чтение counter.
- raw() — атомарная пара (ms, sub). Компенсация флага OCF2A корректна
  при условии t < OCR2A_VALUE.
- micros() — разрешение 4 мкс, переполнение ~4.77 часа.
  Формула: (m<<10)-(m<<4)-(m<<3)+(t<<2). Без умножений.
- subNow() — единица = 4 мкс, переполнение ~262 мс.
  Для таймаутов периферии < 131 мс.
- ms()/sec()/us() — constexpr, сворачиваются при компиляции.
- Единица интервалов в API — миллисекунда. Пересчётов внутри библиотеки нет.

## time/Tick.cpp
- Единственный .cpp в библиотеке. Содержит только counter и ISR(TIMER2_COMPA_vect).
- Никаких диагностических счётчиков или дополнительных ISR.
- Векторы TIMER2_COMPB и TIMER2_OVF свободны.

## time/Deadline.h
- Deadline: одноразовый срок, беззнаковая арифметика, корректна при переполнении.
- Period: периодический триггер, constexpr-конструктор (.bss, без ctors).
  last_ += step_ предотвращает накопление дрейфа.
- ShortDeadline: субмиллисекундная шкала, разрешение 4 мкс, макс ~262 мс.
  arm/armUs/expired — HW_INLINE.
- Все сроки в миллисекундах (Deadline/Period) или отсчётах subNow (ShortDeadline).
  Методы НЕ принимают «микросекунды» напрямую, кроме armUs().
	
	## peripheral/Adc.h
- Adc<REF,CLK>: 0 байт RAM, все методы static. Владеет ADC единолично.
- init(didrMask): ADMUX=REF, ADCSRB=0, DIDR0=mask, ADCSRA=ADEN|CLK.
- start(ch): Busy если ADSC=1; BadParam если ch не в {0..8,14,15}. Неблокирующий.
- ready(): ADSC==0. take(): NotReady если идёт конверсия; иначе ADC (ADCL->ADCH).
- Первая конверсия после init — 25 тактов ADC, далее 13.
- Каналы 6/7 (A6/A7) допустимы для ADC, GPIO-типов для них нет.
- Смена канала при высокоомном источнике: первую выборку отбрасывать.
- 6-7 бит: value>>3 / value>>4. Быстрый делитель (Div16) снижает ENOB до ~7-8.
- Adc и AdcStream взаимоисключающие: второй init() запрещён.

## peripheral/AdcStream.h / .cpp
- AdcStream<REF,CLK,CH>: один канал, free-running, единовладелец ADC.
- Не совмещать с adc::Adc. Один экземпляр на прошивку.
- Буфер фиксирован: detail::BUF_LOG2 (по умолч. 6 -> 64 выборки, 128 Б).
- ISR(ADC_vect) пишет только buf и head; tail/missed — только consumer (SPSC).
- Переполнение: перезапись старейшего, missed += потерянные (consumer sync).
- available/take/read — неблокирующие; take -> NotReady если пусто.
- SAMPLE_RATE = F_CPU / divOf(CLK) / 14 (free-running, замерено; single-shot = 13).
- Нагрузка CPU ≈ cycles_ISR * SAMPLE_RATE / F_CPU (замерить).
- Oversampling/decimation — отдельный consumer-накопитель (этап A5).

## logic/Decimator.h
- Decimator<EXTRA>: сумма 4^EXTRA выборок >> EXTRA -> (10+EXTRA) бит.
- EXTRA 0..4. 6 Б RAM. constexpr-конструктор. feed() неблокирующий.
- Про-тир: value как есть (10+EXTRA бит, суб-LSB разрешение).
- Средний тир: value >> EXTRA (10-bit, шумоподавление ~sqrt(N)).
- Даёт РАЗРЕШЕНИЕ, не абсолютную точность (INL/offset остаются).
- Требует ~1 LSB шума (dither). Для точности — калибровка по bandgap.
- Подтверждено O3/Q2: математика верна; bandgap слишком чист для замера ENOB.

## logic/Rms.h
- Rms<LOG2N,BITS>: полный RMS (DC+AC) = sqrt(mean(x^2)), без вычитания среднего.
- Окно N=2^LOG2N, среднее = сдвиг. BITS 1..14 (10 raw / 12 / 14 oversampled).
- sumSq uint32; static_assert(maxVal^2*N <= 2^32-1) ловит переполнение на компиляции.
- 6 Б RAM. feed() неблокирующий: NotReady пока окно не полно; ok -> floor(RMS).
- isqrt32: digit-by-digit, без деления/float, ~15 итераций; вызывается раз в окно.
- AC-only (вычитание среднего) — отдельный модуль, не здесь.

## bus/Spi.h
- SpiMaster<MODE,CLK>: аппаратный SPI master, 0 Б RAM.
- init(): SS(PB2) выход+HIGH (иначе потеря MSTR), MOSI/SCK выходы, MISO вход.
- SCK_HZ = F_CPU / spiDiv(CLK), таблица 19-2. Максимум 8 МГц @16 МГц.
- transfer(b): bounded-blocking, максимум 8 SCK, возврат rx. Безопасен в master.
- start/ready/take: неблокирующий конвейер; повторный start до ready -> WCOL -> IoFault.
- SCK = D13 = LED: мерцание при передаче нормально.
- CS устройства — снаружи, через hw::Pin; модуль управляет только шиной.
- Блочная передача через ISR-буфер — отдельный модуль (под дисплей), позже.

## bus/I2c.h
- I2c<HZ,PULLUP>: TWI master, bounded-with-timeout write/read. 0 Б RAM.
- SCL = F_CPU/(16+2*TWBR), TWPS=0. 100к->TWBR=72, 400к->TWBR=12.
- PULLUP=true: внутренние ~30-50к на A4/A5 (bench). Внешние 2.2-4.7к для 400к.
- Коды: TX 0x08/0x18/0x28; RX 0x40/0x50/0x58. Timeout -> stop + Err::Timeout.
- write(addr,p,0) — скан адреса (SLA+W + stop). read n==0 -> BadParam.
- Блокировка ограничена Deadline (toMs). Неблокирующий state-machine — позже.