#include <Wire.h>
#include <LiquidCrystal.h>

#define PSU1 0x07
#define PSU2 0x06

LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

float readPMBus(byte addr, byte command) {

  Wire.beginTransmission(addr);
  Wire.write(command);

  if (Wire.endTransmission(false) != 0) {
    return -1;
  }

  Wire.requestFrom(addr, (uint8_t)2);

  if (Wire.available() == 2) {
    byte low  = Wire.read();
    byte high = Wire.read();

    int value = (high << 8) | low;

    Serial.print("Addr ");
    Serial.print(addr);
    Serial.print(" RAW: ");
    Serial.println(value);

    return value * 0.01;  
  }

  return -1;
}

void setup() {
  Wire.begin();  
  Serial.begin(9600);

  lcd.begin(16, 2);
  lcd.print("Dual PSU");
  delay(2000);
  lcd.clear();
}

void loop() {

  float v1 = readPMBus(PSU1, 0x8B);
  delay(100);
  float i1 = readPMBus(PSU1, 0x8C);

  delay(200);

  float v2 = readPMBus(PSU2, 0x8B);
  delay(100);
  float i2 = readPMBus(PSU2, 0x8C);

  delay(200);

  Serial.print("PS1 V: "); Serial.print(v1);
  Serial.print(" I: "); Serial.print(i1);

  Serial.print(" | PS2 V: "); Serial.print(v2);
  Serial.print(" I: "); Serial.println(i2);

  lcd.setCursor(0,0);
  if (v1 < 0 || i1 < 0) {
    lcd.print("PS1 Error     ");
  } else {
    lcd.print("V1:");
    lcd.print(v1,1);
    lcd.print(" I1:");
    lcd.print(i1,1);
    lcd.print("   ");
  }

  lcd.setCursor(0,1);
  if (v2 < 0 || i2 < 0) {
    lcd.print("PS2 Error     ");
  } else {
    lcd.print("V2:");
    lcd.print(v2,1);
    lcd.print(" I2:");
    lcd.print(i2,1);
    lcd.print("   ");
  }

  delay(1000);
}