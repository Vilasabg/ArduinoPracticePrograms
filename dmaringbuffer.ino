#include <Arduino.h>

#define SIZE 8

volatile uint16_t buffer[SIZE];
volatile uint8_t index = 0;

volatile uint8_t half_flag = 0;
volatile uint8_t full_flag = 0;

/* ===== ADC ISR ===== */
ISR(ADC_vect)
{
    buffer[index] = ADC;   // read ADC value

    index++;

    if (index == SIZE/2)
        half_flag = 1;

    if (index == SIZE)
    {
        full_flag = 1;
        index = 0;   // circular buffer restart
    }
}

/* ===== Setup ===== */
void setup()
{
    Serial.begin(9600);

    // ADC setup
    ADMUX = (1 << REFS0);  // AVcc reference, ADC0

    ADCSRA = (1 << ADEN)  |  // Enable ADC
             (1 << ADIE)  |  // Enable ADC interrupt
             (1 << ADATE) |  // Auto trigger
             (1 << ADPS2) | (1 << ADPS1); // Prescaler = 64

    sei();  // Enable global interrupts

    ADCSRA |= (1 << ADSC);  // Start ADC
}

/* ===== Loop ===== */
void loop()
{
    if (half_flag)
    {
        half_flag = 0;

        Serial.print("Half: ");
        for (int i = 0; i < SIZE/2; i++)
        {
            Serial.print(buffer[i]);
            Serial.print(" ");
        }
        Serial.println();
    }

    if (full_flag)
    {
        full_flag = 0;

        Serial.print("Full: ");
        for (int i = SIZE/2; i < SIZE; i++)
        {
            Serial.print(buffer[i]);
            Serial.print(" ");
        }
        Serial.println();
    }
}