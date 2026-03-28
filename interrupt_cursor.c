#include <stdio.h>

#define LEDR_BASE 0xFF200000
#define SWITCH_BASE 0xFF200040
#define PS2_BASE 0xFF200100
#define VGA_BASE 0xFF203020

#define SCREEN_W 320
#define SCREEN_H 240
#define CURSOR_SIZE 6

#define X_MIN 0
#define X_MAX (SCREEN_W - CURSOR_SIZE)
#define Y_MIN 0
#define Y_MAX (SCREEN_H - CURSOR_SIZE)

#define BLACK  0x0000
#define WHITE  0xFFFF
#define TAIL_LEN    10 //number of ghost positions stored , tail length
#define TAIL_MIN_SZ  0 // smallest tail square size in pixels
#define TAIL_STEP    1 //how many pixels of movement before a new box is added

//FSM states -- no FRUIT or BOMB, collision module will handle those later
enum States {
    STATE_START,
    STATE_PLAY,
    STATE_GAMEOVER
};

volatile enum States current_state = STATE_START;

//Cursor positions
volatile int x_pos = 160;
volatile int y_pos = 120;

int tail_x[TAIL_LEN]; //stored positions (oldest)
int tail_y[TAIL_LEN]; 
int tail_head  = 0; //Points to where the next position will be written.
                    //  When the buffer is full, this overwrites the oldest point.
int tail_count = 0; //how many valid entries are stored so far
int tail_last_x = 160;  //Last position where we added a tail point.
                        // Ensures we only add new points after moving enough (TAIL_STEP).
int tail_last_y = 120;

//For VGA buffer
volatile int pixel_buffer_start;
short int Buffer1[240][512];
short int Buffer2[240][512];

//PS/2 Helper functions
int  read_ps2_byte(volatile int *ps2_ptr, unsigned char *byte);
void write_ps2_byte(volatile int *ps2_ptr, unsigned char byte);
void clear_ps2_fifo(volatile int *ps2_ptr);
int  wait_for_ps2_byte(volatile int *ps2_ptr, unsigned char *byte, int timeout);
void init_mouse(volatile int *ps2_ptr);

//Delay so we can see state changes, clamp so mouse doesn't exit the screen
void delay(void);
int clamp(int value, int min, int max);
int abs_val(int v);

//VGA helper functions
void plot_pixel(int x, int y, short int colour);
void clear_screen(void);
void wait_for_vsync(void);
void draw_cursor(int x, int y);
void update_cursor_position(int dx, int dy);
void push_tail(int x, int y);
void draw_tail(void);
void reset_game(void);


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
    if (v < 0) {
        return -v;
    } else {
        return v;
    }
}

// Reset all game state back to initial values
void reset_game(void) {
    x_pos = 160;
    y_pos = 120;
    tail_head = 0;
    tail_count = 0;
    tail_last_x = 160;
    tail_last_y = 120;
}


int read_ps2_byte(volatile int *ps2_ptr, unsigned char *byte) {//ONLY USE DATA IF RVALID IS 1
    int data = *ps2_ptr; //read data reg THIS ALSO POPS THIS DATA OFF THE FIFO WITHOUT ME DOIN ANYTHING
    if (data & 0x8000) {// checking if bit 15 or RVALID is 1, if not the byte is not valid so ignore 
        *byte = (unsigned char)(data & 0xFF);//if valid, keep only the lower actual data bits of data
        return 1;
    }
    return 0;
}

void write_ps2_byte(volatile int *ps2_ptr, unsigned char byte) {
    *ps2_ptr = (int)byte;//put this byte into the PS/2 data register so the core sends it to the mouse
}

//read until read_ps2_byte says no valid data remains
//Each successful read removes one item from that FIFO
//So loop keeps consuming bytes until the FIFO is empty
void clear_ps2_fifo(volatile int *ps2_ptr) {//what happens when we call clear
    unsigned char throwaway_byte;

    while (1) {
        int success = read_ps2_byte(ps2_ptr, &throwaway_byte);

        if (success == 0) {
            break;
        }
        //otherwise we got a byte so inf loop
    }
}

// polling 
// keep waiting for RVALID=1 and a byte arrives, or timeout expires
int wait_for_ps2_byte(volatile int *ps2_ptr, unsigned char *byte, int timeout) {
    while (timeout > 0) {
        if (read_ps2_byte(ps2_ptr, byte) == 1) {
            return 1;
        }
        timeout--;
    }
    return 0;
}

void init_mouse(volatile int *ps2_ptr) {
    unsigned char byte_received;

    // Remove any old leftover bytes
    clear_ps2_fifo(ps2_ptr);

    // Tell mouse to reset
    write_ps2_byte(ps2_ptr, 0xFF);

    // Wait for mouse responses after reset
    wait_for_ps2_byte(ps2_ptr, &byte_received, 1000000);   // aknowledgment 0xFA
    wait_for_ps2_byte(ps2_ptr, &byte_received, 1000000);   // self-test passed 0xAA
    wait_for_ps2_byte(ps2_ptr, &byte_received, 1000000);   // mouse ID 0x00

    // Remove any extra leftover bytes
    clear_ps2_fifo(ps2_ptr);

    // Tell mouse to start sending movement/button data (3 byte packets)
    write_ps2_byte(ps2_ptr, 0xF4);

    // Wait for acknowledgment 
    wait_for_ps2_byte(ps2_ptr, &byte_received, 1000000);

    //clear one last time to be safe
    clear_ps2_fifo(ps2_ptr);
}

//DRAW MOUSE
void plot_pixel(int x, int y, short int colour) {
    volatile short int *pixel_address = (volatile short int *)(pixel_buffer_start + (y << 10) + (x << 1));
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

    // Request a buffer swap (start synchronization)
    *pixel_ctrl_ptr = 1;

    // Keep checking the status register until sync is done
    while (1) {
        int status = *(pixel_ctrl_ptr + 3);   // read status register

        if ((status & 0x01) == 0) {
            break;  // sync finished
        }
    }
}

void draw_cursor(int x, int y) {
    int row, col;
    for (row = 0; row < CURSOR_SIZE; row++) { //height
        for (col = 0; col < CURSOR_SIZE; col++) {//width
            int px = x + col;   // horizontal position
            int py = y + row;   // vertical position
            // Make sure the pixel is inside the screen boundaries
            if (px >= 0 && px < SCREEN_W && py >= 0 && py < SCREEN_H)
                // Draw a white pixel at that position
                plot_pixel(px, py, WHITE);
        }
    }
}

void update_cursor_position(int dx, int dy) {
    x_pos = clamp(x_pos + dx, X_MIN, X_MAX);
    y_pos = clamp(y_pos - dy, Y_MIN, Y_MAX);
}

 //saves a new tail position whn cursor moved since last saved point
 //use circular buffer --> tail_head points to the oldest position, 
 //draw tail goes from oldest to newest and makes newer points brighter and larger 
void push_tail(int x, int y) {
    //how far the cursor moved since the last saved point
    int dist = abs_val(x - tail_last_x) + abs_val(y - tail_last_y);
    // If it hasn't moved far enough, don't add a new tail point
    if (dist < TAIL_STEP) return;
    //Store the new position at the current write index
    tail_x[tail_head] = x;
    tail_y[tail_head] = y;
    //Move the write index forward (wrap around if at the end)
    tail_head = (tail_head + 1) % TAIL_LEN;
    //Increase count until buffer is full
    if (tail_count < TAIL_LEN)
        tail_count++;
    //Update last recorded position
    tail_last_x = x;
    tail_last_y = y;
}

void draw_tail(void) {
    int i, age, idx, frac16, size, half, r, g, b;
    short int colour;
    int px, py, row, col;

    // If we don't have enough points theres nothing to draw
    if (tail_count < 2) return;

    // Loop through all stored tail points (oldest → newest)
    for (i = 0; i < tail_count; i++) {

        // Find the correct index in the circular buffer
        // (handles wrap-around properly)
        idx = (tail_head - tail_count + i + TAIL_LEN * 2) % TAIL_LEN;

        age = i;  // 0 = oldest, larger = more recent

        // Scale age from 0 → 16 (used for size + brightness)
        frac16 = age * 16 / (tail_count - 1);

        // Make older points smaller, newer ones bigger
        size = TAIL_MIN_SZ + ((CURSOR_SIZE - TAIL_MIN_SZ) * frac16) / 16;
        if (size < 1) size = 1;

        // Make older points darker, newer ones brighter (grey color)
        r = 4  + (20 * frac16) / 16;
        g = 8  + (40 * frac16) / 16;
        b = 4  + (20 * frac16) / 16;

        // Combine RGB into 16-bit colour
        colour = (short int)((r << 11) | (g << 5) | b);

        // Used to center the square around the stored position
        half = size / 2;

        // Draw a square for this tail point
        for (row = 0; row < size; row++) {
            for (col = 0; col < size; col++) {

                // Calculate pixel position
                px = tail_x[idx] + (CURSOR_SIZE / 2) - half + col;
                py = tail_y[idx] + (CURSOR_SIZE / 2) - half + row;

                // Only draw if inside screen bounds
                if (px >= 0 && px < SCREEN_W && py >= 0 && py < SCREEN_H)
                    plot_pixel(px, py, colour);
            }
        }
    }
}

//MAIN
int main(void) {
    volatile int *LEDR_ptr = (int *)LEDR_BASE;
    volatile int *ps2_ptr = (int *)PS2_BASE;
    volatile int *pixel_ctrl_ptr = (int *)VGA_BASE;

    unsigned char byte;//Stores one byte read from the PS/2 port

    //double buffer
    *(pixel_ctrl_ptr + 1) = (int)&Buffer1;//buffer1 is the back buffer
    wait_for_vsync();//Swap buffers at the next vertical sync
    pixel_buffer_start = *pixel_ctrl_ptr; // Get the address of the current front buffer
    clear_screen();

    *(pixel_ctrl_ptr + 1) = (int)&Buffer2; //buffer2 is the back buffer
    pixel_buffer_start = *(pixel_ctrl_ptr + 1);//drawing happens in buffer 2
    clear_screen();

    init_mouse(ps2_ptr);//initialize mouse
    //NOTHING CAN CHANGE TO BOMB OR HIT FRUIT RN 

    while (1) {
        //FSM
        if (current_state == STATE_START) {
            *LEDR_ptr = 0x01; // LED 0 on = waiting on start screen

            //PS2 3 bit packet
            //byte 1 = status
            //byte 2 = x
            //byte 3 = y
            while (read_ps2_byte(ps2_ptr, &byte)) {//keep looping as long as theres a VALID ps2 byte available
                //each time through the loop, byte gets one new byte from the mouse FIFO.
                static int count = 0; //how many bytes of the current packet have been collected so far. (static so looping doesnt reset it)
                static unsigned char packet[3];//3 bytes of mouse packet

                packet[count++] = byte;

                if (count == 1) {
                    //Bit 3 of a valid PS/2 mouse packet's first byte is supposed to always be 1. (LINK)
                    if ((packet[0] & 0x08) == 0) {
                        count = 0;//restart the count
                    }
                }
                else if (count == 3) {//full packet
                    int left  =  packet[0] & 0x1;//bit 0 of first byte (left mouse button, if pressed becomes 1)
                    int right = (packet[0] >> 1) & 0x1; //bit 1 of first byte (right click)

                    count = 0;//reset packet count

                    if (left || right) {//any click from start goes to play
                        current_state = STATE_PLAY;
                        break;
                    }
                }
            }
        }

        else if (current_state == STATE_PLAY) {
            *LEDR_ptr = 0x02; // LED 1 on = playing

            //PS2 3 bit packet
            //byte 1 = status
            //byte 2 = x
            //byte 3 = y
            while (read_ps2_byte(ps2_ptr, &byte)) {//keep looping as long as theres a VALID ps2 byte available
                //each time through the loop, byte gets one new byte from the mouse FIFO.
                static int count = 0; //how many bytes of the current packet have been collected so far. (static so looping doesnt reset it)
                static unsigned char packet[3];//3 bytes of mouse packet

                packet[count++] = byte; 

                if (count == 1) {
                    //Bit 3 of a valid PS/2 mouse packet's first byte is supposed to always be 1. (LINK)
                    if ((packet[0] & 0x08) == 0) {
                        count = 0;//restart the count
                    }
                }
                else if (count == 3) {//full packet
                    //ps2 is SIGNED, signed is stored in bit 4 of byte 1
                    int dx = (int)packet[1]; //x movement 
                    if (packet[0] & 0x10) {
                        dx = dx - 256;
                    }
                    int dy = (int)packet[2]; //y movement
                    if(packet[0] & 0x20){
                        dy = dy - 256;
                    }

                    //first byte has overflow x = bit6 and overflowy = bit7
                    //if overflow happens, ignore that by setting it to 0
                    if (packet[0] & 0x40) dx = 0;
                    if (packet[0] & 0x80) dy = 0;

                    count = 0;//reset packet count

                    update_cursor_position(dx, dy);//move cursor

                }
            }
        }

        else if (current_state == STATE_GAMEOVER) {
            *LEDR_ptr = 0x10; // LED 4 on = game over

            //PS2 3 bit packet
            //byte 1 = status
            //byte 2 = x
            //byte 3 = y
            while (read_ps2_byte(ps2_ptr, &byte)) {//keep looping as long as theres a VALID ps2 byte available
                //each time through the loop, byte gets one new byte from the mouse FIFO.
                static int count = 0; //how many bytes of the current packet have been collected so far. (static so looping doesnt reset it)
                static unsigned char packet[3];//3 bytes of mouse packet

                packet[count++] = byte;

                if (count == 1) {
                    //Bit 3 of a valid PS/2 mouse packet's first byte is supposed to always be 1. (LINK)
                    if ((packet[0] & 0x08) == 0) {
                        count = 0;//restart the count
                    }
                }
                else if (count == 3) {//full packet
                    int left  =  packet[0] & 0x1;//bit 0 of first byte (left mouse button, if pressed becomes 1)
                    int right = (packet[0] >> 1) & 0x1; //bit 1 of first byte (right click)

                    count = 0;//reset packet count

                    if (left || right) {//any click from game over resets and goes back to start
                        reset_game();
                        current_state = STATE_START;
                        break;
                    }
                }
            }
        }

        /* Record tail snapshot before drawing */
        push_tail(x_pos, y_pos);

        //Draw frame
        clear_screen();
        draw_tail();
        draw_cursor(x_pos, y_pos);

        wait_for_vsync();
        pixel_buffer_start = *(pixel_ctrl_ptr + 1);
    }

    return 0;
}