# 詳細設計書 — 組込み開発実習

<!-- 作成者: 岩野拓翔 / 日付: 2026-05-26 / グループ: 1-k -->

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
| 作品タイトル | 回転機械の安全監視装置|
| 状態の種類（1-2 状態遷移から） | [待機中] ──（回転開始）──→ [計測中] ──（rps表示更新）──（rps設定値超過）──→ [警告中] |
| 実装する関数の数（2-2 関数一覧から） | 9 個 |
| グローバル変数の合計バイト数（2-1 SRAM確認から） | 16B |

---

## 1. グローバル変数・定数の設計

> ※ 基本設計書（2-1 データ設計）をもとに、**型と初期値まで**決めます。
> ここで設計した変数は、この後の関数設計でそのまま使います。

```
【ピン定義】（basic_design.md 3-1 から転記）
<<<<<<< HEAD
PIN_POT : const uint8_t = A1 // A1: ポテンショメータ入力
=======
<<<<<<< HEAD
PIN_ENCODER : const uint8_t = 2 // D2: ロータリーエンコーダ入力
PIN_POT : const uint8_t = A0 // A0: ポテンショメータ入力
=======
PIN_POT : const uint8_t = A1 // A1: ポテンショメータ入力
>>>>>>> 0fa8001fd10e1a738073c67920f0e0e3435714f3
>>>>>>> 9518da998d04755cdcb0666a04acb8b573d76891
PIN_SEG_1 : const uint8_t = 3 // D3: 4桁7セグ表示制御ライン1
PIN_SEG_2 : const uint8_t = 4 // D4: 4桁7セグ表示制御ライン2
PIN_SEG_3 : const uint8_t = 5 // D5: 4桁7セグ表示制御ライン3
PIN_SEG_4 : const uint8_t = 6 // D6: 4桁7セグ表示制御ライン4
PIN_BUZZER : const uint8_t = 7 // D7: パッシブブザー出力
PIN_MOTOR_PWM : const uint8_t = 9 // D9: モーターPWM出力

【状態管理】（basic_design.md 1-2 の状態名から転記）
  currentState  : byte = 0   // 0:待機 1:動作中 2:警告中

【タイマー（millis()用）】（basic_design.md 2-3 から転記）
lastPulseReadMillis : unsigned long = 0 // 回転パルス監視の前回時刻
<<<<<<< HEAD
=======
<<<<<<< HEAD
lastPotReadMillis : unsigned long = 0 // ポテンショメータ読取の前回時刻
=======
>>>>>>> 0fa8001fd10e1a738073c67920f0e0e3435714f3
>>>>>>> 9518da998d04755cdcb0666a04acb8b573d76891
lastRpsCalcMillis : unsigned long = 0 // rps計算の前回時刻
lastDisplayMillis : unsigned long = 0 // 表示更新の前回時刻
lastBuzzerMillis : unsigned long = 0 // ブザー断続制御の前回時刻

【センサー・入力値】（basic_design.md 2-1 から転記）
potValue : uint16_t = 0 // ポテンショメータの現在値（0〜1023）

【その他のフラグ・カウンター】
  （自分のものを追加）
pulseCount : volatile uint16_t = 0 // 回転パルスのカウント値（割り込みで加算）
rpsCurrent : uint16_t = 0 // 現在の回転数（rps）
rpsThreshold : uint16_t = 1500 // 警告しきい値（rps）
displayValue : uint16_t = 0 // ディスプレイ表示値
```
【周期定数】
INTERVAL_PULSE_READ : const unsigned long = 100 // 回転パルス監視の前回時刻（100ms周期）
<<<<<<< HEAD
=======
<<<<<<< HEAD
INTERVAL_POT_READ : const unsigned long = 200 // ポテンショメータ読取の前回時刻（200ms周期）
=======
>>>>>>> 0fa8001fd10e1a738073c67920f0e0e3435714f3
>>>>>>> 9518da998d04755cdcb0666a04acb8b573d76891
INTERVAL_RPS_CALC : const unsigned long = 100 // rps計算の前回時刻（100ms周期）
INTERVAL_DISPLAY : const unsigned long = 100 // 表示更新の前回時刻（100ms周期）
INTERVAL_BUZZER : const unsigned long = 200 //ブザー断続制御の前回時刻（200ms周期）

---

## 2. 各関数の詳細設計

> ※ 基本設計書（2-2 関数一覧）で定義した各関数の「中身」を設計します。
> **疑似コード**（日本語＋処理の流れ）で書いてください。実際のC++コードは書かなくてOKです。

---

### `setup()` — 初期化処理

```
【処理の流れ】
1.各ピンのモードを設定する
   - PIN_ENCODER     → INPUT（割り込み設定も行う）
   - PIN_POT         → INPUT
   - PIN_SEG_1〜4    → OUTPUT
   - PIN_BUZZER      → OUTPUT
   - PIN_MOTOR_PWM   → OUTPUT
2. グローバル変数を初期化する
   - 各カウンタ・状態変数を0または初期値にリセット
3. 必要なライブラリやデバイスの初期化
   - 7セグメント表示用ライブラリの初期化
```

---

### `loop()` — メインループ

> ※ loop() は「状態ごとに何をするか」だけ書く。細かい処理は各関数に任せる。

```
【処理の流れ】

＜毎ループ実行すること＞
  - 現在時刻を取得: now = millis()
  - 割り込みでpulseCountが増えているか監視
  - ポテンショメータ値を定期的に取得
  - 必要な周期ごとに各処理（rps計算、表示更新、ブザー制御）を呼ぶ

＜currentState が 0（待機中）のとき＞
  - モーターの回転開始を監視
  - rpsが0より大きくなったら currentState = 1

＜currentState が 1（計測中）のとき＞
  - rpsを計算し、表示を更新
  - rpsがしきい値を超えたら currentState = 2

＜currentState が 2（警告中）のとき＞
  - ブザーを鳴らし続ける
  - rpsがしきい値以下に戻ったら currentState = 1
  - 必要に応じてリセット要求を監視し、リセット時は currentState = 0
```

---

### （関数ごとに以下のブロックをコピーして追加してください）

> ※ 基本設計書 2-2 の関数一覧に記載した関数を1つずつ設計します。

---

### `関数名()` — （役割を1行で書く）

**basic_design.md 2-2 との対応：** （基本設計書の関数一覧の説明を転記）
'
**引数：** `引数名`（型）: 何の値か

**戻り値：** 型（なしの場合は void）

```
【処理の流れ】
1.
2.
3.

【エラー・異常ケース】
- 異常な値が来た場合:

・setup() — 初期化処理
basic_design.md 2-2 との対応： ピンモード設定・変数初期化・起動確認

引数： なし
戻り値： void

【処理の流れ】
1. 各ピンのモードを設定（INPUT/OUTPUT/割り込み）
2. グローバル変数を初期値にリセット
3. 7セグメント表示やシリアル通信など必要なライブラリ初期化
4. 起動確認（7セグ表示・ブザーなど）

【エラー・異常ケース】
- 初期化失敗時はシリアル出力でエラー通知

・loop() — メインループ
basic_design.md 2-2 との対応： 状態に応じて各関数を呼び出すメインループ

引数： なし
戻り値： void

【処理の流れ】
1. 現在時刻を取得
2. 各周期ごとに入力・計算・出力関数を呼ぶ
3. 状態（currentState）に応じて処理分岐

【エラー・異常ケース】
- 状態遷移が不正な場合はエラー表示

・onPulse() — 回転パルスをカウントする
basic_design.md 2-2 との対応： 割り込みで回転パルスをカウント

引数： なし
戻り値： void

【処理の流れ】
1. pulseCountをインクリメント

【エラー・異常ケース】
- カウンタがオーバーフローした場合はリセット

・calcRps() — 一定周期でrpsを計算し、状態を判定
basic_design.md 2-2 との対応： rps計算・状態判定

引数： なし
戻り値： void

【処理の流れ】
1. 一定周期ごとにpulseCountからrpsを計算
2. rpsCurrentに値を格納
3. 状態遷移条件を判定

【エラー・異常ケース】
- 計算結果が異常値（極端に大きい/小さい）ならエラー表示

・checkThreshold() — rpsが設定値を超えたか判定し状態遷移
basic_design.md 2-2 との対応： しきい値判定・状態遷移

引数： なし
戻り値： void

【処理の流れ】
1. rpsCurrentとrpsThresholdを比較
2. 超過時はcurrentStateを警告中に遷移

【エラー・異常ケース】
- rpsThresholdが不正値ならエラー表示

・updateDisplay() — rps値をディスプレイに表示する
basic_design.md 2-2 との対応： 表示更新

引数： なし
戻り値： void

【処理の流れ】
1. displayValueにrpsCurrentを代入
2. 7セグメントディスプレイに表示

【エラー・異常ケース】
- 表示範囲外の値はエラー表示

・updateBuzzer() — 警告中にブザーを鳴らす
basic_design.md 2-2 との対応： ブザー制御

引数： なし
戻り値： void

【処理の流れ】
1. currentStateが警告中ならブザーON
2. それ以外はブザーOFF

【エラー・異常ケース】
- ブザーが鳴りっぱなしの場合は強制OFF

・readPotentiometer() — モーター速度を調整
basic_design.md 2-2 との対応： ポテンショメータ読取

引数： なし
戻り値： int値（ポテンショメータの値）

【処理の流れ】
1. アナログ入力から値を取得
2. potValueに格納

【エラー・異常ケース】
- 値が0〜1023の範囲外なら補正
```

---


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
  （basic_design.md 2-3 のタイミング設計から転記して具体化する）

  【処理の流れ（本システム）】
  1. now = millis() を取得する
  2. 回転パルス監視の周期判定
     if (now - lastPulseReadMillis >= INTERVAL_PULSE_READ)
     回転パルス関連の監視処理を実行
     lastPulseReadMillis = now
  3. ポテンショメータ読取の周期判定
     if (now - lastPotReadMillis >= INTERVAL_POT_READ)
     readPotentiometer() を実行し potValue を更新
     lastPotReadMillis = now
 4. rps計算の周期判定
     if (now - lastRpsCalcMillis >= INTERVAL_RPS_CALC)
     calcRps() を実行し rpsCurrent を更新
     checkThreshold() で状態遷移を判定
     lastRpsCalcMillis = now
 5. 表示更新の周期判定
     if (now - lastDisplayMillis >= INTERVAL_DISPLAY)
     updateDisplay() を実行し 7セグ表示を更新
     lastDisplayMillis = now
 6. ブザー制御の周期判定
     if (now - lastBuzzerMillis >= INTERVAL_BUZZER)
     updateBuzzer() を実行
     lastBuzzerMillis = now
 7. どの条件も満たさない場合は何もしない（次ループで再判定）
```

---

### 3-3. その他の重要ロジック（任意）

> **【任意】** 複雑なロジックがある場合のみ記入してください。
> 例：「距離に応じたLED点灯パターン」「ゲームの衝突判定」「温度の閾値判定」

```
【処理の流れ】
1.
2.
3.

【入力値と出力値の関係】

```

---

## 4. デバッグ出力計画（任意）

> **【任意】** 関数設計（Section 2）と並行して記入すると効果的です。
> 「動かない」ときに何を確認すればいいかを事前に計画しておきます。
> 実装後は不要な Serial.println() を削除すること。

| No | 確認したい内容 | 挿入する関数 | Serial.println の内容例 |
|:---|:---|:---|:---|
| 1 | 初期化が最後まで完了したか | setup() | Serial.println("setup done"); |
| 2 | 現在状態が意図どおりか | loop() | 	Serial.println(String("state=") + currentState); |
| 3 | 割り込みパルスが入っているか | onPulse() | Serial.println(String("pulseCount=") + pulseCount); |
| 4 | rps計算が周期どおり動くか | calcRps() | 	Serial.println(String("rpsCurrent=") + rpsCurrent); |
| 5 | しきい値判定が正しいか | checkThreshold() | Serial.println(String("rps=") + rpsCurrent + ", th=" + rpsThreshold); |
| 6 | 	しきい値超過時に状態遷移したか | checkThreshold() | 	Serial.println("threshold over -> state=2"); |
| 7 | 	ポテンショメータ値が読めているか | readPotentiometer() | Serial.println(String("potValue=") + potValue); |
| 8 | 表示値がrpsと一致しているか | updateDisplay() | Serial.println(String("displayValue=") + displayValue); |
| 9 | 9	ブザーON/OFFが状態連動しているか | updateBuzzer() | Serial.println(String("buzzer state=") + currentState); |
| 10 | 	millis周期判定が機能しているか | loop() | Serial.println(String("now=") + now); |

---

## 5. 単体テスト仕様書（V字モデル：詳細設計 ↔ 単体テスト）

> ※ 各関数・部品が「単体で正しく動くか」を確認するテスト項目を設計します。
> 「実際の結果」欄は実装後に記入します。

### 5-1. 入力系テスト

| No | テスト対象の関数 | 入力・操作 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | onPulse() | ロータリーエンコーダを1パルスだけ入力する | pulseCount が 1 増加する | | [ ] |
| 2 | onPulse() | 	ロータリーエンコーダを連続回転させる | 転させる	入力パルス数に応じて pulseCount が連続増加する | | [ ] |
| 3 | readPotentiometer() | ポテンショメータを最小位置にする | 0 付近の値が取得される | | [ ] |
| 4 | readPotentiometer() | ポテンショメータを中央位置にする | 512 付近の値が取得される | | [ ] |
| 5 | readPotentiometer() | ポテンショメータを最大位置にする | 1023 付近の値が取得される | | [ ] |
| 6 | calcRps() | 一定回転でパルスを入力し 100ms 周期で計算させる | rpsCurrent が正の値で安定して更新される | | [ ] |
| 7 | calcRps() | 回転を止めて一定時間待つ | rpsCurrent が 0 に近づく、または 0 になる | | [ ] |
| 8 | checkThreshold() | rpsCurrent < rpsThreshold の条件を与える | currentState が 警告中 に遷移しない | | [ ] |
| 9 | checkThreshold() | rpsCurrent >= rpsThreshold の条件を与える | currentState が 警告中 に遷移する | | [ ] |
| 10 | checkThreshold() | rpsThreshold を下限・上限付近で変更して判定する | しきい値変更後の基準で正しく判定される | | [ ] |

### 5-2. 出力系テスト

| No | テスト対象の関数 | 入力・操作 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | updateDisplay() | rpsCurrent=0 を設定して実行 | 7セグに 0 が表示される | | [ ] |
| 2 | updateDisplay() | rpsCurrent=1234 を設定して実行 | 7セグに 1234 が表示される | | [ ] |
| 3 | updateDisplay() | rpsCurrent を連続更新（例: 100→200→300）して実行 | 表示が遅延なく追従する | | [ ] |
| 4 | updateDisplay() | 表示上限を超える値（例: 10000）を設定して実行 | 仕様どおりに丸め/上限制限表示される | | [ ] |
| 5 | updateBuzzer() | currentState=0（待機中）で実行 | ブザーは鳴らない（OFF）| | [ ] |
| 6 | updateBuzzer() | currentState=1（計測中）で実行 | ブザーは鳴らない（OFF）| | [ ] |
| 7 | updateBuzzer() | currentState=2（警告中）で実行 | ブザーが鳴る（ON）| | [ ] |
| 8 | updateBuzzer() | currentState を 2→1 に変更して実行 | ブザーが停止する | | [ ] |

### 5-3. タイミング・並行動作テスト

| No | テスト内容 | テスト手順 | 期待する結果 | 実際の結果 | 合否 |
|:---|:---|:---|:---|:---|:---|
| 1 | delayによる処理停止がないか | 回転中にポテンショメータを連続で回し、同時に表示とブザー動作を観察する | 入力読取、表示更新、ブザー制御が止まらず同時に動作する | | [ ] |
| 2 | millisタイマーの周期精度 | rps計算と表示更新の時刻をシリアル出力し、連続10回の間隔を測る | おおむね100ms周期で実行される | | [ ] |
| 3 | 複数周期の並行実行ができるか | 100ms処理と200ms処理を同時有効にして1分間動作させる | 100ms系と200ms系が互いに干渉せず継続動作する | | [ ] |
| 4 | 警告遷移時の並行性 | rpsをしきい値直下から直上へ上げる | 状態遷移、表示更新、ブザー開始が同一運転中に遅延なく反映される | | [ ] |
| 5 | 警告解除時の並行性 | rpsをしきい値超過からしきい値未満へ下げる | ザー停止と表示継続が正常に行われ、鳴りっぱなしにならない | | [ ] |
| 6 | 高頻度入力時の取りこぼし確認 | エンコーダを速く回しながら表示とブザーを同時動作させる | パルス計測が極端に欠落せず、表示が大きく破綻しない | | [ ] |
| 7 | 長時間動作の安定性 | 10分以上連続運転し、周期処理のズレと停止有無を確認する | 周期処理が継続し、状態更新と出力が安定している | | [ ] |

---

## 6. AIレビュー記録

> グループレビューの前に必ず実施してください。

### Q1: 実装上の問題確認

> 「この詳細設計書に書いた関数と処理フローをもとに Arduino でコードを書きます。バグになりやすい箇所・処理の抜け・型の問題はありますか？」

**AIの回答（要約）：**
1.高: 状態定義が基本設計と不一致です（型と状態名の両方）。
  状態定義 が int / 0:待機 1:動作中 2:警告中 になっていますが、基本設計は byte / 0:待機 1:計測 2:警告です。
  参照: 基本設計の状態
  影響: 状態遷移ロジック、テスト期待値、デバッグログの解釈がズレます。

2.高: テンプレ残骸が実仕様と混在しています。
  setup例 に PIN_BUTTON / PIN_LED が残り、
  loop例 に完了/エラー状態が残っています。
  影響: 実装者が誤って旧テンプレに従うリスクが高いです。

3.中: 3-1が「設定を行わない」と書かれているのに、内容はボタンのデバウンス例のままです。
  参照: 3-1設定なし宣言, 3-1内容
  影響: 「何を採用しないのか」「代替ロジックは何か」が不明です。

4.中: SRAM合計が 1B になっており、現行の変数量と整合していません。
  参照: SRAM記載
  影響: リソース見積もりの信頼性が下がります。

5.中: 表や文に誤記・ノイズがあり、テスト仕様の可読性を下げています。
  例: No重複, 文言崩れ, ザー停止
  影響: レビュー時の誤解や記入ミスを誘発します。

**対応した内容：**
グローバル変数・定数の型と初期値が明記され、SRAM合計も現実的な値（16B）に修正済み
状態管理（currentState）はbyte型で0:待機/1:動作中/2:警告中と明確化
テンプレ残骸（PIN_BUTTON, PIN_LED, 完了/エラー状態）は削除、実装対象の状態・ピンのみ記載
---

### Q2: 単体テスト仕様の確認

> 「Section 5 の単体テスト仕様書で、各関数の動作が正しく検証できていますか？テストが不足している項目や、境界値テストが必要な箇所を教えてください。」

**AIの回答（要約）：**
良い点: 入力系/出力系/並行動作で分割され、主要関数（onPulse, readPotentiometer, calcRps, checkThreshold, updateDisplay, updateBuzzer）は概ねカバーできています。
参照: 5-1, 5-2, 5-3

**対応した内容：**

---

## 7. グループレビュー記録

### 7-1. 指摘一覧

| No | 指摘内容 | 指摘者 | 対応 |
|:---|:---|:---|:---|
| 1 | 桑畑さん | longなど大きい型を使用しているが何故か？ | 今作ってるいるものが多くなったらどうなるかを想定する必要がある為、一応変数の型も大きくした。 |
| 2 |  |  |  |
| 3 |  |  |  |

### 7-2. レビューを受けて変更した点

-
-

---

*初版: YYYY-MM-DD / AIレビュー: YYYY-MM-DD / グループレビュー後更新: YYYY-MM-DD*
