int count = 1;
int maxcount = 200;
int step = 0;   

void pulse() {
  PORTB |= (1<<1);
  delayMicroseconds(1);
  PORTB &= ~(1<<1);
  delayMicroseconds(150);
}

void send4(int x) {
  if(x&1) PORTD |= (1<<4); else PORTD &= ~(1<<4);
  if(x&2) PORTD |= (1<<5); else PORTD &= ~(1<<5);
  if(x&4) PORTD |= (1<<6); else PORTD &= ~(1<<6);
  if(x&8) PORTD |= (1<<7); else PORTD &= ~(1<<7);
  pulse();
}

void cmd(int x) {
  PORTB &= ~(1<<0);
  send4(x>>4);
  send4(x);
}

void data(char x) {
  PORTB |= (1<<0);
  send4(x>>4);
  send4(x);
}

void printnum(int n) {
  char a[5];
  sprintf(a,"%d",n);
  int i=0;
  while(a[i]) {
    data(a[i]);
    i++;
  }
}

void clearLine1() {
  cmd(0x80);
  for(int i=0;i<16;i++) data(' ');
}

void setup() {
  DDRB |= (1<<0) | (1<<1);
  DDRD |= (1<<4) | (1<<5) | (1<<6) | (1<<7);

  delay(100);

  send4(3); delay(5);
  send4(3); delay(5);
  send4(3); delay(1);
  send4(2);

  cmd(0x28);
  cmd(0x0C);
  cmd(0x06);
  cmd(0x01);
  delay(5);

  cmd(0x80);
  data('H'); data('e'); data('l'); data('l'); data('o');
  data(' '); data('w'); data('o'); data('r'); data('l'); data('d');
}

void loop() {
  cmd(0xC0);
  data(' '); data(' '); data(' '); data(' ');
  cmd(0xC0);
  printnum(count);

  delay(100);
  count++;

  if(count > maxcount) {
    count = 1;
    step++;

    if(step == 1) {
      clearLine1();
      cmd(0x80);
      data('H'); data('i'); data(' ');
      data('V'); data('i'); data('l'); data('a'); data('s'); data('a');
    }

    else if(step == 2) {
      clearLine1();
    }
  }
}