#include <Arduino.h>

unsigned long t = 0;
int state = 0;

void pulse() {
  PORTB |= (1<<1);        
  delayMicroseconds(1);
  PORTB &= ~(1<<1);       
  delayMicroseconds(100);
}

void send4(uint8_t x) {
  PORTD &= 0x0F;              
  PORTD |= (x << 4) & 0xF0;   
  pulse();
}

void cmd(uint8_t x) {
  PORTB &= ~(1<<0);  
  send4(x >> 4);
  send4(x);
}

void data(uint8_t x) {
  PORTB |= (1<<0);   
  send4(x >> 4);
  send4(x);
}

void print(const char *s) {
  while(*s) data(*s++);
}

void lcd_init() {
  DDRB |= (1<<0) | (1<<1); 
  DDRD |= (1<<4) | (1<<5) | (1<<6) | (1<<7);

  delay(100);

  send4(0x03); delay(5);
  send4(0x03); delay(5);
  send4(0x03); delay(1);
  send4(0x02);

  cmd(0x28);  
  cmd(0x0C);   
  cmd(0x06);   
  cmd(0x01);   
  delay(5);
}

void setup() {
  lcd_init();

  cmd(0x80);
  print("HELLO WORLD");

  t = millis();
  state = 1;
}

void loop() {

  if(state == 1 && millis() - t > 2000) {
    cmd(0x01);
    delayMicroseconds(2000);  

    cmd(0x80);
    print("HI VILASA");

    t = millis();
    state = 2;
  }

  else if(state == 2 && millis() - t > 2000) {
    cmd(0x01);
    delayMicroseconds(2000);

    cmd(0x80);
    print("HELLO WORLD");

    t = millis();
    state = 1;
  }
}