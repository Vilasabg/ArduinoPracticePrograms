#include <avr/io.h>
#include <avr/interrupt.h>

#define F_CPU 16000000UL
#define BAUD 9600
#define UBRR_VAL ((F_CPU / (16UL * BAUD)) - 1)

#define BUF_SIZE 32  

typedef struct {
  volatile uint8_t buf[BUF_SIZE];
  volatile uint8_t head;
  volatile uint8_t tail;
} ring_t;

ring_t rx_rb = {0};
ring_t tx_rb = {0};

volatile uint16_t frame_err = 0;
volatile uint16_t parity_err = 0;
volatile uint16_t overrun_err = 0;
volatile uint16_t soft_overflow = 0;

uint8_t rb_is_empty(ring_t *rb) {
  return rb->head == rb->tail;
}

uint8_t rb_is_full(ring_t *rb) {
  return ((rb->head + 1) % BUF_SIZE) == rb->tail;
}

void rb_push(ring_t *rb, uint8_t d) {
  uint8_t next = (rb->head + 1) % BUF_SIZE;
  if (next != rb->tail) {
    rb->buf[rb->head] = d;
    rb->head = next;
  } else {
    soft_overflow++;
  }
}

uint8_t rb_pop(ring_t *rb, uint8_t *d) {
  if (rb_is_empty(rb)) return 0;
  *d = rb->buf[rb->tail];
  rb->tail = (rb->tail + 1) % BUF_SIZE;
  return 1;
}

void uart_init() {
  UBRR0H = (UBRR_VAL >> 8);
  UBRR0L = UBRR_VAL;

  UCSR0B = (1 << RXEN0) | (1 << TXEN0) | (1 << RXCIE0);
  UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

ISR(USART_RX_vect) {
  uint8_t status = UCSR0A;
  uint8_t data   = UDR0;

  if (status & (1 << FE0)) { frame_err++; return; }
  if (status & (1 << UPE0)) { parity_err++; return; }
  if (status & (1 << DOR0)) { overrun_err++; return; }

  rb_push(&rx_rb, data);
}

ISR(USART_UDRE_vect) {
  uint8_t d;

  if (rb_pop(&tx_rb, &d)) {
    UDR0 = d;
  } else {
    UCSR0B &= ~(1 << UDRIE0);
  }
}

void uart_write(uint8_t d) {
  while (rb_is_full(&tx_rb));
  rb_push(&tx_rb, d);
  UCSR0B |= (1 << UDRIE0);
}

uint8_t uart_read(uint8_t *d) {
  return rb_pop(&rx_rb, d);
}

void uart_print(const char *s) {
  while (*s) uart_write(*s++);
}

void process_uart()
{
  static char cmd[3];
  static uint8_t idx = 0;
  uint8_t d;

  while (uart_read(&d)) {

    if (d == '\n' || d == '\r') continue;

    if (idx < 2) {
      cmd[idx++] = d;
    }

    if (idx == 2) {
      cmd[2] = '\0';

      if (cmd[0] == 'h' && cmd[1] == 'i') {
        uart_print("hello\n");
      } else {
        uart_write(cmd[0]);
        uart_write(cmd[1]);
        uart_write('\n');
      }

      idx = 0;
    }
  }
}

void stress_test()
{
  static uint8_t val = 0;
  uart_write(val++); 
}

void setup() {
  uart_init();
  sei();

  uart_print("UART READY\n");
}

void loop() {

  process_uart();   

}