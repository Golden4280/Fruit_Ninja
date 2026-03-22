#include <stdio.h>

#define LEDR_BASE   0xFF200000
#define SWITCH_BASE 0xFF200040
#define KEY_BASE    0xFF200050
#define PS2_BASE    0xFF200100
#define VGA_BASE    0xFF203020

#define SCREEN_W 320
#define SCREEN_H 240
#define CURSOR_SIZE 5

#define X_MIN 0
#define X_MAX (SCREEN_W - CURSOR_SIZE)
#define Y_MIN 0
#define Y_MAX (SCREEN_H - CURSOR_SIZE)

#define BLACK  0x0000
#define WHITE  0xFFFF

/* ---------------------------------------------------------------
 * Tail settings — tweak these to taste
 *
 *   TAIL_LEN   : number of ghost positions stored (more = longer tail)
 *   TAIL_MIN_SZ: smallest ghost square size in pixels
 *   TAIL_STEP  : how many pixels of movement before a new ghost is added
 *
 * The tail records a new snapshot only when the cursor has moved at
 * least TAIL_STEP pixels (Manhattan distance) since the last snapshot.
 * This keeps the tail "sparse" when barely moving so it feels natural
 * rather than a dense blur sitting under a stationary cursor.
 * --------------------------------------------------------------- */
#define TAIL_LEN    10
#define TAIL_MIN_SZ  0
#define TAIL_STEP    1

enum States {
    STATE_START,
    STATE_PLAY,
    STATE_FRUIT,
    STATE_BOMB,
    STATE_GAMEOVER
};

volatile enum States current_state = STATE_START;
volatile int fruit_count = 0;

volatile int x_pos = 160;
volatile int y_pos = 120;

/* ---------------------------------------------------------------
 * Tail ring buffer
 *
 * tail_x[], tail_y[]  — stored positions (oldest first after wrap)
 * tail_head           — index of the NEXT write slot (oldest entry)
 * tail_count          — how many valid entries are stored so far
 * tail_last_x/y       — last position a snapshot was recorded at
 *                       (used for TAIL_STEP threshold)
 * --------------------------------------------------------------- */
int tail_x[TAIL_LEN];
int tail_y[TAIL_LEN];
int tail_head  = 0;
int tail_count = 0;
int tail_last_x = 160;
int tail_last_y = 120;

/* VGA globals */
volatile int pixel_buffer_start;
short int Buffer1[240][512];
short int Buffer2[240][512];

/* ---------------------------------------------------------------
 * Forward declarations
 * --------------------------------------------------------------- */
static void handler(void) __attribute__((interrupt("machine")));
void KEY_ISR(void);
void set_KEY(void);

int  read_ps2_byte(volatile int *ps2_ptr, unsigned char *byte);
void write_ps2_byte(volatile int *ps2_ptr, unsigned char byte);
void clear_ps2_fifo(volatile int *ps2_ptr);
int  wait_for_ps2_byte(volatile int *ps2_ptr, unsigned char *byte, int timeout);
void init_mouse(volatile int *ps2_ptr);

void delay(void);
int  clamp(int value, int min, int max);
int  abs_val(int v);

void plot_pixel(int x, int y, short int colour);
void clear_screen(void);
void wait_for_vsync(void);
void draw_cursor(int x, int y);
void update_cursor_position(int dx, int dy);
void push_tail(int x, int y);
void draw_tail(void);

/* ---------------------------------------------------------------
 * Utility
 * --------------------------------------------------------------- */
void delay(void) {
    volatile int i;
    for (i = 0; i < 500000; i++);
}

int clamp(int value, int min, int max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

int abs_val(int v) {
    return v < 0 ? -v : v;
}

/* ---------------------------------------------------------------
 * Interrupt handler
 * --------------------------------------------------------------- */
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
        } else if (current_state == STATE_GAMEOVER) {
            fruit_count = 0;
            x_pos = 160;
            y_pos = 120;
            tail_head  = 0;
            tail_count = 0;
            tail_last_x = 160;
            tail_last_y = 120;
            current_state = STATE_START;
        }
    }
}

void set_KEY(void) {
    volatile int *KEY_ptr = (int *)KEY_BASE;
    *(KEY_ptr + 3) = 0xF;
    *(KEY_ptr + 2) = 0x1;
}

/* ---------------------------------------------------------------
 * PS/2
 * --------------------------------------------------------------- */
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
        if (read_ps2_byte(ps2_ptr, byte)) return 1;
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

/* ---------------------------------------------------------------
 * VGA
 * --------------------------------------------------------------- */
void plot_pixel(int x, int y, short int colour) {
    volatile short int *pixel_address =
        (volatile short int *)(pixel_buffer_start + (y << 10) + (x << 1));
    *pixel_address = colour;
}

void clear_screen(void) {
    int x, y;
    for (x = 0; x < SCREEN_W; x++)
        for (y = 0; y < SCREEN_H; y++)
            plot_pixel(x, y, BLACK);
}

void wait_for_vsync(void) {
    volatile int *pixel_ctrl_ptr = (int *)VGA_BASE;
    *pixel_ctrl_ptr = 1;
    while (*(pixel_ctrl_ptr + 3) & 0x01) {}
}

void draw_cursor(int x, int y) {
    int row, col;
    for (row = 0; row < CURSOR_SIZE; row++) {
        for (col = 0; col < CURSOR_SIZE; col++) {
            int px = x + col;
            int py = y + row;
            if (px >= 0 && px < SCREEN_W && py >= 0 && py < SCREEN_H)
                plot_pixel(px, py, WHITE);
        }
    }
}

void update_cursor_position(int dx, int dy) {
    x_pos = clamp(x_pos + dx, X_MIN, X_MAX);
    y_pos = clamp(y_pos - dy, Y_MIN, Y_MAX);
}

/* ---------------------------------------------------------------
 * push_tail — record a new tail snapshot if the cursor has moved
 *             far enough from the last recorded position.
 *
 * The ring buffer stores positions from oldest (tail_head) to newest
 * (tail_head - 1 mod TAIL_LEN).  draw_tail() iterates the buffer
 * from oldest to newest and assigns each position a brightness and
 * size based on how recent it is.
 * --------------------------------------------------------------- */
void push_tail(int x, int y) {
    int dist = abs_val(x - tail_last_x) + abs_val(y - tail_last_y);

    if (dist < TAIL_STEP) return;

    tail_x[tail_head] = x;
    tail_y[tail_head] = y;
    tail_head = (tail_head + 1) % TAIL_LEN;

    if (tail_count < TAIL_LEN) tail_count++;

    tail_last_x = x;
    tail_last_y = y;
}

/* ---------------------------------------------------------------
 * draw_tail — draw every ghost in the ring buffer.
 *
 * For each ghost we compute:
 *
 *   age   = 0 (oldest) … tail_count-1 (most recent, just behind cursor)
 *   frac  = age / (tail_count - 1)   → 0.0 … 1.0 scaled by 16 to
 *           avoid floating point:
 *           frac16 = age * 16 / (tail_count - 1)   (0 … 16)
 *
 * Size:
 *   size = TAIL_MIN_SZ + (CURSOR_SIZE - TAIL_MIN_SZ) * frac16 / 16
 *   Oldest  → TAIL_MIN_SZ pixels square (tiny)
 *   Newest  → CURSOR_SIZE pixels square (same as cursor, blends in)
 *
 * Colour (RGB565 grey):
 *   The 5-bit R/B channels and 6-bit G channel are set proportionally.
 *   Oldest  → very dim  (~4/31 full brightness)
 *   Newest  → ~24/31    (bright but not full white, so cursor stands out)
 *
 *   colour = (r << 11) | (g << 5) | b
 *   r, b ∈ [0..31],  g ∈ [0..63]
 *
 *   r = 4  + (24 - 4)  * frac16 / 16  =  4 + 20 * frac16 / 16
 *   g = 8  + (48 - 8)  * frac16 / 16  =  8 + 40 * frac16 / 16
 *   b = 4  + (24 - 4)  * frac16 / 16  =  4 + 20 * frac16 / 16
 *
 * The ghost is centred on the stored position so it shrinks inward
 * symmetrically, which makes it look like the cursor is "trailing"
 * a shrinking echo rather than a left-aligned stub.
 * --------------------------------------------------------------- */
void draw_tail(void) {
    int i, age, idx, frac16, size, half, r, g, b;
    short int colour;
    int px, py, row, col;

    if (tail_count < 2) return;

    for (i = 0; i < tail_count; i++) {
        /* i=0 is oldest, i=tail_count-1 is most recent */
        idx = (tail_head - tail_count + i + TAIL_LEN * 2) % TAIL_LEN;

        age    = i;
        frac16 = age * 16 / (tail_count - 1);   /* 0 … 16, integer */

        /* Size: smallest for oldest, grows toward cursor size */
        size = TAIL_MIN_SZ + ((CURSOR_SIZE - TAIL_MIN_SZ) * frac16) / 16;
        if (size < 1) size = 1;

        /* Colour: dim grey for oldest, bright grey for newest */
        r = 4  + (20 * frac16) / 16;
        g = 8  + (40 * frac16) / 16;
        b = 4  + (20 * frac16) / 16;
        colour = (short int)((r << 11) | (g << 5) | b);

        /* Centre the ghost on the stored cursor centre */
        half = size / 2;

        for (row = 0; row < size; row++) {
            for (col = 0; col < size; col++) {
                px = tail_x[idx] + (CURSOR_SIZE / 2) - half + col;
                py = tail_y[idx] + (CURSOR_SIZE / 2) - half + row;
                if (px >= 0 && px < SCREEN_W && py >= 0 && py < SCREEN_H)
                    plot_pixel(px, py, colour);
            }
        }
    }
}

/* ---------------------------------------------------------------
 * main
 * --------------------------------------------------------------- */
int main(void) {
    volatile int *LEDR_ptr       = (int *)LEDR_BASE;
    volatile int *switch_ptr     = (int *)SWITCH_BASE;
    volatile int *ps2_ptr        = (int *)PS2_BASE;
    volatile int *pixel_ctrl_ptr = (int *)VGA_BASE;

    unsigned char byte;

    /* VGA double-buffer init */
    *(pixel_ctrl_ptr + 1) = (int)&Buffer1;
    wait_for_vsync();
    pixel_buffer_start = *pixel_ctrl_ptr;
    clear_screen();

    *(pixel_ctrl_ptr + 1) = (int)&Buffer2;
    pixel_buffer_start = *(pixel_ctrl_ptr + 1);
    clear_screen();

    /* Peripheral init */
    init_mouse(ps2_ptr);
    set_KEY();

    /* Interrupt setup (identical to your working file 1) */
    {
        int mtvec_value  = (int)&handler;
        int mstatus_mask = 0x8;
        int mie_value;

        __asm__ volatile("csrw mtvec, %0"   :: "r"(mtvec_value));
        __asm__ volatile("csrc mstatus, %0" :: "r"(mstatus_mask));

        __asm__ volatile("csrr %0, mie"     : "=r"(mie_value));
        __asm__ volatile("csrc mie, %0"     :: "r"(mie_value));

        mie_value = (1 << 18);
        __asm__ volatile("csrs mie, %0"     :: "r"(mie_value));
        __asm__ volatile("csrs mstatus, %0" :: "r"(mstatus_mask));
    }

    while (1) {
        int sw           = *switch_ptr & 0x1F;
        int manual_fruit =  sw & 0x1;
        int manual_bomb  = (sw >> 1) & 0x1;

        /* --- FSM --- */
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
                while (read_ps2_byte(ps2_ptr, &byte)) {
                    static int count = 0;
                    static unsigned char packet[3];

                    packet[count++] = byte;

                    if (count == 1) {
                        if ((packet[0] & 0x08) == 0) { count = 0; }
                    }
                    else if (count == 3) {
                        int left  =  packet[0] & 0x1;
                        int right = (packet[0] >> 1) & 0x1;

                        int dx = (int)packet[1] - ((packet[0] & 0x10) ? 256 : 0);
                        int dy = (int)packet[2] - ((packet[0] & 0x20) ? 256 : 0);

                        if (packet[0] & 0x40) dx = 0;
                        if (packet[0] & 0x80) dy = 0;

                        count = 0;

                        update_cursor_position(dx, dy);

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

        /* Record tail snapshot before drawing */
        push_tail(x_pos, y_pos);

        /* Draw frame: tail first (behind cursor), cursor on top */
        clear_screen();
        draw_tail();
        draw_cursor(x_pos, y_pos);

        wait_for_vsync();
        pixel_buffer_start = *(pixel_ctrl_ptr + 1);
    }

    return 0;
}