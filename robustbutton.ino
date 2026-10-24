#define BUTTON 2

#define DEBOUNCE 20
#define LONG_PRESS 1000
#define REPEAT_INTERVAL 300
#define STUCK_TIME 5000

int buttonState = HIGH;
int lastReading = HIGH;

unsigned long changeTime = 0;
unsigned long pressTime = 0;
unsigned long lastRepeatTime = 0;

bool longDone = false;
bool stuckDone = false;

void setup() {
  pinMode(BUTTON, INPUT_PULLUP);
  Serial.begin(9600);
}

void loop() {
  int reading = digitalRead(BUTTON);
  unsigned long now = millis();

  if (reading != lastReading) {
    changeTime = now;
  }

  if ((now - changeTime) > DEBOUNCE) {

    if (reading != buttonState) {
      buttonState = reading;

      if (buttonState == LOW) {
        pressTime = now;
        lastRepeatTime = now;

        longDone = false;
        stuckDone = false;

        Serial.println("PRESS");
      } 
      else {
        if ((now - pressTime) < LONG_PRESS) {
          Serial.println("SHORT PRESS");
        }
        Serial.println("RELEASE");
      }
    }

    if (buttonState == LOW) {
      unsigned long duration = now - pressTime;

      if (duration >= LONG_PRESS && !longDone) {
        Serial.println("LONG PRESS");
        longDone = true;
      }

      if (duration >= LONG_PRESS) {
        if ((now - lastRepeatTime) >= REPEAT_INTERVAL) {
          Serial.println("REPEAT");
          lastRepeatTime = now;
        }
      }

      if (duration >= STUCK_TIME && !stuckDone) {
        Serial.println("STUCK");
        stuckDone = true;
      }
    }
  }

  lastReading = reading;
}