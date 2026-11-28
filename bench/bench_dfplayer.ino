#include <Arduino.h>
#include "../src/time/Tick.h"
#include "../src/peripheral/DfPlayer.h"
#include "dbg.h"

// ============================================================================
// bench_dfplayer — кадр протокола + отправка (TX-only, без обратной связи).
// Платформа: ATmega328P/328, Nano. Контекст: main.
// Requires: DFPlayer Mini, TX=D8 → RX модуля (через 1 кОм), питание 5V, SD с MP3/.
// Без модуля: send_ok=1 всё равно (TX не знает о получателе) — Info по звуку.
// Ожидание: chk_vol=65254 (0xFEE6 для cmd=06 param=15), send_ok=1,
//           bad_vol=1 (volume(31) → BadParam), bad_trk=1 (playTrack(0) → BadParam),
//           frame_us ~10000..12000 (10 байт @9600).
// Звук — на слух (Info). UART-часть проверяет протокол и валидацию.
// ============================================================================
using Mp3 = peripheral::DfPlayer<hw::board::D8, 9600>;

// Проверка checksum без отправки (вычисление как в sendCmd).
__attribute__((noinline)) static uint16_t calcChk(uint8_t cmd, uint16_t param) {
    uint16_t sum = (uint16_t)(0xFF + 0x06 + cmd + 0x00 + (param >> 8) + (param & 0xFF));
    return (uint16_t)(0u - sum);
}

void setup() {
    dbg::begin(9600);
    time::init();
    Mp3::init();

    uint16_t chkVol = calcChk(Mp3::CMD_VOLUME, 15);   // 0xFEE6 = 65254

    // Валидация параметров (без железа).
    uint8_t badVol = !Mp3::volume(31).ok() ? 1 : 0;
    uint8_t badTrk = !Mp3::playTrack(0).ok() ? 1 : 0;

    // Отправка: громкость 15, играть MP3/0001.mp3. Пауза 30 мс между командами.
    uint32_t t0 = time::micros();
    base::Status sv = Mp3::volume(15);
    uint32_t frameUs = time::micros() - t0;

    delay(30);
    base::Status sp = Mp3::playMp3(1);
    delay(30);

    dbg::text("\r\n=== Z4: DFPlayer ===\r\n");
    dbg::row("chk_vol  =", chkVol);                  // 65254
    dbg::row("bad_vol  =", badVol);                  // 1
    dbg::row("bad_trk  =", badTrk);                  // 1
    dbg::row("send_ok  =", (sv.ok() && sp.ok()) ? 1 : 0);  // 1
    dbg::row("frame_us =", frameUs);                 // ~10400
    dbg::flush();
}

void loop() {}