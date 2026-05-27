#include <SPI.h>
#include <MFRC522.h>

constexpr uint8_t RST_PIN = 9;
constexpr uint8_t SS_PIN = 10;

#define UID "21 A3 4D 5D" // 取得した識別子を記述

int led_blue = 8;
int led_red = 5; // D3(PWM)はtone()と競合しやすいためD5を使用
int buzzerPin = 7; // パッシブブザー

MFRC522 mfrc522(SS_PIN, RST_PIN);
MFRC522::MIFARE_Key key;

void dump_byte_array(byte *buffer, byte bufferSize);
void playSuccessTone();
void playErrorTone();

void setup() {
  Serial.begin(9600);
  pinMode(led_blue, OUTPUT);
  pinMode(led_red, OUTPUT);
  pinMode(buzzerPin, OUTPUT);

  while (!Serial);
  SPI.begin();
  mfrc522.PCD_Init();
  mfrc522.PCD_DumpVersionToSerial();
  Serial.println(F("Scan PICC to see UID, SAK, type, and data blocks..."));

  dump_byte_array(key.keyByte, MFRC522::MF_KEY_SIZE);
  Serial.println();
}

void loop() {
  if (!mfrc522.PICC_IsNewCardPresent()) {
    return;
  }
  if (!mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  String strBuf[mfrc522.uid.size];
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    strBuf[i] = String(mfrc522.uid.uidByte[i], HEX);
    if (strBuf[i].length() == 1) {
      strBuf[i] = "0" + strBuf[i];
    }
  }

  String strUID = strBuf[0] + " " + strBuf[1] + " " + strBuf[2] + " " + strBuf[3];
  if (strUID.equalsIgnoreCase(UID)) {
    Serial.println("chantoku");
    digitalWrite(led_blue, HIGH);
    playSuccessTone();
    delay(1000);
    digitalWrite(led_blue, LOW);
  } else {
    Serial.println("error!");
    analogWrite(led_red, 180);
    playErrorTone();
    delay(1000);
    analogWrite(led_red, 0);
  }

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
}

void playSuccessTone() {
  tone(buzzerPin, 1047, 120); // C6
  delay(150);
  tone(buzzerPin, 1319, 120); // E6
  delay(150);
  tone(buzzerPin, 1568, 180); // G6
  delay(220);
  noTone(buzzerPin);
}

void playErrorTone() {
  tone(buzzerPin, 440, 250); // A4
  delay(300);
  tone(buzzerPin, 262, 350); // C4
  delay(400);
  noTone(buzzerPin);
}

void dump_byte_array(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
}
