# 詳細設計書 — 組込み開発実習

<!-- 作成者: 富樫輝 / 日付: 2026-05-22 / グループ: 1-K -->

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
| 作品タイトル | アナログスティック式電卓 |
| 状態の種類（1-2 状態遷移から） | 待機中、入力中、計算中、結果表示中、エラー中、リセット中 |
| 実装する関数の数（2-2 関数一覧から） | 32個 |
| グローバル変数の合計バイト数（2-1 SRAM確認から） | 65B |

---

## 1. グローバル変数・定数の設計

> ※ 基本設計書（2-1 データ設計）をもとに、**型と初期値まで**決めます。
> ここで設計した変数は、この後の関数設計でそのまま使います。

```
【ピン定義】（basic_design.md 3-1 から転記）
  PIN_STICK_X   = A0   // アナログスティックX軸
  PIN_STICK_Y   = A1   // アナログスティックY軸
  PIN_STICK_SW  = 2    // スティック押込みSW
  PIN_BTN_OP    = 3    // 演算子切替ボタン
  PIN_BTN_CONFIRM = 4  // 入力確定ボタン
  PIN_BTN_CALC  = 5    // 計算実行ボタン
  PIN_BTN_RESET = 6    // リセットボタン
  PIN_BTN_CANCEL = 7   // 入力取消ボタン
  PIN_BUZZER    = 8    // パッシブブザー

【状態管理】
  currentState    : uint8_t = 0   // 現在の状態（待機/入力/計算/結果/エラー/リセット）
  prevState       : uint8_t = 0   // 1つ前の状態

【計算用データ】
  operand1        : long = 0      // 第1オペランド
  operand2        : long = 0      // 第2オペランド
  calcResult      : long = 0      // 計算結果
  inputBuffer     : long = 0      // 入力中の数値バッファ
  inputDigits     : uint8_t = 0   // 入力中の桁数
  digitRangeMode  : uint8_t = 0   // 数字レンジ（0:0〜4, 1:5〜9）
  currentOperator : char = '+'    // 現在の演算子
  hasOperand1     : bool = false  // 第1オペランド確定済みか
  hasOperand2     : bool = false  // 第2オペランド確定済みか
  hasResult       : bool = false  // 結果表示中か
  errorCode       : uint8_t = 0   // エラー種別（0:なし, 1:0除算, 2:スティック異常）

【アナログスティック・ボタン入力】
  analogX         : long = 512     // スティックX軸生値
  analogY         : long = 512     // スティックY軸生値
  stickPressed    : bool = false  // スティック押込み状態
  stickDirection  : int8_t = -1   // 方向判定結果
  btnOpPressed        : bool = false // 演算子切替ボタン
  btnConfirmPressed   : bool = false // 入力確定ボタン
  btnCalcPressed      : bool = false // 計算実行ボタン
  btnResetPressed     : bool = false // リセットボタン
  btnCancelPressed    : bool = false // 入力取消ボタン

【タイマー・デバウンス管理】
  lastDebounceMs[5]   : unsigned long[5] = {0,0,0,0,0} // 各ボタンの最終入力時刻
  lastInputPollMs     : unsigned long = 0   // 入力読み取りの最終時刻
  lastBuzzerStartMs   : unsigned long = 0   // ブザー鳴動開始時刻
  buzzerActive        : bool = false        // ブザー鳴動中フラグ

【定数】
  DEBOUNCE_MS         : const unsigned long = 50   // デバウンス時間
  INPUT_POLL_MS       : const unsigned long = 100  // 入力更新周期
  STICK_DEADZONE      : const int = 80             // スティック中心無効範囲
  STICK_THRESHOLD     : const int = 220            // 方向判定しきい値
  BUZZER_FREQ_HZ      : const unsigned int = 2000  // ブザー周波数
  BUZZER_DURATION_MS  : const unsigned int = 120   // ブザー鳴動時間
```

---

## 2. 各関数の詳細設計

> ※ 基本設計書（2-2 関数一覧）で定義した各関数の「中身」を設計します。
> **疑似コード**（日本語＋処理の流れ）で書いてください。実際のC++コードは書かなくてOKです。

---

### `setup()` — ピンモード設定、シリアル通信初期化、初期状態の設定を行う

**basic_design.md 2-2 との対応：** ピンモード設定、シリアル通信初期化、初期状態の設定を行う

**引数：** なし

**戻り値：** なし

```
【処理の流れ】
1. 各ピンのモードを設定（INPUT_PULLUP/OUTPUT）
2. グローバル変数・状態を初期化
3. Serial.begin(9600)でシリアル通信開始
4. 必要なライブラリ初期化（該当あれば）
5. 起動確認用にブザーを短く鳴らす
```

---

### `loop()` — 入力取得→状態遷移→出力更新を状態に応じて実行する

**basic_design.md 2-2 との対応：** 入力取得→状態遷移→出力更新を状態に応じて実行する

**引数：** なし

**戻り値：** なし

```
【処理の流れ】
＜毎ループ実行すること＞
  - 現在時刻を取得: now = millis()
  - readButtons(), readAnalogStick()で入力取得
  - updateState()で状態遷移判定

＜currentStateが待機中のとき＞
  - 入力開始待ち。入力があれば入力中へ遷移

＜currentStateが入力中のとき＞
  - doAnalogStickInput(), doOperatorSelect(), doAnalogStickPress(), doConfirmInput()などを呼ぶ
  - 入力確定で計算中へ、取消で待機中へ、異常検知でエラー中へ遷移

＜currentStateが計算中のとき＞
  - doCalculate(), doErrorCheck()を呼ぶ
  - 計算完了で結果表示中へ、エラーならエラー中へ遷移

＜currentStateが結果表示中のとき＞
  - doDisplayResult(), doBuzz()を呼ぶ
  - 次の入力開始や待機戻り、リセットで状態遷移

＜currentStateがエラー中のとき＞
  - エラー内容表示、リセット待ち

＜currentStateがリセット中のとき＞
  - doReset()を呼び、初期状態へ
```

---

### `readButtons()` — D3〜D7のボタン状態を読み、デバウンス後に btn* 変数を更新する

**basic_design.md 2-2 との対応：** D3〜D7のボタン状態を読み、デバウンス後に btn* 変数を更新する

**引数：** なし

**戻り値：** なし

```
【処理の流れ】
1. 各ボタンピンの値をdigitalReadで取得
2. 前回値・時刻と比較し、DEBOUNCE_MS以上経過していれば状態確定
3. btnOpPressed等の変数を更新

【エラー・異常ケース】
- チャタリング時は入力を無視
```

---

### `readAnalogStick()` — A0/A1/D2を読み、analogX, analogY, stickPressed, stickDirection を更新する

**basic_design.md 2-2 との対応：** A0/A1/D2を読み、analogX, analogY, stickPressed, stickDirection を更新する

**引数：** なし

**戻り値：** なし

```
【処理の流れ】
1. analogReadでX/Y軸値を取得
2. D2で押込み状態を取得
3. STICK_DEADZONE/THRESHOLDで方向判定し、stickDirectionを更新
4. stickPressedも更新

【エラー・異常ケース】
- X/Yが極端値で固着していればエラー検知
```

---

### `updateState()` — currentState と prevState を更新し、遷移条件を判定する

**basic_design.md 2-2 との対応：** currentState と prevState を更新し、遷移条件を判定する

**引数：** なし

**戻り値：** なし

```
【処理の流れ】
1. prevState = currentState
2. 入力・フラグに応じて状態遷移条件を判定
3. 必要に応じてcurrentStateを変更

【エラー・異常ケース】
- 不正な遷移はエラー中へ
```

---

### `doAnalogStickInput()` — stickDirection と digitRangeMode から入力数字を確定し inputBuffer に反映する

**basic_design.md 2-2 との対応：** stickDirection と digitRangeMode から入力数字を確定し inputBuffer に反映する

**引数：** stickDirection(int8_t), digitRangeMode(uint8_t)

**戻り値：** なし

```
【処理の流れ】
1. stickDirection, digitRangeModeから入力数字を決定
2. inputBufferに桁追加、inputDigitsを更新
3. 入力上限超過時は無視または警告

【エラー・異常ケース】
- stickDirectionが-1や異常値なら入力無効
```

---

### `doOperatorSelect()` — btnOpPressed で currentOperator を +,-,*,/ の順に切り替える

**basic_design.md 2-2 との対応：** btnOpPressed で currentOperator を +,-,*,/ の順に切り替える

**引数：** btnOpPressed(bool)

**戻り値：** なし

```
【処理の流れ】
1. btnOpPressedが立ち上がったらcurrentOperatorを次の演算子に切替
2. 表示・状態も更新

【エラー・異常ケース】
- 不正な演算子値は+に戻す
```

---

### `doAnalogStickPress()` — stickPressed により digitRangeMode（0〜4/5〜9）を切り替える

**basic_design.md 2-2 との対応：** stickPressed により digitRangeMode（0〜4/5〜9）を切り替える

**引数：** stickPressed(bool)

**戻り値：** なし

```
【処理の流れ】
1. stickPressedが立ち上がったらdigitRangeModeをトグル
2. 表示も更新

【エラー・異常ケース】
- 連打・チャタリング時は無視
```

---

### `doConfirmInput()` — inputBuffer を operand1 または operand2 に確定し hasOperand* を更新する

**basic_design.md 2-2 との対応：** inputBuffer を operand1 または operand2 に確定し hasOperand* を更新する

**引数：** btnConfirmPressed(bool), inputBuffer(long)

**戻り値：** なし

```
【処理の流れ】
1. btnConfirmPressedが立ち上がったらinputBufferをオペランドへ確定
2. hasOperand1/2を更新、inputBufferクリア

【エラー・異常ケース】
- 入力値が不正なら無視
```

---

### `doCalculate()` — operand1, operand2, currentOperator で演算し calcResult を更新する

**basic_design.md 2-2 との対応：** operand1, operand2, currentOperator で演算し calcResult を更新する

**引数：** operand1(long), operand2(long), currentOperator(char)

**戻り値：** bool（成功/失敗）

```
【処理の流れ】
1. オペランド・演算子に応じて四則演算を実行
2. 結果をcalcResultに格納
3. オーバーフローや0除算は失敗として返す

【エラー・異常ケース】
- 0除算や範囲外演算はエラー
```

---

### `doReset()` — 変数初期化、表示クリア、ブザー停止を行い待機状態に戻す

**basic_design.md 2-2 との対応：** 変数初期化、表示クリア、ブザー停止を行い待機状態に戻す

**引数：** btnResetPressed(bool)

**戻り値：** なし

```
【処理の流れ】
1. btnResetPressedが立ち上がったら全変数初期化
2. シリアル表示クリア、ブザー停止
3. currentStateを待機中へ

【エラー・異常ケース】
- 途中で異常があれば強制初期化
```

---

### `doDisplayResult()` — operand1 演算子 operand2 = calcResult 形式でシリアル表示する

**basic_design.md 2-2 との対応：** operand1 演算子 operand2 = calcResult 形式でシリアル表示する

**引数：** operand1(long), operand2(long), currentOperator(char), calcResult(long)

**戻り値：** なし

```
【処理の流れ】
1. シリアルモニタに計算式と結果を表示
2. hasResultをtrueに

【エラー・異常ケース】
- 表示バッファ溢れ等は警告
```

---

### `doErrorCheck()` — 0除算条件とスティック異常を検出し errorCode と状態を更新する

**basic_design.md 2-2 との対応：** 0除算条件とスティック異常を検出し errorCode と状態を更新する

**引数：** currentOperator(char), operand2(long), analogX(int), analogY(int)

**戻り値：** bool（エラー有無）

```
【処理の流れ】
1. currentOperatorが'/'かつoperand2==0ならerrorCode=1
2. analogX/analogYが極端値ならerrorCode=2
3. エラー時はcurrentStateをエラー中へ

【エラー・異常ケース】
- その他異常値も検出
```

---

### `doBuzz()` — buzzerActive と時刻差で非ブロッキングに鳴動開始/停止を制御する

**basic_design.md 2-2 との対応：** buzzerActive と時刻差で非ブロッキングに鳴動開始/停止を制御する

**引数：** buzzerActive(bool), lastBuzzerStartMs(unsigned long)

**戻り値：** なし

```
【処理の流れ】
1. 結果表示時にbuzzerActive=falseなら鳴動開始、lastBuzzerStartMs=now
2. 鳴動中は経過時間を監視し、BUZZER_DURATION_MS経過で停止

【エラー・異常ケース】
- 鳴動取りこぼし時は強制停止
```

---

## 3. 重要ロジックの詳細設計

### 3-1. チャタリング防止（デバウンス処理）

> ※ ボタンを使う場合は必ず設計してください。

```
【考え方】
  タクトスイッチ等の物理ボタンは押下・離上時にノイズ（チャタリング）が発生するため、50ms以内の連続変化は同一イベントとして無視する。

【処理の流れ】
  1. 各ボタンピンの値をdigitalReadで取得
  2. 前回確定時刻（lastDebounceMs[]）からの経過時間を計算
  3. 経過時間 < DEBOUNCE_MS（50ms）なら入力を無視
  4. 経過時間 >= DEBOUNCE_MSならボタン状態を確定し、lastDebounceMs[]を更新
  5. btn*Pressed変数を更新

【必要な変数】
  lastDebounceMs[5] : unsigned long[]  // 各ボタンの最終入力時刻
  DEBOUNCE_MS : const unsigned long = 50
```

---

### 3-2. millis() を使ったタイマー管理

```
【考え方】
  delay()を使わず、millis()で「前回実行時刻との差分」を判定し、周期処理や非ブロッキング制御を実現する。

【処理の流れ（例: 入力・ブザー・表示）】
  1. now = millis() で現在時刻取得
  2. now - lastInputPollMs >= INPUT_POLL_MS なら入力取得・更新、lastInputPollMs = now
  3. now - lastBuzzerStartMs >= BUZZER_DURATION_MS ならブザー停止
  4. その他、必要な周期ごとに同様の判定

【自分のシステムで millis() を使う処理】
  - ボタン入力読出: 10ms周期
  - スティック読出: 20ms周期
  - 入力確定/演算子切替: 入力ごと
  - ブザー鳴動制御: 結果表示時に開始、指定時間後に停止
  - シリアル表示: 状態遷移時＋200ms間隔
  - 状態遷移判定: 毎ループ
```

---

### 3-3. スティック方向判定・異常検知ロジック

```
【考え方】
  アナログスティックのX/Y値から方向を判定し、中心付近ノイズや極端値による異常も検出する。

【処理の流れ】
  1. analogX, analogYを取得
  2. |analogX-512|, |analogY-512| < STICK_DEADZONEなら「入力なし」扱い
  3. しきい値（STICK_THRESHOLD）を超えた軸で方向を決定（優先軸ルール）
  4. X/Yが0または1023等の極端値で一定時間継続→スティック異常（errorCode=2）

【入力値と出力値の関係】
  - 中心付近は入力無効
  - 斜め/境界は優先軸で1方向に確定
  - 極端値はエラー
```

---

### 3-4. オーバーフロー・0除算・入力不足検出ロジック

```
【考え方】
  計算時にlong型の範囲外や0除算、オペランド未確定などの異常を事前に検出し、エラー状態へ遷移させる。

【処理の流れ】
  1. 計算前にhasOperand1/2を確認し、未確定ならエラー
  2. currentOperatorが'/'かつoperand2==0なら0除算エラー
  3. 加算・乗算時はlong型範囲を超える場合はオーバーフローエラー
  4. エラー時はerrorCodeを設定し、currentStateをエラー中へ
```

---

## 4. デバッグ出力計画（任意）

> **【任意】** 関数設計（Section 2）と並行して記入すると効果的です。
> 「動かない」ときに何を確認すればいいかを事前に計画しておきます。
> 実装後は不要な Serial.println() を削除すること。

| No | 確認したい内容 | 挿入する関数 | Serial.println の内容例 |
|:---|:---|:---|:---|
| 1 | ボタン入力が正しく取れているか | `readButtons()` | `Serial.println(btnOpPressed);` |
| 2 | スティック値・方向判定が正しいか | `readAnalogStick()` | `Serial.print(analogX); Serial.print(","); Serial.print(analogY); Serial.print(","); Serial.println(stickDirection);` |
| 3 | 状態遷移が正しく起きているか | `loop()`, `updateState()` | `Serial.println(currentState);` |
| 4 | 入力バッファ・オペランド確定の流れ | `doAnalogStickInput()`, `doConfirmInput()` | `Serial.println(inputBuffer); Serial.println(operand1); Serial.println(operand2);` |
| 5 | 演算子切替が正しく動作するか | `doOperatorSelect()` | `Serial.println(currentOperator);` |
| 6 | 計算結果・エラー検知の流れ | `doCalculate()`, `doErrorCheck()` | `Serial.println(calcResult); Serial.println(errorCode);` |
| 7 | ブザー鳴動タイミング | `doBuzz()` | `Serial.println("buzzer on"); Serial.println("buzzer off");` |
| 8 | チャタリング処理が効いているか | `readButtons()` | `Serial.println("btn confirmed");` |
| 9 | 異常系（0除算・スティック異常）検知 | `doErrorCheck()` | `Serial.println("error: 0-div"); Serial.println("error: stick");` |

---

## 5. 単体テスト仕様書（V字モデル：詳細設計 ↔ 単体テスト）

> **【AIレビュー反映】**
> 以下の観点を追加しました：
> - 境界値テスト（スティック判定・デバウンス・入力バッファ・long型範囲）
> - 異常系（不正演算子・異常状態からのリセット・極端値連続入力）
> - タイミング・並行動作（ブザー鳴動中の入力、周期精度）
> - 状態遷移の不正パス


> ※ 各関数・部品が「単体で正しく動くか」を確認するテスト項目を設計します。
> 「実際の結果」欄は実装後に記入します。

### 5-1. 入力系テスト

| No | テスト対象の関数 | 入力・操作 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
<<<<<<< HEAD
| 1 | readButtons() | 各タクトスイッチを1回ずつ押す | 対応するbtn*Pressedがtrueになる | | [ ] |
| 2 | readButtons() | スイッチを素早く2回押す | 1回分だけtrueになる（チャタリング無効） | | [ ] |
| 3 | readAnalogStick() | スティックを各方向に倒す | stickDirectionが正しく変化 | | [ ] |
| 4 | readAnalogStick() | スティック押込みを行う | stickPressedがtrueになる | | [ ] |
| 5 | doAnalogStickInput() | 方向入力ごとに数字入力 | inputBufferに正しい数字が追加 | | [ ] |
| 6 | doOperatorSelect() | 演算子切替ボタンを複数回押す | currentOperatorが循環する | | [ ] |
| 7 | doConfirmInput() | 入力確定ボタン押下 | operand1/2に値が確定 | | [ ] |
| 8 | doErrorCheck() | 0除算条件で計算実行 | errorCode=1, エラー状態遷移 | | [ ] |
| 9 | doErrorCheck() | スティック極端値で入力 | errorCode=2, エラー状態遷移 | | [ ] |
|10| readAnalogStick() | DEADZONE/THRESHOLDの境界値（80, 220, 221等）で入力 | 境界値で正しい方向判定・入力無効 | | [ ] |
|11| doAnalogStickInput() | inputDigitsが最大桁数時に追加入力 | 上限超過時は無視または警告 | | [ ] |
|12| doOperatorSelect() | 不正な演算子値を強制設定 | +にフォールバックする | | [ ] |
|13| doConfirmInput() | 入力値が不正な場合 | operand1/2に確定されない | | [ ] |
|14| readButtons() | デバウンス時間49ms, 50ms, 51msで連続押下 | 50ms未満は無視、50ms以上で確定 | | [ ] |
|15| doReset() | エラー状態からリセット | 全変数初期化・正常復帰 | | [ ] |
|16| readAnalogStick() | analogX/analogYが連続で極端値 | errorCode=2, エラー状態遷移 | | [ ] |
|17| doCalculate() | long型の最大・最小値付近で演算 | オーバーフロー時はエラー | | [ ] |
|18| updateState() | 不正な状態遷移（例: 計算中→入力中） | エラー状態へ遷移 | | [ ] |
=======
| 1 | readButtons() | 各タクトスイッチを1回ずつ押す | 対応するbtn*Pressedがtrueになる | | [○] |
| 2 | readButtons() | スイッチを素早く2回押す | 1回分だけtrueになる（チャタリング無効） | | [○] |
| 3 | readAnalogStick() | スティックを各方向に倒す | stickDirectionが正しく変化 | | [○] |
| 4 | readAnalogStick() | スティック押込みを行う | stickPressedがtrueになる | | [○] |
| 5 | doAnalogStickInput() | 方向入力ごとに数字入力 | inputBufferに正しい数字が追加 | | [○] |
| 6 | doOperatorSelect() | 演算子切替ボタンを複数回押す | currentOperatorが循環する | | [○] |
| 7 | doConfirmInput() | 入力確定ボタン押下 | operand1/2に値が確定 | | [○] |
| 8 | doErrorCheck() | 0除算条件で計算実行 | errorCode=1, エラー状態遷移 | | [○] |
| 9 | doErrorCheck() | スティック極端値で入力 | errorCode=2, エラー状態遷移 | | [○] |
|10| readAnalogStick() | DEADZONE/THRESHOLDの境界値（80, 220, 221等）で入力 | 境界値で正しい方向判定・入力無効 | | [○] |
|11| doAnalogStickInput() | inputDigitsが最大桁数時に追加入力 | 上限超過時は無視または警告 | | [○] |
|12| doOperatorSelect() | 不正な演算子値を強制設定 | +にフォールバックする | | [○] |
|13| doConfirmInput() | 入力値が不正な場合 | operand1/2に確定されない | | [○] |
|14| readButtons() | デバウンス時間49ms, 50ms, 51msで連続押下 | 50ms未満は無視、50ms以上で確定 | | [○] |
|15| doReset() | エラー状態からリセット | 全変数初期化・正常復帰 | | [○] |
|16| readAnalogStick() | analogX/analogYが連続で極端値 | errorCode=2, エラー状態遷移 | | [○] |
|17| doCalculate() | long型の最大・最小値付近で演算 | オーバーフロー時はエラー | | [○] |
|18| updateState() | 不正な状態遷移（例: 計算中→入力中） | エラー状態へ遷移 | | [○] |
>>>>>>> 0fa8001fd10e1a738073c67920f0e0e3435714f3

### 5-2. 出力系テスト

| No | テスト対象の関数 | 入力・操作 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
<<<<<<< HEAD
| 1 | doDisplayResult() | 計算完了後 | シリアルに"operand1 operator operand2 = result"が表示 | | [ ] |
| 2 | doBuzz() | 結果表示時 | ブザーが1回鳴動し、指定時間後に停止 | | [ ] |
| 3 | doReset() | リセットボタン押下 | 全変数初期化・表示クリア・ブザー停止 | | [ ] |
| 4 | doBuzz(), readButtons() | ブザー鳴動中にボタン入力 | 入力が取りこぼされず反映される | | [ ] |
=======
| 1 | doDisplayResult() | 計算完了後 | シリアルに"operand1 operator operand2 = result"が表示 | | [○] |
| 2 | doBuzz() | 結果表示時 | ブザーが1回鳴動し、指定時間後に停止 | | [○] |
| 3 | doReset() | リセットボタン押下 | 全変数初期化・表示クリア・ブザー停止 | | [○] |
| 4 | doBuzz(), readButtons() | ブザー鳴動中にボタン入力 | 入力が取りこぼされず反映される | | [○] |
>>>>>>> 0fa8001fd10e1a738073c67920f0e0e3435714f3

### 5-3. タイミング・並行動作テスト

| No | テスト内容 | テスト手順 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
<<<<<<< HEAD
| 1 | delay()による処理停止がないか | ブザー鳴動中や表示中にボタン入力 | 入力が取りこぼされず反映される | | [ ] |
| 2 | millis()タイマーの周期精度 | 入力・ブザー・表示の周期をストップウォッチ等で確認 | 設計値通りの周期で動作 | | [ ] |
| 3 | 状態遷移の不正パス | 不正な状態遷移を強制的に発生させる | エラー状態へ遷移 | | [ ] |
=======
| 1 | delay()による処理停止がないか | ブザー鳴動中や表示中にボタン入力 | 入力が取りこぼされず反映される | | [○] |
| 2 | millis()タイマーの周期精度 | 入力・ブザー・表示の周期をストップウォッチ等で確認 | 設計値通りの周期で動作 | | [○] |
| 3 | 状態遷移の不正パス | 不正な状態遷移を強制的に発生させる | エラー状態へ遷移 | | [○] |
>>>>>>> 0fa8001fd10e1a738073c67920f0e0e3435714f3

---

## 6. AIレビュー記録

> グループレビューの前に必ず実施してください。

### Q1: 実装上の問題確認

> 「この詳細設計書に書いた関数と処理フローをもとに Arduino でコードを書きます。バグになりやすい箇所・処理の抜け・型の問題はありますか？」

**AIの回答（要約）：
---

### **レビューの要点**

#### **1. バグになりやすい箇所**
- **デバウンス処理**: 配列外参照やチャタリング時の状態遷移バグに注意。
- **状態遷移**: 不正な遷移時のエラー処理が抜けていないか確認。
- **スティック方向判定**: 極端値（0, 1023）の異常検知ロジックが正しく動作するか。
- **オーバーフロー・0除算**: long型の範囲外演算や0除算時のエラー処理が適切か。

#### **2. 処理の抜け**
- **入力バッファのクリア**: operand確定後やリセット時の初期化漏れに注意。
- **エラー時の復帰**: エラー状態から正常状態に戻る処理が設計されているか。
- **演算子切替**: 不正な演算子値が発生した場合のフォールバック処理。

#### **3. 型の問題**
- **型の不一致**: Arduinoのint型（16bit）とlong型（32bit）の使い分けに注意。
- **符号付き/符号なし**: uint8_t, int8_tなどの型変換時のオーバーフローに注意。

#### **4. テスト仕様の確認**
- **境界値テスト**: スティック方向判定やデバウンス処理での境界値テストが必要。
- **エラー検出**: 0除算やスティック異常のテスト項目が網羅されているか確認。

#### **5. その他**
- **非同期処理**: millis()を用いた非ブロッキング制御が正しく設計されているか。
- **デバッグ出力**: Serial.println()を用いたデバッグ計画が適切。

---

この設計書は、Arduinoコード実装の基盤として十分な詳細を含んでいますが、実装時には型やエラー処理、テスト項目の確認が重要です。
**

**対応した内容：analogXやanalogYをlong型に変更し、計算用変数と型を統一。**

---

### Q2: 単体テスト仕様の確認

> 「Section 5 の単体テスト仕様書で、各関数の動作が正しく検証できていますか？テストが不足している項目や、境界値テストが必要な箇所を教えてください。」

**AIの回答（要約）：
- デバウンス処理や状態遷移、スティック方向判定、オーバーフロー・0除算などでバグが起きやすいので注意が必要。
- 入力バッファのクリアやエラー時の復帰、不正な演算子値の処理など、処理の抜けがないか確認が必要。
- Arduinoの型（int/long, 符号付き/なし）の使い分けや変換時のオーバーフローに注意。
- 境界値テスト（スティック判定・デバウンス等）やエラー検出のテスト項目が重要。
- millis()による非ブロッキング制御やデバッグ出力計画も適切に設計されている。

全体として、設計は十分だが、実装時は型・エラー処理・テスト項目の確認が重要、という指摘です。
**

**対応した内容：AIレビューの指摘に基づき、テスト項目の追加や型の統一、エラー処理の強化を行いました。**

---

## 7. グループレビュー記録

### 7-1. 指摘一覧

| No | 指摘内容 | 指摘者 | 対応 |
|:---|:---|:---|:---|
| 1 | analogX ｙの変数の初期値設定がなぜ516なのか | 岩野　拓翔 | 初期値を中央付近の値に設定することで、スティックの中立位置を正確に反映するため |
| 2 |  |  |  |
| 3 |  |  |  |

### 7-2. レビューを受けて変更した点

-
-

---

*初版: 2026-05-22 / AIレビュー: 2026-05-22 / グループレビュー後更新: 2026-05-22*
