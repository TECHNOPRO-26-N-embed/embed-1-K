#include <avr/io.h>
#include <util/delay.h>
#include <stdbool.h>

// 定数定義
#define RFID_SS_PIN PB2 // RFIDのSSピン
#define RFID_RST_PIN PB1 // RFIDのリセットピン
#define BUZZER_PIN PD7 // ブザーのピン
#define LCD_ADDR 0x27 // LCDのI2Cアドレス

#define CARD_CHECK_INTERVAL_MS 200 // カードチェック間隔
#define LCD_UPDATE_INTERVAL_MS 500 // LCD更新間隔
#define BUZZER_CONTROL_INTERVAL_MS 100 // ブザー制御間隔
#define ERROR_RETRY_INTERVAL_MS 1000 // エラーリトライ間隔
#define PASSWORD_TIMEOUT_MS 10000 // パスワードタイムアウト
#define RFID_NO_CARD_TIMEOUT_MS 1500 // カード未検出タイムアウト

#define MAX_RETRY_COUNT 3 // 最大リトライ回数

// 状態を表す列挙型
typedef enum {
    STATE_WAIT, // 待機状態
    STATE_JUDGE, // 判定状態
    STATE_RESULT, // 結果表示状態
    STATE_ERROR, // エラー状態
    STATE_TIMEOUT // タイムアウト状態
} SystemState;

// グローバル変数
SystemState currentState = STATE_WAIT; // 現在の状態
unsigned long lastCardCheckTime = 0; // 最後にカードをチェックした時刻
unsigned long lastLcdUpdateTime = 0; // 最後にLCDを更新した時刻
unsigned long lastBuzzerControlTime = 0; // 最後にブザーを制御した時刻
unsigned long errorRetryStartTime = 0; // エラーリトライ開始時刻
unsigned long stateEnterTime = 0; // 状態遷移時刻

uint8_t rfidData[16] = {0}; // RFIDデータ
char lcdMessage[17] = ""; // LCD表示メッセージ
bool isCardDetected = false; // カード検出フラグ
bool isReadingInProgress = false; // 読み取り中フラグ
bool isJudgeSuccessful = false; // 判定結果
bool hasErrorOccurred = false; // エラーフラグ
int errorCode = 0; // エラーコード
uint8_t retryCount = 0; // リトライ回数
uint8_t buzzerPattern = 0; // ブザーパターン

// 関数プロトタイプ宣言
void initializeSystem();
void mainLoop();
bool checkForCard();
void updateLcdDisplay(const char* message);
void setBuzzerPattern(bool isSuccess);
void controlBuzzerNonBlocking();
bool validateCardData(uint8_t data[16]);
void handleSystemError(int errorCode);

// システム初期化
void initializeSystem() {
    // ピンモード設定
    DDRD |= (1 << BUZZER_PIN); // ブザーピンを出力に設定

    // シリアル通信初期化（デバッグ用）
    // UART初期化コードを記述する場合、ここに追加

    // LCD初期化（仮）
    // 実際のLCD初期化コードを記述

    // 状態初期化
    currentState = STATE_WAIT;
    stateEnterTime = 0;
}

// メインループ
void mainLoop() {
    unsigned long currentTime = 0; // 現在時刻（タイマーを使用)

    switch (currentState) {
        case STATE_WAIT:
            if (checkForCard()) {
                currentState = STATE_JUDGE;
                stateEnterTime = currentTime;
            }
            break;

        case STATE_JUDGE:
            isJudgeSuccessful = validateCardData(rfidData);
            currentState = isJudgeSuccessful ? STATE_RESULT : STATE_ERROR;
            stateEnterTime = currentTime;
            break;

        case STATE_RESULT:
            updateLcdDisplay(isJudgeSuccessful ? "ACCESS GRANTED" : "ACCESS DENIED");
            setBuzzerPattern(isJudgeSuccessful);
            _delay_ms(2000);
            currentState = STATE_WAIT;
            break;

        case STATE_ERROR:
            handleSystemError(errorCode);
            if (retryCount < MAX_RETRY_COUNT) {
                retryCount++;
                currentState = STATE_WAIT;
            } else {
                currentState = STATE_WAIT;
                hasErrorOccurred = false;
                errorCode = 0;
            }
            break;

        case STATE_TIMEOUT:
            updateLcdDisplay("NO CARD");
            _delay_ms(2000);
            currentState = STATE_WAIT;
            break;
    }
}

// カード検出処理
bool checkForCard() {
    // ダミー処理（実際のRFIDライブラリを使用）
    if (true) { // 仮の条件
        isCardDetected = true;
        return true;
    }
    return false;
}

// LCDにメッセージを表示
void updateLcdDisplay(const char* message) {
    // LCD表示処理（仮）
}

// ブザーのパターンを設定
void setBuzzerPattern(bool isSuccess) {
    buzzerPattern = isSuccess ? 1 : 2;
    lastBuzzerControlTime = 0;
}

// ブザーの非ブロッキング制御
void controlBuzzerNonBlocking() {
    // 実際のブザー制御処理を記述
}

// カードデータの判定
bool validateCardData(uint8_t data[16]) {
    // 仮の判定処理
    return true;
}

// システムエラー処理
void handleSystemError(int errorCode) {
    updateLcdDisplay("ERROR");
    _delay_ms(1000);
}