#define INPUT_PIN 2
#define OUTPUT_PIN 3

volatile unsigned long startTime = 0;
volatile unsigned long width = 0;
volatile unsigned long period = 0;
volatile unsigned long lastRise = 0;

void setup() {
  pinMode(INPUT_PIN, INPUT);
  pinMode(OUTPUT_PIN, OUTPUT);

  Serial.begin(9600);

  attachInterrupt(digitalPinToInterrupt(INPUT_PIN), measure, CHANGE);
}

void loop() {

  digitalWrite(OUTPUT_PIN, HIGH);
  delayMicroseconds(500);
  digitalWrite(OUTPUT_PIN, LOW);
  delayMicroseconds(500);

  static unsigned long lastPrint = 0;

  if (millis() - lastPrint > 500) {
    lastPrint = millis();

    Serial.print("Width: ");
    Serial.print(width);

    if (period > 0) {
      Serial.print("  Freq: ");
      Serial.print(1000000UL / period);
    }

    Serial.println();
  }
}

void measure() {
  unsigned long t = micros();

  if (digitalRead(INPUT_PIN) == HIGH) {
    period = t - lastRise;
    lastRise = t;
    startTime = t;
  } else {
    width = t - startTime;
  }
}