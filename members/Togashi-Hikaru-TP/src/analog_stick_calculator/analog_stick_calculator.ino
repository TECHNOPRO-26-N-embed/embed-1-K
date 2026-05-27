#include <Arduino.h>
#include <limits.h>

// アナログスティックとボタンで2項演算を行う状態遷移型の電卓プログラム。
//
// 設計方針:
// - loop() は「入力読取 -> 状態遷移判定 -> 状態処理」の3段で構成する。
// - 各入力はイベント（押下エッジ）として扱い、押しっぱなしを1回入力として処理する。
// - エラーは errorCode に集約し、updateState() で STATE_ERROR へ遷移させる。
// - 計算は computeChecked() に一本化し、0除算/オーバーフローを安全に検出する。

// Pin mapping
// ジョイスティック2軸（アナログ）
const uint8_t PIN_STICK_X = A0;
const uint8_t PIN_STICK_Y = A1;
// ジョイスティック押し込みSW（デジタル）
const uint8_t PIN_STICK_SW = 2;
// 演算子切替ボタン
const uint8_t PIN_BTN_OP = 3;
// 入力確定ボタン（入力バッファ -> オペランド）
const uint8_t PIN_BTN_CONFIRM = 4;
// 計算実行ボタン
const uint8_t PIN_BTN_CALC = 5;
// 全体リセットボタン
const uint8_t PIN_BTN_RESET = 6;
// 取り消し/戻るボタン
const uint8_t PIN_BTN_CANCEL = 7;
// 完了通知用ブザー
const uint8_t PIN_BUZZER = 8;

// State IDs
// 待機状態（何も確定されていない初期状態）
const uint8_t STATE_IDLE = 0;
// 入力状態（桁入力・オペランド確定・演算子選択）
const uint8_t STATE_INPUT = 1;
// 計算状態（バリデーション + 演算）
const uint8_t STATE_CALCULATING = 2;
// 結果表示状態（シリアル表示 + ブザー）
const uint8_t STATE_RESULT = 3;
// エラー状態（原因表示、キャンセルで復帰）
const uint8_t STATE_ERROR = 4;
// リセット状態（全コンテキスト初期化）
const uint8_t STATE_RESET = 5;

// Error codes
// エラーなし
const uint8_t ERROR_NONE = 0;
// 0除算
const uint8_t ERROR_DIV_ZERO = 1;
// スティック異常値（端値固定）
const uint8_t ERROR_STICK_FAULT = 2;
// 算術オーバーフロー
const uint8_t ERROR_OVERFLOW = 3;
// オペランド未入力など入力不足
const uint8_t ERROR_INPUT_INCOMPLETE = 4;
// 未定義演算子
const uint8_t ERROR_INVALID_OPERATOR = 5;

// Direction IDs
// 方向なし（ニュートラル）
const int8_t DIR_NONE = -1;
// 5方向入力: 上 / 右上 / 右下 / 左下 / 左上
const int8_t DIR_UP = 0;
const int8_t DIR_RIGHT_UP = 1;
const int8_t DIR_RIGHT_DOWN = 2;
const int8_t DIR_LEFT_DOWN = 3;
const int8_t DIR_LEFT_UP = 4;

// Global runtime variables
// 現在状態と直前状態（状態遷移時の「入場処理」に使用）
uint8_t currentState = STATE_IDLE;
uint8_t prevState = STATE_IDLE;

// 計算対象オペランド・結果
long operand1 = 0;
long operand2 = 0;
long calcResult = 0;
// 桁入力の一時バッファ（確定前の数値）
long inputBuffer = 0;
// inputBuffer の桁数（最大桁制限に使用）
uint8_t inputDigits = 0;
// 入力レンジ: 0=0-4, 1=5-9
uint8_t digitRangeMode = 0;
// 選択中演算子
char currentOperator = '+';
// 各データが確定済みかを示すフラグ
bool hasOperand1 = false;
bool hasOperand2 = false;
bool hasResult = false;
// 現在保持中のエラーコード
uint8_t errorCode = ERROR_NONE;
// ERROR状態の入場メッセージを1回化するフラグ
bool errorEntryShown = false;

// 直近で読み取ったスティック生値と入力イベント
long analogX = 512;
long analogY = 512;
bool stickPressed = false;
int8_t stickDirection = DIR_NONE;
// 各ボタンの「押下イベント（立ち上がり）」フラグ
bool btnOpPressed = false;
bool btnConfirmPressed = false;
bool btnCalcPressed = false;
bool btnResetPressed = false;
bool btnCancelPressed = false;

// 各ボタンの最終デバウンス時刻
unsigned long lastDebounceMs[5] = {0, 0, 0, 0, 0};
// スティック入力の周期制御時刻
unsigned long lastInputPollMs = 0;
// ブザー制御時刻（非ブロッキング）
unsigned long lastBuzzerStartMs = 0;
// 状態遷移した時刻（必要に応じてタイムアウト設計へ拡張可）
unsigned long stateEnteredMs = 0;
// ブザー鳴動中かどうか
bool buzzerActive = false;

// 入力安定化・応答性に関わる各種定数
const unsigned long DEBOUNCE_MS = 50;
const unsigned long INPUT_POLL_MS = 100;
const int STICK_DEADZONE = 80;
const int STICK_THRESHOLD = 220;
const unsigned int BUZZER_FREQ_HZ = 2000;
const unsigned int BUZZER_DURATION_MS = 120;
const uint8_t MAX_INPUT_DIGITS = 9;
const unsigned long STICK_FAULT_HOLD_MS = 1000;

// loop() 冒頭で毎周期更新する現在時刻キャッシュ
unsigned long nowMs = 0;

// 関数プロトタイプ（処理単位を明確化）
void setup();
void loop();
void readButtons();
void readAnalogStick();
void updateState();
void doAnalogStickInput(int8_t direction, uint8_t rangeMode);
void doOperatorSelect(bool opButtonEvent);
void doAnalogStickPress(bool stickPressEvent);
void doConfirmInput(bool confirmButtonEvent, long bufferedValue);
bool doCalculate(long a, long b, char op);
void doReset(bool resetButtonEvent);
void doDisplayResult(long a, long b, char op, long result);
bool doErrorCheck(char op, long b, long x, long y);
void doBuzz(bool activeFlag, unsigned long startedAtMs);

void changeState(uint8_t newState);
void clearInputBuffer();
void clearCalculationContext();
void clearAllRuntime();
void showInputStatus();
void showError();
bool computeChecked(long a, long b, char op, long &out);

// 起動時に入出力ピンと実行時変数を初期化する。
void setup() {
  // 各入力はプルアップで受ける（押下時LOW）
  pinMode(PIN_STICK_SW, INPUT_PULLUP);
  pinMode(PIN_BTN_OP, INPUT_PULLUP);
  pinMode(PIN_BTN_CONFIRM, INPUT_PULLUP);
  pinMode(PIN_BTN_CALC, INPUT_PULLUP);
  pinMode(PIN_BTN_RESET, INPUT_PULLUP);
  pinMode(PIN_BTN_CANCEL, INPUT_PULLUP);
  // ブザーは出力ピン
  pinMode(PIN_BUZZER, OUTPUT);

  // 実行時コンテキストを初期化
  clearAllRuntime();
  currentState = STATE_IDLE;
  prevState = STATE_IDLE;
  stateEnteredMs = millis();

  // デバッグ/表示用シリアル開始
  Serial.begin(9600);

  // 起動通知音
  tone(PIN_BUZZER, BUZZER_FREQ_HZ, 80);

  Serial.println(F(""));
  Serial.println(F("=== 2項演算電卓ガイド ==="));
  Serial.println(F("[数字入力]"));
  Serial.println(F("- スティックを倒す: 1桁選択（0-4 または 5-9）"));
  Serial.println(F("- スティック押し込み: 桁レンジ切替"));
  Serial.println(F("- CONFIRM: 現在入力を確定"));
  Serial.println(F(" * 1回目の確定 -> operand1"));
  Serial.println(F(" * 2回目の確定 -> operand2"));
  Serial.println(F("[演算子] OPボタンで + -> - -> * -> /"));
  Serial.println(F("[実行] 2つのオペランド確定後にCALC"));
  Serial.println(F("[取消] 入力中 -> operand2 -> operand1 の順で戻る"));
  Serial.println(F("[リセット] 初期状態に戻る"));
  Serial.println(F("[エラー] ERROR状態はCANCELで解除"));
}

// 入力読取・状態遷移・状態別処理を1周期で実行するメインループ。
void loop() {
  // この周期での基準時刻を確定
  nowMs = millis();

  // 毎ループで入力を取得し、状態遷移判定に必要なイベントフラグを更新する。
  readButtons();
  readAnalogStick();

  // 演算子切替は状態共通のため先に処理する。
  doOperatorSelect(btnOpPressed);

  if (currentState == STATE_INPUT) {
    // 入力状態では桁レンジ切替・桁追加・確定の順で処理
    doAnalogStickPress(stickPressed);
    doAnalogStickInput(stickDirection, digitRangeMode);
    doConfirmInput(btnConfirmPressed, inputBuffer);

    // キャンセルは「入力バッファ -> operand2 -> operand1」の順で1段階ずつ戻す
    if (btnCancelPressed) {
      if (inputDigits > 0) {
        clearInputBuffer();
        //showInputStatus();
      } else if (hasOperand2) {
        operand2 = 0;
        hasOperand2 = false;
<<<<<<< HEAD
=======
<<<<<<< HEAD
        Serial.println(F("cancel: operand2 cleared"));
      } else if (hasOperand1) {
        operand1 = 0;
        hasOperand1 = false;
        Serial.println(F("cancel: operand1 cleared"));
=======
>>>>>>> 0da5c498f512e1082bed53b5d83363c49acbf2af
        Serial.println(F("取消: 第2オペランドをクリア"));
      } else if (hasOperand1) {
        operand1 = 0;
        hasOperand1 = false;
        Serial.println(F("取消: 第1オペランドをクリア"));
<<<<<<< HEAD
=======
>>>>>>> 0fa8001fd10e1a738073c67920f0e0e3435714f3
>>>>>>> 0da5c498f512e1082bed53b5d83363c49acbf2af
      }
    }
  }

  updateState();

  // 基本設計書の状態遷移（待機/入力/計算/結果/エラー/リセット）に対応。
  switch (currentState) {
    case STATE_IDLE:
      // 状態遷移直後だけ1回だけ実行したい処理は prevState で判定する
      if (prevState != STATE_IDLE) {
        noTone(PIN_BUZZER);
        buzzerActive = false;
<<<<<<< HEAD
        Serial.println(F("状態: 待機(IDLE)"));
=======
<<<<<<< HEAD
        Serial.println(F("state: IDLE"));
=======
        Serial.println(F("状態: 待機(IDLE)"));
>>>>>>> 0fa8001fd10e1a738073c67920f0e0e3435714f3
>>>>>>> 0da5c498f512e1082bed53b5d83363c49acbf2af
        // 入場処理を消費したら、次ループで再実行しないよう同期する。
        prevState = STATE_IDLE;
      }
      break;

    case STATE_INPUT:
      if (prevState != STATE_INPUT) {
        // 結果表示のフラグをクリアして新規入力へ
        hasResult = false;
<<<<<<< HEAD
        Serial.println(F("状態: 入力(INPUT)"));
=======
<<<<<<< HEAD
        Serial.println(F("state: INPUT"));
=======
        Serial.println(F("状態: 入力(INPUT)"));
>>>>>>> 0fa8001fd10e1a738073c67920f0e0e3435714f3
>>>>>>> 0da5c498f512e1082bed53b5d83363c49acbf2af
        // 入場処理を消費したら、次ループで再実行しないよう同期する。
        prevState = STATE_INPUT;
        //showInputStatus();
      }
      break;

    case STATE_CALCULATING: {
      if (prevState != STATE_CALCULATING) {
<<<<<<< HEAD
        Serial.println(F("状態: 計算中(CALCULATING)"));
=======
<<<<<<< HEAD
        Serial.println(F("state: CALCULATING"));
=======
        Serial.println(F("状態: 計算中(CALCULATING)"));
>>>>>>> 0fa8001fd10e1a738073c67920f0e0e3435714f3
>>>>>>> 0da5c498f512e1082bed53b5d83363c49acbf2af
      }

      if (doErrorCheck(currentOperator, operand2, analogX, analogY)) {
        // 演算前チェックでエラーが見つかった場合は即エラー状態へ
        changeState(STATE_ERROR);
        break;
      }

      if (doCalculate(operand1, operand2, currentOperator)) {
        // 計算成功で結果状態へ
        hasResult = true;
        changeState(STATE_RESULT);
      } else {
        // 失敗時（overflow/invalid等）はエラー状態へ
        changeState(STATE_ERROR);
      }
      break;
    }

    case STATE_RESULT:
      if (prevState != STATE_RESULT) {
        // 結果表示は状態入場時に1回だけ行う
        doDisplayResult(operand1, operand2, currentOperator, calcResult);
        // doBuzz() が初回呼び出しで鳴動開始できるようfalseに初期化
        buzzerActive = false;
        // 入場処理を消費したら、次ループで再実行しないよう同期する。
        prevState = STATE_RESULT;
      }
      // 結果状態の間だけブザー制御
      doBuzz(buzzerActive, lastBuzzerStartMs);
      break;

    case STATE_ERROR:
      if (!errorEntryShown) {
        // エラー状態に入った瞬間に原因を表示
        showError();
        errorEntryShown = true;
      }
      // キャンセル押下でエラー解除して待機へ戻る
      if (btnCancelPressed) {
        errorCode = ERROR_NONE;
        changeState(STATE_IDLE);
      }
      break;

    case STATE_RESET:
      // RESET状態に入った直後だけdoResetを実行
      if (prevState != STATE_RESET) {
        doReset(true);
      }
      break;

    default:
      errorCode = ERROR_INVALID_OPERATOR;
      changeState(STATE_ERROR);
      break;
  }

  btnOpPressed = false;
  btnConfirmPressed = false;
  btnCalcPressed = false;
  btnResetPressed = false;
  btnCancelPressed = false;
  stickPressed = false;
  // stickDirection は readAnalogStick() のポーリング更新で上書きするためここでは維持
}

// 各ボタンの押下エッジをデバウンス付きで検出し、イベントフラグへ反映する。
void readButtons() {
  // 取り扱うボタンを配列化して同一ロジックで処理
  const uint8_t pins[5] = {
    PIN_BTN_OP,
    PIN_BTN_CONFIRM,
    PIN_BTN_CALC,
    PIN_BTN_RESET,
    PIN_BTN_CANCEL,
  };

  static bool lastRaw[5] = {false, false, false, false, false};
  static bool stablePressed[5] = {false, false, false, false, false};

  bool events[5] = {false, false, false, false, false};

  for (uint8_t i = 0; i < 5; ++i) {
    // プルアップ入力のためLOWを押下と解釈
    bool rawPressed = (digitalRead(pins[i]) == LOW);

    // 生値が変化した時刻を記録（デバウンス開始）
    if (rawPressed != lastRaw[i]) {
      lastDebounceMs[i] = nowMs;
      lastRaw[i] = rawPressed;
    }

    // 一定時間同じレベルが続いたときだけ押下イベントを立てる。
    if ((nowMs - lastDebounceMs[i]) >= DEBOUNCE_MS && stablePressed[i] != rawPressed) {
      stablePressed[i] = rawPressed;
      if (stablePressed[i]) {
        // 押下エッジのみイベント化（離した瞬間はイベント化しない）
        events[i] = true;
      }
    }
  }

  btnOpPressed = events[0];
  btnConfirmPressed = events[1];
  btnCalcPressed = events[2];
  btnResetPressed = events[3];
  btnCancelPressed = events[4];
}

// スティック軸値と押込SWを読み取り、方向と故障判定を更新する。
void readAnalogStick() {
  // 入力取り込み周期を固定し、ノイズと過剰反応を抑える。
  if ((nowMs - lastInputPollMs) < INPUT_POLL_MS) {
    return;
  }
  lastInputPollMs = nowMs;

  analogX = analogRead(PIN_STICK_X);
  analogY = analogRead(PIN_STICK_Y);

  static bool swLastRaw = false;
  static bool swStablePressed = false;
  static unsigned long swLastDebounceMs = 0;

  bool swRawPressed = (digitalRead(PIN_STICK_SW) == LOW);
  // 押し込みSWもボタンと同様にデバウンス処理
  if (swRawPressed != swLastRaw) {
    swLastDebounceMs = nowMs;
    swLastRaw = swRawPressed;
  }

  if ((nowMs - swLastDebounceMs) >= DEBOUNCE_MS && swStablePressed != swRawPressed) {
    swStablePressed = swRawPressed;
    if (swStablePressed) {
      stickPressed = true;
    }
  }

  const long dx = analogX - 512;
  const long dy = analogY - 512;
  const long adx = labs(dx);
  const long ady = labs(dy);

  // 中央付近は無効（デッドゾーン）として扱い、意図しない入力を防ぐ。
  if (adx < STICK_DEADZONE && ady < STICK_DEADZONE) {
    stickDirection = DIR_NONE;
  } else if (adx < STICK_THRESHOLD && ady < STICK_THRESHOLD) {
    // 閾値未満はニュートラル扱い（やや傾けただけでは入力しない）
    stickDirection = DIR_NONE;
  } else if (dy > 0 && ady > adx) {
    // Y正方向優勢は「上」
    stickDirection = DIR_UP;
  } else if (dx >= 0 && dy >= 0) {
    // 第1象限
    stickDirection = DIR_RIGHT_UP;
  } else if (dx >= 0 && dy < 0) {
    // 第4象限
    stickDirection = DIR_RIGHT_DOWN;
  } else if (dx < 0 && dy < 0) {
    // 第3象限
    stickDirection = DIR_LEFT_DOWN;
  } else {
    // 第2象限
    stickDirection = DIR_LEFT_UP;
  }
  /* デバッグ用にスティック状態をシリアル出力
   これにより、スティックの傾きと押し込みが正しく認識されているか確認できる。
   また、異常な値が継続していないかも監視できる。
  *//*
  Serial.print(F("analogX: "));
  Serial.print(analogX);
  Serial.print(F(", analogY: "));
  Serial.print(analogY);
  Serial.print(F(", stickDirection: "));
  Serial.println(stickDirection);*/

  static unsigned long extremeSinceMs = 0;
  bool extremeValue = (analogX <= 1 || analogX >= 2000 || analogY <= 1 || analogY >= 2000);

  // 端値が一定時間継続した場合のみ、スティック故障エラーとみなす。
  if (extremeValue) {
    if (extremeSinceMs == 0) {
      extremeSinceMs = nowMs;
    } else if ((nowMs - extremeSinceMs) >= STICK_FAULT_HOLD_MS) {
      errorCode = ERROR_STICK_FAULT;
    }
  } else {
    extremeSinceMs = 0;
  }
}

// 現在の入力・エラー状況に応じて状態遷移を決定する。
void updateState() {
  // リセットは全状態から最優先で受け付ける。
  if (btnResetPressed) {
    changeState(STATE_RESET);
    return;
  }

  // エラー確定時はエラー状態へ遷移する。
  if (errorCode != ERROR_NONE && currentState != STATE_RESET && currentState != STATE_ERROR) {
    changeState(STATE_ERROR);
    return;
  }

  switch (currentState) {
    case STATE_IDLE:
      // 待機中に入力操作が始まったら入力状態へ遷移する。
      // これにより、初回のスティック押し込み/倒しから入力を開始できる。
      if (stickPressed || stickDirection != DIR_NONE || inputDigits > 0 || hasOperand1 || hasOperand2) {
        changeState(STATE_INPUT);
      }
      break;

    case STATE_INPUT:
      // 計算実行は2オペランド確定後のみ許可
      if (btnCalcPressed) {
        if (hasOperand1 && hasOperand2) {
          changeState(STATE_CALCULATING);
        } else {
          errorCode = ERROR_INPUT_INCOMPLETE;
          changeState(STATE_ERROR);
        }
      }
      if (!hasOperand1 && inputDigits == 0 && btnCancelPressed) {
        // 何も確定されていない時のキャンセルで待機へ戻る
        changeState(STATE_IDLE);
      }
      break;

    case STATE_CALCULATING:
      break;

    case STATE_RESULT:
      if (btnCancelPressed) {
        // 結果を破棄して待機へ
        clearCalculationContext();
        changeState(STATE_IDLE);
      }
      if (btnOpPressed) {
        // 演算子押下時は直前結果を次計算の第1オペランドとして引き継ぐ
        operand1 = calcResult;
        hasOperand1 = true;
        operand2 = 0;
        hasOperand2 = false;
        hasResult = false;
        clearInputBuffer();
        errorCode = ERROR_NONE;
        changeState(STATE_INPUT);
        break;
      }
      if (stickDirection != DIR_NONE || btnConfirmPressed) {
        // 何か入力が始まったら新しい計算セッションへ
        clearCalculationContext();
        changeState(STATE_INPUT);
      }
      break;

    case STATE_ERROR:
      break;

    case STATE_RESET:
      break;

    default:
      errorCode = ERROR_INVALID_OPERATOR;
      changeState(STATE_ERROR);
      break;
  }
}

// スティック方向とレンジモードから1桁を選び、入力バッファへ追加する。
void doAnalogStickInput(int8_t direction, uint8_t rangeMode) {
  static int8_t lastConsumedDirection = DIR_NONE;

  if (direction == DIR_NONE) {
    // ニュートラルに戻ったら次の同方向入力を許可
    lastConsumedDirection = DIR_NONE;
    return;
  }
  // 同方向の連続入力を1回に抑え、ニュートラル復帰後に次入力を受け付ける。
  if (direction == lastConsumedDirection) {
    return;
  }
  lastConsumedDirection = direction;

  if (direction < DIR_UP || direction > DIR_LEFT_UP) {
    // 想定外方向値は無視
    return;
  }

  if (inputDigits >= MAX_INPUT_DIGITS) {
    // 桁数上限に達したら追加しない
<<<<<<< HEAD
    Serial.println(F("警告: 最大桁数に達しました"));
=======
<<<<<<< HEAD
    Serial.println(F("warn: max digits reached"));
=======
    Serial.println(F("警告: 最大桁数に達しました"));
>>>>>>> 0fa8001fd10e1a738073c67920f0e0e3435714f3
>>>>>>> 0da5c498f512e1082bed53b5d83363c49acbf2af
    return;
  }

  static const uint8_t digitTable[2][5] = {
    {0, 1, 2, 3, 4},
    {5, 6, 7, 8, 9},
  };

  uint8_t mode = (rangeMode == 0) ? 0 : 1;
  uint8_t digit = digitTable[mode][direction];

  // 10進数として1桁追加
  inputBuffer = (inputBuffer * 10L) + digit;
  ++inputDigits;

  showInputStatus();
}

// 演算子切替ボタン押下ごとに + -> - -> * -> / を巡回する。
void doOperatorSelect(bool opButtonEvent) {
  if (!opButtonEvent) {
    return;
  }

  switch (currentOperator) {
    case '+':
      currentOperator = '-';
      break;
    case '-':
      currentOperator = '*';
      break;
    case '*':
      currentOperator = '/';
      break;
    case '/':
      currentOperator = '+';
      break;
    default:
      currentOperator = '+';
      break;
  }

<<<<<<< HEAD
  Serial.print(F("演算子: "));
=======
<<<<<<< HEAD
  Serial.print(F("operator: "));
=======
  Serial.print(F("演算子: "));
>>>>>>> 0fa8001fd10e1a738073c67920f0e0e3435714f3
>>>>>>> 0da5c498f512e1082bed53b5d83363c49acbf2af
  Serial.println(currentOperator);
}

// スティック押込みで入力数字レンジ（0-4/5-9）をトグルする。
void doAnalogStickPress(bool stickPressEvent) {
  if (!stickPressEvent) {
    return;
  }

  // 押すたびにレンジを反転
  digitRangeMode = (digitRangeMode == 0) ? 1 : 0;

<<<<<<< HEAD
  Serial.print(F("入力レンジ: "));
=======
<<<<<<< HEAD
  Serial.print(F("digit range: "));
=======
  Serial.print(F("入力レンジ: "));
>>>>>>> 0fa8001fd10e1a738073c67920f0e0e3435714f3
>>>>>>> 0da5c498f512e1082bed53b5d83363c49acbf2af
  if (digitRangeMode == 0) {
    Serial.println(F("0-4"));
  } else {
    Serial.println(F("5-9"));
  }
}

// 入力バッファをオペランドへ確定し、次入力のためにバッファをクリアする。
void doConfirmInput(bool confirmButtonEvent, long bufferedValue) {
  if (!confirmButtonEvent) {
    return;
  }
  if (inputDigits == 0) {
    // 空確定は無効
<<<<<<< HEAD
    Serial.println(F("警告: 確定する数字がありません"));
=======
<<<<<<< HEAD
    Serial.println(F("warn: no digits to confirm"));
=======
    Serial.println(F("警告: 確定する数字がありません"));
>>>>>>> 0fa8001fd10e1a738073c67920f0e0e3435714f3
>>>>>>> 0da5c498f512e1082bed53b5d83363c49acbf2af
    return;
  }

  // 1つ目確定後は同じ手順で2つ目を確定する（設計書の入力フローに準拠）。
  if (!hasOperand1) {
    // まず operand1 を確定
    operand1 = bufferedValue;
    hasOperand1 = true;
<<<<<<< HEAD
    Serial.print(F("第1オペランド確定: "));
=======
<<<<<<< HEAD
    Serial.print(F("operand1 fixed: "));
=======
    Serial.print(F("第1オペランド確定: "));
>>>>>>> 0fa8001fd10e1a738073c67920f0e0e3435714f3
>>>>>>> 0da5c498f512e1082bed53b5d83363c49acbf2af
    Serial.println(operand1);
    clearInputBuffer();
    return;
  }

  // 次に operand2 を確定
  operand2 = bufferedValue;
  hasOperand2 = true;
<<<<<<< HEAD
  Serial.print(F("第2オペランド確定: "));
=======
<<<<<<< HEAD
  Serial.print(F("operand2 fixed: "));
=======
  Serial.print(F("第2オペランド確定: "));
>>>>>>> 0fa8001fd10e1a738073c67920f0e0e3435714f3
>>>>>>> 0da5c498f512e1082bed53b5d83363c49acbf2af
  Serial.println(operand2);
  clearInputBuffer();
}

// 指定演算を実行し、成功時に計算結果へ反映する。
bool doCalculate(long a, long b, char op) {
  long out = 0;
  // 安全計算関数に処理を委譲
  if (!computeChecked(a, b, op, out)) {
    return false;
  }
  calcResult = out;
  return true;
}

// 実行時コンテキストを初期化して待機状態へ戻す。
void doReset(bool resetButtonEvent) {
  if (!resetButtonEvent) {
    return;
  }

  // 全実行時データをクリアして初期状態へ
  clearAllRuntime();
  noTone(PIN_BUZZER);
  Serial.println();
<<<<<<< HEAD
  Serial.println(F("状態: リセット(RESET) -> 待機(IDLE)"));
=======
<<<<<<< HEAD
  Serial.println(F("state: RESET -> IDLE"));
=======
  Serial.println(F("状態: リセット(RESET) -> 待機(IDLE)"));
>>>>>>> 0fa8001fd10e1a738073c67920f0e0e3435714f3
>>>>>>> 0da5c498f512e1082bed53b5d83363c49acbf2af
  changeState(STATE_IDLE);
}

// 計算式と結果をシリアルモニタへ整形出力する。
void doDisplayResult(long a, long b, char op, long result) {
  // 例: 12 + 34 = 46
  Serial.print(a);
  Serial.print(' ');
  Serial.print(op);
  Serial.print(' ');
  Serial.print(b);
  Serial.print(F(" = "));
  Serial.println(result);
}

// 0除算とスティック故障条件を監視し、エラー遷移要否を返す。
bool doErrorCheck(char op, long b, long x, long y) {
  // 除算演算で第2オペランドが0なら即エラー。
  if (op == '/' && b == 0) {
    errorCode = ERROR_DIV_ZERO;
    return true;
  }

  if (x <= 1 || x >= 2000 || y <= 1 || y >= 2000) {
    // readAnalogStick() 側で継続判定され ERROR_STICK_FAULT が立っていれば異常扱い
    if (errorCode == ERROR_STICK_FAULT) {
      return true;
    }
  }

  return false;
}

// 計算完了通知ブザーを非ブロッキングで開始・停止する。
void doBuzz(bool activeFlag, unsigned long startedAtMs) {
  // delayを使わず、結果表示中のみ非ブロッキングで鳴動制御する。
  if (!activeFlag) {
    // 初回呼び出しで鳴動開始
    tone(PIN_BUZZER, BUZZER_FREQ_HZ);
    buzzerActive = true;
    lastBuzzerStartMs = nowMs;
    return;
  }

  // 規定時間経過で停止
  if ((nowMs - startedAtMs) >= BUZZER_DURATION_MS) {
    noTone(PIN_BUZZER);
    buzzerActive = false;
  }
}

// 状態変更時のみ現在/前回状態と遷移時刻を更新する。
void changeState(uint8_t newState) {
  if (currentState == newState) {
    // 同一状態への遷移要求は無視
    return;
  }
  prevState = currentState;
  currentState = newState;
  if (newState == STATE_ERROR) {
    // ERRORへ入るたびに1回表示フラグをリセット
    errorEntryShown = false;
  }
  stateEnteredMs = nowMs;
}

// 入力中の数値と桁数を初期値へ戻す。
void clearInputBuffer() {
  // 入力途中データのみクリア
  inputBuffer = 0;
  inputDigits = 0;
}

// 計算関連データ（オペランド・結果・エラー）を初期化する。
void clearCalculationContext() {
  // 計算セッション単位のデータを初期化
  operand1 = 0;
  operand2 = 0;
  calcResult = 0;
  hasOperand1 = false;
  hasOperand2 = false;
  hasResult = false;
  clearInputBuffer();
  errorCode = ERROR_NONE;
}

// 実行時に保持する全フラグ/時刻/入力値を初期状態へ戻す。
void clearAllRuntime() {
  // 計算セッション情報の初期化
  clearCalculationContext();
  // UI/入力状態の初期化
  currentOperator = '+';
  digitRangeMode = 0;
  analogX = 512;
  analogY = 512;
  stickDirection = DIR_NONE;
  stickPressed = false;
  btnOpPressed = false;
  btnConfirmPressed = false;
  btnCalcPressed = false;
  btnResetPressed = false;
  btnCancelPressed = false;
  errorEntryShown = false;
  buzzerActive = false;
  lastBuzzerStartMs = 0;
  lastInputPollMs = 0;

  // ボタンデバウンス履歴もクリア
  for (uint8_t i = 0; i < 5; ++i) {
    lastDebounceMs[i] = 0;
  }
}

// 現在の入力バッファ状態と演算子をシリアルへ表示する。
void showInputStatus() {
  // 現在の入力進捗を1行で可視化
  //Serial.print(F("input= "));
  Serial.print(inputBuffer);
  Serial.print("\n");
  /*Serial.print(F(" digits= "));
  Serial.print(inputDigits);
  Serial.print(F(" range= "));
  Serial.print((digitRangeMode == 0) ? F("0-4") : F("5-9"));
  Serial.print(F(" op= "));
  Serial.println(currentOperator);*/
}

// エラーコードに対応するメッセージをシリアルへ表示する。
void showError() {
<<<<<<< HEAD
  Serial.print(F("状態: エラー(ERROR) コード="));
=======
<<<<<<< HEAD
  Serial.print(F("state: ERROR code="));
=======
  Serial.print(F("状態: エラー(ERROR) コード="));
>>>>>>> 0fa8001fd10e1a738073c67920f0e0e3435714f3
>>>>>>> 0da5c498f512e1082bed53b5d83363c49acbf2af
  Serial.println(errorCode);

  // エラーコードごとに具体的な原因を表示
  switch (errorCode) {
    case ERROR_DIV_ZERO:
<<<<<<< HEAD
=======
<<<<<<< HEAD
      Serial.println(F("error: divide by zero"));
      break;
    case ERROR_STICK_FAULT:
      Serial.println(F("error: stick abnormal value"));
      break;
    case ERROR_OVERFLOW:
      Serial.println(F("error: arithmetic overflow"));
      break;
    case ERROR_INPUT_INCOMPLETE:
      Serial.println(F("error: operands are incomplete"));
      break;
    case ERROR_INVALID_OPERATOR:
      Serial.println(F("error: invalid operator"));
      break;
    default:
      Serial.println(F("error: unknown"));
=======
>>>>>>> 0da5c498f512e1082bed53b5d83363c49acbf2af
      Serial.println(F("エラー: 0で割ることはできません"));
      break;
    case ERROR_STICK_FAULT:
      Serial.println(F("エラー: スティック異常値"));
      break;
    case ERROR_OVERFLOW:
      Serial.println(F("エラー: 算術オーバーフロー"));
      break;
    case ERROR_INPUT_INCOMPLETE:
      Serial.println(F("エラー: オペランドが不足しています"));
      break;
    case ERROR_INVALID_OPERATOR:
      Serial.println(F("エラー: 無効な演算子"));
      break;
    default:
      Serial.println(F("エラー: 不明"));
<<<<<<< HEAD
=======
>>>>>>> 0fa8001fd10e1a738073c67920f0e0e3435714f3
>>>>>>> 0da5c498f512e1082bed53b5d83363c49acbf2af
      break;
  }
}

// 四則演算を安全に実行し、0除算・オーバーフローを検出する。
bool computeChecked(long a, long b, char op, long &out) {
  // 一時的に long long で受け、long 範囲外を判定する。
  long long raw = 0;

  switch (op) {
    case '+':
      // 加算
      raw = static_cast<long long>(a) + static_cast<long long>(b);
      break;

    case '-':
      // 減算
      raw = static_cast<long long>(a) - static_cast<long long>(b);
      break;

    case '*':
      // 乗算
      raw = static_cast<long long>(a) * static_cast<long long>(b);
      break;

    case '/':
      // 0除算を禁止
      if (b == 0) {
        errorCode = ERROR_DIV_ZERO;
        return false;
      }
      // 2の補数環境での最小値/-1のあふれを保護
      if (a == LONG_MIN && b == -1) {
        errorCode = ERROR_OVERFLOW;
        return false;
      }
      // 整数除算
      raw = a / b;
      break;

    default:
      // 定義外演算子
      errorCode = ERROR_INVALID_OPERATOR;
      return false;
  }

  // いったんlong longで計算し、long範囲外をオーバーフローとして検出する。
  if (raw > LONG_MAX || raw < LONG_MIN) {
    errorCode = ERROR_OVERFLOW;
    return false;
  }

  // 問題なければ結果を返す
  out = static_cast<long>(raw);
  errorCode = ERROR_NONE;
  return true;
}
