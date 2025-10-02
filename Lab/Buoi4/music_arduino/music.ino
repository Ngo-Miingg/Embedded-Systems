#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

const int BUZ_PIN   = 10;
const int RED_PIN   = 9;
const int GREEN_PIN = 6;
const int BLUE_PIN  = 5;
const int BUTTON_PIN = 2;
const int POT_PIN   = A0; // Biến trở điều chỉnh âm lượng

// Nốt nhạc Happy Birthday (tần số Hz)
int melody[] = {
  262, 262, 294, 262, 349, 330,
  262, 262, 294, 262, 392, 349,
  262, 262, 523, 440, 349, 330, 294,
  466, 466, 440, 349, 392, 349
};

// Trường độ từng nốt (4 = nốt đen, 8 = móc đơn)
int noteDurations[] = {
  8,8,4,4,4,2,
  8,8,4,4,4,2,
  8,8,4,4,4,4,2,
  8,8,4,4,4,2
};

void setup() {
  pinMode(BUZ_PIN, OUTPUT);
  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0,0);
  lcd.print("Press button");
  lcd.setCursor(0,1);
  lcd.print("For surprise!");
}

void setColor(int r, int g, int b) {
  analogWrite(RED_PIN, r);
  analogWrite(GREEN_PIN, g);
  analogWrite(BLUE_PIN, b);
}

// Hàm phát nhạc với chỉnh âm lượng
void playSong() {
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("HAPPY BIRTHDAY!");
  lcd.setCursor(0,1);
  lcd.print("To You!");

  for (int i = 0; i < 25; i++) {
    int duration = 1000 / noteDurations[i];

    // Đọc POT → giá trị âm lượng
    int raw_value = analogRead(POT_PIN);        // 0–1023
    int volume = map(raw_value, 0, 1023, 0, 255); // PWM 0–255

    // Tạo âm bằng tone()
    tone(BUZ_PIN, melody[i], duration);

    // Đồng bộ LED đổi màu
    setColor(random(0, volume), random(0, volume), random(0, volume));

    // In giá trị ra LCD (hàng 2)
    lcd.setCursor(0,1);
    lcd.print("Vol:");
    lcd.print(volume);
    lcd.print("   "); // xóa ký tự thừa

    delay(duration * 1.3);
    noTone(BUZ_PIN);
  }

  setColor(255,150,0); // kết thúc với màu vàng ấm
}

void loop() {
  if (digitalRead(BUTTON_PIN) == LOW) {
    playSong();
    delay(2000); // chờ trước khi chơi lại
  }
}
