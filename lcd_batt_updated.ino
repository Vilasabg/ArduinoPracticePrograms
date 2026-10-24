#define F_CPU 8000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/atomic.h>
#include <string.h>
#include <stdbool.h>

#define AI __attribute__((always_inline)) static inline

// ===== Timing helpers (only used where explicitly blocking) =====
#define DELAY_US(t) __builtin_avr_delay_cycles( (t) * (F_CPU / 1000000UL) )
#define DELAY_MS(t) __builtin_avr_delay_cycles( (t) * (F_CPU / 1000UL) )

// ===== LCD pin map (our local LCD) =====
    #define LCD_RS     PD3
    #define LCD_EN     PD4
    #define LCD_D4     PD5
    #define LCD_D5     PD6
    #define LCD_D6     PD7
    #define LCD_D7     PB0

// ===== Sniff inputs from the master LCD bus =====
#define RS_IN      PC0
#define D4_IN      PC2
#define D5_IN      PC3
#define D6_IN      PC4
#define D7_IN      PC5
#define E_IN       PD2  // INT0

// ===== Bit-atomic helpers for PORTB (avoid full-port RMW) =====
AI void pb_set(uint8_t bit){ asm volatile("sbi %0,%1"::"I"(_SFR_IO_ADDR(PORTB)),"I"(bit)); }
AI void pb_clr(uint8_t bit){ asm volatile("cbi %0,%1"::"I"(_SFR_IO_ADDR(PORTB)),"I"(bit)); }

// ===== Bit-atomic helpers for PORTD low bits with shadow (PD0..PD4) =====
static volatile uint8_t portd_low_shadow = 0; // mirrors desired levels of PD0..PD4

AI void pd_set(uint8_t bit){
  asm volatile("sbi %0,%1"::"I"(_SFR_IO_ADDR(PORTD)),"I"(bit));
  portd_low_shadow |= (uint8_t)(1u << bit);
}
AI void pd_clr(uint8_t bit){
  asm volatile("cbi %0,%1"::"I"(_SFR_IO_ADDR(PORTD)),"I"(bit));
  portd_low_shadow &= (uint8_t)~(1u << bit);
}

// ===== Globals =====
volatile char displayBuf[4][20];
volatile uint8_t nibble, byteBuf;
volatile bool highNibble = true;
volatile uint8_t curAddr = 0;
volatile uint8_t updateCounter = 0;
volatile bool divisionFlag = false;
volatile bool byteReceived = false;

// Track rows ever touched by the master since last Clear/PD1 reinit
static volatile uint8_t rowEverTouched[4] = {0,0,0,0};

// Shadow of what we last sent to our LCD (avoid rewriting unchanged rows)
static char lastFrame[4][20];  // zero-inited → first update forced

// Thresholds
uint16_t highThresholdEnter, highThresholdExit;
uint16_t lowThresholdEnter, lowThresholdExit;

// Precomputed row offsets for 20x4 DDRAM (our LCD)
static const uint8_t rowOffsets[4] = {0x00, 0x40, 0x14, 0x54};

// Hysteresis states for Mains alerts
typedef enum { NORMAL, MAINS_HIGH, MAINS_LOW, MAINS_FAIL } AlertState;
volatile AlertState gridAlertState = NORMAL;

// Hysteresis states for Load alerts
typedef enum { NORMAL_LOAD, OVER_LOAD } LoadState;
volatile LoadState loadState = NORMAL_LOAD;

// Millis counter for non-blocking timing (100 ms tick)
volatile uint32_t millis_counter = 0;

// Current output voltage and load
uint16_t current_out_val = 0;
volatile uint16_t load_val = 0;
volatile bool overLoadFlag = false;

// Overload timer
volatile uint32_t overload_start_time = 0;
volatile bool overload_timer_active = false;
volatile bool tripped = false;

// Confirmation counters
uint8_t overload_enter_count = 0;
uint8_t overload_exit_count  = 0;

// PD1 high timer for reset
volatile uint32_t pd1_high_start = 0;
volatile bool pd1_high_timer_active = false;

// Track the row where "Out Load" pattern was last seen
volatile uint8_t out_load_row = 255; // invalid

// Master overload flag
volatile bool masterOverloadFlag = false;

// Grid displayed value
volatile uint16_t grid_displayed = 0;

// ===== Utils =====
static inline uint32_t millis_now(void){
    uint32_t t; ATOMIC_BLOCK(ATOMIC_RESTORESTATE){ t = millis_counter; } return t;
}

// 00..99 table in PROGMEM
static const char d2[100][2] PROGMEM = {
    "00","01","02","03","04","05","06","07","08","09",
    "10","11","12","13","14","15","16","17","18","19",
    "20","21","22","23","24","25","26","27","28","29",
    "30","31","32","33","34","35","36","37","38","39",
    "40","41","42","43","44","45","46","47","48","49",
    "50","51","52","53","54","55","56","57","58","59",
    "60","61","62","63","64","65","66","67","68","69",
    "70","71","72","73","74","75","76","77","78","79",
    "80","81","82","83","84","85","86","87","88","89",
    "90","91","92","93","94","95","96","97","98","99"
};

AI void put3(char *dst, uint16_t v){
    uint8_t h = 0;
    while (v >= 100) { v -= 100; h++; }    // v now 0..99, h = hundreds
    uint8_t tens = v / 10;
    uint8_t ones = v - (uint8_t)(tens * 10);
    dst[0] = '0' + h;
    dst[1] = '0' + tens;
    dst[2] = '0' + ones;
}


AI bool match(const char* p, const char* lit, uint8_t n){
    return memcmp(p, lit, n) == 0;
}

#define COPY_LIT(dst, idx, S) do{ \
    static const char __c[] PROGMEM = S; \
    memcpy_P(&(dst)[idx], __c, sizeof(__c)-1); \
    (idx) += (uint8_t)(sizeof(__c)-1); \
}while(0)

// ===== Interleaved nibble→(PD map) table for fast LCD writes =====
#define PD_MAP(n) ( ((n)&0x01 ? (1<<LCD_D4) : 0) | ((n)&0x02 ? (1<<LCD_D5) : 0) | ((n)&0x04 ? (1<<LCD_D6) : 0) )
// (PB0 handled separately with SBI/CBI; no PB map here)

static const uint8_t mapPD[16] PROGMEM = {
  PD_MAP(0x0),PD_MAP(0x1),PD_MAP(0x2),PD_MAP(0x3),
  PD_MAP(0x4),PD_MAP(0x5),PD_MAP(0x6),PD_MAP(0x7),
  PD_MAP(0x8),PD_MAP(0x9),PD_MAP(0xA),PD_MAP(0xB),
  PD_MAP(0xC),PD_MAP(0xD),PD_MAP(0xE),PD_MAP(0xF)
};

// ===== Fast EN pulse =====
AI void lcdPulseEnable(void) {
  asm volatile(
    "sbi %0, %1\n\t"
    "nop\n\tnop\n\tnop\n\t"
    "cbi %0, %1\n\t"
    :
    : "I" (_SFR_IO_ADDR(PORTD)), "I" (LCD_EN)
  );
}

// ===== Send one nibble: full PORTD update; PB0 via SBI/CBI only =====
AI void lcdSendNibbleBase(uint8_t nib, uint8_t pd_base) {
  // --- Write PD5..PD7 via table (full PORTD write is fine) ---
  asm volatile(
    "ldi  r30, lo8(%[tbl])\n\t"
    "ldi  r31, hi8(%[tbl])\n\t"
    "add  r30, %[n]\n\t"
    "adc  r31, __zero_reg__\n\t"
    "lpm  r18, Z\n\t"               // r18 = pd map
    "or   r18, %[pdb]\n\t"          // pd = base | mapPD
    "out  %[portd], r18\n\t"
    :
    : [n] "r" ( (uint8_t)(nib & 0x0F) ),
      [pdb] "r" (pd_base),
      [tbl] "p" (mapPD),
      [portd] "I" (_SFR_IO_ADDR(PORTD))
    : "r18","r30","r31"
  );

  // --- PB0 = nibble bit3 (D7) — atomic bit op; won’t disturb PB2/PB3 ---
  if (nib & 0x08) pb_set(LCD_D7); else pb_clr(LCD_D7);

  lcdPulseEnable();
}

// ====== Blocking byte (only for init / rare screens) ======
static inline void lcdSendByte_blocking(uint8_t b, uint8_t isData) {
    // RS
    if (isData) pd_set(LCD_RS);
    else        pd_clr(LCD_RS);

    uint8_t pd_base = (uint8_t)(portd_low_shadow & 0x1F);
    lcdSendNibbleBase(b >> 4, pd_base);
    lcdSendNibbleBase(b & 0x0F, pd_base);

    if (!isData && (b == 0x01 || b == 0x02)) DELAY_US(2000); // clear/home
    else                                       DELAY_US(50);
}
static inline void lcdCommand_blocking(uint8_t x){ lcdSendByte_blocking(x, 0); }
static inline void lcdData_blocking(uint8_t x){ lcdSendByte_blocking(x, 1); }
static inline void lcdGoto_blocking(uint8_t addr){ lcdCommand_blocking(0x80 | addr); }
static inline void lcdPrint_blocking(const char* s){ while(*s) lcdData_blocking(*s++); }
static inline void lcdPrint_P_blocking(const char* p){ char c; while((c = pgm_read_byte(p++))) lcdData_blocking(c); }

// ===== LCD init (blocking, runs rarely) =====
static inline void lcdInit(void) {
    DDRD |= (1 << LCD_RS) | (1 << LCD_EN) | (1 << LCD_D4) | (1 << LCD_D5) | (1 << LCD_D6);
    DDRB |= (1 << LCD_D7);

    DELAY_MS(40);
    // 3x 0x03 then 0x02 (spec)
    pd_clr(LCD_RS); // RS=0
    uint8_t pd_base = (uint8_t)(portd_low_shadow & 0x1F);
    lcdSendNibbleBase(0x03, pd_base); DELAY_MS(5);
    lcdSendNibbleBase(0x03, pd_base); DELAY_US(100);
    lcdSendNibbleBase(0x03, pd_base); DELAY_US(100);
    lcdSendNibbleBase(0x02, pd_base); DELAY_US(100);

    lcdCommand_blocking(0x28); // 4-bit, 2-line
    lcdCommand_blocking(0x0C);
    lcdCommand_blocking(0x06);
    lcdCommand_blocking(0x01);
    DELAY_MS(2);
}

// ===== Reverse-battery strings =====
static const char msg_rev1[] PROGMEM = "Battery is connected";
static const char msg_rev2[] PROGMEM = "in reverse";

// ===== Write into capture buffer =====
static inline void writeToBuffer(uint8_t addr, char data) {
    uint8_t row;
    if (addr < 0x14) row = 0;
    else if (addr < 0x28) { row = 1; addr -= 0x14; }
    else if (addr < 0x54) { row = 2; addr -= 0x40; }
    else { row = 3; addr -= 0x54; }

    if (addr < 20 && row < 4) {
        displayBuf[row][addr] = data;
        rowEverTouched[row] = 1;          // sticky: master touched this row
    }
}
//added
#define BATT_LOW  630
#define BATT_HIGH 810

static uint8_t calculateBatteryPercent(uint16_t v) {

    if (v <= BATT_LOW) return 0;
    if (v >= BATT_HIGH) return 100;

    uint16_t diff = BATT_HIGH - BATT_LOW;
    uint16_t val  = v - BATT_LOW;

    return (uint8_t)((val * 100UL) / diff);
}
static inline uint16_t parse3Digit(const char* p) {
    return (uint16_t)(p[0]-'0')*100 + (uint16_t)(p[1]-'0')*10 + (uint16_t)(p[2]-'0');
}

// ======== LCD NON-BLOCKING QUEUE + PUMP (Timer1 Compare B) ========
typedef struct { uint8_t v; uint8_t flags; } LcdQItem; // flags bit0=isData
#define LCDQ_SIZE 128
static volatile LcdQItem lcdq[LCDQ_SIZE];
static volatile uint8_t lcdq_head = 0, lcdq_tail = 0;

// Timer1: 8 MHz / 64 = 125 kHz => tick = 8 µs
// normal busy ~40 µs -> 5 ticks; clear/home ~2 ms -> 250 ticks
#define LCD_TICKS_NORMAL 5u
#define LCD_TICKS_LONG   250u

AI bool lcdq_try_push(uint8_t val, uint8_t isData){
    uint8_t next = (uint8_t)((lcdq_head + 1) & (LCDQ_SIZE - 1));
    if (next == lcdq_tail) return false; // full
    lcdq[lcdq_head].v = val;
    lcdq[lcdq_head].flags = (uint8_t)(isData & 1);
    lcdq_head = next;
    return true;
}
AI void lcdq_reset(void){
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE){
        lcdq_head = lcdq_tail = 0;
        TIMSK &= ~(1 << OCIE1B);   // stop pump
        TIFR  |= (1 << OCF1B);     // clear pending flag
    }
}
AI void lcdq_kick(void){
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE){
        if (lcdq_head == lcdq_tail) return;           // nothing to do
        if (TIMSK & (1 << OCIE1B)) return;            // already running
        uint16_t base = TCNT1;
        uint16_t top  = OCR1A;
        uint16_t next = (uint16_t)(base + 1);
        if (next > top) next -= (uint16_t)(top + 1);
        OCR1B = next;
        TIFR  |= (1 << OCF1B);       // clear flag
        TIMSK |= (1 << OCIE1B);      // enable pump
    }
}

// Convenience enqueue APIs
AI void lcdq_command(uint8_t cmd){ (void)lcdq_try_push(cmd, 0); }
AI void lcdq_data(uint8_t d){ (void)lcdq_try_push(d, 1); }
AI void lcdq_goto(uint8_t addr){ lcdq_command(0x80 | addr); }

// The pump: sends one byte (both nibbles), then arms next wait
ISR(TIMER1_COMPB_vect){
    if (lcdq_head == lcdq_tail){
        TIMSK &= ~(1 << OCIE1B);
        return;
    }
    uint8_t idx = lcdq_tail;
    uint8_t val = lcdq[idx].v;
    uint8_t flags = lcdq[idx].flags;
    lcdq_tail = (uint8_t)((idx + 1) & (LCDQ_SIZE - 1));

    // Set RS quickly (and update shadow to match)
    asm volatile(
      "sbrs %[f],0\n\t" "rjmp 1f\n\t"
      "sbi  %0,%1\n\t"  "rjmp 2f\n\t"
    "1:\n\t"
      "cbi  %0,%1\n\t"
    "2:\n\t"
      :: "I"(_SFR_IO_ADDR(PORTD)), "I"(LCD_RS), [f]"r"(flags)
    );
    if (flags & 1) portd_low_shadow |= (uint8_t)(1u << LCD_RS);
    else           portd_low_shadow &= (uint8_t)~(1u << LCD_RS);

    uint8_t pd_base = (uint8_t)(portd_low_shadow & 0x1F);

    lcdSendNibbleBase(val >> 4, pd_base);
    lcdSendNibbleBase(val & 0x0F,  pd_base);

    uint16_t ticks = LCD_TICKS_NORMAL;
    if (!(flags & 1) && (val == 0x01 || val == 0x02)) ticks = LCD_TICKS_LONG;

    uint16_t base = TCNT1;
    uint16_t top  = OCR1A;
    uint16_t next = (uint16_t)(base + ticks);
    if (next > top) next -= (uint16_t)(top + 1);
    OCR1B = next;
    TIFR  |= (1 << OCF1B);
    TIMSK |= (1 << OCIE1B);
}

// ===== Parse master buffer into state =====
static void parseBuffer(void) {
    char src[4][20];
    memcpy(src, (const void*)displayBuf, sizeof src);

    current_out_val = 0;
    load_val = 0;
    masterOverloadFlag = false;
    grid_displayed = 0;
    out_load_row = 255;

    bool hz_found = false;
    for (uint8_t row = 0; row < 4; row++) {
        for (uint8_t col = 0; col < 19; col++) {
            char* p = &src[row][col];
            if (p[0] == 'H' && p[1] == 'z') { hz_found = true; break; }
        }
        if (hz_found) break;
    }

    bool overload_found = false;
    for (uint8_t row = 0; row < 4; row++) {
        for (uint8_t col = 0; col <= 12; col++) {
            char* p = &src[row][col];
            if (match(p, "Overload", 8)) { overload_found = true; break; }
        }
        if (overload_found) break;
    }
    masterOverloadFlag = overload_found;

    for (uint8_t r = 0; r < 4; r++) {
        for (uint8_t c = 0; c < 20; c++) {
            char* ptr = &src[r][c];

            // Out XXXV Load YYY%
            if (c <= 2 &&
                ptr[0]=='O' && ptr[1]=='u' && ptr[2]=='t' && ptr[3]==' ' &&
                ptr[4]>='0' && ptr[4]<='9' && ptr[5]>='0' && ptr[5]<='9' && ptr[6]>='0' && ptr[6]<='9' &&
                ptr[7]=='V' && ptr[8]==' ' &&
                ptr[9]=='L' && ptr[10]=='o' && ptr[11]=='a' && ptr[12]=='d' && ptr[13]==' ' &&
                ptr[14]>='0' && ptr[14]<='9' && ptr[15]>='0' && ptr[15]<='9' && ptr[16]>='0' && ptr[16]<='9' &&
                ptr[17]=='%') {

                uint16_t out_val = parse3Digit(ptr + 4);
                load_val = parse3Digit(ptr + 14);
                if (divisionFlag) out_val >>= 1;
                current_out_val = out_val;
                if (!tripped) out_load_row = r;

                // Load hysteresis / confirmation
                if (loadState == NORMAL_LOAD) {
                    if (load_val > 81) { if (++overload_enter_count >= 20) { loadState = OVER_LOAD; overload_enter_count = 0; } }
                    else overload_enter_count = 0;
                } else {
                    if (load_val < 74) { if (++overload_exit_count >= 50) { loadState = NORMAL_LOAD; overload_exit_count = 0; } }
                    else overload_exit_count = 0;
                }
                overLoadFlag = (loadState == OVER_LOAD);
            }

            // Grid XXXV
            if (c <= 11 && match(ptr, "Grid ", 5) && ptr[8]=='V' &&
                ptr[5]>='0' && ptr[5]<='9' && ptr[6]>='0' && ptr[6]<='9' && ptr[7]>='0' && ptr[7]<='9') {

                uint16_t grid_raw = parse3Digit(ptr + 5);
                grid_displayed = divisionFlag ? (grid_raw >> 1) : grid_raw;

                // Mains FAIL: 0..50 V takes precedence
                if (grid_raw <= 50) {
                    gridAlertState = MAINS_FAIL;
                } else {
                    if (gridAlertState == MAINS_FAIL) {
                        gridAlertState = NORMAL;
                    }
                    if (gridAlertState == NORMAL) {
                        if (grid_raw > highThresholdEnter && !hz_found) gridAlertState = MAINS_HIGH;
                        else if (grid_raw < lowThresholdEnter)          gridAlertState = MAINS_LOW;
                    } else if (gridAlertState == MAINS_HIGH && !hz_found) {
                        if (grid_raw <= highThresholdExit) gridAlertState = NORMAL;
                    } else if (gridAlertState == MAINS_LOW) {
                        if (grid_raw >= lowThresholdExit) gridAlertState = NORMAL;
                    }
                }
            }

            // Out XXXV standalone
            if (c <= 11 && match(ptr, "Out ", 4) && ptr[7]=='V' &&
                ptr[4]>='0' && ptr[4]<='9' && ptr[5]>='0' && ptr[5]<='9' && ptr[6]>='0' && ptr[6]<='9') {
                uint16_t val = parse3Digit(ptr + 4);
                if (divisionFlag) val >>= 1; current_out_val = val;
            }
        }
    }
}

// ===== Update our LCD (enqueue-only, non-blocking) =====
static void updateLCD(void) {
    // Snapshot to avoid tearing
    char src[4][20];
    memcpy(src, (const void*)displayBuf, sizeof src);

    // Force rows the master never touched (since last Clear/PD1) to blank
    for (uint8_t r = 0; r < 4; r++){
        if (!rowEverTouched[r]) {
            for (uint8_t i = 0; i < 20; i++) src[r][i] = ' ';
        }
    }

    char temp[4][20];
    uint8_t temp_idx;

    for (uint8_t r = 0; r < 4; r++) {
        temp_idx = 0;
        for (uint8_t c = 0; c < 20; c++) {
            char* ptr = &src[r][c];

            // Out XXXV Load YYY%
            if (c <= 2 &&
                ptr[0]=='O' && ptr[1]=='u' && ptr[2]=='t' && ptr[3]==' ' &&
                ptr[4]>='0' && ptr[4]<='9' && ptr[5]>='0' && ptr[5]<='9' && ptr[6]>='0' && ptr[6]<='9' &&
                ptr[7]=='V' && ptr[8]==' ' &&
                ptr[9]=='L' && ptr[10]=='o' && ptr[11]=='a' && ptr[12]=='d' && ptr[13]==' ' &&
                ptr[14]>='0' && ptr[14]<='9' && ptr[15]>='0' && ptr[15]<='9' && ptr[16]>='0' && ptr[16]<='9' &&
                ptr[17]=='%') {

                if (tripped || ((!(PIND & (1 << PD1))) && current_out_val < 10)) {
                    COPY_LIT(temp[r], temp_idx, "Over Load Tripped...");
                    c += 19;
                } else if (masterOverloadFlag) {
                    COPY_LIT(temp[r], temp_idx, "Over Load 150%      "); // 20 chars
                    c += 19;
                } else if (overLoadFlag) {
                    COPY_LIT(temp[r], temp_idx, "Over Load 110%  ");   // 16 chars so far
                    uint32_t elapsed_ms = millis_now() - overload_start_time;
                    uint32_t elapsed_sec = elapsed_ms / 1000;
                    uint32_t remaining_sec = (elapsed_sec >= 600) ? 0 : (600 - elapsed_sec);
                    char *d = &temp[r][temp_idx]; put3(d, (uint16_t)remaining_sec); temp_idx += 3; temp[r][temp_idx++] = 's';
                    c += 19;
                } else {
                    COPY_LIT(temp[r], temp_idx, "Out ");
                    put3(&temp[r][temp_idx], current_out_val); temp_idx += 3;
                    temp[r][temp_idx++] = 'V';
                    COPY_LIT(temp[r], temp_idx, " Load ");
                    uint16_t displayed_load = (uint16_t)((load_val * 135UL + 50) / 100);
                    put3(&temp[r][temp_idx], displayed_load); temp_idx += 3;
                    temp[r][temp_idx++] = '%';
                    COPY_LIT(temp[r], temp_idx, "  ");
                    c += 19;
                }
            } else if (tripped && r == out_load_row && c == 0) {
                COPY_LIT(temp[r], temp_idx, "Over Load Tripped...");
                c += 19;
            } else if (c <= 11 && match(ptr, "Grid ", 5) && ptr[8]=='V' &&
                       ptr[5]>='0' && ptr[5]<='9' && ptr[6]>='0' && ptr[6]<='9' && ptr[7]>='0' && ptr[7]<='9') {

                if (gridAlertState == MAINS_HIGH) {
                    COPY_LIT(temp[r], temp_idx, "Mains HIGH");
                    c += 9;
                } else if (gridAlertState == MAINS_LOW) {
                    COPY_LIT(temp[r], temp_idx, "Mains LOW       ");
                    c += 15;
                } else if (gridAlertState == MAINS_FAIL) {
                    COPY_LIT(temp[r], temp_idx, "Mains FAIL");
                    c += 9;
                } else {
                    // Show "Input 230V" (10 chars) while consuming 9 source chars ("Grid XXXV")
                    COPY_LIT(temp[r], temp_idx, "Input ");
                    put3(&temp[r][temp_idx], grid_displayed); temp_idx += 3;
                    temp[r][temp_idx++] = 'V';
                    c += 8;  // net +1 shift right
                }
            } else if (c <= 11 && match(ptr, "Out ", 4) && ptr[7]=='V' &&
                       ptr[4]>='0' && ptr[4]<='9' && ptr[5]>='0' && ptr[5]<='9' && ptr[6]>='0' && ptr[6]<='9') {
                COPY_LIT(temp[r], temp_idx, "Out ");
                put3(&temp[r][temp_idx], current_out_val); temp_idx += 3; temp[r][temp_idx++] = 'V';
                c += 7;
            } 
            //added
           else if (c <= 15 && match(ptr, "Batt ", 5)) {

                uint16_t value = 0;
                uint8_t i = 5;

                value = (ptr[i] - '0');
                i++;

                if (ptr[i] >= '0' && ptr[i] <= '9') {
                    value = value * 10 + (ptr[i] - '0');
                    i++;

                    if (ptr[i] >= '0' && ptr[i] <= '9') {
                        value = value * 10 + (ptr[i] - '0');
                        i++;
                    }
                }

                value *= 10;

                if (ptr[i] == '.') {
                    i++;
                    value += (ptr[i] - '0');  
                    i++;
                }

                uint8_t percent = calculateBatteryPercent(value);

                COPY_LIT(temp[r], temp_idx, "Batt ");
                put3(&temp[r][temp_idx], percent);
                temp_idx += 3;
                temp[r][temp_idx++] = '%';

                c += i;
            }
            else if (c <= 17 && match(ptr, "MLS", 3)) {
                COPY_LIT(temp[r], temp_idx, "GTD");
                c += 2;
            } else if (c <= 16 && match(ptr, "Medi", 4)) {
                COPY_LIT(temp[r], temp_idx, "XGEN");
                c += 3;
            } else {
                temp[r][temp_idx++] = *ptr;
            }
        }
        while (temp_idx < 20) temp[r][temp_idx++] = ' ';
    }

    // Enqueue ONLY rows that changed
    for (uint8_t r = 0; r < 4; r++) {
        if (memcmp(lastFrame[r], temp[r], 20) != 0) {
            lcdq_goto(rowOffsets[r]);
            for (uint8_t c = 0; c < 20; c++) {
                (void)lcdq_try_push((uint8_t)temp[r][c], 1);
            }
            memcpy(lastFrame[r], temp[r], 20);
        }
    }
    lcdq_kick();
}

// ===== Interrupts =====
ISR(INT0_vect) {
    // Sample PINC ASAP while E is high
    uint8_t p;
    asm volatile("in %0, %1" : "=r"(p) : "I"(_SFR_IO_ADDR(PINC)));

    // Extract RS and build nibble quickly
    uint8_t rs = (p >> RS_IN) & 1;
    uint8_t n  = 0;
    asm volatile(
        "bst %[pin], %[b4]\n\t"  "bld %[nib], 0\n\t"
        "bst %[pin], %[b5]\n\t"  "bld %[nib], 1\n\t"
        "bst %[pin], %[b6]\n\t"  "bld %[nib], 2\n\t"
        "bst %[pin], %[b7]\n\t"  "bld %[nib], 3\n\t"
        : [nib] "+r"(n)
        : [pin] "r"(p),
          [b4] "I"(D4_IN), [b5] "I"(D5_IN),
          [b6] "I"(D6_IN), [b7] "I"(D7_IN)
        : "cc"
    );

    nibble = n;

    if (highNibble) {
        byteBuf = (uint8_t)(n << 4);
        highNibble = false;
    } else {
        byteBuf |= n;
        highNibble = true;

        if (!rs) {  // Command
            if (byteBuf & 0x80) {
                curAddr = (uint8_t)(byteBuf & 0x7F);
            } else if (byteBuf == 0x01) {
                // Master Clear Display: clear buffer and "ever touched" markers
                memset((void*)displayBuf, ' ', sizeof(displayBuf));
                rowEverTouched[0]=rowEverTouched[1]=rowEverTouched[2]=rowEverTouched[3]=0;
                curAddr = 0;
            } else if (byteBuf == 0x02) {
                curAddr = 0;
            }
        } else {    // Data
            writeToBuffer(curAddr, byteBuf);
            curAddr++;
        }
        byteReceived = true;
    }
}

ISR(TIMER1_COMPA_vect) {
    millis_counter += 100; // 100 ms tick
}

int main(void) {
    // ===== Ports =====
    DDRB  = (DDRB & ~(1 << PB1)) | (1 << PB5) | (1 << PB2) | (1 << PB3); // PB5,PB2,PB3 out; PB1 in
    PORTB |= (1 << PB1); // pull-up PB1

    DDRD  &= ~((1 << PD1) | (1 << PD0)); // PD1, PD0 in
    PORTD |= (1 << PD1);                 // pull-up PD1

    // Sync shadow for PD0..PD4 after initial PORTD setup
    portd_low_shadow = (uint8_t)(PORTD & 0x1F);

    // Init LCD (blocking)
    lcdInit();

    // Timer1 CTC @ 100 ms tick on OCR1A; COMPB used by LCD pump
    TCCR1A = 0;
    TCCR1B = (1 << WGM12) | (1 << CS11) | (1 << CS10); // /64 → 125 kHz
    OCR1A  = 12499;                                    // 10 Hz
    TIMSK |= (1 << OCIE1A);                            // enable Compare A

    // Reverse-battery branch (blocking)
    if (PIND & (1 << PD0)) {
        sei();
        lcdGoto_blocking(0x40); lcdPrint_P_blocking(msg_rev1);
        lcdGoto_blocking(0x19); lcdPrint_P_blocking(msg_rev2);

        uint8_t  buzz_state = 0;
        uint32_t last_millis = 0;

        while (1) {
            uint32_t current_millis = millis_now();
            switch (buzz_state) {
                case 0: pb_set(PB3); last_millis = current_millis; buzz_state = 1; break;
                case 1: if (current_millis - last_millis >= 100) { pb_clr(PB3); last_millis = current_millis; buzz_state = 2; } break;
                case 2: if (current_millis - last_millis >= 100) { buzz_state = 0; } break;
            }
        }
    }

    // Division flag per PB1 + indicator PB5 (bit-atomic)
    if (!(PINB & (1 << PB1))) { divisionFlag = true;  pb_set(PB5); }
    else                      { divisionFlag = false; pb_clr(PB5); }

    // Splash (blocking, once)
    lcdCommand_blocking(0x01);
    DELAY_MS(2);
    lcdGoto_blocking(0x00);
    lcdPrint_P_blocking(PSTR("Soft Ver:- 4.8C"));
    lcdGoto_blocking(0x40);
    if (divisionFlag) lcdPrint_P_blocking(PSTR("SET Voltage: 115V"));
    else              lcdPrint_P_blocking(PSTR("SET Voltage: 230V"));
    DELAY_MS(1000);

    // Thresholds (fixed)
    highThresholdEnter = 258; highThresholdExit = 255; lowThresholdEnter = 202; lowThresholdExit = 205;

    // Clear buffers
    memset((void*)displayBuf, ' ', sizeof(displayBuf));
    memset(lastFrame, 0x00, sizeof(lastFrame)); // force first frame

    // Sniffer inputs
    DDRC &= ~((1 << RS_IN) | (1 << D4_IN) | (1 << D5_IN) | (1 << D6_IN) | (1 << D7_IN));
    DDRD &= ~(1 << E_IN);
    PORTD &= ~(1 << E_IN);   // ensure no pull-up on PD2 (E line)

    // INT0 on rising edge
    GICR  |= (1 << INT0);
    MCUCR  = (MCUCR & ~((1<<ISC01)|(1<<ISC00))) | (1<<ISC01) | (1<<ISC00);

    sei();

    // ===== Debounce/Buzzer state =====
    static bool pd1_for_pb2 = 0;               // debounced PD1
    static bool pd1_pb2_timer_active = false;  // debounce timer flag
    static uint32_t pd1_pb2_start = 0;
    static bool prev_pd1_deb = 1;

    // Seed debounce to actual pin level
    pd1_for_pb2 = (PIND & (1 << PD1)) != 0;
    prev_pd1_deb = pd1_for_pb2;
    pd1_pb2_timer_active = false;

    bool buzzing = false;
    uint32_t last_buzz_time = 0;
    uint32_t buzz_start_time = 0;
    const uint32_t buzz_duration = 200;

    while (1) {
        uint32_t current_millis = millis_now();
        bool current_pd1 = (PIND & (1 << PD1)) != 0;

        // PD1 high for 100ms to reset tripped
        if (current_pd1) {
            if (!pd1_high_timer_active) { pd1_high_start = current_millis; pd1_high_timer_active = true; }
            if ((current_millis - pd1_high_start) >= 100) {
                tripped = false; overLoadFlag = false; loadState = NORMAL_LOAD;
                overload_enter_count = 0; overload_exit_count = 0; pd1_high_timer_active = false;
            }
        } else {
            pd1_high_timer_active = false;
        }

        // === PB2 control with 100ms debounce (bit-atomic) ===
        if (current_pd1 != pd1_for_pb2) {
            if (!pd1_pb2_timer_active) { pd1_pb2_start = current_millis; pd1_pb2_timer_active = true; }
            if ((current_millis - pd1_pb2_start) >= 100) { pd1_for_pb2 = current_pd1; pd1_pb2_timer_active = false; }
        } else pd1_pb2_timer_active = false;

        // Debounced falling-edge on PD1 → re-init LCD and refresh immediately
        if (prev_pd1_deb && !pd1_for_pb2) {
            lcdq_reset();              // stop pump & clear queue
            lcdInit();                 // blocking re-init
            memset((void*)displayBuf, ' ', sizeof(displayBuf));
            rowEverTouched[0]=rowEverTouched[1]=rowEverTouched[2]=rowEverTouched[3]=0;  // reset markers
            memset(lastFrame, 0x00, sizeof(lastFrame)); // force next frame write
            parseBuffer();
            updateLCD();               // enqueue fresh frame
            updateCounter = 0;
        }
        prev_pd1_deb = pd1_for_pb2;

        if (!pd1_for_pb2 && !tripped) pb_set(PB2); // PD1 stable LOW → relay on
        else                           pb_clr(PB2);

        // Overload timer
        if (overLoadFlag && !tripped) {
            if (!overload_timer_active) { overload_start_time = current_millis; overload_timer_active = true; }
            if ((current_millis - overload_start_time) >= 600000UL) { tripped = true; overload_timer_active = false; }
        } else overload_timer_active = false;

        // Non-blocking buzzer logic (bit-atomic writes)
        if (!current_pd1 && current_out_val < 10) {
            pb_set(PB3);
        } else if ((overLoadFlag || masterOverloadFlag) || gridAlertState != NORMAL) {
            uint32_t buzz_interval;
            if (masterOverloadFlag && !current_pd1)      buzz_interval = 500;
            else if (overLoadFlag && !current_pd1)       buzz_interval = 1000;
            else                                         buzz_interval = (current_out_val > 80) ? 5000 : 10000;

            if (!buzzing && (current_millis - last_buzz_time >= buzz_interval)) {
                pb_set(PB3); buzzing = true; buzz_start_time = current_millis; last_buzz_time = current_millis;
            } else if (buzzing && (current_millis - buzz_start_time >= buzz_duration)) {
                pb_clr(PB3); buzzing = false;
            }
        } else {
            pb_clr(PB3); buzzing = false; last_buzz_time = current_millis;
        }

        // Byte reception -> parse/update
        if (byteReceived) {
            byteReceived = false;
            parseBuffer();
            updateCounter++;
            if (updateCounter >= 2) { updateLCD(); updateCounter = 0; }
        }
    }
}
