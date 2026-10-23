#define N 300
#define PIN A0

int samples[N];

void setup() {
  Serial.begin(19200);   

  pinMode(9, OUTPUT);
  analogWrite(9, 128);  
}

void loop() {

  unsigned long t1 = micros();

  for(int i = 0; i < N; i++) {
    samples[i] = analogRead(PIN);
    delayMicroseconds(200);  
  }

  unsigned long t2 = micros();

  float sampling_rate = (N * 1000000.0) / (t2 - t1);

  long sum = 0;
  for(int i = 0; i < N; i++) {
    sum += samples[i];
  }
  int mid = sum / N;
  long sum_sq = 0;
  for(int i = 0; i < N; i++) {
    int x = samples[i] - mid;
    sum_sq += (long)x * x;
  }

  float rms = sqrt(sum_sq / (float)N);

  int crossings = 0;
  int threshold = 10;

  for(int i = 1; i < N; i++) {
    int prev = samples[i-1] - mid;
    int curr = samples[i] - mid;

    if(prev < -threshold && curr >= threshold) {
      crossings++;
    }
  }

  float freq = (crossings * sampling_rate) / N;

  
  Serial.print("Mid: ");
  Serial.print(mid);

  Serial.print("  RMS: ");
  Serial.print(rms);

  Serial.print("  Freq: ");
  Serial.println(freq);

  delay(500);
}