#include <stdio.h>
#include "altera_up_avalon_ps2.h"
#include "altera_up_ps2_mouse.h"

#define LEDR_BASE   0xFF200000
#define SWITCH_BASE 0xFF200040
#define KEY_BASE    0xFF200050

/* internal cursor limits */
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

/* LACIE's INTERRUPTS */
static void handler(void) __attribute__((interrupt("machine")));
void KEY_ISR(void);
void set_KEY(void);

void delay(void) {
    volatile int i;
    for (i = 0; i < 500000; i++);
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
    int pressed = *(KEY_ptr + 3);
    *(KEY_ptr + 3) = pressed;

    if (pressed & 0x1) {
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
    *(KEY_ptr + 3) = 0xF;
    *(KEY_ptr + 2) = 0x1;
}

/* clamp helper so cursor stays in bounds */
int clamp(int value, int min, int max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

/* MAIN */
int main(void) {
    volatile int *LEDR_ptr   = (int *)LEDR_BASE;
    volatile int *switch_ptr = (int *)SWITCH_BASE;

    unsigned char byte;

    alt_up_ps2_dev *ps2 = alt_up_ps2_open_dev("/dev/ps2_0");

    if (ps2 == NULL) {
        while (1); /* fail safe */
    }

    /* Initialize mouse */
    alt_up_ps2_init(ps2);
    alt_up_ps2_mouse_reset(ps2);
    alt_up_ps2_mouse_set_mode(ps2, 0xF4); /* enable data reporting */

    /* Interrupt stuff */
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

            if (manual_fruit) {
                fruit_count++;
                current_state = STATE_FRUIT;
            }
            else if (manual_bomb) {
                current_state = STATE_BOMB;
            }
            else {
                if (alt_up_ps2_read_data_byte(ps2, &byte) == 0) {

                    /*
                        Standard 3-byte packet:
                        packet[0] = buttons/sign bits
                        packet[1] = X movement
                        packet[2] = Y movement

                        bit0 = left button
                        bit1 = right button
                        bit3 = always 1 in first byte
                    */
                    static int count = 0;
                    static unsigned char packet[3];

                    packet[count++] = byte;

                    /* first byte sanity check */
                    if (count == 1) {
                        if ((packet[0] & 0x08) == 0) {
                            count = 0;
                        }
                    }

                    if (count == 3) {
                        count = 0;

                        int left  = packet[0] & 0x1;
                        int right = (packet[0] >> 1) & 0x1;

                        /* signed movement deltas */
                        int dx = (char)packet[1];
                        int dy = (char)packet[2];

                        /*
                            update internal cursor
                            PS/2 positive Y is up, so subtract for screen-style coords
                        */
                        x_pos = clamp(x_pos + dx, X_MIN, X_MAX);
                        y_pos = clamp(y_pos - dy, Y_MIN, Y_MAX);

                        /* click actions stay same as your version */
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