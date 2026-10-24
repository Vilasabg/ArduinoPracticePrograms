int latchPin = 4;
int clockPin = 7;
int dataPin  = 8;

#define A 0x01
#define B 0x02
#define C 0x04
#define D 0x08
#define E 0x10
#define F 0x20
#define G 0x40

uint8_t segment[10] = {
  A|B|C|D|E|F,//0
  F|E,        //1     
  A|B|G|E|D,  //2
  A|B|G|C|D,  //3
  F|G|B|C,    //4
  A|F|G|C|D,  //5
  A|F|E|D|C|G,//6
  A|B|C,      //7
  A|B|C|D|E|F|G,//8
  A|B|C|G|F|D  //9
};

bool COMMON_ANODE = true;

uint8_t getSeg(uint8_t num) {
  if (COMMON_ANODE)
    return ~segment[num];
  else
    return segment[num];
}

void send(uint8_t seg, uint8_t dig) {
  digitalWrite(latchPin, LOW);
  shiftOut(dataPin, clockPin, MSBFIRST, seg);
  shiftOut(dataPin, clockPin, MSBFIRST, dig);
  digitalWrite(latchPin, HIGH);
}

void displaySame(int num) {

  send(getSeg(num), 0x0E);  
  send(getSeg(num), 0x0D);  
  send(getSeg(num), 0x0B);  
  send(getSeg(num), 0x07);  

}

int count = 0;
unsigned long prevTime = 0;

void setup() {
  pinMode(latchPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(dataPin, OUTPUT);
}

void loop() {

  displaySame(count);

  if (millis() - prevTime >= 500) {
    prevTime = millis();
    count++;
    if (count > 9)
      count = 0;
  }
}