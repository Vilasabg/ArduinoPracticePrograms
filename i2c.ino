#include <LiquidCrystal.h>

// RS, EN, D4, D5, D6, D7
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

void setup() {
  lcd.begin(16, 2);

  lcd.setCursor(0, 0);
  lcd.print("hi manish");

  //lcd.setCursor(0, 1);
  //lcd.print("Keypad Shield");
}

void loop() {
}