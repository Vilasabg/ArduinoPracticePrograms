#include <avr/io.h>
#include <avr/interrupt.h>

#define F_CPU 16000000UL
#define PRESCALER 8

volatile uint16_t last_capture = 0;
volatile uint32_t freq = 0;
volatile uint8_t first_capture = 1;
volatile uint16_t overflow_count = 0;

// Timer1 overflow ISR
ISR(TIMER1_OVF_vect) {
    overflow_count++;
}

// Timer1 Input Capture ISR (D8)
ISR(TIMER1_CAPT_vect) {
    uint16_t now = ICR1;
    uint32_t ticks;

    if (first_capture) {
        last_capture = now;
        overflow_count = 0;
        first_capture = 0;
        return;
    }

    ticks = (uint32_t)overflow_count * 65536UL + (uint32_t)(now - last_capture);
    last_capture = now;
    overflow_count = 0;

    if (ticks != 0) {
        freq = (F_CPU / PRESCALER) / ticks;
    }
}

void setup() {
    Serial.begin(9600);

    // Input Capture setup (D8)
    pinMode(8, INPUT);
    TCCR1A = 0;
    TCCR1B = (1 << ICES1) | (1 << CS11); // rising edge, prescaler 8
    TIMSK1 |= (1 << ICIE1) | (1 << TOIE1); // enable input capture + overflow

    // Output signal on D9 (timer controlled LED blink)
    pinMode(9, OUTPUT);
    TCCR2A = (1 << COM2B0) | (1 << WGM21); // toggle OC2B (D9), CTC mode
    TCCR2B = (1 << CS22) | (1 << CS20);    // prescaler 128
    OCR2A = 12499;                          // ~5 Hz blinking LED

    sei(); // enable global interrupts
}

void loop() {
    Serial.println(freq); // print measured frequency
    delay(500);
}