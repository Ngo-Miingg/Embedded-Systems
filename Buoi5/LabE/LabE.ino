#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>

// ====== CẤU HÌNH PHẦN CỨNG ======
#define SS_PIN      10
#define RST_PIN     9
#define LED_OK      6
#define LED_FAIL    5
#define BUZZER_PIN  7
#define LCD_ADDR    0x27   // đổi nhanh 0x27 hoặc 0x3F tùy module LCD

// ====== CẤU HÌNH DỮ LIỆU ======
#define UID_LEN     4
#define MAX_SLOTS   5
#define EEPROM_BASE 0

// ====== BIẾN TOÀN CỤC ======
LiquidCrystal_I2C lcd(LCD_ADDR, 16, 2);
MFRC522 rfid(SS_PIN, RST_PIN);

int failCount = 0;           // Đếm số lần sai
unsigned long lockStart = 0; // Thời điểm bắt đầu khóa
bool isLocked = false;

// ====== QUẢN LÝ EEPROM ======
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
      if (buf[i] != uid[i]) { ok = false; break; }
    if (ok) return true;
  }
  return false;
}

void clearSlot(int slot) {
  byte z[UID_LEN] = {0, 0, 0, 0};
  eepromWriteUID(slot, z);
}

// ====== LCD UI ======
void uiIdle() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Ready...");
  lcd.setCursor(0, 1);
  lcd.print("Tap your card");
}

void uiShowUID(const char *msg, const byte *uid) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(msg);
  lcd.setCursor(0, 1);
  lcd.print("ID:");
  for (int i = 0; i < UID_LEN; i++) {
    if (uid[i] < 0x10) lcd.print("0");
    lcd.print(uid[i], HEX);
    if (i < UID_LEN - 1) lcd.print(":");
  }
}

// ====== Buzzer / LED ======
void beepOK() {
  for (int i = 0; i < 2; i++) {
    tone(BUZZER_PIN, 1500, 80);
    digitalWrite(LED_OK, HIGH);
    delay(120);
    digitalWrite(LED_OK, LOW);
    delay(80);
  }
}

void beepFail() {
  tone(BUZZER_PIN, 400, 400);
  digitalWrite(LED_FAIL, HIGH);
  delay(400);
  digitalWrite(LED_FAIL, LOW);
}

// ====== RFID ======
bool readUID(byte *uid) {
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) return false;
  for (int i = 0; i < UID_LEN; i++) uid[i] = rfid.uid.uidByte[i];
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  return true;
}

// ====== Serial command ======
void handleSerial() {
  if (!Serial.available()) return;
  String cmd = Serial.readStringUntil('\n');
  cmd.trim();

  if (cmd.startsWith("ADD")) {
    int slot = cmd.charAt(3) - '0';
    if (slot < 0 || slot >= MAX_SLOTS) { Serial.println("Slot? 0–4"); return; }
    String data = cmd.substring(5);
    data.replace(":", " ");
    data.trim();

    byte uid[UID_LEN]; int count = 0;
    while (data.length() > 0 && count < UID_LEN) {
      int space = data.indexOf(' ');
      String part = (space == -1) ? data : data.substring(0, space);
      uid[count++] = (byte)strtol(part.c_str(), NULL, 16);
      if (space == -1) break;
      data = data.substring(space + 1);
    }
    if (count == UID_LEN) {
      eepromWriteUID(slot, uid);
      Serial.print("Added slot "); Serial.println(slot);
    } else Serial.println("Use: ADD0 DE:AD:BE:EF");

  } else if (cmd.startsWith("DEL")) {
    int slot = cmd.charAt(3) - '0';
    clearSlot(slot);
    Serial.print("Cleared slot "); Serial.println(slot);

  } else if (cmd == "LIST") {
    Serial.println("Slots:");
    byte u[UID_LEN];
    for (int i = 0; i < MAX_SLOTS; i++) {
      eepromReadUID(i, u);
      Serial.print("SLOT"); Serial.print(i); Serial.print(" = ");
      for (int j = 0; j < UID_LEN; j++) {
        if (u[j] < 0x10) Serial.print("0");
        Serial.print(u[j], HEX);
        if (j < UID_LEN - 1) Serial.print(":");
      }
      Serial.println();
    }

  } else {
    Serial.println("Cmds: ADDn UID | DELn | LIST");
  }
}

// ====== SETUP ======
void setup() {
  Serial.begin(9600);
  SPI.begin();
  rfid.PCD_Init();
  pinMode(LED_OK, OUTPUT);
  pinMode(LED_FAIL, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  lcd.init();
  lcd.backlight();
  uiIdle();
  Serial.println("RFID Access Ready");
}

// ====== MAIN LOOP ======
void loop() {
  handleSerial();

  if (isLocked) {
    if (millis() - lockStart >= 2000) { isLocked = false; failCount = 0; uiIdle(); }
    else return;
  }

  byte uid[UID_LEN];
  if (!readUID(uid)) return;

  bool ok = eepromMatchAny(uid);
  uiShowUID(ok ? "Welcome" : "Denied", uid);

  if (ok) {
    failCount = 0;
    beepOK();
  } else {
    failCount++;
    beepFail();
    if (failCount >= 3) {
      lcd.clear();
      lcd.print("Wait 2s...");
      isLocked = true;
      lockStart = millis();
    }
  }

  // Ghi nhật ký ra Serial (timestamp,uid,result)
  Serial.print(millis());
  Serial.print(",");
  for (int i = 0; i < UID_LEN; i++) {
    if (uid[i] < 0x10) Serial.print("0");
    Serial.print(uid[i], HEX);
    if (i < UID_LEN - 1) Serial.print(":");
  }
  Serial.print(",");
  Serial.println(ok ? "OK" : "FAIL");

  delay(100);
  uiIdle();
}
