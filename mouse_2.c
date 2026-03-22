#include <stdio.h>

#define LEDR_BASE   0xFF200000
#define SWITCH_BASE 0xFF200040
#define KEY_BASE    0xFF200050
#define PS2_BASE    0xFF200100

#define X_MIN 0
#define X_MAX 319
#define Y_MIN 0
#define Y_MAX 239

enum States {
    STATE_START,
    STATE_PLAY,
    STATE_FRUIT,
    STATE_BOMB,
    STATE_GAMEOVER
};

volatile enum States current_state = STATE_START;
volatile int fruit_count = 0;

/* internal mouse position */
volatile int x_pos = 160;
volatile int y_pos = 120;

/* interrupt functions */
static void handler(void) __attribute__((interrupt("machine")));
void KEY_ISR(void);
void set_KEY(void);

/* PS/2 helper functions */
int read_ps2_byte(volatile int *ps2_ptr, unsigned char *byte);
void write_ps2_byte(volatile int *ps2_ptr, unsigned char byte);
void clear_ps2_fifo(volatile int *ps2_ptr);
int wait_for_ps2_byte(volatile int *ps2_ptr, unsigned char *byte, int timeout);
void init_mouse(volatile int *ps2_ptr);

/* utility functions */
void delay(void);
int clamp(int value, int min, int max);

void delay(void) {
    volatile int i;
    for (i = 0; i < 500000; i++);
}

int clamp(int value, int min, int max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

static void handler(void) {
    int mcause_value;
    __asm__ volatile("csrr %0, mcause" : "=r"(mcause_value));

    if (mcause_value == 0x80000012) {
        KEY_ISR();
    }
}

void KEY_ISR(void) {
    volatile int *KEY_ptr = (int *)KEY_BASE;
    int pressed = *(KEY_ptr + 3);   /* edge capture */
    *(KEY_ptr + 3) = pressed;       /* clear edge bits */

    if (pressed & 0x1) {            /* KEY0 only */
        if (current_state == STATE_START) {
            current_state = STATE_PLAY;
        }
        else if (current_state == STATE_GAMEOVER) {
            fruit_count = 0;
            x_pos = 160;
            y_pos = 120;
            current_state = STATE_START;
        }
    }
}

void set_KEY(void) {
    volatile int *KEY_ptr = (int *)KEY_BASE;
    *(KEY_ptr + 3) = 0xF;   /* clear edge capture */
    *(KEY_ptr + 2) = 0x1;   /* enable KEY0 interrupt */
}

/*
    PS/2 data register:
    - bit 15 = RVALID
    - bits 7:0 = DATA
*/
int read_ps2_byte(volatile int *ps2_ptr, unsigned char *byte) {
    int data = *ps2_ptr;

    if (data & 0x8000) {
        *byte = (unsigned char)(data & 0xFF);
        return 1;
    }

    return 0;
}

void write_ps2_byte(volatile int *ps2_ptr, unsigned char byte) {
    *ps2_ptr = (int)byte;
}

void clear_ps2_fifo(volatile int *ps2_ptr) {
    unsigned char dummy;
    while (read_ps2_byte(ps2_ptr, &dummy)) {
    }
}

int wait_for_ps2_byte(volatile int *ps2_ptr, unsigned char *byte, int timeout) {
    while (timeout-- > 0) {
        if (read_ps2_byte(ps2_ptr, byte)) {
            return 1;
        }
    }
    return 0;
}

/*
    Standard mouse init:
    0xFF = reset
    expect ACK, BAT, device ID
    0xF4 = enable data reporting
*/
void init_mouse(volatile int *ps2_ptr) {
    unsigned char byte;

    clear_ps2_fifo(ps2_ptr);

    /* reset mouse */
    write_ps2_byte(ps2_ptr, 0xFF);
    wait_for_ps2_byte(ps2_ptr, &byte, 1000000);   /* ACK */
    wait_for_ps2_byte(ps2_ptr, &byte, 1000000);   /* 0xAA BAT passed */
    wait_for_ps2_byte(ps2_ptr, &byte, 1000000);   /* device ID */

    clear_ps2_fifo(ps2_ptr);

    /* enable data reporting */
    write_ps2_byte(ps2_ptr, 0xF4);
    wait_for_ps2_byte(ps2_ptr, &byte, 1000000);   /* ACK */

    clear_ps2_fifo(ps2_ptr);
}

int main(void) {
    volatile int *LEDR_ptr   = (int *)LEDR_BASE;
    volatile int *switch_ptr = (int *)SWITCH_BASE;
    volatile int *ps2_ptr    = (int *)PS2_BASE;

    unsigned char byte;

    /* initialize mouse */
    init_mouse(ps2_ptr);

    /* initialize KEY interrupt */
    set_KEY();

    int mstatus_value, mtvec_value, mie_value;

    mtvec_value = (int)&handler;
    __asm__ volatile("csrw mtvec, %0" :: "r"(mtvec_value));

    mstatus_value = 0b1000;
    __asm__ volatile("csrc mstatus, %0" :: "r"(mstatus_value));

    __asm__ volatile("csrr %0, mie" : "=r"(mie_value));
    __asm__ volatile("csrc mie, %0" :: "r"(mie_value));

    mie_value = (1 << 18);
    __asm__ volatile("csrs mie, %0" :: "r"(mie_value));
    __asm__ volatile("csrs mstatus, %0" :: "r"(mstatus_value));

    while (1) {
        int sw = *switch_ptr & 0x1F;
        int manual_fruit = sw & 0x1;
        int manual_bomb  = (sw >> 1) & 0x1;

        if (current_state == STATE_START) {
            *LEDR_ptr = ((fruit_count & 0x1F) << 5) | 0x01;
        }

        else if (current_state == STATE_PLAY) {
            *LEDR_ptr = ((fruit_count & 0x1F) << 5) | 0x02;

            /* backup manual testing with switches */
            if (manual_fruit) {
                fruit_count++;
                current_state = STATE_FRUIT;
            }
            else if (manual_bomb) {
                current_state = STATE_BOMB;
            }
            else {
                if (read_ps2_byte(ps2_ptr, &byte)) {
                    static int count = 0;
                    static unsigned char packet[3];

                    packet[count++] = byte;

                    /*
                        Standard 3-byte packet:
                        packet[0] = buttons/sign bits
                        packet[1] = X movement
                        packet[2] = Y movement

                        packet[0] bit0 = left button
                        packet[0] bit1 = right button
                        packet[0] bit3 = always 1
                    */

                    /* first byte must have bit 3 set */
                    if (count == 1) {
                        if ((packet[0] & 0x08) == 0) {
                            count = 0;
                        }
                    }
                    else if (count == 3) {
                        int left  = packet[0] & 0x1;
                        int right = (packet[0] >> 1) & 0x1;

                        int dx = (char)packet[1];
                        int dy = (char)packet[2];

                        count = 0;

                        /* update internal cursor */
                        x_pos = clamp(x_pos + dx, X_MIN, X_MAX);
                        y_pos = clamp(y_pos - dy, Y_MIN, Y_MAX);

                        /* click actions */
                        if (left) {
                            fruit_count++;
                            current_state = STATE_FRUIT;
                        }
                        else if (right) {
                            current_state = STATE_BOMB;
                        }
                    }
                }
            }
        }

        else if (current_state == STATE_FRUIT) {
            *LEDR_ptr = ((fruit_count & 0x1F) << 5) | 0x04;
            delay();
            current_state = STATE_PLAY;
        }

        else if (current_state == STATE_BOMB) {
            *LEDR_ptr = ((fruit_count & 0x1F) << 5) | 0x08;
            delay();
            current_state = STATE_GAMEOVER;
        }

        else if (current_state == STATE_GAMEOVER) {
            *LEDR_ptr = ((fruit_count & 0x1F) << 5) | 0x10;
        }
    }

    return 0;
}