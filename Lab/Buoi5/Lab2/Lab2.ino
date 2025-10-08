#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define SS_PIN   10
#define RST_PIN   9
#define LED_OK    6
#define LED_FAIL  5
#define BUZZER    3   // optional

MFRC522 rfid(SS_PIN, RST_PIN);
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Thay bằng UID thực
byte ALLOWED_1[] = {0x63, 0xAA, 0xF8, 0x0D};  // ✅ UID hợp lệ của bạn
byte ALLOWED_2[] = {0x12, 0x34, 0x56, 0x78};  // có thể thêm thẻ thứ hai nếu muốn

bool matchUID(const byte *uid, const byte *allow, byte sz) {
  for (byte i = 0; i < sz; i++) 
    if (uid[i] != allow[i]) return false;
  return true;
}

bool isAllowed(const byte *uid, byte sz) {
  return matchUID(uid, ALLOWED_1, sz) || matchUID(uid, ALLOWED_2, sz);
}

void toneOK(){ tone(BUZZER, 1200, 120); }
void toneFAIL(){ tone(BUZZER, 300, 150); }

void uiIdle() {
  lcd.clear();
  lcd.setCursor(0,0); lcd.print("RFID Door Lock");
  lcd.setCursor(0,1); lcd.print("Tap your card...");
  digitalWrite(LED_OK, LOW);
  digitalWrite(LED_FAIL, LOW);
}

void setup() {
  pinMode(LED_OK, OUTPUT);
  pinMode(LED_FAIL, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  lcd.init(); 
  lcd.backlight();
  uiIdle();

  Serial.begin(9600);
  SPI.begin();
  rfid.PCD_Init();
  Serial.println("RFID system ready...");
}

void loop() {
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) return;

  byte *uid = rfid.uid.uidByte;
  byte sz   = rfid.uid.size;

  Serial.print("UID (");
  Serial.print(sz);
  Serial.print(" bytes): ");
  for (byte i=0; i<sz; i++) {
    if (uid[i] < 0x10) Serial.print("0");
    Serial.print(uid[i], HEX);
    Serial.print(i < sz - 1 ? ":" : "\n");
  }

  if (isAllowed(uid, sz)) {
    digitalWrite(LED_OK, HIGH);
    digitalWrite(LED_FAIL, LOW);
    lcd.clear();
    lcd.setCursor(0,0); lcd.print("Access Granted!");
    lcd.setCursor(0,1); lcd.print("Welcome!");
    toneOK();
  } else {
    digitalWrite(LED_OK, LOW);
    digitalWrite(LED_FAIL, HIGH);
    lcd.clear();
    lcd.setCursor(0,0); lcd.print("Access Denied!");
    toneFAIL();
  }

  delay(1500);
  uiIdle();

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}
