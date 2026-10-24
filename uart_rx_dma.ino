#define SIZE 16

uint8_t buf[SIZE];
uint8_t head = 0;
uint8_t tail = 0;

void setup() {
    Serial.begin(9600);
}

void loop() {

    while (Serial.available()) {

        uint8_t data = Serial.read();

        uint8_t next = (head + 1) % SIZE;

        if (next != tail) {
            buf[head] = data;
            head = next;
        }
    }

    while (tail != head) {

        uint8_t data = buf[tail];
        tail = (tail + 1) % SIZE;

        if(data != '\n' && data != '\r') {
        Serial.print("RX: ");
        Serial.println((char)data);
}
    }
}