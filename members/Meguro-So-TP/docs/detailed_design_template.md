# 詳細設計書 — 組込み開発実習

<!-- 作成者: あなたの名前 / 日付: YYYY-MM-DD / グループ: 〇-〇 -->

> **このドキュメントの目的**
> 基本設計書（basic_design.md）で「**どのような構造で作るか**」を決めました。
> この詳細設計書では「**各処理を具体的にどう実装するか**」を決めます。
> 書き終わったとき、**コードの骨格がほぼ完成している**状態を目指してください。

> [!NOTE]
> **V字モデルにおける位置づけ**
> 詳細設計書 ←→ **単体テスト**（関数・部品ごとのテスト）が対応します。
> 「この関数が正しく動くか」の確認は Section 5 の単体テスト仕様書で計画します。
> ※ 必須機能全体が動くかの「結合テスト」は基本設計書（Section 6）に記載します。

---

## 0. 基本設計書との接続確認

| 項目 | basic_design.md から転記 |
|:--|:--|
| 作品タイトル | ICカードで遊ぼう |
| 状態の種類（1-2 状態遷移から） | 待機 / 真偽判定 / 結果出力 / エラー / タイムアウト |
| 実装する関数の数（2-2 関数一覧から） | 12個 |
| グローバル変数の合計バイト数（2-1 SRAM確認から） | 88B |

---

## 1. グローバル変数・定数の設計

> ※ 基本設計書（2-1 データ設計）をもとに、**型と初期値まで**決めます。
> ここで設計した変数は、この後の関数設計でそのまま使います。

```
【ピン定義】（basic_design.md 3-1 から転記）
  RFID_SS_PIN      : const uint8_t = 10   // RC522 SS
  RFID_RST_PIN     : const uint8_t = 9    // RC522 RST
  RFID_MOSI_PIN    : const uint8_t = 11   // SPI MOSI
  RFID_MISO_PIN    : const uint8_t = 12   // SPI MISO
  RFID_SCK_PIN     : const uint8_t = 13   // SPI SCK
  BUZZER_PIN       : const uint8_t = 7    // Passive Buzzer
  LCD_ADDR         : const uint8_t = 0x27 // LCD1602 I2Cアドレス（機種により要確認）

【状態管理】（basic_design.md 1-2 の状態名から転記）
  enum State {
    STATE_WAIT = 0,
    STATE_JUDGE,
    STATE_RESULT,
    STATE_ERROR,
    STATE_TIMEOUT
  }
  currentState : State = STATE_WAIT

【タイマー（millis()用）】（basic_design.md 2-3 から転記）
  lastCardCheckMillis        : unsigned long = 0
  lastLcdUpdateMillis        : unsigned long = 0
  lastBuzzerControlMillis    : unsigned long = 0
  lastStateMonitorMillis     : unsigned long = 0
  errorRetryStartMillis      : unsigned long = 0
  stateEnterMillis           : unsigned long = 0
  passwordInputStartMillis   : unsigned long = 0

  CARD_CHECK_INTERVAL_MS        : const unsigned long = 200
  LCD_UPDATE_INTERVAL_MS        : const unsigned long = 500
  BUZZER_CONTROL_INTERVAL_MS    : const unsigned long = 100
  STATE_MONITOR_INTERVAL_MS     : const unsigned long = 200
  ERROR_RETRY_INTERVAL_MS       : const unsigned long = 1000
  PASSWORD_TIMEOUT_MS           : const unsigned long = 10000
  RFID_NO_CARD_TIMEOUT_MS       : const unsigned long = 1500

【センサー・入力値】（basic_design.md 2-1 から転記）
  rfidData          : byte[16] = {0}
  writeDataBuffer   : byte[16] = {0}
  displayMessage    : char[17] = ""
  passwordBuffer    : char[9]  = ""
  isICDetected      : bool = false

【その他のフラグ・カウンター】
  judgeResult       : bool = false
  errorFlag         : bool = false
  errorCode         : int = 0
  retryCount        : uint8_t = 0
  passwordFailCount : uint8_t = 0
  isLocked          : bool = false
  lockStartMillis   : unsigned long = 0
  soundPattern      : uint8_t = 0
```

---

## 2. 各関数の詳細設計

> ※ 基本設計書（2-2 関数一覧）で定義した各関数の「中身」を設計します。
> **疑似コード**（日本語＋処理の流れ）で書いてください。実際のC++コードは書かなくてOKです。

---

### `setup()` — 初期化処理

```
【処理の流れ】
1. ピンモードを設定する
   - PIN_BUTTON  → INPUT_PULLUP
   - PIN_LED_*   → OUTPUT
   - PIN_BUZZER  → OUTPUT

2. ライブラリの初期化（使うものだけ）
   - 例: lcd.begin(16, 2)
   - 例: servo.attach(PIN_SERVO)

3. Serial.begin(9600)（デバッグ用）

4. 起動確認（任意）: 緑LEDを1秒点灯して消灯
```

**↓ 自分の setup() を設計してください**
```
【処理の流れ】
1. シリアル通信を開始する（デバッグ用）。
2. ピンを初期化する（BUZZER_PIN: 出力）。
3. SPI・RFID・LCDを初期化する。
4. LCDに起動メッセージを表示する（例: SYSTEM READY）。
5. currentState を STATE_WAIT に設定し、タイマー変数を現在時刻で初期化する。
```

---

### `loop()` — メインループ

> ※ loop() は「状態ごとに何をするか」だけ書く。細かい処理は各関数に任せる。

```
【処理の流れ】

＜毎ループ実行すること＞
  - 入力を読む（readButton(), readSensor() などを呼ぶ）
  - 現在時刻を取得: now = millis()

＜currentState が 0（待機中）のとき＞
  - センサー値を監視する
  - 検知条件を満たしたら → currentState = 1

＜currentState が 1（動作中）のとき＞
  - メイン処理を行う
  - 終了条件を満たしたら → currentState = 2

＜currentState が 2（完了）のとき＞
  - 完了表示をする
  - リセットボタンが押されたら → currentState = 0

＜currentState が 3（エラー）のとき＞
  - エラー表示をする / リセットを待つ
```

**↓ 自分の loop() を設計してください**
```
【処理の流れ】
1. 現在時刻 now = millis() を取得する。
3. controlBuzzerPattern() を呼んで非ブロッキングで鳴動を進める。

＜currentState が STATE_WAIT（待機）のとき＞
- readICCard() を周期監視で実行する。
- 読み取り成功なら STATE_JUDGE へ遷移する。
- 1.5秒未検出なら STATE_TIMEOUT へ遷移する。

＜currentState が STATE_JUDGE（真偽判定）のとき＞
- judgeICData(rfidData) を実行する。
- 判定成功なら STATE_RESULT、失敗なら STATE_ERROR へ遷移する。

＜currentState が STATE_RESULT（結果出力）のとき＞
- displayToLCD() で結果を表示する。
- controlBuzzer(judgeResult) で結果音を開始する。
- 出力完了後に STATE_WAIT へ戻る。

＜currentState が STATE_ERROR（エラー）のとき＞
- handleError(errorCode) を実行する。
- リトライ条件を満たす場合は再試行、終了条件で STATE_WAIT へ戻る。

＜currentState が STATE_TIMEOUT（タイムアウト）のとき＞
- displayToLCD("NO CARD") を表示する。
- 警告音を鳴らし、一定時間後に STATE_WAIT へ戻る。
```


> ※ ボタンを使う場合は必ず設計してください。

### `readICCard()` — ICカードからデータを読み取る

**basic_design.md 2-2 との対応：** F01 ICチップ読み取り

**引数：** なし

**戻り値：** `bool`（成功: true / 失敗: false）

```
【処理の流れ】
1. カードの有無を確認する。
2. カードがある場合、データ読み取りを実行する。
3. 成功時は rfidData に格納して true を返す。
4. 失敗時は errorCode を更新して false を返す。

【エラー・異常ケース】
- カード未検出: false を返して待機継続。
```

---

### `displayToLCD(message)` — LCDへメッセージを表示する

**basic_design.md 2-2 との対応：** F02 ディスプレイ表示

**引数：** `message`（`const char*`）: 表示文字列

**戻り値：** `void`

```
【処理の流れ】
1. 表示更新周期（500ms）を満たしているか確認する。
2. 表示文字列を16文字以内に整形する。
3. LCDの表示を更新する。
4. lastLcdUpdateMillis を更新する。

【エラー・異常ケース】
- 文字列が長い場合: 末尾を切り詰めて表示する。
```

---

### `controlBuzzer(isSuccess)` — 成否に応じた鳴動パターンを設定する

**basic_design.md 2-2 との対応：** F03 ブザー音出力

**引数：** `isSuccess`（`bool`）: 判定の成否

**戻り値：** `void`

```
【処理の流れ】
1. isSuccess の値から soundPattern を選択する。
2. 鳴動開始時刻を記録する。
3. 実際のON/OFFは controlBuzzerPattern() に委譲する。

【エラー・異常ケース】
- パターン不正時: 無音パターンを設定する。
```

---

### `controlBuzzerPattern()` — ブザーを非ブロッキングで駆動する

**basic_design.md 2-2 との対応：** F04 音分岐制御

**引数：** なし

**戻り値：** `void`

```
【処理の流れ】
1. 100ms周期で処理するか判定する。
2. soundPattern に応じて BUZZER_PIN の ON/OFF を切り替える。
3. 鳴動シーケンス完了時にブザーを停止する。

【エラー・異常ケース】
- 最大鳴動時間超過: 強制停止し errorFlag を立てる。
```

---


### `judgeICData(data)` — ICデータの真偽判定を行う

**basic_design.md 2-2 との対応：** F06 判定処理

**引数：** `data`（`byte[16]`）: 判定対象データ

**戻り値：** `bool`（一致: true / 不一致: false）

```
【処理の流れ】
1. 判定対象バイト列を抽出する。
2. 登録済みの許可データと比較する。
3. 一致なら true、不一致なら false を返す。

【エラー・異常ケース】
- データ長不正: false を返し errorCode を更新する。
```

---

### `handleError(errorCode)` — エラー表示と復帰処理を行う

**basic_design.md 2-2 との対応：** F07 エラー処理

**引数：** `errorCode`（`int`）: エラー種別

**戻り値：** `void`

```
【処理の流れ】
1. エラー種別に応じた表示（READ ERR / WRITE ERR など）を行う。
2. 警告音パターンを開始する。
3. リトライ回数と経過時間を判定する。
4. 終了条件を満たしたら STATE_WAIT へ戻す。

【エラー・異常ケース】
- リトライ上限超過: 強制的に待機へ復帰する。
```

---

### （以下はテンプレ参考。上の定義を正とする）

> ※ 基本設計書 2-2 の関数一覧に記載した関数を1つずつ設計します。

---

### `関数名()` — （役割を1行で書く）

**basic_design.md 2-2 との対応：** （基本設計書の関数一覧の説明を転記）

**引数：** `引数名`（型）: 何の値か

**戻り値：** 型（なしの場合は void）

```
【処理の流れ】
1.
2.
3.

【エラー・異常ケース】
```

---

### `（参考）setup()` — ピン設定・ライブラリ初期化・起動確認

**basic_design.md 2-2 との対応：** 初期化

**引数：** なし

**戻り値：** void

```
【処理の流れ】
1. シリアル通信を開始する（デバッグ用）。
2. BUZZER_PIN のピンモードを設定。
3. SPI, RFID, LCD を初期化。
4. LCDに起動メッセージを表示。
5. currentState を STATE_WAIT に設定し、タイマー変数を初期化。

【エラー・異常ケース】
- ライブラリ初期化失敗時はエラー表示し、最小機能で継続。
```

---

### `（参考）loop()` — 状態に応じて各関数を呼び出す

**basic_design.md 2-2 との対応：** メイン制御

**引数：** なし

**戻り値：** void

```
【処理の流れ】
1. 毎ループで now = millis() を取得。
2. controlBuzzerPattern() を毎回実行。
3. 状態ごとに readICCard(), judgeICData(), displayToLCD(), controlBuzzer(), handleError() などを呼び分け。
4. 結果出力/エラー/タイムアウト後は必ず STATE_WAIT へ戻す。

【エラー・異常ケース】
- 不正な状態値は STATE_ERROR を経由して STATE_WAIT へ復帰。
```

---

### `（参考）readICCard()` — ICカードからデータを読み取る

**basic_design.md 2-2 との対応：** F01 ICチップ読み取り

**引数：** なし

**戻り値：** bool

```
【処理の流れ】
1. 新規カードの有無を確認。
2. カードがあればUIDとデータを読み取る。
3. 成功時は rfidData に格納し true、失敗時は false。

【エラー・異常ケース】
- 読み取り失敗時は errorCode を更新。
```

---

### `（参考）displayToLCD(message)` — LCDにメッセージを表示

**basic_design.md 2-2 との対応：** F02 ディスプレイ表示

**引数：** message（const char*）: 表示文字列

**戻り値：** void

```
【処理の流れ】
1. 500ms周期で表示更新。
2. 16文字以内に整形しLCDへ表示。
3. lastLcdUpdateMillis を更新。

【エラー・異常ケース】
- 長すぎる場合は切り詰めて表示。
```

---

### `（参考）controlBuzzer(isSuccess)` — 判定結果に応じて鳴動パターンを設定

**basic_design.md 2-2 との対応：** F03 ブザー音出力

**引数：** isSuccess（bool）: 判定成否

**戻り値：** void

```
【処理の流れ】
1. isSuccess で soundPattern を選択。
2. 鳴動開始時刻を記録。
3. 実際のON/OFFは controlBuzzerPattern() に委譲。

【エラー・異常ケース】
- パターン不正時は無音パターン。
```

---

### `（参考）controlBuzzerPattern()` — 非ブロッキングでブザーON/OFF

**basic_design.md 2-2 との対応：** F04 音分岐制御

**引数：** なし

**戻り値：** void

```
【処理の流れ】
1. 100ms周期で処理。
2. soundPatternごとにBUZZER_PINをON/OFF。
3. 鳴動完了時はOFFに戻す。

【エラー・異常ケース】
- 鳴動時間超過時は強制停止。
```

---

### `（参考）readRemoteInput()` — 赤外線リモコンからコマンド取得

**basic_design.md 2-2 との対応：** F05 リモコン入力取得

**引数：** なし

**戻り値：** uint16_t

```
【処理の流れ】
1. IR受信バッファに新規データがあるか確認。
2. 受信値を irCommand に格納。
3. 受信済みフラグをクリア。
4. コマンド値を返す。

【エラー・異常ケース】
- 未定義コードは0を返す。
```

---

### `（参考）judgeICData(data)` — 読み取ったICデータを判定

**basic_design.md 2-2 との対応：** F06 判定処理

**引数：** data（byte[16]）: 判定対象データ

**戻り値：** bool

```
【処理の流れ】
1. 判定対象バイト列を抽出。
2. 登録済みデータと比較。
3. 一致ならtrue、不一致ならfalse。

【エラー・異常ケース】
- データ長不正時はfalse。
```

---

### `（参考）handleError(errorCode)` — エラー表示と復帰処理

**basic_design.md 2-2 との対応：** F07 エラー処理

**引数：** errorCode（int）: エラー種別

**戻り値：** void

```
【処理の流れ】
1. errorCodeに応じた表示。
2. 警告音パターンを開始。
3. リトライ回数と経過時間を判定。
4. 終了条件でSTATE_WAITへ戻す。

【エラー・異常ケース】
- リトライ上限超過時は強制的に待機へ。
```

---

### `writeICCard(data)` — ICカードへデータを書き込む

**basic_design.md 2-2 との対応：** A01 ICチップ書き込み

**引数：** data（byte[16]）: 書き込みデータ

**戻り値：** bool

```
【処理の流れ】
1. 書き込み対象カードの有無を確認。
2. 認証後、指定ブロックへ書き込む。
3. 成功時はtrue、失敗時は再試行後にfalse。

【エラー・異常ケース】
- 認証失敗や書き込み失敗時はerrorCodeを更新。
```

---

### `inputPassword()` — リモコンでパスワード入力

**basic_design.md 2-2 との対応：** A02 パスワード入力

**引数：** なし

**戻り値：** char[9]

```
【処理の流れ】
1. リモコン数値入力をpasswordBufferに格納。
2. 決定キーまたは8文字到達で確定。
3. passwordBufferを返す。

【エラー・異常ケース】
- 10秒間入力がなければ空文字で確定。
```

---

### `playWithRemoteAndLCD()` — リモコンとLCDで遊ぶ

**basic_design.md 2-2 との対応：** A03 遊び機能

**引数：** なし

**戻り値：** void

```
【処理の流れ】
1. リモコンコマンドで遊びモードを判定。
2. LCDにスクロールや演出を表示。
3. 終了コマンドで待機表示に戻す。

【エラー・異常ケース】
- 無効コマンドは無視。
```

---

## 3. 重要ロジックの詳細設計

---

### 3-1. millis() を使ったタイマー管理

```
【考え方】
  「前回実行した時刻」を記録しておき、「今の時刻 − 前回時刻 ≥ 周期」なら実行する。

【処理の流れ（例: LED点滅）】
  1. now = millis()
  2. now - lastMillis_LED >= LED_INTERVAL かどうか確認
  3. 条件を満たした場合: LEDのON/OFFを切り替え、lastMillis_LED = now
  4. 条件を満たさない場合: 何もしない（次のループで再チェック）

【自分のシステムで millis() を使う処理】
  （basic_design.md 2-3 のタイミング設計から転記して具体化する）
```

---

### 3-2. その他の重要ロジック（任意）

> **【任意】** 複雑なロジックがある場合のみ記入してください。
> 例：「距離に応じたLED点灯パターン」「ゲームの衝突判定」「温度の閾値判定」

```
【ロジック1: 状態遷移ロジック】
【処理の流れ】
1. loop() で currentState を確認する。
2. currentState に応じて loop() 内の状態分岐で処理を実行する。
3. 各状態関数は次状態を決定し、遷移後は必ず復帰可能な経路（最終的に STATE_WAIT）を保証する。

【ロジック2: タイムアウト制御】
【処理の流れ】
1. 状態に入った時刻を記録する（例: stateEnterMillis）。
2. now - stateEnterMillis を監視し、閾値（RFID未検出1.5秒等）を超えたらタイムアウト判定する。
3. タイムアウト時は STATE_TIMEOUT へ遷移し、表示・警告音後に STATE_WAIT へ戻す。

【ロジック3: millisベースの周期実行】
【処理の流れ】
1. 処理ごとに前回実行時刻を保持する。
2. now - lastXxxMillis >= interval のときだけ対象処理を実行する。
3. 実行後に lastXxxMillis = now を更新し、delay を使わず並行処理を維持する。

【ロジック4: ブザー音分岐パターン制御】
【処理の流れ】
1. 判定結果や異常種別から soundPattern（成功音/失敗音/警告音）を決める。
2. controlBuzzerPattern() で100ms周期のON/OFFシーケンスを進める。
3. 鳴動終了または最大鳴動時間超過時は BUZZER_PIN をLOWにして停止する。

【ロジック5: RFID読み取り/書き込みのリトライ制御】
【処理の流れ】
1. RFID処理失敗時に retryCount をインクリメントする。
2. retryCount が上限未満なら再試行し、上限到達ならエラーへ遷移する。
3. エラー遷移時はエラー表示と警告音を行い、必要ログを残す。

【ロジック6: エラー処理と安全復帰】
【処理の流れ】
1. errorCode に対応したメッセージ（READ ERR / WRITE ERR / SYSTEM ERR）を表示する。
2. 警告音を鳴らし、再試行可否または強制復帰を判定する。
3. いずれのエラーでも最終的に STATE_WAIT に戻してハングを防止する。

【ロジック7: パスワード入力/認証ロジック】
【処理の流れ】
1. リモコン入力を passwordBuffer に格納し、確定キーまたは桁数到達で照合する。
2. 認証失敗時は passwordFailCount を加算し、残回数を表示する。
3. 上限回数超過で30秒ロックし、経過後に入力受付を再開する。
```

---

## 4. デバッグ出力計画（任意）

> **【任意】** 関数設計（Section 2）と並行して記入すると効果的です。
> 「動かない」ときに何を確認すればいいかを事前に計画しておきます。
> 実装後は不要な Serial.println() を削除すること。

| No | 確認したい内容 | 挿入する関数 | Serial.println の内容例 |
|:---|:---|:---|:---|
| 1 | RFIDカードを検出できているか | readICCard() | Serial.println("RFID detected=" + String(isICDetected)); |
| 2 | 読み取りデータが取得できているか | readICCard() | Serial.println("UID/Data read ok"); |
| 3 | 状態遷移が正しく起きているか | loop() | Serial.println("state=" + String(currentState)); |
| 4 | 判定結果が想定通りか | judgeICData() | Serial.println("judgeResult=" + String(judgeResult)); |
| 5 | タイムアウト判定が正しく動くか | loop() | Serial.println("elapsed=" + String(now - stateEnterMillis)); |
| 6 | ブザーが停止漏れなく制御されるか | controlBuzzerPattern() | Serial.println("pattern=" + String(soundPattern)); |
| 7 | リモコン入力が受信できているか | readRemoteInput() | Serial.println("irCommand=" + String(irCommand, HEX)); |
| 8 | エラー発生時の復帰動作が正しいか | handleError() | Serial.println("errorCode=" + String(errorCode)); |

---

## 5. 単体テスト仕様書（V字モデル：詳細設計 ↔ 単体テスト）

> ※ 各関数・部品が「単体で正しく動くか」を確認するテスト項目を設計します。
> 「実際の結果」欄は実装後に記入します。

### 5-1. 入力系テスト

| No | テスト対象の関数 | 入力・操作 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | readICCard() | RFIDカードをかざす | true を返し、rfidData が更新される | | [ ] |
| 2 | readICCard() | カードをかざさない（1.5秒） | false を返し、タイムアウト遷移条件を満たす | | [ ] |
| 3 | readRemoteInput() | 定義済みリモコンキーを押す | irCommand に対応コードが格納される | | [ ] |
| 4 | readRemoteInput() | 未定義リモコンキーを押す | 0 を返す、または状態変更しない | | [ ] |
| 5 | inputPassword() | 正しい8桁を入力して確定する | passwordBuffer が期待値と一致する | | [ ] |
| 6 | inputPassword() | 誤ったパスワードを入力する | 不一致として失敗回数が加算される | | [ ] |
| 7 | inputPassword() | 入力途中で10秒操作しない | タイムアウト扱いで入力が確定/破棄される | | [ ] |

### 5-2. 出力系テスト

| No | テスト対象の関数 | 入力・操作 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | displayToLCD() | "OK" を渡す | LCDに OK が正しく表示される | | [ ] |
| 2 | displayToLCD() | 16文字超の文字列を渡す | 16文字以内に整形されて表示される | | [ ] |
| 3 | controlBuzzer(true) | 成功判定を渡す | 成功音パターンで鳴動し、規定時間で停止する | | [ ] |
| 4 | controlBuzzer(false) | 失敗判定を渡す | 警告/失敗音パターンで鳴動し、規定時間で停止する | | [ ] |
| 5 | handleError(errorCode) | READ ERR を発生させる | エラー表示と警告音が出力され、待機へ復帰する | | [ ] |
| 6 | playWithRemoteAndLCD() | 遊び機能開始コマンドを送る | LCD演出が表示され、終了コマンドで待機表示へ戻る | | [ ] |

### 5-3. タイミング・並行動作テスト

| No | テスト内容 | テスト手順 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | 非ブロッキング動作確認 | ブザー鳴動中にリモコン入力を行う | 入力が取りこぼされず受信される | | [ ] |
| 2 | RFID監視周期の確認 | カードの出し入れを繰り返す | 約200ms周期で検出判定が行われる | | [ ] |
| 3 | LCD更新周期の確認 | 表示内容を連続更新させる | 約500ms周期で更新される | | [ ] |
| 4 | RFID未検出タイムアウト | カードをかざさず待機する | 1.5秒後に NO CARD 表示と警告音が出る | | [ ] |
| 5 | パスワード入力タイムアウト | 途中入力後に10秒放置する | 入力がタイムアウト処理される | | [ ] |
| 6 | ブザー最大鳴動時間制御 | 異常系を連続発生させる | 最大鳴動時間で必ず停止する | | [ ] |

---

## 6. AIレビュー記録

> グループレビューの前に必ず実施してください。

### Q1: 実装上の問題確認

> 「この詳細設計書に書いた関数と処理フローをもとに Arduino でコードを書きます。バグになりやすい箇所・処理の抜け・型の問題はありますか？」

**AIの回答（要約）：**

- 状態遷移（必ずSTATE_WAITに戻る経路）とエラー復帰の抜けに注意。
- millis()タイマー変数の初期化・型（unsigned long）・オーバーフロー対策。
- 配列長・char配列終端・グローバル変数型の不一致。
- delay()禁止、非ブロッキング制御の徹底。
- エラー処理・リトライ条件の曖昧さ。
- テスト仕様にない境界値・異常系の追加テスト推奨。

**対応した内容：**

---

### Q2: 単体テスト仕様の確認

> 「Section 5 の単体テスト仕様書で、各関数の動作が正しく検証できていますか？テストが不足している項目や、境界値テストが必要な箇所を教えてください。」

**AIの回答（要約）：**
配列長・バッファオーバー（passwordBuffer, displayMessage等の最大長超過時の挙動）
タイムアウトやリトライ上限など異常系・境界値のテスト
グローバル変数の初期化漏れや型不一致時の動作
エラーコードやフラグの異常値（未定義値や異常遷移時の復帰）
LCDやブザー等の物理的エラー時の挙動（可能なら

**対応した内容：**

---

## 7. グループレビュー記録

### 7-1. 指摘一覧

| No | 指摘内容 | 指摘者 | 対応 |
|:---|:---|:---|:---|
| 1 |  |  |  |
| 2 |  |  |  |
| 3 |  |  |  |

### 7-2. レビューを受けて変更した点

-
-

---

*初版: YYYY-MM-DD / AIレビュー: YYYY-MM-DD / グループレビュー後更新: YYYY-MM-DD*
