#pragma once

// ============================================================================
// Hd44780 — символьный LCD (HD44780/совместимые), 4-bit write-only.
// ----------------------------------------------------------------------------
// Платформа: ATmega328P/328, Arduino Nano (guard: hw/Mcu.h).
// Ресурсы:   6 GPIO: RS, EN, D4, D5, D6, D7. RW не используется (write-only,
//            фиксированные задержки вместо чтения busy flag) — экономит пин.
//            0 байт RAM: все методы static, пины — параметры шаблона.
// Предусл.:  init() до остальных методов. Питание LCD должно быть стабильно
//            до init() (внутренняя пауза 40 мс покрывает power-on reset).
//            time::init() НЕ требуется — задержки cycle-counted.
// Контекст:  main (blocking); не из ISR. Блокировка: команды ~37 мкс,
//            clear/home ~1.52 мс, init ~50 мс (однократно).
// Огранич.:  Только запись (RW=GND). Чтение busy/DDRAM невозможно.
//            Геометрия 20×4 (адреса строк 0x00/0x40/0x14/0x54). Для 16×2
//            те же адреса строк 0/1 работают; 16×4/20×2 — проверить адресацию.
//            CGRAM (свои символы) — через command()/data(), API позже.
//            Задержки — минимумы по datasheet, не точные тайминги.
// Зависит:   hw/Mcu.h, hw/Pin.h, base/Status.h.
// ============================================================================
#include "../hw/Mcu.h"
#include "../hw/Pin.h"
#include "../base/Status.h"

namespace peripheral {

// Datasheet HD44780, таблица 6 (instructions), рисунок 24 (4-bit init).
// EN pulse width min 450 нс; data setup min 80 нс; hold min 10 нс.
// Instruction execution: 37 мкс (большинство), clear/home 1.52 мс.
template<typename RS, typename EN, typename D4, typename D5, typename D6, typename D7>
struct Hd44780 {
    static constexpr uint32_t CYC_US = F_CPU / 1000000UL;   // тактов на мкс

    // Короткая задержка (compile-time). Для длинных — delayMs<N>().
    template<uint32_t CYCLES>
    static inline void delayCyc() { __builtin_avr_delay_cycles(CYCLES); }

    // Длинная задержка: N × 1 мс циклом (короткий builtin внутри — малый код).
    template<uint16_t MS>
    static inline void delayMs() {
        for (uint16_t i = 0; i < MS; ++i)
            __builtin_avr_delay_cycles(F_CPU / 1000UL);
    }

    // ---- Инициализация ----------------------------------------------------
    // Последовательность 4-bit init по datasheet (Figure 24).
    static void init() {
        RS::init(hw::Dir::Out, hw::Level::Low);
        EN::init(hw::Dir::Out, hw::Level::Low);
        D4::init(hw::Dir::Out, hw::Level::Low);
        D5::init(hw::Dir::Out, hw::Level::Low);
        D6::init(hw::Dir::Out, hw::Level::Low);
        D7::init(hw::Dir::Out, hw::Level::Low);

        delayMs<40>();              // power-on reset (>40 мс после VCC stable)

        write4(0x03); delayMs<5>(); // 8-bit mode attempt 1 (>4.1 мс)
        write4(0x03); delayMs<5>(); // attempt 2 (>4.1 мс)
        write4(0x03); delayCyc<200 * CYC_US>();  // attempt 3 (>100 мкс)
        write4(0x02); delayCyc<200 * CYC_US>();  // switch to 4-bit

        command(0x28);              // function set: 4-bit, 2 lines, 5x8
        command(0x08);              // display off
        command(0x01);              // clear (1.52 мс внутри command)
        command(0x06);              // entry mode: increment, no shift
        command(0x0C);              // display on, cursor off, blink off
    }

    // ---- Команды ----------------------------------------------------------
    static void command(uint8_t cmd) {
        RS::low();
        send(cmd);
        // clear/home требуют 1.52 мс, остальные 37 мкс.
        if (cmd == 0x01 || cmd == 0x02) delayMs<2>();
        else                            delayCyc<50 * CYC_US>();
    }

    static void clear() { command(0x01); }
    static void home()  { command(0x02); }

    // Курсор: col 0..19, row 0..3 (20×4). Вне диапазона — молча игнорируется.
    static void setCursor(uint8_t col, uint8_t row) {
        if (col >= 20 || row >= 4) return;
        static constexpr uint8_t ROW_OFF[4] = { 0x00, 0x40, 0x14, 0x54 };
        command((uint8_t)(0x80 | (ROW_OFF[row] + col)));
    }

    // Управление отображением: display/cursor/blink on-off.
    static void display(bool on, bool cursor = false, bool blink = false) {
        command((uint8_t)(0x08 | (on ? 0x04 : 0) | (cursor ? 0x02 : 0) | (blink ? 0x01 : 0)));
    }

    // ---- Данные -----------------------------------------------------------
    static void data(uint8_t b) {
        RS::high();
        send(b);
        delayCyc<50 * CYC_US>();
    }

    static void print(const char* s) { while (*s) data((uint8_t)*s++); }

    // Печать числа без snprintf (десятичное, без ведущих нулей).
    static void print(uint32_t v) {
        char buf[11];
        int8_t i = 10;
        buf[i] = 0;
        do { buf[--i] = (char)('0' + (v % 10u)); v /= 10u; } while (v && i > 0);
        print(&buf[i]);
    }

private:
    // Один ниббл (младшие 4 бита b) на D4..D7 + строб EN.
    static void write4(uint8_t b) {
        if (b & 0x01) D4::high(); else D4::low();
        if (b & 0x02) D5::high(); else D5::low();
        if (b & 0x04) D6::high(); else D6::low();
        if (b & 0x08) D7::high(); else D7::low();
        // Data setup выполняется естественно (sbi/cbi по 2 такта).
        EN::high();
        delayCyc<16>();             // EN pulse >= 450 нс (1 мкс @16M)
        EN::low();
        delayCyc<16>();             // hold + cycle margin
    }

    // Байт двумя нибблами: старший, затем младший.
    static void send(uint8_t b) {
        write4((uint8_t)(b >> 4));
        write4((uint8_t)(b & 0x0F));
    }
};

} // namespace peripheral