# 詳細設計書 — 組込み開発実習

<!-- 作成者: リュ　ジェソク / 日付: 2026-05-25 / グループ: 1-K -->

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
| 作品タイトル | 寝坊防止アラート |
| 状態の種類（1-2 状態遷移から） | 4種類（時刻設定待ち / 待機中 / 出題中 / 入力エラー） |
| 実装する関数の数（2-2 関数一覧から） | 16個 |
| グローバル変数の合計バイト数（2-1 SRAM確認から） | 約34B |

---

## 1. グローバル変数・定数の設計

> ※ 基本設計書（2-1 データ設計）をもとに、**型と初期値まで**決めます。
> ここで設計した変数は、この後の関数設計でそのまま使います。

```
【ピン定義】（basic_design.md 3-1 から転記）
  PIN_IR        : const byte = 2    // IR受信モジュール
  PIN_BUZZER    : const byte = 3    // パッシブブザー（tone）
  PIN_LED_R     : const byte = 5    // RGB LED R
  PIN_LED_G     : const byte = 6    // RGB LED G
  PIN_LED_B     : const byte = 9    // RGB LED B
  PIN_LCD_SDA   : const byte = A4   // I2C SDA
  PIN_LCD_SCL   : const byte = A5   // I2C SCL

【状態管理】（basic_design.md 1-2 の状態名から転記）
  currentState  : byte = 0  // 0:時刻設定待ち 1:待機中 2:出題中 3:入力エラー

【タイマー（millis()用）】（basic_design.md 2-3 から転記）
  lastClockMillis   : unsigned long = 0   // 60000msごとに現在時刻+1分
  lastBlinkMillis   : unsigned long = 0   // LED点滅更新
  lastBeepMillis    : unsigned long = 0   // ブザーON/OFF更新
  lastLcdMillis     : unsigned long = 0   // LCD再描画更新
  lastIrReceiveMs   : unsigned long = 0   // IR重複入力抑制
  lastPatternMs     : unsigned long = 0   // 音/光パターン切替

【センサー・入力値】（basic_design.md 2-1 から転記）
  nowHH, nowMM      : byte = 0, 0
  alarmHH, alarmMM  : byte = 7, 0
  irCode            : unsigned long = 0
  inputDigits[4]    : char = {'0','0','0','0'}
  inputIndex        : byte = 0
  quizIndex         : byte = 0
  errorType         : byte = 0   // 0なし/1範囲外/2受信失敗/3不正キー

【その他のフラグ・カウンター】
  isAlarmRinging    : bool = false
  toneMode          : byte = 0
  ledMode           : byte = 0
  isAlarmInputPhase : bool = false
  DEBOUNCE_DELAY_MS : const unsigned long = 50
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
1. ピンモードを設定する
  - PIN_IR は入力
  - PIN_BUZZER, PIN_LED_R, PIN_LED_G, PIN_LED_B は出力

2. ライブラリを初期化する
  - IR受信開始（受信再開を有効化）
  - LCD初期化・バックライトON

3. 初期状態を設定する
  - currentState = 0（時刻設定待ち）
  - nowHH/nowMM, inputIndex, errorType を初期化
  - lastClockMillis = millis()

4. 起動確認を行う
  - RGBを青で300ms点灯
  - 完了後に消灯

5. デバッグ用シリアルを開始する
  - Serial.begin(9600)
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

＜毎ループ実行すること＞
  - now = millis() を取得
  - irCode = readRemoteCode() を呼ぶ
  - updateClockByMillis() を呼ぶ
  - updateLcdView(currentState) を呼ぶ
  - updateAlarmOutputs(currentState) を呼ぶ

＜currentState が 0（時刻設定待ち）のとき＞
  - handleTimeInput(false) で現在時刻入力を受け付ける
  - 成功したら handleTimeInput(true) でアラーム時刻入力へ進む
  - 両方完了したら currentState = 1（待機中）
  - 受信失敗/不正入力があれば errorType を設定し currentState = 3

＜currentState が 1（待機中）のとき＞
  - checkAlarmTrigger() で時刻一致を判定
  - 一致したら startAlarmSequence() を1回実行して currentState = 2
  - 一致しない間はLED緑点滅と時刻表示を維持

＜currentState が 2（出題中）のとき＞
  - runQuizStep() で問題表示と回答入力を実行
  - 入力が来たら judgeQuizAnswer(irCode) で正誤判定
  - 正解ならアラーム停止して currentState = 1（待機中）
  - 不正キー/受信失敗は errorType 設定して currentState = 3

＜currentState が 3（入力エラー）のとき＞
  - playErrorBeep(errorType) を実行
  - LCDへエラー理由と再入力案内を表示
  - currentState = 0（時刻設定待ち）へ戻す

```

---

### `readRemoteCode()` — IR受信コードを読み取り、重複入力を抑制する

**basic_design.md 2-2 との対応：** （共通）IR入力処理

**引数：** なし

**戻り値：** `unsigned long`（受信キーコード。受信なしは0）

```
【処理の流れ】
1. IR受信データがあるか確認する
2. 受信成功ならキーコードを取り出し、受信再開を呼ぶ
3. now - lastIrReceiveMs < 50ms のときは同一入力として無視し0を返す
4. 有効入力なら lastIrReceiveMs を更新してキーコードを返す

【エラー・異常ケース】
- デコード失敗: errorType=2 を設定し 0 を返す
```

---

### `updateClockByMillis()` — 1分周期で現在時刻を進める

**basic_design.md 2-2 との対応：** （共通）時刻進行

**引数：** なし

**戻り値：** `void`

```
【処理の流れ】
1. now = millis() を取得する
2. now - lastClockMillis >= 60000 を満たすか確認する
3. 満たす間は nowMM を+1し、60になったら0へ戻して nowHH を+1
4. nowHH が24になったら0へ戻す
5. lastClockMillis += 60000 で基準時刻を進める

【エラー・異常ケース】
- lastClockMillis の遅延蓄積: while で複数分を消化して時刻ずれを最小化
```

---

### `updateLcdView(state)` — 状態に応じてLCD表示を更新する

**basic_design.md 2-2 との対応：** （共通）表示更新

**引数：** `state`（byte）: 現在状態

**戻り値：** `void`

```
【処理の流れ】
1. now - lastLcdMillis >= 150ms のときだけ更新する
2. state=0: 時刻入力ガイド（HHMM）を表示
3. state=1: 現在時刻とアラーム時刻を表示
4. state=2: 問題文と入力中回答を表示
5. state=3: errorTypeに対応したエラーメッセージを表示
6. lastLcdMillis = now を更新する

【エラー・異常ケース】
- 表示文字列が長すぎる場合: 16文字に切り詰めて表示
```

---

### `updateAlarmOutputs(state)` — 状態に応じてRGB LEDとブザーを制御する

**basic_design.md 2-2 との対応：** （共通）出力更新

**引数：** `state`（byte）: 現在状態

**戻り値：** `void`

```
【処理の流れ】
1. state=0: RGBを青固定、ブザー停止
2. state=1: 500ms周期で緑点滅、ブザー停止
3. state=2: mixAlarmTonePatterns(now) と mixLedFlashPatterns(now) を呼ぶ
4. state=3: 黄点滅（赤+緑）を短周期で実行

【エラー・異常ケース】
- stateが範囲外: 安全側として全出力OFFにする
```

---

### `handleTimeInput(isAlarmInput)` — HHMM入力を受け付けて妥当性を確認する

**basic_design.md 2-2 との対応：** F01 現在時刻・アラーム時刻入力

**引数：** `isAlarmInput`（bool）: false=現在時刻、true=アラーム時刻

**戻り値：** `bool`（4桁入力と妥当性確認が完了したらtrue）

```
【処理の流れ】
1. irCode が数字キーなら inputDigits[inputIndex] に格納し inputIndex++
2. 取消キーなら入力をクリアし inputIndex=0
3. 4桁そろったら HH/MM に変換して範囲確認（HH<=23, MM<=59）
4. 妥当なら対象（nowHH/MM または alarmHH/MM）へ反映し inputIndex=0 で true
5. 不正なら errorType=1, inputIndex=0, currentState=3 にして false

【エラー・異常ケース】
- 数字以外のキー: errorType=3 を設定して false
```

---

### `checkAlarmTrigger()` — 現在時刻とアラーム時刻の一致を判定する

**basic_design.md 2-2 との対応：** F02 毎日繰り返しアラーム

**引数：** なし

**戻り値：** `bool`（一致時true）

```
【処理の流れ】
1. nowHH==alarmHH かつ nowMM==alarmMM を確認
2. 一致時は true を返す
3. 非一致時は false を返す

【エラー・異常ケース】
- 時刻値が範囲外のとき: false を返し errorType=1 を設定
```

---

### `startAlarmSequence()` — 出題開始時の初期化を行う

**basic_design.md 2-2 との対応：** F03 時刻到達時の同時出力開始

**引数：** なし

**戻り値：** `void`

```
【処理の流れ】
1. isAlarmRinging=true、quizIndex=0、inputIndex=0 を設定
2. toneMode と ledMode を初期モードに設定
3. lastBeepMillis, lastBlinkMillis, lastPatternMs を現在時刻へ更新
4. LCDへ最初の問題文を表示する

【エラー・異常ケース】
- 問題データ未設定: errorType=2 を設定し currentState=3 に遷移
```

---

### `runQuizStep()` — 問題表示と回答入力の進行を管理する

**basic_design.md 2-2 との対応：** F04 問題表示と回答入力

**引数：** なし

**戻り値：** `void`

```
【処理の流れ】
1. quizIndex に対応した問題文をLCDへ表示する
2. irCode が数字キーなら回答バッファへ追加する
3. 回答確定キー受信時に judgeQuizAnswer(irCode) を呼ぶ
4. 正解なら次状態へ遷移、誤答なら入力をリセットして再入力待ち

【エラー・異常ケース】
- 許可外キー: errorType=3 を設定し currentState=3
```

---

### `judgeQuizAnswer(keyCode)` — 回答の正誤を判定する

**basic_design.md 2-2 との対応：** F05 正解時のみ解除

**引数：** `keyCode`（unsigned long）: 確定入力キーコード

**戻り値：** `bool`（正解ならtrue）

```
【処理の流れ】
1. 入力バッファを数値へ変換し、現在問題の正解と比較する
2. 一致ならブザー停止、成功音2回、isAlarmRinging=false
3. currentState=1（待機中）へ戻して true を返す
4. 不一致ならアラーム継続で false を返す

【エラー・異常ケース】
- バッファ桁不足: 判定せず false を返す
```

---

### `playErrorBeep(errorType)` — エラー種別に応じて短い警告音を鳴らす

**basic_design.md 2-2 との対応：** F06 エラー種別ごとの警告音

**引数：** `errorType`（byte）: 1範囲外/2受信失敗/3不正キー

**戻り値：** `void`

```
【処理の流れ】
1. errorTypeに対応する周波数と回数を決定する
2. 100msの短音を所定回数だけ鳴らす（ノンブロッキング）
3. 再生後に noTone(PIN_BUZZER) を実行する
4. errorType を 0 に戻す

【エラー・異常ケース】
- 未定義 errorType: 既定音1回のみ鳴らして終了
```

---

### `setAlarmToneMode(mode)` — アラーム音パターンを選択する

**basic_design.md 2-2 との対応：** A01 ブザー音色切替

**引数：** `mode`（byte）: 音パターン番号

**戻り値：** `void`

```
【処理の流れ】
1. modeに応じて音階テーブルを選択する
2. toneModeへ保存する
3. 再生開始位置を0へ初期化する

【エラー・異常ケース】
- 不正mode: デフォルト（通常）へフォールバック
```

---

### `setLedColorMode(colorMode)` — RGB発光テーマを設定する

**basic_design.md 2-2 との対応：** A02 RGB色カスタム

**引数：** `colorMode`（byte）: 色テーマ番号

**戻り値：** `void`

```
【処理の流れ】
1. colorModeに対応する色セットを選ぶ
2. ledModeへ保存する
3. 現在状態に応じた初期色を適用する

【エラー・異常ケース】
- 不正colorMode: 既定テーマへ戻す
```

---

### `mixAlarmTonePatterns(nowMs)` — 複数の音パターンを周期切替で再生する

**basic_design.md 2-2 との対応：** A03 アラーム音混在再生

**引数：** `nowMs`（unsigned long）: 現在時刻

**戻り値：** `void`

```
【処理の流れ】
1. nowMs - lastPatternMs が切替周期（2000〜4000ms）を超えたか確認
2. 超えたら toneMode を次のモードへ循環更新する
3. modeごとの短周期ON/OFF（200ms）で tone() を実行する
4. 切替時刻と再生インデックスを更新する

【エラー・異常ケース】
- 鳴動停止中に呼ばれた場合: noToneして即終了
```

---

### `mixLedFlashPatterns(nowMs)` — 複数の発光パターンを循環させる

**basic_design.md 2-2 との対応：** A04 発光パターン混在

**引数：** `nowMs`（unsigned long）: 現在時刻

**戻り値：** `void`

```
【処理の流れ】
1. nowMs - lastBlinkMillis で点灯/消灯更新タイミングを判定する
2. パターンA: 全色同時点滅（500ms）
3. パターンB: 赤→青→緑の交互点灯（300ms）
4. パターンC: 赤青交互の高速点滅（150ms）
5. nowMs - lastPatternMs がしきい値超過で次パターンへ循環する

【エラー・異常ケース】
- ledModeが範囲外: 全消灯して既定モードへ戻す
```

---

## 3. 重要ロジックの詳細設計

### 3-1. チャタリング防止（デバウンス処理）

> ※ ボタンを使う場合は必ず設計してください。

```
【考え方】
  IRリモコンの長押し/連打で同じコードが短時間に連続受信されるため、50ms以内は同一入力として無視する。

【処理の流れ】
  1. IR受信でキーコードを取得する
  2. 前回受信時刻（lastIrReceiveMs）との差を計算する
  3. 経過時間 < DEBOUNCE_DELAY_MS（50ms）なら無視する
  4. 経過時間 >= DEBOUNCE_DELAY_MS なら有効入力として確定する
  5. lastIrReceiveMs を現在時刻で更新する

【必要な変数（Section 1 に追加済みか確認）】
  lastIrReceiveMs   : unsigned long       // 前回受信した時刻
  DEBOUNCE_DELAY_MS : const unsigned long = 50
```

---

### 3-2. millis() を使ったタイマー管理

```
【考え方】
  「前回実行した時刻」を記録しておき、「今の時刻 − 前回時刻 ≥ 周期」なら実行する。

【処理の流れ（例: LED点滅）】
  1. now = millis()
  2. now - lastMillis_LED >= LED_INTERVAL かどうか確認
  3. 条件を満たした場合: LEDのON/OFFを切り替え、lastMillis_LED = now
  4. 条件を満たさない場合: 何もしない（次のループで再チェック）

【自分のシステムで millis() を使う処理】
  - IR入力監視: 約20ms周期で受信確認（入力遅延を抑える）
  - アラーム鳴動ON/OFF: 200ms周期で切替（入力受付を止めない）
  - 音パターン切替: 2000〜4000ms周期で切替（慣れ防止）
  - RGB発光パターン切替: 500〜1500ms周期（視覚刺激維持）
  - 現在時刻進行: 60000ms周期（1分進行）
  - LCD再描画: 100〜200ms周期（表示追従性確保）
```

---

### 3-3. その他の重要ロジック（任意）

> **【任意】** 複雑なロジックがある場合のみ記入してください。
> 例：「距離に応じたLED点灯パターン」「ゲームの衝突判定」「温度の閾値判定」

```
【処理の流れ】
1. 出題開始時に quizIndex=0 を設定し、問題文をLCDに表示する。
2. リモコン入力を回答バッファへ蓄積し、確定キーで採点する。
3. 不正解なら次入力へ戻し、正解なら停止音を2回鳴らして待機中へ遷移する。

【入力値と出力値の関係】
  - 入力: IRキーコード（数字、確定、取消）
  - 中間: 回答バッファ（char配列）
  - 出力: 正解時はアラーム停止、誤答時はアラーム継続
```

---

## 4. デバッグ出力計画（任意）

> **【任意】** 関数設計（Section 2）と並行して記入すると効果的です。
> 「動かない」ときに何を確認すればいいかを事前に計画しておきます。
> 実装後は不要な Serial.println() を削除すること。

| No | 確認したい内容 | 挿入する関数 | Serial.println の内容例 |
|:---|:---|:---|:---|
| 1 | IRコードが正しく受信できているか | `readRemoteCode()` | `Serial.println(irCode, HEX);` |
| 2 | 状態遷移が正しく起きているか | `loop()` | `Serial.println(currentState);` |
| 3 | 1分進行ロジックが正しいか | `updateClockByMillis()` | `Serial.println(String(nowHH)+":"+String(nowMM));` |
| 4 | 問題の正誤判定が正しいか | `judgeQuizAnswer()` | `Serial.println("quiz result=" + String(isCorrect));` |

---

## 5. 単体テスト仕様書（V字モデル：詳細設計 ↔ 単体テスト）

> ※ 各関数・部品が「単体で正しく動くか」を確認するテスト項目を設計します。
> 「実際の結果」欄は実装後に記入します。

### 5-1. 入力系テスト

| No | テスト対象の関数 | 入力・操作 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | readRemoteCode() | 数字キー「1」を1回押す | 対応するキーコードを返す | | [ ] |
| 2 | readRemoteCode() | 50ms以内に同じキーを2回押す | 1回分のみ有効入力になる | | [ ] |
| 3 | handleTimeInput(false) | 「2359」を入力して確定 | trueを返し nowHH=23, nowMM=59 になる | | [ ] |
| 4 | handleTimeInput(true) | 「2460」を入力して確定 | falseを返し errorType=1 になる | | [ ] |
| 5 | judgeQuizAnswer() | 誤答→正答の順で入力 | 誤答ではfalse、正答でtrueになる | | [ ] |

### 5-2. 出力系テスト

| No | テスト対象の関数 | 入力・操作 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | updateAlarmOutputs(0) | state=0（時刻設定待ち）を渡す | RGB青固定、ブザー停止 | | [ ] |
| 2 | updateAlarmOutputs(1) | state=1（待機中）を渡す | 緑LEDがゆっくり点滅する | | [ ] |
| 3 | mixAlarmTonePatterns() | 出題中に60秒動作 | 音パターンが2種類以上切り替わる | | [ ] |
| 4 | mixLedFlashPatterns() | 出題中に60秒動作 | 発光パターンが複数切り替わる | | [ ] |

### 5-3. タイミング・並行動作テスト

| No | テスト内容 | テスト手順 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | delay()による処理停止がないか | 出題中に連続でリモコン入力する | 入力受付が止まらず応答する | | [ ] |
| 2 | millis()タイマーの周期精度 | 待機時のLED点滅を計測する | 約500ms周期で点滅する | | [ ] |
| 3 | 時刻進行の正しさ | 10分相当の経過を観測する | nowMMが10分進行する | | [ ] |

---

## 6. AIレビュー記録

> グループレビューの前に必ず実施してください。

### Q1: 実装上の問題確認

> 「この詳細設計書に書いた関数と処理フローをもとに Arduino でコードを書きます。バグになりやすい箇所・処理の抜け・型の問題はありますか？」

**AIの回答（要約）：**
- `char inputDigits[4]` は文字列終端が不要な設計で問題ないが、表示時は固定長で扱う必要がある。
- `updateClockByMillis()` で `lastClockMillis = now` とすると遅延時に誤差が蓄積するため、加算更新が安全。
- エラー状態からの復帰先が明確でないと行き止まりになるため、`currentState=0` への復帰を固定すると良い。
- IR受信失敗時に常時エラー遷移すると操作不能になりうるため、タイムアウト条件を設けるべき。

**対応した内容：**
- LCD表示は固定長4桁入力として実装する方針を明記。
- 時刻更新は `lastClockMillis += 60000` の方式を採用。
- 入力エラー状態は警告音後に必ず時刻設定待ちへ戻す仕様に統一。
- `readRemoteCode()` でデコード失敗と未受信を分離して扱うようにした。

---

### Q2: 単体テスト仕様の確認

> 「Section 5 の単体テスト仕様書で、各関数の動作が正しく検証できていますか？テストが不足している項目や、境界値テストが必要な箇所を教えてください。」

**AIの回答（要約）：**
- 単体テストは正常系が中心なので、境界値（00:00、23:59、24:00、12:60）を追加すると妥当性が高い。
- `judgeQuizAnswer()` は「桁不足で確定キー」のケースを追加すると入力欠陥を検出しやすい。
- 出力系は「鳴動停止後に noTone されるか」を確認する項目が有効。

**対応した内容：**
- 入力系テストに範囲外時刻入力（2460）を追加。
- 判定系テストに誤答→正答の遷移テストを追加。
- 出力系テストに音/光パターン混在の長時間確認項目を追加。

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

*初版: 2026-05-25 / AIレビュー: 2026-05-25 / グループレビュー後更新: YYYY-MM-DD*
