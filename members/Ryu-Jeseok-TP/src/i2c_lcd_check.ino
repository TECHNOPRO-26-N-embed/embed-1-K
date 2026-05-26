#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// よく使われるLCD I2Cアドレス候補
const byte CANDIDATE_COUNT = 2;
const uint8_t CANDIDATE_ADDR[CANDIDATE_COUNT] = {0x27, 0x3F};

LiquidCrystal_I2C *lcd = nullptr;

bool existsOnI2C(uint8_t addr) {
  Wire.beginTransmission(addr);
  return (Wire.endTransmission() == 0);
}

void setup() {
  Serial.begin(9600);
  Wire.begin();

  Serial.println("I2C scan start...");

  uint8_t foundAddr = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    if (existsOnI2C(addr)) {
      Serial.print("Found I2C device: 0x");
      if (addr < 16) Serial.print('0');
      Serial.println(addr, HEX);
      if (foundAddr == 0) foundAddr = addr;
    }
  }

  // 0x27 / 0x3F 優先でLCDとして初期化を試す
  for (byte i = 0; i < CANDIDATE_COUNT; i++) {
    if (existsOnI2C(CANDIDATE_ADDR[i])) {
      foundAddr = CANDIDATE_ADDR[i];
      break;
    }
  }

  if (foundAddr == 0) {
    Serial.println("No I2C device found. Check SDA(A4), SCL(A5), VCC, GND.");
    return;
  }

  lcd = new LiquidCrystal_I2C(foundAddr, 16, 2);
  lcd->init();
  lcd->backlight();
  lcd->setCursor(0, 0);
  lcd->print("LCD OK Addr:");
  lcd->setCursor(0, 1);
  lcd->print("0x");
  if (foundAddr < 16) lcd->print('0');
  lcd->print(foundAddr, HEX);

  Serial.print("LCD initialized at 0x");
  if (foundAddr < 16) Serial.print('0');
  Serial.println(foundAddr, HEX);
  Serial.println("If text is blank, turn contrast screw on I2C backpack.");
}

void loop() {
}
