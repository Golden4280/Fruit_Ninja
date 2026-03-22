#include <stdio.h>

#define LEDR_BASE   0xFF200000
#define SWITCH_BASE 0xFF200040
#define KEY_BASE    0xFF200050
#define PS2_BASE    0xFF200100
#define VGA_BASE    0xFF203020

#define SCREEN_W 320
#define SCREEN_H 240

#define CURSOR_SIZE 6

#define BLACK 0x0000
#define WHITE 0xFFFF

enum States {
    STATE_START,
    STATE_PLAY,
    STATE_FRUIT,
    STATE_BOMB,
    STATE_GAMEOVER
};

volatile enum States current_state = STATE_START;
volatile int fruit_count = 0;

/* cursor position */
volatile int x_pos = 160;
volatile int y_pos = 120;

/* VGA globals */
volatile int pixel_buffer_start;
short int Buffer1[240][512];
short int Buffer2[240][512];

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

/* utility */
void delay(void);

/* VGA helper functions */
void plot_pixel(int x, int y, short int colour);
void clear_screen(void);
void wait_for_vsync(void);
void draw_cursor(int x, int y);
void update_cursor_position(int dx, int dy);

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
    while (read_ps2_byte(ps2_ptr, &dummy)) {}
}

int wait_for_ps2_byte(volatile int *ps2_ptr, unsigned char *byte, int timeout) {
    while (timeout-- > 0) {
        if (read_ps2_byte(ps2_ptr, byte)) {
            return 1;
        }
    }
    return 0;
}

void init_mouse(volatile int *ps2_ptr) {
    unsigned char byte;

    clear_ps2_fifo(ps2_ptr);

    write_ps2_byte(ps2_ptr, 0xFF);
    wait_for_ps2_byte(ps2_ptr, &byte, 1000000);
    wait_for_ps2_byte(ps2_ptr, &byte, 1000000);
    wait_for_ps2_byte(ps2_ptr, &byte, 1000000);

    clear_ps2_fifo(ps2_ptr);

    write_ps2_byte(ps2_ptr, 0xF4);
    wait_for_ps2_byte(ps2_ptr, &byte, 1000000);

    clear_ps2_fifo(ps2_ptr);
}

/* VGA */

void plot_pixel(int x, int y, short int colour) {
    volatile short int *pixel_address;
    pixel_address = (volatile short int *)(pixel_buffer_start + (y << 10) + (x << 1));
    *pixel_address = colour;
}

void clear_screen(void) {
    int x, y;
    for (x = 0; x < SCREEN_W; x++) {
        for (y = 0; y < SCREEN_H; y++) {
            plot_pixel(x, y, BLACK);
        }
    }
}

void wait_for_vsync(void) {
    volatile int *pixel_ctrl_ptr = (int *)VGA_BASE;
    int status;

    *pixel_ctrl_ptr = 1;
    status = *(pixel_ctrl_ptr + 3);

    while (status & 0x01) {
        status = *(pixel_ctrl_ptr + 3);
    }
}

void draw_cursor(int x, int y) {
    int row, col;

    for (row = 0; row < CURSOR_SIZE; row++) {
        for (col = 0; col < CURSOR_SIZE; col++) {
            int px = x + col;
            int py = y + row;

            if (px >= 0 && px < SCREEN_W && py >= 0 && py < SCREEN_H) {
                plot_pixel(px, py, WHITE);
            }
        }
    }
}

void update_cursor_position(int dx, int dy) {
    x_pos = x_pos + dx;
    y_pos = y_pos - dy;

    if (x_pos < 0) x_pos = 0;
    if (x_pos > SCREEN_W - CURSOR_SIZE) x_pos = SCREEN_W - CURSOR_SIZE;

    if (y_pos < 0) y_pos = 0;
    if (y_pos > SCREEN_H - CURSOR_SIZE) y_pos = SCREEN_H - CURSOR_SIZE;
}

int main(void) {
    volatile int *LEDR_ptr = (int *)LEDR_BASE;
    volatile int *switch_ptr = (int *)SWITCH_BASE;
    volatile int *ps2_ptr = (int *)PS2_BASE;
    volatile int *pixel_ctrl_ptr = (int *)VGA_BASE;

    unsigned char byte;

    *(pixel_ctrl_ptr + 1) = (int)&Buffer1;
    wait_for_vsync();
    pixel_buffer_start = *pixel_ctrl_ptr;
    clear_screen();

    *(pixel_ctrl_ptr + 1) = (int)&Buffer2;
    pixel_buffer_start = *(pixel_ctrl_ptr + 1);
    clear_screen();

    init_mouse(ps2_ptr);
    set_KEY();

    while (1) {

        while (read_ps2_byte(ps2_ptr, &byte)) {
            static int count = 0;
            static unsigned char packet[3];

            packet[count++] = byte;

            if (count == 1) {
                /* Bit 3 of the status byte must always be 1 — if not, we're
                   out of sync. Discard and reset. */
                if ((packet[0] & 0x08) == 0) {
                    packet[0] = 0;
                    count = 0;
                    continue;
                }
            }
            else if (count == 3) {
                /* FIX: Apply sign bits from status byte (bits 4 and 5).
                   The PS/2 protocol stores the sign of dx in bit 4 of the
                   status byte and the sign of dy in bit 5. We must use these
                   to extend the 8-bit values to signed integers — NOT a
                   simple (signed char) cast, which ignores those bits. */
                int dx = (int)packet[1] - ((packet[0] & 0x10) ? 256 : 0);
                int dy = (int)packet[2] - ((packet[0] & 0x20) ? 256 : 0);

                /* Discard movement on overflow (bits 6 and 7 of status) */
                if (packet[0] & 0x40) dx = 0;
                if (packet[0] & 0x80) dy = 0;

                count = 0;

                update_cursor_position(dx, dy);
            }
        }

        clear_screen();
        draw_cursor(x_pos, y_pos);

        wait_for_vsync();
        pixel_buffer_start = *(pixel_ctrl_ptr + 1);
    }

    return 0;
}