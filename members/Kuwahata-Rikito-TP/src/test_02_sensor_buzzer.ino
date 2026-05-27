#include <Arduino.h>

// =====================================================
// test_02_sensor_buzzer.ino
// 目的: HC-SR04とブザーを接続し、距離に応じた音を確認する
// =====================================================

// ピン定義
const uint8_t PIN_TRIG = 9;   // HC-SR04 Trig
const uint8_t PIN_ECHO = 10;  // HC-SR04 Echo
const uint8_t PIN_BUZZER = 6; // パッシブブザー

// しきい値(mm)
const int NEAR_THRESHOLD_MM = 100;
const int MID_THRESHOLD_MM = 300;

// 測定・鳴動周期
const unsigned long MEASURE_INTERVAL_MS = 100;
const unsigned long MID_BEEP_INTERVAL_MS = 100;
const unsigned long FAR_BEEP_ON_MS = 100;
const unsigned long FAR_BEEP_OFF_MS = 900;
const unsigned long ECHO_TIMEOUT_US = 25000UL;

// 周波数
const int FREQ_NEAR = 2200;
const int FREQ_MID = 1500;
const int FREQ_FAR = 900;

// 状態保持
int distanceMm = 0;
bool buzzerOn = false;
unsigned long lastMeasureMillis = 0;
unsigned long lastBeepToggleMillis = 0;

int readDistanceMm();
uint8_t classifyZone(int measuredMm);
void outputBuzzerByZone(uint8_t zone);

void setup() {
  // センサーのピンモード設定
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);

  // ブザーの初期化
  pinMode(PIN_BUZZER, OUTPUT);
  noTone(PIN_BUZZER);

  // 基準時刻を合わせる
  lastMeasureMillis = millis();
  lastBeepToggleMillis = millis();
}

void loop() {
  unsigned long now = millis();

  // 一定周期で距離を読み取る
  if (now - lastMeasureMillis >= MEASURE_INTERVAL_MS) {
    lastMeasureMillis = now;
    int measured = readDistanceMm();

    // 有効値なら現在距離を更新
    if (measured > 0) {
      distanceMm = measured;
    }
  }

  // 距離区分を決めて、音へ反映する
  uint8_t zone = classifyZone(distanceMm);
  outputBuzzerByZone(zone);
}

int readDistanceMm() {
  // Trigを安定化
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);

  // 10usパルスを出して測距開始
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);

  // Echoパルス幅を取得
  unsigned long durationUs = pulseIn(PIN_ECHO, HIGH, ECHO_TIMEOUT_US);
  if (durationUs == 0) {
    return 0;
  }

  // mm変換(distance(mm) = duration(us) * 0.1715)
  long measuredMm = (long)(durationUs * 1715L) / 10000L;
  if (measuredMm < 1 || measuredMm > 4000) {
    return 0;
  }

  return (int)measuredMm;
}

uint8_t classifyZone(int measuredMm) {
  if (measuredMm <= 0) {
    // 測定失敗時は遠距離扱いにして安全側へ倒す
    return 2;
  }

  if (measuredMm <= NEAR_THRESHOLD_MM) {
    return 0;
  }

  if (measuredMm <= MID_THRESHOLD_MM) {
    return 1;
  }

  return 2;
}

void outputBuzzerByZone(uint8_t zone) {
  unsigned long now = millis();

  if (zone == 0) {
    // 近距離: 連続高音
    tone(PIN_BUZZER, FREQ_NEAR);
    buzzerOn = true;
    return;
  }

  if (zone == 1) {
    // 中距離: 100msごとにON/OFF
    if (now - lastBeepToggleMillis >= MID_BEEP_INTERVAL_MS) {
      lastBeepToggleMillis = now;
      buzzerOn = !buzzerOn;

      if (buzzerOn) {
        tone(PIN_BUZZER, FREQ_MID);
      } else {
        noTone(PIN_BUZZER);
      }
    }
    return;
  }

  // 遠距離: 短音 + 長い無音
  unsigned long interval = buzzerOn ? FAR_BEEP_ON_MS : FAR_BEEP_OFF_MS;
  if (now - lastBeepToggleMillis >= interval) {
    lastBeepToggleMillis = now;
    buzzerOn = !buzzerOn;

    if (buzzerOn) {
      tone(PIN_BUZZER, FREQ_FAR);
    } else {
      noTone(PIN_BUZZER);
    }
  }
}
