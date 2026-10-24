void setup() {
  Serial.begin(9600);
  pinMode(9, OUTPUT); 
  pinMode(8, OUTPUT); 
}

unsigned long previous1 = 0;
unsigned long previous2 = 0;

void loop() {
  unsigned long now = millis();

  if (now - previous1 >= 500) {
    previous1 = now;
    digitalWrite(9, !digitalRead(9));
    Serial.println("Task 1");
  }

  if (now - previous2 >= 1000) {
    previous2 = now;
    digitalWrite(8, !digitalRead(8));
    Serial.println("Task 2");
  }
}