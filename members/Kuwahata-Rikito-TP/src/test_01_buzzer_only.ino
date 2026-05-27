#include <Arduino.h>

// =====================================================
// test_01_buzzer_only.ino
// 目的: ブザー単体の配線確認と鳴動確認を行う
// =====================================================

// ブザー接続ピン
const uint8_t PIN_BUZZER = 6;

// デモで使う周波数(Hz)
const int FREQ_HIGH = 2200; // 高音
const int FREQ_MID = 1500;  // 中音
const int FREQ_LOW = 900;   // 低音

// 音を切り替える周期(ms)
const unsigned long STEP_INTERVAL_MS = 800;

void setup() {
  // ブザー出力ピンの初期化
  pinMode(PIN_BUZZER, OUTPUT);

  // 起動時は確実に停止状態にする
  noTone(PIN_BUZZER);
}

void loop() {
  // 0:高音 -> 1:中音 -> 2:低音 の順で鳴らす
  static uint8_t step = 0;
  static unsigned long lastStepMillis = 0;

  unsigned long now = millis();

  // 一定周期で音階を切り替える
  if (now - lastStepMillis >= STEP_INTERVAL_MS) {
    lastStepMillis = now;

    if (step == 0) {
      tone(PIN_BUZZER, FREQ_HIGH);
    } else if (step == 1) {
      tone(PIN_BUZZER, FREQ_MID);
    } else {
      tone(PIN_BUZZER, FREQ_LOW);
    }

    step = (uint8_t)((step + 1) % 3);
  }
}
