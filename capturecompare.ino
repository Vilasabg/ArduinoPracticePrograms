int prevState = -1; 

void setup() {
  Serial.begin(9600);
}

void loop() {
  int currValue = analogRead(A0);
  int currentState;

  if (currValue < 100) {
    currentState = 0; 
  }
  else if (currValue > 900) {
    currentState = 1;
  }
  else {
    return;
  }

  if (currentState != prevState) {
    if (currentState == 1) {
      Serial.println("INCREASED");
    } else {
      Serial.println("DECREASED");
    }
    prevState = currentState;
  }

  delay(200);
}