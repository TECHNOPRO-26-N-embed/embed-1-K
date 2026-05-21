# 設計書（第1回）

## 1. システム概要
Arduinoを用いてウルトラソニックセンサーで物体までの距離を測定し、その距離に応じてパッシブブザーから異なる音を出力するシステムを設計する。

## 2. 構成図

```
[物体] ←→ [ウルトラソニックセンサー] ──> [Arduino] ──> [パッシブブザー]
```

## 3. ハードウェア構成
- Arduino Uno
- ウルトラソニックセンサー
- パッシブブザー
- ジャンパーワイヤー
- 必要に応じて抵抗

## 4. ソフトウェア設計
### 4.1 距離測定
- ウルトラソニックセンサーから距離を取得する関数を作成
- 測定間隔は100msごと

### 4.2 距離判定ロジック
- 測定した距離を3段階に分類
  - 10cm未満：近い
  - 10cm以上30cm未満：中間
  - 30cm以上：遠い

### 4.3 ブザー制御
- 距離ごとに異なる音を出す
  - 近い：高い音で連続的に鳴る
  - 中間：中くらいの音で断続的に鳴る
  - 遠い：低い音で短く1回だけ鳴る
- Arduinoのtone関数を利用

#### サンプルコード例
```cpp
// ...existing code...
const int trigPin = 9;
const int echoPin = 10;
const int buzzerPin = 6;

void setup() {
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(buzzerPin, OUTPUT);
}

void loop() {
  long duration, distance;
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  duration = pulseIn(echoPin, HIGH);
  distance = duration * 0.034 / 2;

  if (distance < 10) {
    tone(buzzerPin, 1000); // 高い音
    delay(200);
    noTone(buzzerPin);
  } else if (distance < 30) {
    tone(buzzerPin, 600); // 中くらいの音
    delay(100);
    noTone(buzzerPin);
    delay(100);
  } else {
    tone(buzzerPin, 300); // 低い音
    delay(50);
    noTone(buzzerPin);
    delay(200);
  }
  delay(100);
}
```

## 5. テスト計画
- 距離ごとにブザーの音が正しく変化するか確認
- センサーの測定精度を確認

## 6. 今後の拡張案
- LEDによる視覚的フィードバック追加
- 距離判定の閾値や音パターンの調整
- ケース設計や安全対策
