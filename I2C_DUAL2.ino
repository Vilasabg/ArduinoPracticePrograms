#include <Wire.h>
#include <SoftI2C.h>
#include <LiquidCrystal.h>

// -------- SWAPPED --------
#define PSU1 0x06   // Hardware I2C
#define PSU2 0x07   // Software I2C

SoftI2C myWire(A0, A1);   // SDA=A0, SCL=A1

LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

float v1=0,i1=0,v2=0,i2=0;

// -------- HW READ --------
float readHW(byte addr, byte cmd) {

  Wire.beginTransmission(addr);
  Wire.write(cmd);

  if (Wire.endTransmission(false) != 0) return -1;

  delay(5);

  Wire.requestFrom(addr, (uint8_t)2);

  if (Wire.available() == 2) {
    byte low  = Wire.read();
    byte high = Wire.read();

    int raw = (high << 8) | low;

    if (raw > 0 && raw < 60000)
      return raw * 0.01;
  }

  return -1;
}

// -------- SW READ --------
float readSW(byte addr, byte cmd) {

  myWire.beginTransmission(addr);
  myWire.write(cmd);

  if (myWire.endTransmission() != 0) return -1;

  delay(5);

  myWire.requestFrom(addr, (uint8_t)2);

  if (myWire.available() == 2) {
    byte low  = myWire.read();
    byte high = myWire.read();

    int raw = (high << 8) | low;

    if (raw > 0 && raw < 60000)
      return raw * 0.01;
  }

  return -1;
}

// -------- SMOOTH --------
float smooth(float oldVal, float newVal) {
  if (newVal < 0) return oldVal;
  return oldVal*0.7 + newVal*0.3;
}

void setup() {
  Wire.begin();
  myWire.begin();

  Serial.begin(9600);

  lcd.begin(16,2);
  lcd.print("Swap Test");
  delay(2000);
  lcd.clear();
}

void loop() {

  // ---- PSU1 (now 0x06 on hardware) ----
  float nv1 = readHW(PSU1, 0x8B);
  delay(50);
  float ni1 = readHW(PSU1, 0x8C);

  delay(150);

  // ---- PSU2 (now 0x07 on software) ----
  float nv2 = readSW(PSU2, 0x8B);
  delay(50);
  float ni2 = readSW(PSU2, 0x8C);

  v1 = smooth(v1, nv1);
  i1 = smooth(i1, ni1);
  v2 = smooth(v2, nv2);
  i2 = smooth(i2, ni2);

  Serial.print("PS1 V: "); Serial.print(v1);
  Serial.print(" I: "); Serial.print(i1);
  Serial.print(" | PS2 V: "); Serial.print(v2);
  Serial.print(" I: "); Serial.println(i2);

  lcd.setCursor(0,0);
  lcd.print("V1:");
  lcd.print(v1,1);
  lcd.print(" I1:");
  lcd.print(i1,1);
  lcd.print("   ");

  lcd.setCursor(0,1);
  lcd.print("V2:");
  lcd.print(v2,1);
  lcd.print(" I2:");
  lcd.print(i2,1);
  lcd.print("   ");

  delay(500);
}