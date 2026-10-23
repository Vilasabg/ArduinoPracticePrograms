#define LED 13
#define BOOT_DELAY 2000  
unsigned long startTime;
bool systemReady = false;

void setup() {
  digitalWrite(LED, LOW);
  pinMode(LED, OUTPUT);
  startTime = millis();
}

void loop() {
  if (!systemReady) {
    if (millis() - startTime > BOOT_DELAY) {
      systemReady = true;  
    }
    return;  

  digitalWrite(LED, HIGH);
  delay(1000);

  digitalWrite(LED, LOW);
  delay(1000);
}