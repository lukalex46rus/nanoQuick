#pragma once

// ============================================================================
// DfPlayer — DFPlayer Mini (MP3-модуль) по программному UART, TX-only.
// ----------------------------------------------------------------------------
// Платформа: ATmega328P/328, Arduino Nano (guard: hw/Mcu.h через SoftUart).
// Ресурсы:   1 GPIO (TX на RX модуля через резистор 1 кОм recommended).
//            Использует bus::SoftUartTx<PIN,BAUD> — IRQ off ~1 мс/байт @9600.
//            0 байт RAM: кадр собирается на стеке (10 байт).
// Предусл.:  init() до команд. Модуль питается и SD-карта вставлена.
//            Пауза >= 30 мс между командами — ответственность вызывающего
//            (модуль может потерять команду при плотной отправке).
// Контекст:  main (blocking); не из ISR. ~10 мс на кадр @9600.
// Огранич.:  TX-only: ответы модуля не читаются (RX не подключён).
//            Статус воспроизведения/ошибки неизвестны — fire-and-forget.
//            Для обратной связи нужен SoftUartRx + парсер ответов (позже).
//            volume 0..30, track/file 1..65535 (нумерация на SD).
// Зависит:   bus/SoftUart.h, base/Status.h, hw/Mcu.h.
// ============================================================================
#include "../hw/Mcu.h"
#include "../base/Status.h"
#include "../bus/SoftUart.h"

namespace peripheral {

// Протокол DFPlayer Mini (DFRobot FN-M16P / совместимые):
// кадр 10 байт: 7E FF 06 CMD FEEDBACK PAR_H PAR_L CHK_H CHK_L EF
// checksum = 0 - (FF + 06 + CMD + FEEDBACK + PAR_H + PAR_L), big-endian.
// FEEDBACK=0x00 — без запроса подтверждения.
template<typename TX_PIN, uint32_t BAUD = 9600>
struct DfPlayer {
    using Tx = bus::SoftUartTx<TX_PIN, BAUD>;

    static constexpr uint8_t CMD_VOLUME    = 0x06;
    static constexpr uint8_t CMD_PLAY_TF   = 0x03;   // play track by index (root)
    static constexpr uint8_t CMD_PLAY_MP3  = 0x12;   // play file in MP3 folder
    static constexpr uint8_t CMD_STOP      = 0x16;
    static constexpr uint8_t CMD_RESET     = 0x0C;

    static base::Status init() { return Tx::init(), base::ok(); }

    // Громкость 0..30. Вне диапазона — BadParam.
    static base::Status volume(uint8_t vol) {
        if (vol > 30) return base::err(base::Err::BadParam);
        return sendCmd(CMD_VOLUME, vol);
    }

    // Трек по индексу в корне SD (1..65535).
    static base::Status playTrack(uint16_t n) {
        if (n == 0) return base::err(base::Err::BadParam);
        return sendCmd(CMD_PLAY_TF, n);
    }

    // Файл в папке MP3 (имена 0001.mp3 .. 9999.mp3).
    static base::Status playMp3(uint16_t n) {
        if (n == 0) return base::err(base::Err::BadParam);
        return sendCmd(CMD_PLAY_MP3, n);
    }

    static base::Status stop()  { return sendCmd(CMD_STOP, 0); }
    static base::Status reset() { return sendCmd(CMD_RESET, 0); }

    // Низкоуровневая отправка произвольной команды.
    static base::Status sendCmd(uint8_t cmd, uint16_t param) {
        uint8_t ph = (uint8_t)(param >> 8);
        uint8_t pl = (uint8_t)(param & 0xFF);
        uint16_t sum = (uint16_t)(0xFF + 0x06 + cmd + 0x00 + ph + pl);
        uint16_t chk = (uint16_t)(0u - sum);

        uint8_t frame[10] = {
            0x7E, 0xFF, 0x06, cmd, 0x00, ph, pl,
            (uint8_t)(chk >> 8), (uint8_t)(chk & 0xFF), 0xEF
        };
        return Tx::write(frame, 10);
    }
};

} // namespace peripheral