#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>

#define SS_PIN 10
#define RST_PIN 9
#define LED_OK 6
#define LED_FAIL 5

LiquidCrystal_I2C lcd(0x27, 16, 2);
MFRC522 rfid(SS_PIN, RST_PIN);

// Cấu hình danh sách: 5 thẻ, mỗi UID tối đa 4 byte (RC522 thường 4)
const int MAX_SLOTS = 5;
const int UID_LEN = 4;
const int EEPROM_BASE = 0;  // vùng bắt đầu lưu

void eepromWriteUID(int slot, const byte *uid) {
  int addr = EEPROM_BASE + slot * UID_LEN;
  for (int i = 0; i < UID_LEN; i++) EEPROM.write(addr + i, uid[i]);
}
void eepromReadUID(int slot, byte *uid) {
  int addr = EEPROM_BASE + slot * UID_LEN;
  for (int i = 0; i < UID_LEN; i++) uid[i] = EEPROM.read(addr + i);
}
bool eepromMatchAny(const byte *uid) {
  byte buf[UID_LEN];
  for (int s = 0; s < MAX_SLOTS; s++) {
    eepromReadUID(s, buf);
    bool ok = true;
    for (int i = 0; i < UID_LEN; i++)
      if (buf[i] != uid[i]) {
        ok = false;
        break;
      }
    if (ok) return true;
  }
  return false;
}
void clearSlot(int slot) {
  byte z[UID_LEN] = { 0, 0, 0, 0 };
  eepromWriteUID(slot, z);
}

void setup() {
  Serial.begin(9600);
  pinMode(LED_OK, OUTPUT);
  pinMode(LED_FAIL, OUTPUT);
  lcd.init();
  lcd.backlight();
  lcd.print("EEPROM Access");
  SPI.begin();
  rfid.PCD_Init();
  delay(600);
  lcd.clear();
  lcd.print("Ready");
  // Mặc định xoá slot 0 để demo dễ: (bình thường không xoá)
  // clearSlot(0);
}

void handleSerial() {
  if (!Serial.available()) return;
  String cmd = Serial.readStringUntil('\n');
  cmd.trim();

  if (cmd.startsWith("ADD ")) {  // ví dụ: ADD DE:AD:BE:EF
    byte uid[UID_LEN];
    int val;
    for (int i = 0; i < UID_LEN; i++) {
      int p = 4 + i * 3;  // sau "ADD "
      String hx = cmd.substring(p, p + 2);
      val = strtol(hx.c_str(), NULL, 16);
      uid[i] = (byte)val;
    }
    eepromWriteUID(0, uid);  // demo: ghi vào slot 0
    Serial.println("ADDED to slot 0");
  } else if (cmd == "DEL0") {
    clearSlot(0);
    Serial.println("CLEARED slot 0");
  } else if (cmd == "SHOW0") {
    byte u[UID_LEN];
    eepromReadUID(0, u);
    Serial.print("SLOT0=");
    for (int i = 0; i < UID_LEN; i++) {
      if (u[i] < 0x10) Serial.print("0");
      Serial.print(u[i], HEX);
      if (i < 3) Serial.print(":");
    }
    Serial.println();
  } else {
    Serial.println("Cmd? Use: ADD xx:xx:xx:xx | DEL0 | SHOW0");
  }
}

void loop() {
  handleSerial();

  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) return;

  byte *uid = rfid.uid.uidByte;
  byte sz = rfid.uid.size;
  if (sz != UID_LEN) {
    digitalWrite(LED_FAIL, HIGH);
    delay(400);
    digitalWrite(LED_FAIL, LOW);
    return;
  }

  if (eepromMatchAny(uid)) {
    digitalWrite(LED_OK, HIGH);
    lcd.clear();
    lcd.print("Welcome");
    delay(800);
    digitalWrite(LED_OK, LOW);
  } else {
    digitalWrite(LED_FAIL, HIGH);
    lcd.clear();
    lcd.print("Denied");
    delay(800);
    digitalWrite(LED_FAIL, LOW);
  }

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}
