#include <stdio.h>
#include "altera_up_avalon_ps2.h"
#include "altera_up_ps2_mouse.h"

#define LEDR_BASE 0xFF200000
#define SWITCH_BASE 0xFF200040
#define KEY_BASE 0xFF200050
#define AUDIO_BASE 0xFF203040

/* internal cursor bounds */
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

enum SliceSound {
    SOUND_NONE,
    SOUND_PEACH,
    SOUND_STRAWBERRY,
    SOUND_MELON,
    SOUND_BOMB
};

volatile enum States current_state = STATE_START;
volatile int fruit_count = 0;

/* internal mouse position */
volatile int x_pos = 160;
volatile int y_pos = 120;

/* 
   - convert each sound into signed 16-bit PCM samples
   - then paste them here as const short arrays
   const short peach_sound[] = { ... };
   const int peach_sound_len = sizeof(peach_sound) / sizeof(peach_sound[0]);
*/

const short miss_sound[] = {
    /* paste miss sound here */
    0
};
const int miss_sound_len = sizeof(miss_sound) / sizeof(miss_sound[0]);

const short peach_sound[] = {
    /* paste peach sound here */
    0
};
const int peach_sound_len = sizeof(peach_sound) / sizeof(peach_sound[0]);

const short strawberry_sound[] = {
    /* paste strawberry sound here */
    0
};
const int strawberry_sound_len = sizeof(strawberry_sound) / sizeof(strawberry_sound[0]);

const short melon_sound[] = {
    /* paste melon sound here */
    0
};
const int melon_sound_len = sizeof(melon_sound) / sizeof(melon_sound[0]);

const short bomb_sound[] = {
    /* paste bomb sound here */
    0
};
const int bomb_sound_len = sizeof(bomb_sound) / sizeof(bomb_sound[0]);

/* key interrupts */
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
    int pressed = *(KEY_ptr + 3);   // edge capture
    *(KEY_ptr + 3) = pressed;       // clear edge bits

    if (pressed & 0x1) {            // KEY0
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
    KEY_ptr[2] = 0x1;   // enable KEY0 interrupt mask
    KEY_ptr[3] = 0xF;   // clear old edge bits
}

/* keep cursor inside bounds */
int clamp(int value, int min, int max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

void audio_init(volatile int *audio_ptr) {
    *audio_ptr = 0x8;   // clear output FIFOs
    *audio_ptr = 0x0;   // leave clear state
}

void audio_write_sample(volatile int *audio_ptr, int sample24) {
    while (1) {
        int fifospace = *(audio_ptr + 1);

        int wsrc = (fifospace >> 16) & 0xFF;
        int wslc = (fifospace >> 24) & 0xFF;

        if (wsrc > 0 && wslc > 0) {
            *(audio_ptr + 2) = sample24;   // left channel
            *(audio_ptr + 3) = sample24;   // right channel
            break;
        }
    }
}

/*
    Convert a 16-bit sample to a bigger audio value for the audio core.
    Square-wave lab used roughly 24-bit-sized values like 0x7FFFFF,
    so here we shift 16-bit PCM upward.
*/
int convert_16bit_to_24ish(short s) {
    return ((int)s) << 8;
}

void play_sound(volatile int *audio_ptr, const short sound[], int length) {
    int i;
    for (i = 0; i < length; i++) {
        int sample24 = convert_16bit_to_24ish(sound[i]);
        audio_write_sample(audio_ptr, sample24);
    }
}

void play_event_sound(volatile int *audio_ptr, enum SliceSound which) {
    if (which == SOUND_NONE) {
        play_sound(audio_ptr, miss_sound, miss_sound_len);
    }
    else if (which == SOUND_PEACH) {
        play_sound(audio_ptr, peach_sound, peach_sound_len);
    }
    else if (which == SOUND_STRAWBERRY) {
        play_sound(audio_ptr, strawberry_sound, strawberry_sound_len);
    }
    else if (which == SOUND_MELON) {
        play_sound(audio_ptr, melon_sound, melon_sound_len);
    }
    else if (which == SOUND_BOMB) {
        play_sound(audio_ptr, bomb_sound, bomb_sound_len);
    }
}

int main(void) {
    volatile int *LEDR_ptr   = (int *)LEDR_BASE;
    volatile int *switch_ptr = (int *)SWITCH_BASE;
    volatile int *audio_ptr  = (int *)AUDIO_BASE;

    unsigned char byte;

    alt_up_ps2_dev *ps2 = alt_up_ps2_open_dev("/dev/ps2_0");
    if (ps2 == NULL) {
        while (1) {
            *LEDR_ptr = 0x3FF;  // obvious fail state
        }
    }

    /* Init mouse */
    alt_up_ps2_init(ps2);
    alt_up_ps2_mouse_reset(ps2);
    alt_up_ps2_mouse_set_mode(ps2, 0xF4);   // enable data reporting

    /* Init audio */
    audio_init(audio_ptr);

    /* KEY interrupt setup */
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

        /*
            SW0 = peach
            SW1 = strawberry
            SW2 = melon
            SW3 = bomb
            SW4 = miss / no fruit
        */

        if (current_state == STATE_START) {
            *LEDR_ptr = ((fruit_count & 0x1F) << 5) | 0x01;
        }

        else if (current_state == STATE_PLAY) {
            *LEDR_ptr = ((fruit_count & 0x1F) << 5) | 0x02;

            /* Manual switch testing first */
            if (sw & 0x01) {
                fruit_count++;
                play_event_sound(audio_ptr, SOUND_PEACH);
                current_state = STATE_FRUIT;
            }
            else if (sw & 0x02) {
                fruit_count++;
                play_event_sound(audio_ptr, SOUND_STRAWBERRY);
                current_state = STATE_FRUIT;
            }
            else if (sw & 0x04) {
                fruit_count++;
                play_event_sound(audio_ptr, SOUND_MELON);
                current_state = STATE_FRUIT;
            }
            else if (sw & 0x08) {
                play_event_sound(audio_ptr, SOUND_BOMB);
                current_state = STATE_BOMB;
            }
            else if (sw & 0x10) {
                play_event_sound(audio_ptr, SOUND_NONE);
            }
            else {
                /*
                    standard 3-byte mouse packets
                    packet[0] bit0 = left button
                    packet[0] bit1 = right button
                    packet[0] bit3 = always 1
                    packet[1] = X movement
                    packet[2] = Y movement
                */
                if (alt_up_ps2_read_data_byte(ps2, &byte) == 0) {
                    static int count = 0;
                    static unsigned char packet[3];

                    packet[count++] = byte;

                    if (count == 1) {
                        /* first byte should have bit3 = 1 */
                        if ((packet[0] & 0x08) == 0) {
                            count = 0;
                        }
                    }
                    else if (count == 3) {
                        int left  = packet[0] & 0x1;
                        int right = (packet[0] >> 1) & 0x1;

                        /* signed movement deltas */
                        int dx = (char)packet[1];
                        int dy = (char)packet[2];

                        count = 0;

                        /* update internal cursor position */
                        x_pos = clamp(x_pos + dx, X_MIN, X_MAX);
                        y_pos = clamp(y_pos - dy, Y_MIN, Y_MAX);

                        /*
                            Placeholder click mapping:
                            left click = peach slice
                            right click = bomb
                            later you can replace this with hitbox logic using x_pos/y_pos
                        */
                        if (left) {
                            fruit_count++;
                            play_event_sound(audio_ptr, SOUND_PEACH);
                            current_state = STATE_FRUIT;
                        }
                        else if (right) {
                            play_event_sound(audio_ptr, SOUND_BOMB);
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