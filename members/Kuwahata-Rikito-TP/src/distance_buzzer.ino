#include <Arduino.h>
#include <TM1637Display.h>

// ==============================
// ピン定義
// ==============================
const uint8_t PIN_TRIG = 9;       // HC-SR04 Trig
const uint8_t PIN_ECHO = 10;      // HC-SR04 Echo
const uint8_t PIN_BUZZER = 6;     // パッシブブザー
const uint8_t PIN_TM1637_CLK = 4; // TM1637 CLK
const uint8_t PIN_TM1637_DIO = 5; // TM1637 DIO

// ==============================
// 状態管理
// ==============================
const uint8_t STATE_WAIT = 0;   // 待機中
const uint8_t STATE_JUDGE = 1;  // 距離判定中
const uint8_t STATE_NOTIFY = 2; // 通知中
const uint8_t STATE_ERROR = 3;  // 異常監視

uint8_t currentState = STATE_WAIT;
uint8_t distanceZone = 2; // 0:近 1:中 2:遠

// ==============================
// タイマー管理(millis用)
// ==============================
unsigned long lastMeasureMillis = 0;
unsigned long lastBeepToggleMillis = 0;

// ==============================
// センサー値・出力状態
// ==============================
int distanceMm = 0;          // 現在の距離(mm)
int prevValidDistanceMm = 0; // 直前の有効距離(mm)
bool buzzerOn = false;       // ブザーが鳴っているかどうか

// ==============================
// しきい値・周期設定
// ==============================
const int nearThresholdMm = 100; // 近距離しきい値
const int midThresholdMm = 300;  // 中距離しきい値
const int HYSTERESIS_MM = 20;    // ヒステリシス幅

const unsigned long MEASURE_INTERVAL_MS = 100;    // 距離計測周期
const unsigned long MID_BEEP_INTERVAL_MS = 100;   // 中距離時ON/OFF周期
const unsigned long FAR_BEEP_ON_MS = 100;         // 遠距離時の短音ON時間
const unsigned long FAR_BEEP_OFF_MS = 900;        // 遠距離時の無音時間
const unsigned long ECHO_TIMEOUT_US = 25000UL;    // Echo待ちタイムアウト(約4m相当)

const int BUZZER_FREQ_NEAR = 2200; // 近距離の高音周波数
const int BUZZER_FREQ_MID = 1500;  // 中距離の中音周波数
const int BUZZER_FREQ_FAR = 900;   // 遠距離の低音周波数

TM1637Display display(PIN_TM1637_CLK, PIN_TM1637_DIO);

int readDistanceMm();
uint8_t classifyDistanceZone(int measuredDistanceMm);
void updateBuzzerOutput(uint8_t zone);
void updateDisplayOutput(int measuredDistanceMm);

void setup() {
  // センサーピンの入出力設定
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);

  // ブザー出力ピンを初期化
  pinMode(PIN_BUZZER, OUTPUT);
  noTone(PIN_BUZZER);

  // TM1637表示器の初期化(明るさ設定と初期表示)
  display.setBrightness(0x0f);
  display.showNumberDec(0, true);

  // 状態変数を初期化
  currentState = STATE_WAIT;
  distanceZone = 2; // 遠距離スタート
  distanceMm = 0;
  prevValidDistanceMm = 0;
  buzzerOn = false;

  // 起動直後の時刻基準を揃える
  lastMeasureMillis = millis();
  lastBeepToggleMillis = millis();
}

void loop() {
  // 毎ループ現在時刻を取得し、各周期判定に使う
  unsigned long now = millis();

  // 状態ごとの処理を行う
  if (currentState == STATE_WAIT) {
    // 計測周期に達したときだけセンサーを読む
    if (now - lastMeasureMillis >= MEASURE_INTERVAL_MS) {
      lastMeasureMillis = now;
      int measured = readDistanceMm();

      // 有効距離が取得できたら判定状態へ進む
      if (measured > 0) {
        distanceMm = measured;
        currentState = STATE_JUDGE;
      } else {
        // 異常値はエラー監視へ遷移
        currentState = STATE_ERROR;
      }
    }
  } else if (currentState == STATE_JUDGE) {
    // 取得済み距離を近/中/遠に分類
    uint8_t zone = classifyDistanceZone(distanceMm);

    // 正常なら通知状態へ進む
    distanceZone = zone;
    currentState = STATE_NOTIFY;
  } else if (currentState == STATE_NOTIFY) {
    // 距離区分に応じたブザー出力を更新
    updateBuzzerOutput(distanceZone);

    // 数値表示を最新距離に更新
    updateDisplayOutput(distanceMm);

    // 次周期は再判定するため判定状態へ戻る
    currentState = STATE_JUDGE;
  } else if (currentState == STATE_ERROR) {
    // 異常中は誤通知を避けるためブザー停止
    noTone(PIN_BUZZER);
    buzzerOn = false;

    // 表示は前回有効値を維持して挙動を安定化
    updateDisplayOutput(prevValidDistanceMm);

    // 次周期で再計測し、復帰可否を判定する
    if (now - lastMeasureMillis >= MEASURE_INTERVAL_MS) {
      lastMeasureMillis = now;
      int measured = readDistanceMm();
      if (measured > 0) {
        distanceMm = measured;
        currentState = STATE_JUDGE;
      }
    }
  } else {
    // 想定外状態が来たら安全側として待機に戻す
    noTone(PIN_BUZZER);
    buzzerOn = false;
    currentState = STATE_WAIT;
  }
}

int readDistanceMm() {
  // TrigをLOWにしてパルス生成前の状態を安定化
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);

  // Trigに10usのHIGHパルスを出して測距開始
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);

  // EchoのHIGH幅を取得(タイムアウト付き)
  unsigned long durationUs = pulseIn(PIN_ECHO, HIGH, ECHO_TIMEOUT_US);

  // タイムアウト時は異常値として0を返す
  if (durationUs == 0) {
    return 0;
  }

  // 音速換算でmmに変換: distance(mm) = duration(us) * 0.1715
  long measuredMm = (long)(durationUs * 1715L) / 10000L;

  // センサー有効範囲外は異常値として0を返す
  if (measuredMm < 1 || measuredMm > 4000) {
    return 0;
  }

  // 有効値なら現在値と前回有効値を更新
  distanceMm = (int)measuredMm;
  prevValidDistanceMm = distanceMm;
  return distanceMm;
}

uint8_t classifyDistanceZone(int measuredDistanceMm) {
  // 異常値入力時は前回ゾーンを維持して急変を防ぐ
  if (measuredDistanceMm < 1 || measuredDistanceMm > 4000) {
    return distanceZone;
  }

  // 現在ゾーンに応じてヒステリシス付き判定を行う
  if (distanceZone == 0) {
    // 近 -> 中へ遷移するには上側境界を超える必要がある
    if (measuredDistanceMm > nearThresholdMm + HYSTERESIS_MM) {
      return 1;
    }
    return 0;
  }

  if (distanceZone == 1) {
    // 中 -> 近へ遷移するには下側境界より小さい必要がある
    if (measuredDistanceMm <= nearThresholdMm - HYSTERESIS_MM) {
      return 0;
    }

    // 中 -> 遠へ遷移するには上側境界を超える必要がある
    if (measuredDistanceMm > midThresholdMm + HYSTERESIS_MM) {
      return 2;
    }

    return 1;
  }

  // distanceZone == 2(遠)想定
  // 遠 -> 中へ遷移するには下側境界まで戻る必要がある
  if (measuredDistanceMm <= midThresholdMm - HYSTERESIS_MM) {
    return 1;
  }
  return 2;
}

void updateBuzzerOutput(uint8_t zone) {
  unsigned long now = millis();

  if (zone == 0) {
    // 近距離: 高音を連続で鳴らし続ける
    tone(PIN_BUZZER, BUZZER_FREQ_NEAR);
    buzzerOn = true;
    return;
  }

  if (zone == 1) {
    // 中距離: 100msごとにON/OFFを切り替える断続音
    if (now - lastBeepToggleMillis >= MID_BEEP_INTERVAL_MS) {
      lastBeepToggleMillis = now;
      buzzerOn = !buzzerOn;

      if (buzzerOn) {
        tone(PIN_BUZZER, BUZZER_FREQ_MID);
      } else {
        noTone(PIN_BUZZER);
      }
    }
    return;
  }

  if (zone == 2) {
    // 遠距離: 短く鳴らして長めに止めることで低頻度通知にする
    unsigned long interval = buzzerOn ? FAR_BEEP_ON_MS : FAR_BEEP_OFF_MS;
    if (now - lastBeepToggleMillis >= interval) {
      lastBeepToggleMillis = now;
      buzzerOn = !buzzerOn;

      if (buzzerOn) {
        tone(PIN_BUZZER, BUZZER_FREQ_FAR);
      } else {
        noTone(PIN_BUZZER);
      }
    }
    return;
  }

  // 不正zoneは安全側として停止
  noTone(PIN_BUZZER);
  buzzerOn = false;
}

void updateDisplayOutput(int measuredDistanceMm) {
  // 表示範囲外を防ぐため0～9999に丸める
  if (measuredDistanceMm < 0) {
    measuredDistanceMm = 0;
  } else if (measuredDistanceMm > 9999) {
    measuredDistanceMm = 9999;
  }

  // 4桁表示(先頭ゼロ埋めあり)で距離を表示
  display.showNumberDec(measuredDistanceMm, true);
}
