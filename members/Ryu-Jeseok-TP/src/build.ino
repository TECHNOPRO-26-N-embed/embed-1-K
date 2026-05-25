#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <IRremote.hpp>

// -------------------- ピン割り当て --------------------
const byte PIN_IR = 2;       // Arduino D2  -> IR受信モジュール OUT
const byte PIN_BUZZER = 3;   // Arduino D3  -> パッシブブザー (+)
const byte PIN_LED_R = 5;    // Arduino D5  -> RGB LED R（220ohm直列）
const byte PIN_LED_G = 6;    // Arduino D6  -> RGB LED G（220ohm直列）
const byte PIN_LED_B = 9;    // Arduino D9  -> RGB LED B（220ohm直列）
// Arduino A4 -> LCD I2C SDA
// Arduino A5 -> LCD I2C SCL

// -------------------- デバイス設定 ----------------------
LiquidCrystal_I2C lcd(0x27, 16, 2);

// アプリ全体の状態遷移
enum AppState : byte {
  STATE_TIME_SETUP = 0,
  STATE_IDLE = 1,
  STATE_QUIZ = 2,
  STATE_ERROR = 3
};

AppState currentState = STATE_TIME_SETUP;

// -------------------- 時刻管理とタイマー ----------------
byte nowHH = 0;
byte nowMM = 0;
byte alarmHH = 7;
byte alarmMM = 0;

unsigned long lastClockMillis = 0;
unsigned long lastBlinkMillis = 0;
unsigned long lastBeepMillis = 0;
unsigned long lastLcdMillis = 0;
unsigned long lastIrReceiveMs = 0;
unsigned long lastPatternMs = 0;
unsigned long lastLedPatternMs = 0;

const unsigned long DEBOUNCE_DELAY_MS = 50;
const unsigned long CLOCK_STEP_MS = 60000;
const unsigned long LCD_UPDATE_MS = 150;
const unsigned long IDLE_BLINK_MS = 500;
const unsigned long BEEP_TOGGLE_MS = 200;
const unsigned long TONE_PATTERN_SWITCH_MS = 3000;
const unsigned long LED_PATTERN_SWITCH_MS = 1000;

// -------------------- 入力/クイズ管理 -------------------
unsigned long irCode = 0;
byte errorType = 0;  // 0:なし 1:範囲外 2:受信失敗 3:不正キー

char inputDigits[5] = {'0', '0', '0', '0', '\0'};
byte inputIndex = 0;
bool isAlarmInputPhase = false;

struct QuizItem {
  const char *question;
  int answer;
};

QuizItem quizTable[] = {
  {"2+3=?", 5},
  {"7-4=?", 3},
  {"6/2=?", 3}
};

const byte QUIZ_COUNT = sizeof(quizTable) / sizeof(quizTable[0]);
byte quizIndex = 0;

char quizInput[6] = {'\0'};
byte quizInputIndex = 0;

bool isAlarmRinging = false;
byte toneMode = 0;
byte ledMode = 0;
int lastTriggeredMinuteOfDay = -1;

// -------------------- IRキー割り当て --------------------
// 21キー系リモコンの代表値。実機と違う場合はここだけ調整する。
const byte IR_CMD_0 = 0x19;
const byte IR_CMD_1 = 0x45;
const byte IR_CMD_2 = 0x46;
const byte IR_CMD_3 = 0x47;
const byte IR_CMD_4 = 0x44;
const byte IR_CMD_5 = 0x40;
const byte IR_CMD_6 = 0x43;
const byte IR_CMD_7 = 0x07;
const byte IR_CMD_8 = 0x15;
const byte IR_CMD_9 = 0x09;
const byte IR_CMD_OK = 0x1C;     // OK
const byte IR_CMD_CLEAR = 0x16;  // *

// -------------------- 関数宣言 --------------------------
unsigned long readRemoteCode();
void updateClockByMillis();
void updateLcdView(byte state);
void updateAlarmOutputs(byte state);
bool handleTimeInput(bool isAlarmInput);
bool checkAlarmTrigger();
void startAlarmSequence();
void runQuizStep();
bool judgeQuizAnswer(unsigned long keyCode);
void playErrorBeep(byte errorTypeParam);
void setAlarmToneMode(byte mode);
void setLedColorMode(byte colorMode);
void mixAlarmTonePatterns(unsigned long nowMs);
void mixLedFlashPatterns(unsigned long nowMs);

int decodeDigitFromCommand(byte cmd);
bool isOkCommand(byte cmd);
bool isClearCommand(byte cmd);
void resetTimeInput();
void resetQuizInput();
void setRgb(byte r, byte g, byte b);
void printLcdLine(byte row, const char *text);
void formatTime(char *out, byte hh, byte mm);

void setup() {
  // 各ピンの入出力モードを設定
  pinMode(PIN_IR, INPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_LED_R, OUTPUT);
  pinMode(PIN_LED_G, OUTPUT);
  pinMode(PIN_LED_B, OUTPUT);

  Serial.begin(9600);

  // IR受信開始（内蔵LEDフィードバック無効）
  IrReceiver.begin(PIN_IR, DISABLE_LED_FEEDBACK);

  lcd.init();
  lcd.backlight();

  // 初期状態へリセット
  currentState = STATE_TIME_SETUP;
  resetTimeInput();
  resetQuizInput();
  errorType = 0;

  unsigned long nowMs = millis();
  lastClockMillis = nowMs;
  lastBlinkMillis = nowMs;
  lastBeepMillis = nowMs;
  lastLcdMillis = 0;
  lastIrReceiveMs = 0;
  lastPatternMs = nowMs;
  lastLedPatternMs = nowMs;

  // 起動確認として青色を短時間点灯
  setRgb(0, 0, 255);
  delay(300);
  setRgb(0, 0, 0);
}

void loop() {
  // 毎ループ共通処理（入力取得・時刻更新・表示・出力）
  irCode = readRemoteCode();
  updateClockByMillis();
  updateLcdView(currentState);
  updateAlarmOutputs(currentState);

  switch (currentState) {
    case STATE_TIME_SETUP:
      // 現在時刻入力フェーズ -> アラーム時刻入力フェーズ
      if (!isAlarmInputPhase) {
        if (handleTimeInput(false)) {
          isAlarmInputPhase = true;
          resetTimeInput();
        }
      } else {
        if (handleTimeInput(true)) {
          isAlarmInputPhase = false;
          resetTimeInput();
          currentState = STATE_IDLE;
        }
      }
      if (errorType != 0) {
        currentState = STATE_ERROR;
      }
      break;

    case STATE_IDLE:
      // アラーム時刻一致で出題状態へ
      if (checkAlarmTrigger()) {
        startAlarmSequence();
        currentState = STATE_QUIZ;
      }
      break;

    case STATE_QUIZ:
      // 出題・回答受付
      runQuizStep();
      if (errorType != 0) {
        currentState = STATE_ERROR;
      }
      break;

    case STATE_ERROR:
      // エラー通知後に設定状態へ戻す
      playErrorBeep(errorType);
      resetTimeInput();
      resetQuizInput();
      isAlarmInputPhase = false;
      currentState = STATE_TIME_SETUP;
      break;
  }
}

unsigned long readRemoteCode() {
  // 受信データがない場合は0
  if (!IrReceiver.decode()) {
    return 0;
  }

  unsigned long nowMs = millis();
  byte cmd = IrReceiver.decodedIRData.command;

  IrReceiver.resume();

  // 連打/長押しによる重複入力を無視
  if ((nowMs - lastIrReceiveMs) < DEBOUNCE_DELAY_MS) {
    return 0;
  }

  lastIrReceiveMs = nowMs;
  return (unsigned long)cmd;
}

void updateClockByMillis() {
  unsigned long nowMs = millis();
  // 遅延があっても取りこぼさないよう while で分単位を消化
  while ((nowMs - lastClockMillis) >= CLOCK_STEP_MS) {
    nowMM++;
    if (nowMM >= 60) {
      nowMM = 0;
      nowHH++;
      if (nowHH >= 24) {
        nowHH = 0;
      }
    }
    lastClockMillis += CLOCK_STEP_MS;
  }
}

void updateLcdView(byte state) {
  unsigned long nowMs = millis();
  // LCD更新しすぎによるちらつきを防止
  if ((nowMs - lastLcdMillis) < LCD_UPDATE_MS) {
    return;
  }
  lastLcdMillis = nowMs;

  char line1[17];
  char line2[17];
  memset(line1, ' ', 16);
  memset(line2, ' ', 16);
  line1[16] = '\0';
  line2[16] = '\0';

  if (state == STATE_TIME_SETUP) {
    snprintf(line1, 17, "%s TIME INPUT", isAlarmInputPhase ? "ALARM" : "NOW");
    snprintf(line2, 17, "HHMM:%s", inputDigits);
  } else if (state == STATE_IDLE) {
    char nowStr[6];
    char alarmStr[6];
    formatTime(nowStr, nowHH, nowMM);
    formatTime(alarmStr, alarmHH, alarmMM);
    snprintf(line1, 17, "NOW   %s", nowStr);
    snprintf(line2, 17, "ALARM %s", alarmStr);
  } else if (state == STATE_QUIZ) {
    snprintf(line1, 17, "Q%d:%s", quizIndex + 1, quizTable[quizIndex].question);
    snprintf(line2, 17, "ANS:%s", quizInput);
  } else {
    snprintf(line1, 17, "INPUT ERROR");
    if (errorType == 1) {
      snprintf(line2, 17, "OUT OF RANGE");
    } else if (errorType == 2) {
      snprintf(line2, 17, "IR RECEIVE FAIL");
    } else if (errorType == 3) {
      snprintf(line2, 17, "INVALID KEY");
    } else {
      snprintf(line2, 17, "RETRY");
    }
  }

  printLcdLine(0, line1);
  printLcdLine(1, line2);
}

void updateAlarmOutputs(byte state) {
  unsigned long nowMs = millis();

  if (state == STATE_TIME_SETUP) {
    setRgb(0, 0, 255);  // 設定中は青固定
    noTone(PIN_BUZZER);
    return;
  }

  if (state == STATE_IDLE) {
    noTone(PIN_BUZZER);
    // 待機中は緑をゆっくり点滅
    if ((nowMs - lastBlinkMillis) >= IDLE_BLINK_MS) {
      lastBlinkMillis = nowMs;
      static bool on = false;
      on = !on;
      if (on) {
        setRgb(0, 255, 0);
      } else {
        setRgb(0, 0, 0);
      }
    }
    return;
  }

  if (state == STATE_QUIZ) {
    // 出題中は音・光を並行して更新
    mixAlarmTonePatterns(nowMs);
    mixLedFlashPatterns(nowMs);
    return;
  }

  if (state == STATE_ERROR) {
    // エラー中は黄点滅
    if ((nowMs - lastBlinkMillis) >= 120) {
      lastBlinkMillis = nowMs;
      static bool on = false;
      on = !on;
      if (on) {
        setRgb(255, 180, 0);  // yellow-ish
      } else {
        setRgb(0, 0, 0);
      }
    }
    noTone(PIN_BUZZER);
    return;
  }

  setRgb(0, 0, 0);
  noTone(PIN_BUZZER);
}

bool handleTimeInput(bool isAlarmInput) {
  // 入力がないループは処理しない
  if (irCode == 0) {
    return false;
  }

  byte cmd = (byte)irCode;
  int digit = decodeDigitFromCommand(cmd);

  if (digit >= 0) {
    // 数字キーは4桁まで受け付け
    if (inputIndex < 4) {
      inputDigits[inputIndex++] = (char)('0' + digit);
      inputDigits[inputIndex] = '\0';
    }
    return false;
  }

  if (isClearCommand(cmd)) {
    resetTimeInput();
    return false;
  }

  if (!isOkCommand(cmd)) {
    // 数字/クリア/OK以外は不正キー
    errorType = 3;
    return false;
  }

  if (inputIndex != 4) {
    errorType = 3;
    return false;
  }

  byte hh = (byte)((inputDigits[0] - '0') * 10 + (inputDigits[1] - '0'));
  byte mm = (byte)((inputDigits[2] - '0') * 10 + (inputDigits[3] - '0'));

  if (hh > 23 || mm > 59) {
    // 24時間制の範囲外
    errorType = 1;
    return false;
  }

  if (isAlarmInput) {
    alarmHH = hh;
    alarmMM = mm;
  } else {
    nowHH = hh;
    nowMM = mm;
    lastClockMillis = millis();
  }

  return true;
}

bool checkAlarmTrigger() {
  // 時刻一致しない場合は未発火
  if (nowHH != alarmHH || nowMM != alarmMM) {
    return false;
  }

  // 同一分での再トリガを防止
  int minuteOfDay = nowHH * 60 + nowMM;
  if (minuteOfDay == lastTriggeredMinuteOfDay) {
    return false;
  }

  lastTriggeredMinuteOfDay = minuteOfDay;
  return true;
}

void startAlarmSequence() {
  // 出題開始時の内部状態を初期化
  isAlarmRinging = true;
  quizIndex = 0;
  resetQuizInput();

  setAlarmToneMode(0);
  setLedColorMode(0);

  unsigned long nowMs = millis();
  lastBeepMillis = nowMs;
  lastBlinkMillis = nowMs;
  lastPatternMs = nowMs;
  lastLedPatternMs = nowMs;
}

void runQuizStep() {
  // 入力がないときは何もしない
  if (irCode == 0) {
    return;
  }

  byte cmd = (byte)irCode;
  int digit = decodeDigitFromCommand(cmd);

  if (digit >= 0) {
    if (quizInputIndex < 5) {
      quizInput[quizInputIndex++] = (char)('0' + digit);
      quizInput[quizInputIndex] = '\0';
    }
    return;
  }

  if (isClearCommand(cmd)) {
    resetQuizInput();
    return;
  }

  if (isOkCommand(cmd)) {
    (void)judgeQuizAnswer(irCode);
    return;
  }

  errorType = 3;
}

bool judgeQuizAnswer(unsigned long keyCode) {
  (void)keyCode;

  // 空回答で確定された場合は不正解扱い
  if (quizInputIndex == 0) {
    return false;
  }

  int userAnswer = atoi(quizInput);
  int correct = quizTable[quizIndex].answer;

  if (userAnswer == correct) {
    // 正解時は次の問題へ
    quizIndex++;
    resetQuizInput();

    if (quizIndex >= QUIZ_COUNT) {
      // 全問正解でアラーム停止
      noTone(PIN_BUZZER);
      tone(PIN_BUZZER, 1600, 120);

      isAlarmRinging = false;
      currentState = STATE_IDLE;
      return true;
    }
    return true;
  }

  return false;
}

void playErrorBeep(byte errorTypeParam) {
  // エラー種別ごとに警告音を変更
  int freq = 900;

  if (errorTypeParam == 1) {
    freq = 700;
  } else if (errorTypeParam == 2) {
    freq = 500;
  } else if (errorTypeParam == 3) {
    freq = 1000;
  }

  tone(PIN_BUZZER, freq, 100);
  errorType = 0;
}

void setAlarmToneMode(byte mode) {
  // 音パターンは2種類で循環
  toneMode = mode % 2;
}

void setLedColorMode(byte colorMode) {
  // LEDパターンは3種類で循環
  ledMode = colorMode % 3;
}

void mixAlarmTonePatterns(unsigned long nowMs) {
  if (!isAlarmRinging) {
    noTone(PIN_BUZZER);
    return;
  }

  // 一定時間ごとに音色モードを切り替える
  if ((nowMs - lastPatternMs) >= TONE_PATTERN_SWITCH_MS) {
    lastPatternMs = nowMs;
    toneMode = (toneMode + 1) % 2;
  }

  // 200ms周期でON/OFFを繰り返して鳴動を作る
  if ((nowMs - lastBeepMillis) >= BEEP_TOGGLE_MS) {
    lastBeepMillis = nowMs;
    static bool toneOn = false;
    static byte idx = 0;
    toneOn = !toneOn;

    if (!toneOn) {
      noTone(PIN_BUZZER);
      return;
    }

    if (toneMode == 0) {
      const int seq0[] = {880, 988, 1047, 1175};
      tone(PIN_BUZZER, seq0[idx % 4]);
      idx++;
    } else {
      const int seq1[] = {1319, 988, 740, 988};
      tone(PIN_BUZZER, seq1[idx % 4]);
      idx++;
    }
  }
}

void mixLedFlashPatterns(unsigned long nowMs) {
  // 一定時間ごとに発光パターンを切り替える
  if ((nowMs - lastLedPatternMs) >= LED_PATTERN_SWITCH_MS) {
    lastLedPatternMs = nowMs;
    ledMode = (ledMode + 1) % 3;
  }

  static byte step = 0;

  if (ledMode == 0) {
    if ((nowMs - lastBlinkMillis) >= 500) {
      lastBlinkMillis = nowMs;
      step ^= 1;
      if (step) {
        setRgb(255, 255, 255);
      } else {
        setRgb(0, 0, 0);
      }
    }
  } else if (ledMode == 1) {
    if ((nowMs - lastBlinkMillis) >= 300) {
      lastBlinkMillis = nowMs;
      step = (step + 1) % 3;
      if (step == 0) setRgb(255, 0, 0);
      if (step == 1) setRgb(0, 0, 255);
      if (step == 2) setRgb(0, 255, 0);
    }
  } else {
    if ((nowMs - lastBlinkMillis) >= 150) {
      lastBlinkMillis = nowMs;
      step ^= 1;
      if (step) {
        setRgb(255, 0, 0);
      } else {
        setRgb(0, 0, 255);
      }
    }
  }
}

int decodeDigitFromCommand(byte cmd) {
  // IRコマンドを数字へ変換（非数字は-1）
  if (cmd == IR_CMD_0) return 0;
  if (cmd == IR_CMD_1) return 1;
  if (cmd == IR_CMD_2) return 2;
  if (cmd == IR_CMD_3) return 3;
  if (cmd == IR_CMD_4) return 4;
  if (cmd == IR_CMD_5) return 5;
  if (cmd == IR_CMD_6) return 6;
  if (cmd == IR_CMD_7) return 7;
  if (cmd == IR_CMD_8) return 8;
  if (cmd == IR_CMD_9) return 9;
  return -1;
}

bool isOkCommand(byte cmd) {
  return cmd == IR_CMD_OK;
}

bool isClearCommand(byte cmd) {
  return cmd == IR_CMD_CLEAR;
}

void resetTimeInput() {
  // HHMM入力バッファを初期化
  inputDigits[0] = '\0';
  inputDigits[1] = '\0';
  inputDigits[2] = '\0';
  inputDigits[3] = '\0';
  inputDigits[4] = '\0';
  inputIndex = 0;
}

void resetQuizInput() {
  // 回答入力バッファを初期化
  quizInput[0] = '\0';
  quizInputIndex = 0;
}

void setRgb(byte r, byte g, byte b) {
  // PWMでRGB各色の明るさを設定
  analogWrite(PIN_LED_R, r);
  analogWrite(PIN_LED_G, g);
  analogWrite(PIN_LED_B, b);
}

void printLcdLine(byte row, const char *text) {
  // 行全体を16文字で上書きして表示残りを防ぐ
  lcd.setCursor(0, row);
  char out[17];
  memset(out, ' ', 16);
  out[16] = '\0';

  size_t n = strlen(text);
  if (n > 16) n = 16;
  memcpy(out, text, n);
  lcd.print(out);
}

void formatTime(char *out, byte hh, byte mm) {
  // HH:MM 形式へ整形
  sprintf(out, "%02u:%02u", hh, mm);
}
