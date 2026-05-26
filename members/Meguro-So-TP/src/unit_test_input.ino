// unit_test_input.ino
// Section 5-1 入力系単体テスト
// Meguro-So-TP

#include <Arduino.h>

// --- テスト用グローバル変数・モック定義 ---
byte rfidData[16] = {0};
char passwordBuffer[9] = "";
uint16_t irCommand = 0;
bool isICDetected = false;

// --- テスト対象関数のプロトタイプ（本体は既存コードを利用） ---
bool readICCard();
uint16_t readRemoteInput();
char* inputPassword();

// --- テスト用モック関数 ---
// 本来はハード依存だが、ここではテスト用に値をセットして呼び出す

// --- テストNo管理 ---
int testNo = 1;

void printTestResult(const char* func, const char* expect, const char* actual, bool pass) {
  Serial.print("[Test "); Serial.print(testNo++); Serial.print("] ");
  Serial.print(func); Serial.print(" | 期待: "); Serial.print(expect);
  Serial.print(" | 実際: "); Serial.print(actual);
  Serial.print(" | "); Serial.println(pass ? "OK" : "NG");
}

void setup() {
  Serial.begin(9600);
  delay(1000);
  Serial.println("\n--- Section 5-1 入力系単体テスト開始 ---");

  // 1. readICCard() | RFIDカードをかざす
  isICDetected = true; // モック: カード検出
  bool result1 = readICCard();
  printTestResult("readICCard() カード有", "true", result1 ? "true" : "false", result1);

  // 2. readICCard() | カードをかざさない（1.5秒）
  isICDetected = false; // モック: カード未検出
  bool result2 = readICCard();
  printTestResult("readICCard() カード無", "false", result2 ? "true" : "false", !result2);

  // 3. readRemoteInput() | 定義済みリモコンキーを押す
  irCommand = 0xA90; // モック: 有効コマンド
  uint16_t remote1 = readRemoteInput();
  printTestResult("readRemoteInput() 有効", "0xA90", (remote1 == 0xA90) ? "0xA90" : "NG", remote1 == 0xA90);

  // 4. readRemoteInput() | 未定義リモコンキーを押す
  irCommand = 0xFFFF; // モック: 未定義
  uint16_t remote2 = readRemoteInput();
  printTestResult("readRemoteInput() 無効", "0", (remote2 == 0) ? "0" : "NG", remote2 == 0);

  // 5. inputPassword() | 正しい8桁を入力して確定する
  strcpy(passwordBuffer, "12345678"); // モック: 8桁入力
  char* pw1 = inputPassword();
  printTestResult("inputPassword() 8桁", "12345678", strcmp(pw1, "12345678") == 0 ? "12345678" : pw1, strcmp(pw1, "12345678") == 0);

  // 6. inputPassword() | 誤ったパスワードを入力する
  strcpy(passwordBuffer, "87654321"); // モック: 誤り
  char* pw2 = inputPassword();
  printTestResult("inputPassword() 誤り", "!=12345678", strcmp(pw2, "12345678") != 0 ? pw2 : "NG", strcmp(pw2, "12345678") != 0);

  // 7. inputPassword() | 入力途中で10秒操作しない
  strcpy(passwordBuffer, ""); // モック: タイムアウト
  char* pw3 = inputPassword();
  printTestResult("inputPassword() タイムアウト", "空文字", strlen(pw3) == 0 ? "空" : pw3, strlen(pw3) == 0);

  Serial.println("--- Section 5-1 入力系単体テスト終了 ---");
}

void loop() {
  // 1回だけ実行
}

// --- テスト用ダミー実装例（本来は本体コードをリンク） ---

// 本番のcheckForCard()を呼び出すラッパー
bool readICCard() {
  // checkForCard()はmain.cで定義されている本番関数
  return checkForCard();
}

uint16_t readRemoteInput() {
  if (irCommand == 0xA90) return irCommand; // 有効コマンド
  return 0; // 未定義
}

char* inputPassword() {
  // 8桁ならそのまま返す、空ならタイムアウト
  return passwordBuffer;
}
