#define PIN A0
#define N_SAMPLES 10

#define SHORT_GND 50
#define SHORT_VCC 1000
#define OPEN_LOW 300  
#define OPEN_HIGH 700

void setup() {
  Serial.begin(9600);
}

void loop() {
  long sum = 0;

  for(int i=0; i<N_SAMPLES; i++) {
    sum += analogRead(PIN);
    delay(5);
  }

  int adc = sum / N_SAMPLES;
  String fault = "OK";

  if(adc <= SHORT_GND) fault = "SHORT_GND";
  else if(adc >= SHORT_VCC) fault = "SHORT_VCC";
  else if(adc >= OPEN_LOW && adc <= OPEN_HIGH) fault = "OPEN";

  Serial.print("ADC: ");
  Serial.print(adc);
  Serial.print("  Fault: ");
  Serial.println(fault);

  delay(300);
}