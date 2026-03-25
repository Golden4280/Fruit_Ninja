#include <stdio.h>

#define LEDR_BASE 0xFF200000
#define SWITCH_BASE 0xFF200040
#define KEY_BASE 0xFF200050
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

//FSM states
enum States {
    STATE_START,
    STATE_PLAY,
    STATE_FRUIT,
    STATE_BOMB,
    STATE_GAMEOVER
};

volatile enum States current_state = STATE_START;
volatile int fruit_count = 0;

//Cursor positions
volatile int x_pos = 160;
volatile int y_pos = 120;

//For VGA buffer
volatile int pixel_buffer_start;
short int Buffer1[240][512];
short int Buffer2[240][512];

//Interrupt key pasted from lacies code
static void handler(void) __attribute__((interrupt("machine")));//tells the compiler this function is a machine-mode interrupt handler and should use the correct interrupt
void KEY_ISR(void);
void set_KEY(void);

//PS/2 Helper functions
int  read_ps2_byte(volatile int *ps2_ptr, unsigned char *byte);
void write_ps2_byte(volatile int *ps2_ptr, unsigne`d char byte);
void clear_ps2_fifo(volatile int *ps2_ptr);
int  wait_for_ps2_byte(volatile int *ps2_ptr, unsigned char *byte, int timeout);
void init_mouse(volatile int *ps2_ptr);

//Delay so we can see state changes, clamp so mouse doesn't exit the screen
void delay(void);
int clamp(int value, int min, int max);

//VGA helper functions
void plot_pixel(int x, int y, short int colour);
void clear_screen(void);
void wait_for_vsync(void);
void draw_cursor(int x, int y);
void update_cursor_position(int dx, int dy);


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
    int pressed = *(KEY_ptr + 3); //read edge capture 
    *(KEY_ptr + 3) = pressed; //clear edge capture

    if (pressed & 0x1) { //if key0 is pressed change state depending on what ur in rn
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
    *(KEY_ptr + 3) = 0xF;  //clear everything
    *(KEY_ptr + 2) = 0x1;  //enable key0 to interrupt
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

//polling 
//keep waiting for RVALID=1 and a byte arrives, or timeout expires
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

//MAIN
int main(void) {
    volatile int *LEDR_ptr = (int *)LEDR_BASE;
    volatile int *switch_ptr = (int *)SWITCH_BASE;
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
    set_KEY();//enable key0 interrupts

    //INTERRUPT SETUP
    int mtvec_value  = (int)&handler;
    int mstatus_mask = 0x8;      
    int mie_value;

    __asm__ volatile("csrw mtvec, %0"    :: "r"(mtvec_value));
    __asm__ volatile("csrc mstatus, %0"  :: "r"(mstatus_mask)); 

    __asm__ volatile("csrr %0, mie"      : "=r"(mie_value));
    __asm__ volatile("csrc mie, %0"      :: "r"(mie_value));     

    mie_value = (1 << 18);
    __asm__ volatile("csrs mie, %0"      :: "r"(mie_value));   
    __asm__ volatile("csrs mstatus, %0"  :: "r"(mstatus_mask));  
    

    while (1) {
        int sw = *switch_ptr & 0x1F;
        int manual_fruit = sw & 0x1;
        int manual_bomb  = (sw >> 1) & 0x1;

        //FSM
        if (current_state == STATE_START) {
            *LEDR_ptr = ((fruit_count & 0x1F) << 5) | 0x01;
        }

        else if (current_state == STATE_PLAY) {
            *LEDR_ptr = ((fruit_count & 0x1F) << 5) | 0x02;

            //FOR TESTING
            if (manual_fruit) {
                fruit_count++;
                current_state = STATE_FRUIT;
            }
            else if (manual_bomb) {
                current_state = STATE_BOMB;
            }
            else {
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
                        //Bit 3 of a valid PS/2 mouse packet’s first byte is supposed to always be 1. (LINK)
                        if ((packet[0] & 0x08) == 0) {
                            count = 0;//restart the count
                        }
                    }
                    else if (count == 3) {//full packet
                        int left  =  packet[0] & 0x1;//bit 0 of first byte (left mouse button, if pressed becomes 1)
                        int right = (packet[0] >> 1) & 0x1; //bit 1 of first byte (right click)
                        
                        //ps2 is SIGNED, signed is stored in bit 4 of byte 1
                        int dx = (int)packet[1]; //x movement 
                        if (packet[0] & 0x10) {
                            dx = dx - 256;
                        }
                        int dy = (int)packet[2]; //y moevement
                        if(packet[0] & 0x20){
                            dy = dy - 256;                        }
    
                        //first byte has overflow x = bit6 and overflowy = bit7
                        //if overflow happens, ignore that by setting it to 0
                        if (packet[0] & 0x40) dx = 0;
                        if (packet[0] & 0x80) dy = 0;

                        count = 0;//reset packet count

                        update_cursor_position(dx, dy);//move cursor

                        if (left) {//if left click increase fruit count
                            fruit_count++;
                            current_state = STATE_FRUIT;
                        }
                        else if (right) {//bomb hit
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

        //Draw frame
        clear_screen();
        draw_cursor(x_pos, y_pos);

        wait_for_vsync();
        pixel_buffer_start = *(pixel_ctrl_ptr + 1);
    }

    return 0;
}