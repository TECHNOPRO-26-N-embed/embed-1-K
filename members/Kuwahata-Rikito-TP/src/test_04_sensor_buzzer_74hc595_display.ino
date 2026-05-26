#include <Arduino.h>

// =====================================================
// test_04_sensor_buzzer_74hc595_display.ino
// 目的: HC-SR04 + ブザー + 4桁7セグ(74HC595 x1) の統合確認
//
// 構成:
// - 74HC595は「セグメント(a,b,c,d,e,f,g,dp)」のみ担当
// - 桁選択(DIG1～DIG4)はArduinoのGPIOで直接制御
// - 4桁を高速で順番に点灯するダイナミック点灯方式
//
// 競合しないピン割り当て:
// - 74HC595: D8, D9, D10
// - 4桁選択: D2, D3, D4, D5
// - ブザー: D6
// - HC-SR04: D11(Trig), D12(Echo)
// =====================================================

// -----------------------------------------------------
// 74HC595配線（Elegooサンプルに合わせる）
// -----------------------------------------------------
const uint8_t PIN_595_LATCH = 9;  // STCP: 出力反映タイミング制御（ラッチ）
const uint8_t PIN_595_CLOCK = 10; // SHCP: シフトクロック
const uint8_t PIN_595_DATA = 8;   // DS: シリアルデータ入力

// -----------------------------------------------------
// 桁選択ピン（4桁）
// -----------------------------------------------------
const uint8_t PIN_DIGIT_1 = 2;
const uint8_t PIN_DIGIT_2 = 3;
const uint8_t PIN_DIGIT_3 = 4;
const uint8_t PIN_DIGIT_4 = 5;
const uint8_t DIGIT_PINS[4] = {PIN_DIGIT_1, PIN_DIGIT_2, PIN_DIGIT_3, PIN_DIGIT_4};

// -----------------------------------------------------
// ブザー + HC-SR04（表示系と競合しないピン）
// -----------------------------------------------------
const uint8_t PIN_BUZZER = 6;
const uint8_t PIN_TRIG = 11;
const uint8_t PIN_ECHO = 12;

// -----------------------------------------------------
// 7セグ数字テーブル（0～F + 消灯）
// bit0=a, bit1=b, ... bit6=g, bit7=dp
// ここでは Elegoo 形式（共通カソード向け: 1で点灯）を基準にする
// -----------------------------------------------------
const uint8_t SEG_TABLE[17] = {
  0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07,
  0x7f, 0x6f, 0x77, 0x7c, 0x39, 0x5e, 0x79, 0x71, 0x00
};

// セグメント信号極性:
// - 共通カソード: true（1=点灯）
// - 共通アノード: false（0=点灯）
const bool SEG_ACTIVE_HIGH = true;

// -----------------------------------------------------
// 距離しきい値(mm)
// -----------------------------------------------------
const int NEAR_THRESHOLD_MM = 100;
const int MID_THRESHOLD_MM = 300;

// -----------------------------------------------------
// ブザー周波数
// -----------------------------------------------------
const int FREQ_NEAR = 2200;
const int FREQ_MID = 1500;
const int FREQ_FAR = 900;

// -----------------------------------------------------
// タイミング設定
// -----------------------------------------------------
const unsigned long MEASURE_INTERVAL_MS = 100;
const unsigned long DISPLAY_SCAN_INTERVAL_MS = 2;
const unsigned long DISPLAY_VALUE_UPDATE_MS = 300;
const unsigned long MID_BEEP_INTERVAL_MS = 100;
const unsigned long FAR_BEEP_ON_MS = 100;
const unsigned long FAR_BEEP_OFF_MS = 900;
const unsigned long ECHO_TIMEOUT_US = 12000UL;

// 1桁あたりの点灯時間(us)。短くすると暗くなる
const unsigned int DIGIT_ON_TIME_US = 500;

// 桁選択極性:
// - 共通カソード4桁モジュール想定: LOWで桁有効
// - 共通アノードの場合は false へ変更して試験
const bool DIGIT_ACTIVE_LOW = true;

// -----------------------------------------------------
// 状態変数
// -----------------------------------------------------
int distanceMm = 0;
bool buzzerOn = false;
uint8_t displayDigits[4] = {0, 0, 0, 0};
uint8_t currentScanDigit = 0;

unsigned long lastMeasureMillis = 0;
unsigned long lastDisplayScanMillis = 0;
unsigned long lastDisplayValueUpdateMillis = 0;
unsigned long lastBeepToggleMillis = 0;

int readDistanceMm();
uint8_t classifyZone(int measuredMm);
void outputBuzzerByZone(uint8_t zone);
void convertNumberToDigits(int valueMm);
void updateDisplayScan();
void writeShiftRegister(uint8_t seg);
void setAllDigitsOff();
uint8_t mapDigitToSegment(uint8_t digit);

void setup() {
  // --- 74HC595制御ピン初期化 ---
  pinMode(PIN_595_LATCH, OUTPUT);
  pinMode(PIN_595_CLOCK, OUTPUT);
  pinMode(PIN_595_DATA, OUTPUT);

  // --- 4桁選択ピン初期化 ---
  for (uint8_t i = 0; i < 4; i++) {
    pinMode(DIGIT_PINS[i], OUTPUT);
  }
  setAllDigitsOff();

  // --- センサー初期化 ---
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);

  // --- ブザー初期化 ---
  pinMode(PIN_BUZZER, OUTPUT);
  noTone(PIN_BUZZER);

  // 起動確認:
  // 1) 全桁を有効化
  // 2) 7セグ全点灯パターンを出力
  // 配線抜けの一次確認に使う
  writeShiftRegister(mapDigitToSegment(8));
  for (uint8_t i = 0; i < 4; i++) {
    digitalWrite(DIGIT_PINS[i], DIGIT_ACTIVE_LOW ? LOW : HIGH);
  }
  delay(500);

  // 起動確認終了後、全消灯して通常ループへ
  setAllDigitsOff();
  writeShiftRegister(mapDigitToSegment(16));

  // 各タイマの基準時刻を揃える
  lastMeasureMillis = millis();
  lastDisplayScanMillis = millis();
  lastDisplayValueUpdateMillis = millis();
  lastBeepToggleMillis = millis();

  // 起動直後の表示値を初期化
  convertNumberToDigits(distanceMm);
}

void loop() {
  unsigned long now = millis();

  // 一定周期で距離を測定
  if (now - lastMeasureMillis >= MEASURE_INTERVAL_MS) {
    lastMeasureMillis = now;

    // センサー値が有効なら更新（タイムアウト時は前回値を保持）
    int measured = readDistanceMm();
    if (measured > 0) {
      distanceMm = measured;
    }

  }

  // 表示値の更新間隔をあけて、数値の変化を見やすくする
  if (now - lastDisplayValueUpdateMillis >= DISPLAY_VALUE_UPDATE_MS) {
    lastDisplayValueUpdateMillis = now;
    convertNumberToDigits(distanceMm);
  }

  // 距離ゾーンに応じてブザー出力を更新
  uint8_t zone = classifyZone(distanceMm);
  outputBuzzerByZone(zone);

  // ダイナミック点灯: 周期的に次の桁へ進める
  if (now - lastDisplayScanMillis >= DISPLAY_SCAN_INTERVAL_MS) {
    lastDisplayScanMillis = now;
    updateDisplayScan();
  }
}

int readDistanceMm() {
  // HC-SR04のTrigを10usパルス駆動して測距開始
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);

  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);

  // Echoパルス幅を取得（timeoutで0が返る）
  unsigned long durationUs = pulseIn(PIN_ECHO, HIGH, ECHO_TIMEOUT_US);
  if (durationUs == 0) {
    return 0;
  }

  // 音速からmm換算: distance(mm) ≒ duration(us) * 0.1715
  long measuredMm = (long)(durationUs * 1715L) / 10000L;

  // 実運用範囲外は無効値扱い
  if (measuredMm < 1 || measuredMm > 4000) {
    return 0;
  }

  return (int)measuredMm;
}

uint8_t classifyZone(int measuredMm) {
  // zone 0: 近距離, zone 1: 中距離, zone 2: 遠距離/無効
  if (measuredMm <= 0) {
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
    // 中距離: 100ms周期でON/OFF
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

  // 遠距離: 短音 + 長め無音
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

void convertNumberToDigits(int valueMm) {
  // 表示可能範囲へ丸める
  if (valueMm < 0) {
    valueMm = 0;
  } else if (valueMm > 9999) {
    valueMm = 9999;
  }

  // 千/百/十/一の位に分解
  displayDigits[0] = (uint8_t)(valueMm / 1000);
  displayDigits[1] = (uint8_t)((valueMm / 100) % 10);
  displayDigits[2] = (uint8_t)((valueMm / 10) % 10);
  displayDigits[3] = (uint8_t)(valueMm % 10);
}

void updateDisplayScan() {
  // ゴースト点灯を避けるため、先に全桁OFF
  setAllDigitsOff();

  // 現在桁に対応する数字パターンを74HC595へ送信
  uint8_t digitValue = displayDigits[currentScanDigit];
  writeShiftRegister(mapDigitToSegment(digitValue));

  // 対象桁だけ有効化
  digitalWrite(
    DIGIT_PINS[currentScanDigit],
    DIGIT_ACTIVE_LOW ? LOW : HIGH
  );

  // 疑似PWM: 点灯時間を短くして見た目の明るさを下げる
  delayMicroseconds(DIGIT_ON_TIME_US);
  setAllDigitsOff();

  // 次回は次の桁へ
  currentScanDigit++;
  if (currentScanDigit >= 4) {
    currentScanDigit = 0;
  }
}

void writeShiftRegister(uint8_t seg) {
  // ラッチLOW中に1byteシフトし、ラッチHIGHで出力確定
  digitalWrite(PIN_595_LATCH, LOW);
  shiftOut(PIN_595_DATA, PIN_595_CLOCK, MSBFIRST, seg);
  digitalWrite(PIN_595_LATCH, HIGH);
}

void setAllDigitsOff() {
  // 桁有効極性に合わせて全桁OFFへ
  for (uint8_t i = 0; i < 4; i++) {
    digitalWrite(DIGIT_PINS[i], DIGIT_ACTIVE_LOW ? HIGH : LOW);
  }
}

uint8_t mapDigitToSegment(uint8_t digit) {
  // digit: 0～15は数字/16進、16は消灯
  uint8_t index = (digit <= 16) ? digit : 16;
  uint8_t pattern = SEG_TABLE[index];

  // 共通アノードの場合はON/OFF極性が逆なので反転
  if (!SEG_ACTIVE_HIGH) {
    pattern = (uint8_t)~pattern;
  }

  return pattern;
}
