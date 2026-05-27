#define IR_USE_AVR_TIMER1
#include <Arduino.h>
#include <LiquidCrystal.h>
#include <IRremote.hpp>

// -------------------- ピン割り当て --------------------
const byte PIN_IR = 11;      // Arduino D11 -> IR受信モジュール OUT
const byte PIN_BUZZER = 3;   // Arduino D3  -> パッシブブザー (+)
const byte PIN_LED_R = 5;    // Arduino D5  -> RGB LED R（220ohm直列）
const byte PIN_LED_G = 6;    // Arduino D6  -> RGB LED G（220ohm直列）
const byte PIN_LED_B = 9;    // Arduino D9  -> RGB LED B（220ohm直列）

// LCD(16x2) 並列接続
const byte LCD_PIN_RS = 7;   // LCD 4番  -> Arduino D7
const byte LCD_PIN_EN = 8;   // LCD 6番  -> Arduino D8
const byte LCD_PIN_D4 = 2;   // LCD 11番 -> Arduino D2
const byte LCD_PIN_D5 = 4;   // LCD 12番 -> Arduino D4
const byte LCD_PIN_D6 = 12;  // LCD 13番 -> Arduino D12
const byte LCD_PIN_D7 = 13;  // LCD 14番 -> Arduino D13

// -------------------- デバイス設定 ----------------------
LiquidCrystal lcd(LCD_PIN_RS, LCD_PIN_EN, LCD_PIN_D4, LCD_PIN_D5, LCD_PIN_D6, LCD_PIN_D7);

// true: 通常運転（リモコン入力あり）
// false: 動作確認モード（リモコン入力なし、起動直後から音/光/LCDを同時出力）
const bool ENABLE_REMOTE_INPUT = true;
const bool REMOTE_DEBUG_PRINT = true;

<<<<<<< HEAD
=======
<<<<<<< HEAD
// ★수정됨: 소등 시 켜진다면 사용 중인 LED가 공통 캐소드 방식이므로 false로 변경
=======
>>>>>>> 0fa8001fd10e1a738073c67920f0e0e3435714f3
>>>>>>> 0da5c498f512e1082bed53b5d83363c49acbf2af
const bool RGB_COMMON_ANODE = false;

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
unsigned long lastRawIrCode = 0;

const unsigned long DEBOUNCE_DELAY_MS = 50;
const unsigned long CLOCK_STEP_MS = 60000;
const unsigned long LCD_UPDATE_MS = 150;
const unsigned long IDLE_BLINK_MS = 500;
const unsigned long TONE_PATTERN_SWITCH_MS = 10000;
const unsigned long LED_PATTERN_SWITCH_MS = 1000;
const unsigned long QUIZ_INPUT_TIMEOUT_MS = 5000;

// -------------------- 入力/クイズ管理 -------------------
unsigned long irCode = 0;
byte errorType = 0; 
// 0:なし 1:範囲外 2:受信失敗 3:不正キー

char inputDigits[5] = {'0', '0', '0', '0', '\0'};
byte inputIndex = 0;
bool isAlarmInputPhase = false;

<<<<<<< HEAD
=======
<<<<<<< HEAD
// ★수정됨: 동적 퀴즈용 변수
=======
>>>>>>> 0fa8001fd10e1a738073c67920f0e0e3435714f3
>>>>>>> 0da5c498f512e1082bed53b5d83363c49acbf2af
char currentQuestion[16] = {'\0'};
int currentAnswer = 0;

char quizInput[6] = {'\0'};
byte quizInputIndex = 0;
unsigned long lastQuizInputMs = 0;

bool isAlarmRinging = false;
byte toneMode = 0;
byte ledMode = 0;
int lastTriggeredMinuteOfDay = -1;

// -------------------- IRキー割り当て --------------------
// 21キー系リモコンの代表値（NECコマンド）
const byte IR_CMD_0 = 0x16;
const byte IR_CMD_1 = 0x0C;
const byte IR_CMD_2 = 0x18;
const byte IR_CMD_3 = 0x5E;
const byte IR_CMD_4 = 0x08;
const byte IR_CMD_5 = 0x1C;
const byte IR_CMD_6 = 0x5A;
const byte IR_CMD_7 = 0x42;
const byte IR_CMD_8 = 0x52;
const byte IR_CMD_9 = 0x4A;

const byte IR_CMD_POWER = 0x45;
const byte IR_CMD_FUNC_STOP = 0x46;
const byte IR_CMD_VOL_PLUS = 0x47;
const byte IR_CMD_PREV = 0x44;
const byte IR_CMD_PLAY_PAUSE = 0x40;
const byte IR_CMD_NEXT = 0x43;
const byte IR_CMD_DOWN = 0x07;
const byte IR_CMD_VOL_MINUS = 0x15;
const byte IR_CMD_EQ = 0x09;
const byte IR_CMD_ST_REPT = 0x19;

const byte IR_CMD_OK = 0xFF;                // 未使用
const byte IR_CMD_CLEAR = IR_CMD_FUNC_STOP; // 入力クリア

// -------------------- 関数宣言 --------------------------
unsigned long readRemoteCode();
void updateClockByMillis();
void updateLcdView(byte state);
void updateAlarmOutputs(byte state);
bool handleTimeInput(bool isAlarmInput);
bool checkAlarmTrigger();
void startAlarmSequence();
void generateNewQuiz();
void runQuizStep();
void playErrorBeep(byte errorTypeParam);
void setAlarmToneMode(byte mode);
void setLedColorMode(byte colorMode);
void mixAlarmTonePatterns(unsigned long nowMs);
void mixLedFlashPatterns(unsigned long nowMs);
int decodeDigitFromCommand(byte cmd);
int decodeDigitFromRaw(unsigned long rawCode);
bool isOkCommand(byte cmd);
bool isClearCommand(byte cmd);
bool isIgnoredControlCommand(byte cmd);
void resetTimeInput();
void resetQuizInput();
void setRgb(byte r, byte g, byte b);
void printLcdLine(byte row, const char *text);
void formatTime(char *out, byte hh, byte mm);
void runOutputSelfTest();

void setup() {
  pinMode(PIN_IR, INPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_LED_R, OUTPUT);
  pinMode(PIN_LED_G, OUTPUT);
  pinMode(PIN_LED_B, OUTPUT);

  Serial.begin(9600);
<<<<<<< HEAD
  randomSeed(analogRead(A0));
=======
<<<<<<< HEAD
  randomSeed(analogRead(A0)); // 난수 시드 초기화
=======
  randomSeed(analogRead(A0));
>>>>>>> 0fa8001fd10e1a738073c67920f0e0e3435714f3
>>>>>>> 0da5c498f512e1082bed53b5d83363c49acbf2af

  if (ENABLE_REMOTE_INPUT) {
    IrReceiver.begin(PIN_IR, DISABLE_LED_FEEDBACK);
  }

  lcd.begin(16, 2);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("LCD START OK");
  lcd.setCursor(0, 1);
  lcd.print("PARALLEL MODE");
  delay(1200);

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
  lastRawIrCode = 0;
  lastPatternMs = nowMs;
  lastLedPatternMs = nowMs;

  setRgb(0, 0, 0);
}

void loop() {
  if (!ENABLE_REMOTE_INPUT) {
    runOutputSelfTest();
    return;
  }

  irCode = readRemoteCode();
  updateClockByMillis();
  updateLcdView(currentState);
  updateAlarmOutputs(currentState);

  switch (currentState) {
    case STATE_TIME_SETUP:
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
      if (checkAlarmTrigger()) {
        startAlarmSequence();
        currentState = STATE_QUIZ;
      }
      break;

    case STATE_QUIZ:
      runQuizStep();
      if (errorType != 0) {
        currentState = STATE_ERROR;
      }
      break;

    case STATE_ERROR:
      playErrorBeep(errorType);
      resetTimeInput();
      resetQuizInput();
      isAlarmInputPhase = false;
      currentState = STATE_TIME_SETUP;
      break;
  }
}

unsigned long readRemoteCode() {
  if (!IrReceiver.decode()) {
    return 0;
  }

  unsigned long nowMs = millis();
  byte cmd = IrReceiver.decodedIRData.command;
  lastRawIrCode = IrReceiver.decodedIRData.decodedRawData;

  if ((IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT) != 0) {
    IrReceiver.resume();
    return 0;
  }

  IrReceiver.resume();

  if ((nowMs - lastIrReceiveMs) < DEBOUNCE_DELAY_MS) {
    return 0;
  }

  if (REMOTE_DEBUG_PRINT) {
    Serial.print("IR CMD: 0x");
    if (cmd < 16) Serial.print('0');
    Serial.print(cmd, HEX);
    Serial.print(" RAW:0x");
    Serial.println(lastRawIrCode, HEX);
  }

  lastIrReceiveMs = nowMs;
  return (unsigned long)cmd;
}

void updateClockByMillis() {
  unsigned long nowMs = millis();
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
    snprintf(line1, 17, "%s HHMM INPUT", isAlarmInputPhase ? "ALARM" : "NOW");
    snprintf(line2, 17, "4DIGIT:%s", inputDigits);
  } else if (state == STATE_IDLE) {
    char nowStr[6];
    char alarmStr[6];
    formatTime(nowStr, nowHH, nowMM);
    formatTime(alarmStr, alarmHH, alarmMM);
    snprintf(line1, 17, "NOW   %s", nowStr);
    snprintf(line2, 17, "ALARM %s", alarmStr);
  } else if (state == STATE_QUIZ) {
    snprintf(line1, 17, "Q: %s", currentQuestion);
    snprintf(line2, 17, "ANS: %s", quizInput);
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

  if (state == STATE_TIME_SETUP || state == STATE_IDLE || state == STATE_ERROR) {
    setRgb(0, 0, 0);
    noTone(PIN_BUZZER);
    return;
  }

  if (state == STATE_QUIZ) {
    if (!isAlarmRinging) {
      setRgb(0, 0, 0);
      noTone(PIN_BUZZER);
      return;
    }
    mixAlarmTonePatterns(nowMs);
    mixLedFlashPatterns(nowMs);
    return;
  }
}

bool handleTimeInput(bool isAlarmInput) {
  if (irCode == 0) {
    return false;
  }

  byte cmd = (byte)irCode;
  int digit = decodeDigitFromCommand(cmd);

  if (digit < 0) {
    digit = decodeDigitFromRaw(lastRawIrCode);
  }

  if (digit >= 0) {
    if (inputIndex < 4) {
      inputDigits[inputIndex++] = (char)('0' + digit);
      inputDigits[inputIndex] = '\0';
    }
    if (inputIndex == 4) {
      byte hhAuto = (byte)((inputDigits[0] - '0') * 10 + (inputDigits[1] - '0'));
      byte mmAuto = (byte)((inputDigits[2] - '0') * 10 + (inputDigits[3] - '0'));

      if (hhAuto > 23 || mmAuto > 59) {
        errorType = 1;
        return false;
      }

      if (isAlarmInput) {
        alarmHH = hhAuto;
        alarmMM = mmAuto;
      } else {
        nowHH = hhAuto;
        nowMM = mmAuto;
        lastClockMillis = millis();
      }
      return true;
    }
    return false;
  }

  if (isClearCommand(cmd)) {
    resetTimeInput();
    return false;
  }

  if (isIgnoredControlCommand(cmd)) {
    return false;
  }

  if (!isOkCommand(cmd)) {
    return false;
  }

  if (inputIndex != 4) {
    errorType = 3;
    return false;
  }

  byte hh = (byte)((inputDigits[0] - '0') * 10 + (inputDigits[1] - '0'));
  byte mm = (byte)((inputDigits[2] - '0') * 10 + (inputDigits[3] - '0'));

  if (hh > 23 || mm > 59) {
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
  if (nowHH != alarmHH || nowMM != alarmMM) {
    return false;
  }

  int minuteOfDay = nowHH * 60 + nowMM;
  if (minuteOfDay == lastTriggeredMinuteOfDay) {
    return false;
  }

  lastTriggeredMinuteOfDay = minuteOfDay;
  return true;
}

<<<<<<< HEAD
=======
<<<<<<< HEAD
// ★수정됨: 동적 난수 생성
=======
>>>>>>> 0fa8001fd10e1a738073c67920f0e0e3435714f3
>>>>>>> 0da5c498f512e1082bed53b5d83363c49acbf2af
void generateNewQuiz() {
  int a = random(1, 10);
  int b = random(1, 10);
  int op = random(0, 3); // 0: +, 1: -, 2: *

  if (op == 0) {
    currentAnswer = a + b;
    snprintf(currentQuestion, 16, "%d+%d=?", a, b);
  } else if (op == 1) {
<<<<<<< HEAD
=======
<<<<<<< HEAD
    // 음수가 나오지 않도록 스왑 처리
=======
>>>>>>> 0fa8001fd10e1a738073c67920f0e0e3435714f3
>>>>>>> 0da5c498f512e1082bed53b5d83363c49acbf2af
    if (b > a) { 
      int temp = a; a = b; b = temp; 
    }
    currentAnswer = a - b;
    snprintf(currentQuestion, 16, "%d-%d=?", a, b);
  } else {
    currentAnswer = a * b;
    snprintf(currentQuestion, 16, "%dx%d=?", a, b);
  }
}

void startAlarmSequence() {
  isAlarmRinging = true;
  resetQuizInput();
  
<<<<<<< HEAD
  generateNewQuiz();
=======
<<<<<<< HEAD
  generateNewQuiz(); // 퀴즈 생성
=======
  generateNewQuiz();
>>>>>>> 0fa8001fd10e1a738073c67920f0e0e3435714f3
>>>>>>> 0da5c498f512e1082bed53b5d83363c49acbf2af

  setAlarmToneMode(0);
  setLedColorMode(0);
  
  unsigned long nowMs = millis();
  lastBeepMillis = nowMs;
  lastBlinkMillis = nowMs;
  lastPatternMs = nowMs;
  lastLedPatternMs = nowMs;
  lastQuizInputMs = nowMs;
}

<<<<<<< HEAD
=======
<<<<<<< HEAD
// ★수정됨: 정답 즉각 확인 로직으로 개편
=======
>>>>>>> 0fa8001fd10e1a738073c67920f0e0e3435714f3
>>>>>>> 0da5c498f512e1082bed53b5d83363c49acbf2af
void runQuizStep() {
  if (irCode == 0) {
    if (quizInputIndex > 0 && (millis() - lastQuizInputMs) >= QUIZ_INPUT_TIMEOUT_MS) {
      resetQuizInput();
    }
    return;
  }

  byte cmd = (byte)irCode;
  int digit = decodeDigitFromCommand(cmd);

  if (digit < 0) {
    digit = decodeDigitFromRaw(lastRawIrCode);
  }

  if (digit >= 0) {
    lastQuizInputMs = millis();
    if (quizInputIndex < 5) {
      quizInput[quizInputIndex++] = (char)('0' + digit);
      quizInput[quizInputIndex] = '\0';
    }
    
<<<<<<< HEAD
=======
<<<<<<< HEAD
    // 입력된 값이 정답과 일치하는지 실시간 확인
=======
>>>>>>> 0fa8001fd10e1a738073c67920f0e0e3435714f3
>>>>>>> 0da5c498f512e1082bed53b5d83363c49acbf2af
    if (atoi(quizInput) == currentAnswer) {
      noTone(PIN_BUZZER);
      tone(PIN_BUZZER, 1600, 120);
      isAlarmRinging = false;
      currentState = STATE_IDLE;
    } 
<<<<<<< HEAD
=======
<<<<<<< HEAD
    // 입력이 길어졌으나 정답이 아닌 경우 자동 초기화 (정답 최대 2자리 9x9=81)
=======
>>>>>>> 0fa8001fd10e1a738073c67920f0e0e3435714f3
>>>>>>> 0da5c498f512e1082bed53b5d83363c49acbf2af
    else if (quizInputIndex >= 3) {
      resetQuizInput();
    }
    return;
  }

  if (isClearCommand(cmd)) {
    resetQuizInput();
    return;
  }

  if (isIgnoredControlCommand(cmd)) {
    return;
  }

  if (isOkCommand(cmd)) {
    if (atoi(quizInput) == currentAnswer) {
      noTone(PIN_BUZZER);
      tone(PIN_BUZZER, 1600, 120);
      isAlarmRinging = false;
      currentState = STATE_IDLE;
    } else {
      resetQuizInput();
    }
    return;
  }
}

void playErrorBeep(byte errorTypeParam) {
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
  toneMode = mode % 2;
}

void setLedColorMode(byte colorMode) {
  ledMode = colorMode % 3;
}

void mixAlarmTonePatterns(unsigned long nowMs) {
  if (!isAlarmRinging) {
    noTone(PIN_BUZZER);
    return;
  }

  // パターンA: マリオ風8bitメロディ（著作権配慮で完全一致は避けたアレンジ）
  static const uint16_t melodyAHz[] = {
    659, 659, 659, 523, 659, 784, 392,
    523, 392, 330, 440, 494, 466, 440,
    392, 659, 784, 880, 698, 784, 659,
    523, 587, 494
  };
  static const uint16_t melodyADur[] = {
    180, 180, 220, 180, 220, 300, 300,
    260, 240, 260, 220, 220, 180, 220,
    180, 180, 220, 260, 220, 220, 220,
    220, 220, 320
  };

  // パターンB: サイレン（高低を往復）
  static const uint16_t melodyBHz[] = {
    880, 988, 1109, 1319, 1109, 988, 880, 740,
    988, 1175, 1397, 1175, 988, 784, 988, 1319
  };
  static const uint16_t melodyBDur[] = {
    160, 160, 160, 180, 160, 160, 160, 180,
    160, 160, 180, 160, 160, 180, 160, 220
  };

  const uint16_t *activeMelodyHz = (toneMode == 0) ? melodyAHz : melodyBHz;
  const uint16_t *activeMelodyDur = (toneMode == 0) ? melodyADur : melodyBDur;
  const byte activeLen = (toneMode == 0)
                           ? (byte)(sizeof(melodyAHz) / sizeof(melodyAHz[0]))
                           : (byte)(sizeof(melodyBHz) / sizeof(melodyBHz[0]));

  static byte noteIndex = 0;

  if ((nowMs - lastPatternMs) >= TONE_PATTERN_SWITCH_MS) {
    lastPatternMs = nowMs;
    toneMode = (toneMode + 1) % 2;
    noteIndex = 0;
    noTone(PIN_BUZZER);
    lastBeepMillis = nowMs;
    return;
  }

  if ((nowMs - lastBeepMillis) >= activeMelodyDur[noteIndex]) {
    lastBeepMillis = nowMs;

    // 休符を短くして体感音量を上げる
    uint16_t playMs = activeMelodyDur[noteIndex];
    if (playMs > 10) {
      playMs -= 10;
    }
    tone(PIN_BUZZER, activeMelodyHz[noteIndex], playMs);

    noteIndex++;
    if (noteIndex >= activeLen) {
      noteIndex = 0;
    }
  }
}

void mixLedFlashPatterns(unsigned long nowMs) {
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
  if (cmd == 0x16 || cmd == 0x19) return 0;
  if (cmd == 0x0C || cmd == 0x45) return 1;
  if (cmd == 0x18 || cmd == 0x46) return 2;
  if (cmd == 0x5E || cmd == 0x47) return 3;
  if (cmd == 0x08 || cmd == 0x44) return 4;
  if (cmd == 0x1C || cmd == 0x40) return 5;
  if (cmd == 0x5A || cmd == 0x43) return 6;
  if (cmd == 0x42 || cmd == 0x07) return 7;
  if (cmd == 0x52 || cmd == 0x15) return 8;
  if (cmd == 0x4A || cmd == 0x09) return 9;
  return -1;
}

int decodeDigitFromRaw(unsigned long rawCode) {
  if (rawCode == 0xFF6897UL) return 0;
  if (rawCode == 0xFF30CFUL) return 1;
  if (rawCode == 0xFF18E7UL) return 2;
  if (rawCode == 0xFF7A85UL) return 3;
  if (rawCode == 0xFF10EFUL) return 4;
  if (rawCode == 0xFF38C7UL) return 5;
  if (rawCode == 0xFF5AA5UL) return 6;
  if (rawCode == 0xFF42BDUL) return 7;
  if (rawCode == 0xFF4AB5UL) return 8;
  if (rawCode == 0xFF52ADUL) return 9;
  return -1;
}

bool isOkCommand(byte cmd) {
  (void)cmd;
  return false;
}

bool isClearCommand(byte cmd) {
  return cmd == IR_CMD_CLEAR;
}

bool isIgnoredControlCommand(byte cmd) {
  return cmd == IR_CMD_POWER ||
         cmd == IR_CMD_VOL_PLUS ||
         cmd == IR_CMD_VOL_MINUS ||
         cmd == IR_CMD_PLAY_PAUSE ||
         cmd == IR_CMD_PREV ||
         cmd == IR_CMD_NEXT ||
         cmd == IR_CMD_DOWN ||
         cmd == IR_CMD_EQ ||
         cmd == IR_CMD_ST_REPT;
}

void resetTimeInput() {
  inputDigits[0] = '\0';
  inputDigits[1] = '\0';
  inputDigits[2] = '\0';
  inputDigits[3] = '\0';
  inputDigits[4] = '\0';
  inputIndex = 0;
}

void resetQuizInput() {
  quizInput[0] = '\0';
  quizInputIndex = 0;
}

void setRgb(byte r, byte g, byte b) {
  if (RGB_COMMON_ANODE) {
    analogWrite(PIN_LED_R, 255 - r);
    analogWrite(PIN_LED_G, 255 - g);
    analogWrite(PIN_LED_B, 255 - b);
  } else {
    analogWrite(PIN_LED_R, r);
    analogWrite(PIN_LED_G, g);
    analogWrite(PIN_LED_B, b);
  }
}

void printLcdLine(byte row, const char *text) {
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
  sprintf(out, "%02u:%02u", hh, mm);
}

void runOutputSelfTest() {
  static bool initialized = false;
  static unsigned long lastLcdMs = 0;
  static unsigned long lastLedMs = 0;
  static unsigned long lastToneMs = 0;
  static byte ledStep = 0;
  static byte toneStep = 0;

  unsigned long nowMs = millis();

  if (!initialized) {
    initialized = true;
    lcd.clear();
    printLcdLine(0, "SELF TEST MODE");
    printLcdLine(1, "RGB+BUZZER TEST");
    setRgb(255, 0, 0);
    tone(PIN_BUZZER, 880);
    lastLcdMs = nowMs;
    lastLedMs = nowMs;
    lastToneMs = nowMs;
    return;
  }

  if ((nowMs - lastLcdMs) >= 300) {
    lastLcdMs = nowMs;
    char line2[17];
    if (ledStep == 0) snprintf(line2, 17, "LED:RED");
    if (ledStep == 1) snprintf(line2, 17, "LED:GREEN");
    if (ledStep == 2) snprintf(line2, 17, "LED:BLUE");
    if (ledStep == 3) snprintf(line2, 17, "LED:WHITE");
    printLcdLine(0, "SELF TEST MODE");
    printLcdLine(1, line2);
  }

  if ((nowMs - lastLedMs) >= 1000) {
    lastLedMs = nowMs;
    ledStep = (ledStep + 1) % 4;
    if (ledStep == 0) setRgb(255, 0, 0);
    if (ledStep == 1) setRgb(0, 255, 0);
    if (ledStep == 2) setRgb(0, 0, 255);
    if (ledStep == 3) setRgb(255, 255, 255);
  }

  if ((nowMs - lastToneMs) >= 200) {
    lastToneMs = nowMs;
    toneStep = (toneStep + 1) % 4;
    const int tones[4] = {880, 988, 1175, 1319};
    tone(PIN_BUZZER, tones[toneStep]);
  }
}