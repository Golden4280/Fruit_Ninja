#include <stdio.h>
#include <stdlib.h>


// objects
#include "Banana.h"
#include "Lemon.h"
#include "Pineapple.h"
#include "Red_Apple.h"
#include "Strawberry.h"
#include "Watermelon.h"
#include "Pomegranate.h"
#include "Bomb.h"

// screens
#include "play.h"
#include "start.h"
#include "gameover.h"

// struct
typedef enum {
  APPLE,
  LEMON,
  BANANA,
  PINEAPPLE,
  WATERMELON,
  STRAWBERRY,
  POMEGRANATE,
  BOMB
} Type;

typedef struct {
  // type of object for different actions/vga
  Type type;
  // direction/velocity
  float vx, vy;
  // position
  float x, y;
  // dimensions
  int w, h;

  float g;

  // check location onscreen
  int onScreen;  // 1 if active

  const unsigned short* image;

} Object;

#define MAX_OBJECTS 7
Object objects[MAX_OBJECTS];

// FUNCTIONS

void plot_pixel(int x, int y, short int colour);
void wait_for_vsync();
void clear_screen();
void randomGenerator(Object* obj);
void physics();
void draw_background(const unsigned short* bg);
void draw_object(int x0, int y0, int w, int h, const unsigned short* obj);
void drawAllObjects();


// DEFINED THINGS

#define VGA 0xff203020
// #define pixel_buffer_start 0x08000000
#define ROWS 240
#define COLS 320
#define pCOLS 512
#define TRANSPARENT 0x0000
volatile int pixel_buffer_start;  // global variable
short int Buffer1[240][512];      // 240 rows, 512 (320 + padding) columns
short int Buffer2[240][512];

// MOUSE DEFINED THINGS
#define LEDR_BASE 0xFF200000
#define SWITCH_BASE 0xFF200040
#define KEY_BASE 0xFF200050
#define PS2_BASE 0xFF200100

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

// MOUSE CODE

//FSM states
enum States {
    STATE_START,
    STATE_PLAY,
    STATE_FRUIT,
    STATE_BOMB,
    STATE_GAMEOVER
};

// boolean for state switching
// no longer using fruit and bomb state
// instead if bomg_hit == 1 then current_state = gameover else stays in play
bool bomb_hit = 0;

volatile enum States current_state = STATE_START;
volatile int fruit_count = 0;

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

// VGA STUFF WAS HERE IN OG CODE

//Interrupt key pasted from lacies code
static void handler(void) __attribute__((interrupt("machine")));//tells the compiler this function is a machine-mode interrupt handler and should use the correct interrupt
void KEY_ISR(void);
void set_KEY(void);

//PS/2 Helper functions
int  read_ps2_byte(volatile int *ps2_ptr, unsigned char *byte);
void write_ps2_byte(volatile int *ps2_ptr, unsigned char byte);
void clear_ps2_fifo(volatile int *ps2_ptr);
int  wait_for_ps2_byte(volatile int *ps2_ptr, unsigned char *byte, int timeout);
void init_mouse(volatile int *ps2_ptr);

//Delay so we can see state changes, clamp so mouse doesn't exit the screen
void delay(void);
int clamp(int value, int min, int max);

// VGA HELPER FOR MOUSE
void draw_cursor(int x, int y);
void update_cursor_position(int dx, int dy);
void push_tail(int x, int y);
void draw_tail(void);

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

// DRAW MOUSE
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
    // If it hasn’t moved far enough, don’t add a new tail point
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

    // If we don’t have enough points theres nothing to draw
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



// VGA AND BACKGROUND

// recall pixel addr = base + (y << 10) + (x << 1)

// PLOT PIXEL FUNCTION
void plot_pixel(int x, int y, short int colour) {
  volatile short int* pixel;

  pixel = (volatile short int*)(pixel_buffer_start + (y << 10) + (x << 1));

  *pixel = colour;
}

// VSYNC FUNCTION
void wait_for_vsync() {
  // addr of first buffer
  volatile int* pixel_ctrl_ptr = (int*)VGA;

  // write 1 to begin sync process
  // sets S bit to 1
  *pixel_ctrl_ptr = 1;

  // polling loop for swap (status reg)
  // wait until S set back to 1
  while ((*(pixel_ctrl_ptr + 3) & 0x01) != 0);
}

// CLEAR SCREEN FUNCTION
// loop through entire background
// set each pixel to black to reset
void clear_screen() {
  for (int x = 0; x < 320; x++) {
    for (int y = 0; y < 240; y++) {
      plot_pixel(x, y, 0x0000);
    }
  }
}

void draw_background(const unsigned short* bg) {
// loop through each indec of array and draw corresponding pixxel
// y*cols + x since we go through every x then increment to next y 

  for (int y = 0; y < ROWS; y++) {
    for (int x = 0; x < COLS; x++) {
      unsigned short colour = bg[y * COLS + x];
      plot_pixel(x, y, colour);
    }
  }
}

// FRUIT GENERATION STARTED

// DRAWING FRUITS AND BOMBS
// for drawing each iteration of the object
// takes in cornor x and y position
// then object size (48x48 pixels always but can make more dynamic if we want
// to) iterates through whole object array takes pointer to specific image array
// (which we define when object is gen)

void draw_object(int x0, int y0, int w, int h, const unsigned short* obj) {

    // loop through width and height of object and if not transparent from included .h file then plot pixel. 

  for (int y = 0; y < h; y++) {
    // given start coordinate at y0
    int y1 = y0 + y;
    // if we are in bounds keep drawing
    if (y1 < 0 || y1 >= 240) continue;

    for (int x = 0; x < w; x++) {
      int x1 = x0 + x;
      // stay in bounds
      if (x1 < 0 || x1 >= 320) continue;

      // maps to location of pixel in array
      unsigned short colour = obj[y * w + x];
      if (colour != TRANSPARENT) {
        plot_pixel(x1, y1, colour);
      }
    }
  }
}

void drawAllObjects() {
    // increment through max objects repretivly (in main) 
  for (int i = 0; i < MAX_OBJECTS; i++) {
    if(objects[i].onScreen) {
      draw_object((int)objects[i].x, (int)objects[i].y, 
                objects[i].w, objects[i].h, objects[i].image);
    }
  }


}

void add_object() {
  
  for (int i = 0; i < MAX_OBJECTS; i++) {
    if (!objects[i].onScreen) {
      if (rand() % 7 == 0) {
        randomGenerator(&objects[i]); // 10% chance of spawning per frmae
      }
      break; // one per frame
    }
  }


}

// RANDOM GEN
void randomGenerator(Object* obj) {
  // Randomize start location
  obj->x =
      rand() % 300 +
      10;  // random value 0-299 (within vga, not at edge) then +10 to avoid 0
           // edge giving object in 10-209 range (10 offset from each side)
  obj->y = 239;  // objects start at bottom

  // Randomize upward velocity
  obj->vy = -(rand() % 5 +
              5);  // gives velocity 5-9 (open to change upon testing)
                   // negative since moving up screen (decreasing y direction)

  // Randomize horizontal velocity
  obj->vx =
      rand() % 5 -
      2;  // gives value -2, -1, 0, 1, 2 for left and right movement on screen

  // Weighted random generator for fruit or bomb
  // start with 60/30/10 (fruit/pom/bomb) split and test
  int chance = rand() % 100;  // number 0-99

  obj->w = 48;
      obj->h = 48;

      obj->onScreen =
          1;  // tells us an object is onscreen for later detection and such

  obj->g = 0.2f;

  // chance of fruit
  if (chance < 70) {
    obj->type = rand() % 6;  // random index 0-5 for the first 6 fruit types

  } else if (chance < 85) {
    obj->type =
        POMEGRANATE;  // If not less than 60 but less than 70 we hit pom range

  } else {
    obj->type = BOMB;  // otherwise we have a bomb
  }

  // mapping type of fruit to array
  switch (obj->type) {
    case APPLE:
      obj->image = Red_Apple;
      break;
    case LEMON:
      obj->image = Lemon;
      break;
    case BANANA:
      obj->image = Banana;
      break;
    case PINEAPPLE:
      obj->image = Pineapple;
      break;
    case WATERMELON:
      obj->image = Watermelon;
      break;
    case STRAWBERRY:
      obj->image = Strawberry;
      break;
    case POMEGRANATE:
      obj->image = Pomegranate;
      break;
    case BOMB:
      obj->image = Bomb;
      break;
    default:
      obj->image = Bomb;

      
  }
}

// GRAVITY MOVEMENT
void physics() {

  for (int i = 0; i < MAX_OBJECTS; i++) {
  if (!objects[i].onScreen) {
    continue;
  }

  // update position
  objects[i].x += objects[i].vx;
  objects[i].y += objects[i].vy;

  // apply force of gravity Fg = mg to vertical velocity
  objects[i].vy += objects[i].g;

  // when object is off screen
  if (objects[i].y > 240 + objects[i].h) {
    objects[i].onScreen = 0;
  }

  }
}

// FRUIT PART ENDED



int main() {

  srand(1);
  volatile int *LEDR_ptr = (int *)LEDR_BASE;
    volatile int *switch_ptr = (int *)SWITCH_BASE;
    volatile int *ps2_ptr = (int *)PS2_BASE;
  volatile int* pixel_ctrl_ptr = (int*)VGA;
  
   unsigned char byte;//Stores one byte read from the PS/2 port

  // initialize location and direction of rectangles(not shown)

  // front buffer: current frame
  // back buffer: next frame
  // when next display is ready, swap ptrs

  /* set front pixel buffer to Buffer 1 */
  *(pixel_ctrl_ptr + 1) =
      (int)&Buffer1;  // first store the address in the  back buffer
  /* now, swap the front/back buffers, to set the front buffer location */
  wait_for_vsync();
  /* initialize a pointer to the pixel buffer, used by drawing functions */
  pixel_buffer_start = *pixel_ctrl_ptr;
  clear_screen();  // pixel_buffer_start points to the pixel buffer

  /* set back pixel buffer to Buffer 2 */
  *(pixel_ctrl_ptr + 1) = (int)&Buffer2;
  pixel_buffer_start = *(pixel_ctrl_ptr + 1);  // we draw on the back buffer
  clear_screen();  // pixel_buffer_start points to the pixel buffer

  draw_background(play);

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

  // INITIALIZE OBJECT ARRAY
  for (int i = 0; i < MAX_OBJECTS; i++) {
    objects[i].onScreen = 0;
  }



  while (1) {

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
                        //Bit 3 of a valid PS/2 mouse packet's first byte is supposed to always be 1. (LINK)
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



    pixel_buffer_start = *(pixel_ctrl_ptr + 1);  // new back buffer

    draw_background(play);

    add_object();
    physics();
    drawAllObjects();

    push_tail(x_pos, y_pos);
    draw_tail();
    draw_cursor(x_pos, y_pos);

    wait_for_vsync();  // swap front and back buffers on VGA vertical sync

    pixel_buffer_start = *(pixel_ctrl_ptr + 1);
  }
}
}

// TESTING ENDED