#include <Arduino.h>

int rs=8,en=9,d4=4,d5=5,d6=6,d7=7;

__attribute__((used, section(".my_section")))
const char msg[] = "HI";

void pulse(){
  digitalWrite(en,HIGH);
  delayMicroseconds(1);
  digitalWrite(en,LOW);
  delayMicroseconds(100);
}

void send4(int data){
  digitalWrite(d4, (data>>0)&1);
  digitalWrite(d5, (data>>1)&1);
  digitalWrite(d6, (data>>2)&1);
  digitalWrite(d7, (data>>3)&1);
  pulse();
}

void lcd_cmd(int cmd){
  digitalWrite(rs,LOW);
  send4(cmd>>4);
  send4(cmd);
}

void lcd_data(char data){
  digitalWrite(rs,HIGH);
  send4(data>>4);
  send4(data);
}

void lcd_print(const char *str){
  while(*str){
    lcd_data(*str++);
  }
}

void lcd_init(){
  pinMode(rs,OUTPUT);
  pinMode(en,OUTPUT);
  pinMode(d4,OUTPUT);
  pinMode(d5,OUTPUT);
  pinMode(d6,OUTPUT);
  pinMode(d7,OUTPUT);

  delay(20);

  lcd_cmd(0x02); 
  lcd_cmd(0x28); 
  lcd_cmd(0x0C); 
  lcd_cmd(0x06); 
  lcd_cmd(0x01); 
}

void setup(){
  Serial.begin(9600);

  lcd_init();

  lcd_cmd(0x80);   
  lcd_print(msg); 

  Serial.println(msg);
}

void loop(){
}