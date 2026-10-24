#include <SoftI2C.h>
#include <LiquidCrystal.h>

SoftI2C myWire(A1, A0); 

#define PSU_ADDR 0x06

LiquidCrystal lcd(8, 9, 4, 5, 6, 7);


float readPMBus(byte command) {

  myWire.beginTransmission(PSU_ADDR);
  myWire.write(command);

  if (myWire.endTransmission(false) != 0) {
    return -1;
  }

  delay(20);

  myWire.requestFrom(PSU_ADDR, (uint8_t)2);

  if (myWire.available() < 2) {
    return -1;
  }

  byte low  = myWire.read();
  byte high = myWire.read();

  int value = (high << 8) | low;

  return value * 0.01;
}

void setup() {
  Serial.begin(9600);
  myWire.begin();

  lcd.begin(16, 2);
  lcd.print("PSU 0x06");
  delay(2000);
  lcd.clear();
}

void loop() {

  float voltage = readPMBus(0x8B); 
  delay(50);
  float current = readPMBus(0x8C);

  
  Serial.print("PSU06 V: ");
  Serial.print(voltage);
  Serial.print(" V  I: ");
  Serial.println(current);

  
  lcd.clear();

  if (voltage < 0 || current < 0) {
    lcd.setCursor(0,0);
    lcd.print("PSU06 ERROR");
    lcd.setCursor(0,1);
    lcd.print("Check I2C");
  } else {
    lcd.setCursor(0,0);
    lcd.print("V:");
    lcd.print(voltage, 2);
    lcd.print("V");

    lcd.setCursor(0,1);
    lcd.print("I:");
    lcd.print(current, 2);
    lcd.print("A");
  }

  delay(1000);
}