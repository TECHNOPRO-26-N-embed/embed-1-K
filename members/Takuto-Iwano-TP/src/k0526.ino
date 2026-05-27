// ポテンショメータ入力ピン
const uint8_t PIN_POT = A1;

// モーター制御ピン
const uint8_t PIN_MOTOR_PWM = 3;
const uint8_t PIN_MOTOR_D1 = 4;
const uint8_t PIN_MOTOR_D2 = 5;

// 風の強さ表示用LEDピン（LED1〜LED5）
const uint8_t PIN_LED_1 = 6;
const uint8_t PIN_LED_2 = 7;
const uint8_t PIN_LED_3 = 8;
const uint8_t PIN_LED_4 = 9;
const uint8_t PIN_LED_5 = 10;
const uint8_t LED_PINS[5] = {PIN_LED_1, PIN_LED_2, PIN_LED_3, PIN_LED_4, PIN_LED_5};

// 警告ブザーの出力ピン
const uint8_t PIN_BUZZER = 12;

// 風の強さを0〜2500で扱う
const int WIND_MIN = 0;
const int WIND_MAX = 2500;

uint16_t potValue = 0;   // 0〜1023
int windValue = 0;       // 0〜2500
uint8_t motorPwm = 0;    // 0〜255
bool buzzerPatternActive = false;
uint8_t buzzerPhase = 0;
unsigned long buzzerPhaseStartMs = 0;

// 5つのLEDをいったんすべて消灯する
void turnOffAllLeds() {
	for (uint8_t i = 0; i < 5; i++) {
		digitalWrite(LED_PINS[i], LOW);
	}
}

// 風の強さ(0〜2500)を0〜5のレベルへ変換する
// 0は全消灯、1〜5は点灯段数を表す
uint8_t getWindLevel(int value) {
	int clamped = constrain(value, WIND_MIN, WIND_MAX);

	if (clamped == 0) {
		return 0;
	}
	if (clamped <= 500) {
		return 1;
	}
	if (clamped <= 1000) {
		return 2;
	}
	if (clamped <= 1500) {
		return 3;
	}
	if (clamped <= 2000) {
		return 4;
	}
	return 5;
}

// 風の強さ(0〜2500)に応じて、対応範囲までのLEDをすべて点灯する
// 0のときは全消灯する
void showWindLevelOnLeds(int value) {
	uint8_t level = getWindLevel(value);

	turnOffAllLeds();

	if (level == 0) {
		return;
	}

	for (uint8_t i = 0; i < level; i++) {
		digitalWrite(LED_PINS[i], HIGH);
	}
}

// 5段階目の間は「ピッ・ピッ・休止」を繰り返す
void updateBuzzer(uint8_t level) {
	if (level != 5) {
		digitalWrite(PIN_BUZZER, LOW);
		buzzerPatternActive = false;
		buzzerPhase = 0;
		return;
	}

	unsigned long now = millis();

	if (!buzzerPatternActive) {
		buzzerPatternActive = true;
		buzzerPhase = 0;
		buzzerPhaseStartMs = now;
		digitalWrite(PIN_BUZZER, HIGH);
		return;
	}

	unsigned long phaseDurationMs = 0;
	if (buzzerPhase == 0 || buzzerPhase == 2) {
		phaseDurationMs = 120; // ピッ
	} else if (buzzerPhase == 1) {
		phaseDurationMs = 120; // ピッまでの短い間
	} else {
		phaseDurationMs = 600; // 次のピッピまでの間
	}

	if ((now - buzzerPhaseStartMs) >= phaseDurationMs) {
		buzzerPhase = static_cast<uint8_t>((buzzerPhase + 1) % 4);
		buzzerPhaseStartMs = now;

		if (buzzerPhase == 0 || buzzerPhase == 2) {
			digitalWrite(PIN_BUZZER, HIGH);
		} else {
			digitalWrite(PIN_BUZZER, LOW);
		}
	}
}

void setup() {
	// 入出力ピンの初期化
	pinMode(PIN_POT, INPUT);
	pinMode(PIN_MOTOR_PWM, OUTPUT);
	pinMode(PIN_MOTOR_D1, OUTPUT);
	pinMode(PIN_MOTOR_D2, OUTPUT);
	pinMode(PIN_BUZZER, OUTPUT);

	for (uint8_t i = 0; i < 5; i++) {
		pinMode(LED_PINS[i], OUTPUT);
	}

	// モーター回転方向は常に右回転固定
	digitalWrite(PIN_MOTOR_D1, HIGH);
	digitalWrite(PIN_MOTOR_D2, LOW);

	// 起動時は停止状態でLED1を点灯
	analogWrite(PIN_MOTOR_PWM, 0);
	showWindLevelOnLeds(0);
	digitalWrite(PIN_BUZZER, LOW);
}

void loop() {
	// ポテンショメータ値を読み取る
	int raw = analogRead(PIN_POT);
	raw = constrain(raw, 0, 1023);
	potValue = static_cast<uint16_t>(raw);

	// 0〜1023を0〜2500へスケーリング
	windValue = map(potValue, 0, 1023, WIND_MIN, WIND_MAX);

	// 風の強さをPWM(0〜255)へ変換してモーター速度に反映
	int pwm = map(windValue, WIND_MIN, WIND_MAX, 0, 255);
	pwm = constrain(pwm, 0, 255);
	motorPwm = static_cast<uint8_t>(pwm);

	// 方向は右回転固定のため、毎回同じ向きに設定
	digitalWrite(PIN_MOTOR_D1, HIGH);
	digitalWrite(PIN_MOTOR_D2, LOW);
	analogWrite(PIN_MOTOR_PWM, motorPwm);

	// 0〜500, 500〜1000, ... の範囲に応じてLEDを段階的に点灯
	showWindLevelOnLeds(windValue);

	// 5段階目(最大)のときだけブザーで警告する
	uint8_t level = getWindLevel(windValue);
	updateBuzzer(level);

	// 値の暴れを少し抑えるため短い待ち時間を入れる
	delay(20);
}
   
