// ポテンショメータの入力ピン（設計書より）
const uint8_t PIN_POT = A1;
// モーターPWM出力ピン（設計書より）
const uint8_t PIN_MOTOR_PWM = 3;
// モーター方向制御ピン（設計書より）
const uint8_t PIN_MOTOR_D1 = 4;
const uint8_t PIN_MOTOR_D2 = 5;

// 7セグ表示モジュールTM1637の接続ピン
const uint8_t PIN_7SEG_CLK = 6;
const uint8_t PIN_7SEG_DIO = 7;

TM1637Display display(PIN_7SEG_CLK, PIN_7SEG_DIO);


uint16_t potValue = 0;     // 0-1023の範囲でポテンショメータ値
uint8_t motorPwm = 0;      // 0-255の範囲でPWM値
int16_t motorCommand = 0;   // -255-255の範囲でモーター指令値

void setup() {
	pinMode(PIN_POT, INPUT); // ポテンショメータ入力ピンを初期化
	pinMode(PIN_MOTOR_PWM, OUTPUT); // モーターPWM出力ピンを初期化
	pinMode(PIN_MOTOR_D1, OUTPUT); // モーター方向制御ピンD1を初期化
	pinMode(PIN_MOTOR_D2, OUTPUT); // モーター方向制御ピンD2を初期化

	digitalWrite(PIN_MOTOR_D1, HIGH); // 初期方向を設定
	digitalWrite(PIN_MOTOR_D2, LOW);
	analogWrite(PIN_MOTOR_PWM, 0); // モーター停止

  display.setBrightness(0x0f); // 7セグ表示の明るさを設定
	display.clear();
}

void loop() {
	// ポテンショメータの値を読み取り、ADC範囲に制限
	int raw = analogRead(PIN_POT);
	raw = constrain(raw, 0, 1023);
	potValue = static_cast<uint16_t>(raw);

	// 0-1023の値を-255から255へ変換し、符号で回転方向を決定
	motorCommand = static_cast<int16_t>(map(potValue, 0, 1023, -255, 255));
	if (motorCommand >= 0) {
		digitalWrite(PIN_MOTOR_D1, HIGH);
		digitalWrite(PIN_MOTOR_D2, LOW);
	} else {
		digitalWrite(PIN_MOTOR_D1, LOW);
		digitalWrite(PIN_MOTOR_D2, HIGH);
	}

	// 絶対値をPWM値として出力してモーター速度を制御
	int pwm = abs(motorCommand);
	pwm = constrain(pwm, 0, 255);
	motorPwm = static_cast<uint8_t>(pwm);
	analogWrite(PIN_MOTOR_PWM, motorPwm);

  // 指令値を4桁7セグに表示（-255から255）
	display.showNumberDec(motorCommand, false, 4, 0);
}
   
