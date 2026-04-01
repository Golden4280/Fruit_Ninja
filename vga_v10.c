#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>


// objects
#include "Banana.h"
#include "Lemon.h"
#include "Pineapple.h"
#include "Red_Apple.h"
#include "Strawberry.h"
#include "Watermelon.h"
#include "Pomegranate.h"
#include "Bomb.h"

//sounds
#include "start_sound.h"
// #include "slice.h"
#include "pomegranates.h"  
#include "bombs.h"
#include "gameovers.h"
#include "watermelons.h"
#include "bananas.h"
#include "Red_Apples.h"
#include "pineapples.h"
#include "strawberrys.h"
#include "lemons.h"

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

  const int *sound;
  int sound_len;

} Object;

#define MAX_OBJECTS 5
Object objects[MAX_OBJECTS];
#define obj_h 48
#define obj_w 48
int spawn_timer = 0;
int burst_count = 0;
int burst_active = 0;

//AUDIO STRUCT
struct audio_t {
	volatile unsigned int control;
	volatile unsigned char rarc;
	volatile unsigned char ralc;
	volatile unsigned char warc;
	volatile unsigned char walc;
    volatile unsigned int ldata;
	volatile unsigned int rdata;
};
volatile struct audio_t * audiop = ((struct audio_t *)0xff203040);

typedef struct {
    const int *data;
    int len;
    int index;
    int playing;
} FxPlayer;

FxPlayer fruit_fx = {0, 0, 0, 0};

//AUDIO INITIALIZE
static int Game_start_packed_len = sizeof(Game_start_packed) / sizeof(Game_start_packed[0]);
// static int Butterfly_Knife03_packed_len = sizeof(Butterfly_Knife03_packed) / sizeof(Butterfly_Knife03_packed[0]);
static int angel_combo_5_packed_len = sizeof(angel_combo_5_packed) / sizeof(angel_combo_5_packed[0]);
static int Bomb_explode_len = sizeof(Bomb_explode) / sizeof(Bomb_explode[0]);
static int Impact_Coconut_len = sizeof(Impact_Coconut) / sizeof(Impact_Coconut[0]);
static int Game_over_len = sizeof(Game_over) / sizeof(Game_over[0]);
static int Impact_Apple_len = sizeof(Impact_Apple_packed) / sizeof(Impact_Apple_packed[0]);
static int Impact_Pineapple_len = sizeof(Impact_Pineapple_packed) / sizeof(Impact_Pineapple_packed[0]);
static int Impact_Banana_len = sizeof(Impact_Banana_packed) / sizeof(Impact_Banana_packed[0]);
static int Impact_Strawberry_len = sizeof(splatter_medium_1_packed) / sizeof(splatter_medium_1_packed[0]);
static int Impact_Lemon_len = sizeof(ui_button_push_packed) / sizeof(ui_button_push_packed[0]);

// FUNCTIONS

void plot_pixel(int x, int y, short int colour);
void wait_for_vsync();
void clear_screen();
void randomGenerator(Object* obj);
void physics();
void draw_background(const unsigned short* bg);
void draw_object(int x0, int y0, int w, int h, const unsigned short* obj);
void drawAllObjects();
void audio_playback_mono(const int *data, int n, int step, int replicate);
void swap(int* a, int* b);
void draw_line(int x0, int y0, int x1, int y1, short int line_color);
void bomb_explosion(int cx, int cy, volatile int *pixel_ctrl_ptr, short int colour);

void start_fx(const int *data, int len);
void service_fx(void);


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
    STATE_GAMEOVER
};

// boolean for state switching
// no longer using fruit and bomb state
// instead if bomg_hit == 1 then current_state = gameover else stays in play
bool bomb_hit = 0;

bool pom_hit = 0;

int miss_count = 0;


int bomb_cx = 0;
int bomb_cy = 0;

int pom_cx = 0;
int pom_cy = 0;


// global scoring variable
// and high score variable for when in gaemover
int score = 0;
int high_score = 0;
int old_score = -1; // for updating


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
        service_fx();
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

        if ((x & 31) == 0) {   // every 32 pixels
            service_fx();
        }
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
        if ((x & 15) == 0) {   // every 16 pixels
            service_fx();
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

    // waiting between bursts
    if (!burst_active) {
        spawn_timer++;

        if (spawn_timer > 80) {   // ~2 seconds (assuming ~60 FPS)
            burst_active = 1;
            burst_count = 0;
            spawn_timer = 0;
        }
        return;
    }

    // during burst: spawn fruits
    if (burst_active) {

        for (int i = 0; i < MAX_OBJECTS; i++) {
            if (!objects[i].onScreen) {
                randomGenerator(&objects[i]);
                burst_count++;
                break;
            }
        }

        // stop burst after N fruits
        if (burst_count >= 10) {
            burst_active = 0;
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
  obj->vy = -(rand() % 7 +
              6);  // gives velocity 5-9 (open to change upon testing)
                   // negative since moving up screen (decreasing y direction)

  // Randomize horizontal velocity
  obj->vx =
      rand() % 5 -
      3;  // gives value -2, -1, 0, 1, 2 for left and right movement on screen

  // Weighted random generator for fruit or bomb
  // start with 60/30/10 (fruit/pom/bomb) split and test
  int chance = rand() % 100;  // number 0-99

  obj->w = 48;
      obj->h = 48;

      obj->onScreen =
          1;  // tells us an object is onscreen for later detection and such

  obj->g = 0.35f;//0.2-> 0.25

  // chance of fruit
  if (chance < 78) {
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
      obj->sound = Impact_Apple_packed;
      obj->sound_len = Impact_Apple_len;
      break;
    case LEMON:
      obj->image = Lemon;
      obj->sound = ui_button_push_packed;
      obj->sound_len = Impact_Lemon_len;
      break;
    case BANANA:
      obj->image = Banana;
      obj->sound = Impact_Banana_packed;
      obj->sound_len = Impact_Banana_len;
      break;
    case PINEAPPLE:
      obj->image = Pineapple;
      obj->sound = Impact_Pineapple_packed;
      obj->sound_len = Impact_Pineapple_len;
      break;
    case WATERMELON:
      obj->image = Watermelon;
      obj->sound = Impact_Coconut;
      obj->sound_len = Impact_Coconut_len;
      break;
    case STRAWBERRY:
      obj->image = Strawberry;
      obj->sound = splatter_medium_1_packed;
      obj->sound_len = Impact_Strawberry_len;
      break;
    case POMEGRANATE:
      obj->image = Pomegranate;
      obj->sound = angel_combo_5_packed;
      obj->sound_len = angel_combo_5_packed_len;
      break;
    case BOMB:
      obj->image = Bomb;
      obj->sound = Bomb_explode;
      obj->sound_len = Bomb_explode_len;
      break;
    default:
      obj->image = Bomb;
      obj->sound = Bomb_explode;
      obj->sound_len = Bomb_explode_len;
      
  }
}

// GRAVITY MOVEMENT
void physics() {

  for (int i = 0; i < MAX_OBJECTS; i++) {
  if (!objects[i].onScreen) {
    continue;
  }

  // update position
  objects[i].x += objects[i].vx*1.5f;
  objects[i].y += objects[i].vy*1.5f;

  // apply force of gravity Fg = mg to vertical velocity
  objects[i].vy += objects[i].g*1.5f;

  // when object is off screen
  if (objects[i].y > 240 + objects[i].h) {
    objects[i].onScreen = 0;
    if (objects[i].type != BOMB) {
        miss_count++;
    }
    
    // inside physics
   
    
  }

  }
}

// COLLLISION FUNCTIONS
// collisions module
// takes all x coordinate input from mouse position
// loops through coordinations of each object
// if equal then check if bomb set state to gameover
//
// else check pomogrante +2
// if neither than incrememtn +1

// scoring module

// return global boolean

// bomb exploding function
// start point takes in bomb start coord and adds offset to start from center
// draws white lines from bomb
// calls delay

void bomb_explosion(int cx, int cy, volatile int *pixel_ctrl_ptr, short int colour)
{
    // 6 directions (diagonal + straight)
    int dir_x[6] = {  1,  1,  0, -1, -1,  0 };
    int dir_y[6] = {  0,  1,  1,  0, -1, -1 };

    // short int colour = 0xFFFF;      // red
    int length = 200;               // line length

    for (int frame = 0; frame < 30; frame++) {

        pixel_buffer_start = *(pixel_ctrl_ptr + 1);  // draw to back buffer


        // draw 6 explosion rays
        for (int r = 0; r < 6; r++) {

            int x1 = cx;
            int y1 = cy;

            int x2 = cx + dir_x[r] * length;
            int y2 = cy + dir_y[r] * length;

            draw_line(x1, y1, x2, y2, colour);
        }
        service_fx();
        wait_for_vsync();
        service_fx();
        pixel_buffer_start = *(pixel_ctrl_ptr + 1);  // next back buffer
    }
}


// DRAW LINE FUNCTION
void draw_line(int x0, int y0, int x1, int y1, short int line_color)
{
    // used to determine how to navigate line
    bool is_steep = abs(y1 - y0) > abs(x1 - x0);

    // if steep, swap x and y of both points
    if (is_steep) {
        swap(&x0, &y0);
        swap(&x1, &y1);
    }

    // ensure left‑to‑right drawing
    if (x0 > x1) {
        swap(&x0, &x1);
        swap(&y0, &y1);
    }

    int deltax = x1 - x0;
    int deltay = abs(y1 - y0);
    int error = -(deltax / 2);
    int y = y0;

    int y_step;
    if (y0 > y1) {
        y_step = -1;
    } else {
        y_step = 1;
    }

    for (int x = x0; x <= x1; x++) {

        if (is_steep) {
            // BOUNDARY CHECK ADDED HERE
            if (y >= 0 && y < 320 && x >= 0 && x < 240) {
                plot_pixel(y, x, line_color);
            }
        } else {
            // BOUNDARY CHECK ADDED HERE
            if (x >= 0 && x < 320 && y >= 0 && y < 240) {
                plot_pixel(x, y, line_color);
            }
        }

        error = error + deltay;

        if (error > 0) {
            y = y + y_step;
            error = error - deltax;
        }
    }
}


// SWAP
void swap(int* a, int* b) {
	int temp = *a;
	*a = *b;
	*b = temp;

}


void collisions() {

    // // use x_pos and y_pos 
    // // exit while once bomb is hit
    // while (bomb_hit == 0) {
    // actually continuously call functin in play and with each call, check all opbject s and positions
        for (int i = 0; i < MAX_OBJECTS; i++) {
            // increment through the entire object (48x48 square)
            if (objects[i].onScreen) {
            
                for (int y = 0; y < obj_h; y++) {
                    if((y&7)==0){
                        service_fx();
                    }
                    int y1 = objects[i].y + y;

                    for (int x = 0; x < obj_w; x++) {
                        int x1 = objects[i].x + x;
                    
                        unsigned short colour = objects[i].image[y * (objects[i].w) + x];
                        if (colour != TRANSPARENT) {
                            if ((x1 == x_pos) && (y1 == y_pos)) {
                            
                                // check if pom
                                if (objects[i].type == POMEGRANATE) {
                                    score += 2;
                                    pom_cx = objects[i].x + objects[i].w/2;
                                    pom_cy = objects[i].y + objects[i].h/2;
                                    pom_hit = 1;
                                    
                                    
                                    // also play sound
                                } else if (objects[i].type == BOMB) {
                                    // if a bomb is hit, stop checking and return so that we can go to collisions
                                    bomb_cx = objects[i].x + objects[i].w/2;
                                    bomb_cy = objects[i].y + objects[i].h/2;
                                    bomb_hit = 1;
                                    




                                    return;
                                } else {
                                // if any other furit increment by 1
                                    score += 1;
                                    start_fx(objects[i].sound, objects[i].sound_len);
                                    
                                }
                            
                                objects[i].onScreen = 0;
                                return;
                            }
                        }
                    }
                }
            }
        }
    }


// new scoring logic using vga buffer
// base address of character buffer
#define CHAR_BUFFER_BASE 0x09000000

// funcction writes single character
// take in x and y start position
// 80 x 60 buffer size so addr = x + y* 128 for byte
void write_char(int x, int y, char c) {
    volatile char *char_buffer = (char *)CHAR_BUFFER_BASE;
    char_buffer[y * 128 + x] = c;
}

// used to write a full string
void write_string(int x, int y, const char* str) {
    while (*str) {
        // incr x and y to move across string
        write_char(x++, y, *str++);
    }
}

// eras prev score before drawing new score with spaces
void clear_text_area(int x, int y, int len) {
    for (int i = 0; i < len; i++)
        write_char(x + i, y, ' ');
}

// SINCE char is 8x16 charx = pixelx/8 and chary = pixely/16
#define CHAR_X_1 (34)
#define CHAR_Y_1 (36)
#define CHAR_X_2 (34)
#define CHAR_Y_2 (38)


void draw_score_top(int score) {
    char buffer[16];
    // format as 3 digits, padded with zeros
    sprintf(buffer, "%03d", score);

    // Clear old score area (3 digits)
    clear_text_area(2, 1, 10);

    // Draw new score at top-left
    write_string(2, 1, buffer);
}

void draw_miss_count(int misses) {

    const char *x_string;

    switch (misses) {
        case 1: x_string = "X    "; break;
        case 2: x_string = "XX   "; break;
        case 3: x_string = "XXX  "; break;
        case 4: x_string = "XXXX "; break;
        case 5: x_string = "XXXXX"; break;
        default: x_string = "     "; break;
    }

    clear_text_area(60, 1, 5);
    write_string(60, 1, x_string);
}

void draw_gameover_scores(int score, int high_score) {
    char line1[32];
    char line2[32];

    sprintf(line1, "SCORE:      %03d", score);
    sprintf(line2, "HIGH SCORE: %03d", high_score);

    // Clear a region in the middle of the screen
    
    clear_text_area(0, CHAR_Y_1, 80);
    clear_text_area(0, CHAR_Y_2, 80);


    write_string(CHAR_X_1, CHAR_Y_1, line1);
    write_string(CHAR_X_2, CHAR_Y_2, line2);
}


int last_left_click = 0;
int last_right_click = 0;
int start_sound_played = 0;
int knife_sound_ready = 1;

int knife_index = 0;
int knife_playing = 0;

// PS2 processing function 
void process_mouse_input(volatile int *ps2_ptr) {
    unsigned char byte;//Stores one byte read from the PS/2 port
    static int count = 0; //how many bytes of the current packet have been collected so far. (static so looping doesnt reset it)
    static unsigned char packet[3];//3 bytes of mouse packet

    
    last_left_click = 0;
    last_right_click = 0;


     while (read_ps2_byte(ps2_ptr, &byte)) {
                packet[count++] = byte; 

                if (count == 1) {
                    //Bit 3 of a valid PS/2 mouse packet's first byte is supposed to always be 1. (LINK)
                    if ((packet[0] & 0x08) == 0) {
                        count = 0;//restart the count
                    }
                }
                else if (count == 3) {//full packet

                    
                    int left  =  packet[0] & 0x01;
                    int right = (packet[0] >> 1) & 0x01;

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

                    

                    update_cursor_position(dx, dy);//move cursor

                   
                    last_left_click = left;
                    last_right_click = right;

                     count = 0;//reset packet count
                }

}
}
//AUDIO FUNCTION
void audio_playback_mono(const int *data, int n, int step, int replicate) {
            int i;

            audiop->control = 0x8; // clear the output FIFOs
            audiop->control = 0x0; // resume input conversion
            for (i = 0; i < n; i+=step) {
              // output data if there is space in the output FIFOs
              for (int r=0; r < replicate; r++) {
				while(audiop->warc == 0);
                audiop->ldata = data[i];
                audiop->rdata = data[i];
			  }
			}
}	

void start_fx(const int *data, int len) {
    if (data == NULL || len <= 0) return;

    fruit_fx.data = data;
    fruit_fx.len = len;
    fruit_fx.index = 0;
    fruit_fx.playing = 1;

    service_fx();
}

void service_fx(void) {
   // int samples_to_write = 64;   // tune this: 32, 64, 128

    while (fruit_fx.playing && audiop->warc > 0) {
        if (fruit_fx.index >= fruit_fx.len) {
            fruit_fx.playing = 0;
            fruit_fx.index = 0;
            break;
        }

        // audiop->ldata = fruit_fx.data[fruit_fx.index];
        // audiop->rdata = fruit_fx.data[fruit_fx.index];
        // fruit_fx.index++; 
        //LACIE change the above three lines to 
        
        audiop->ldata = fruit_fx.data[fruit_fx.index];
        audiop->rdata = fruit_fx.data[fruit_fx.index + 1];
        fruit_fx.index += 8
        ;
        
    }
}

// void check_tail_distance_sound() {
//     if (tail_count < 2) return;

//     int oldest_idx = (tail_head - tail_count + TAIL_LEN) % TAIL_LEN;

//     int dx = abs_val(x_pos - tail_x[oldest_idx]);
//     int dy = abs_val(y_pos - tail_y[oldest_idx]);
//     int dist = dx + dy;

//     if (dist > 20 && knife_sound_ready) {
//         knife_playing = 1;
//         knife_index = 0;
//         knife_sound_ready = 0;
//     }

//     if (dist < 8) {
//         knife_sound_ready = 1;
//     }
// }

// void update_audio() {
//     int s;

//     if (!knife_playing) return;

//     for (s = 0; s < 1; s++) {
//         if (knife_index >= Butterfly_Knife03_packed_len) {
//             knife_playing = 0;
//             break;
//         }

//         if (audiop->warc == 0) {
//             break;
//         }

//         audiop->ldata = Butterfly_Knife03_packed[knife_index];
//         audiop->rdata = Butterfly_Knife03_packed[knife_index];
//         knife_index++;
//     }
// }

// FRUIT PART ENDED

int main(void) {
    srand(1);
    volatile int *LEDR_ptr = (int *)LEDR_BASE;
    volatile int *ps2_ptr = (int *)PS2_BASE;
    volatile int *pixel_ctrl_ptr = (int*)VGA;

    // unsigned char byte;//Stores one byte read from the PS/2 port

    //double buffer
    *(pixel_ctrl_ptr + 1) = (int)&Buffer1;//buffer1 is the back buffer
    service_fx();
    wait_for_vsync();//Swap buffers at the next vertical sync
    service_fx();
    pixel_buffer_start = *pixel_ctrl_ptr; // Get the address of the current front buffer
    clear_screen();

    *(pixel_ctrl_ptr + 1) = (int)&Buffer2; //buffer2 is the back buffer
    pixel_buffer_start = *(pixel_ctrl_ptr + 1);//drawing happens in buffer 2
    clear_screen();

    draw_background(start);
    

    init_mouse(ps2_ptr);//initialize mouse
    //NOTHING CAN CHANGE TO BOMB OR HIT FRUIT RN 

    // INITIALIZE OBJECT ARRAY
    for (int i = 0; i < MAX_OBJECTS; i++) {
        objects[i].onScreen = 0;
    }

    while (1) {
        //FSM

        // process mouse input FIRST
        process_mouse_input(ps2_ptr);
        service_fx();

        // update fsm from clicks
        switch (current_state) {
            case STATE_START:
                // *LEDR_ptr = 0x01;
                // clear all text
                clear_text_area(2, 1, 10);
                clear_text_area(CHAR_X_1, CHAR_Y_1, 80);
                clear_text_area(CHAR_X_2, CHAR_Y_2, 80);
                clear_text_area(60, 1, 80);
                for (int i = 0; i < MAX_OBJECTS; i++) {
                    objects[i].onScreen = 0;
                }
                if (last_left_click) {
                    current_state = STATE_PLAY;
                    audio_playback_mono(Game_start_packed, Game_start_packed_len, 1, 1);
                }
                break;
            case STATE_PLAY:
                // *LEDR_ptr = 0x02;
                // collisions();
                *LEDR_ptr = score;
               // update_audio();

              

                
                break;
            case STATE_GAMEOVER:
                // *LEDR_ptr = 0x10;
                clear_text_area(2, 1, 10);
                clear_text_area(60, 1, 80);
                // clear_text_area(CHAR_X_1, CHAR_Y_1, 80);
                // clear_text_area(CHAR_X_2, CHAR_Y_2, 80);
                
                *LEDR_ptr = high_score;
                if (last_right_click) {
                    score = 0;
                    old_score = -1;
                    
                    x_pos = 160;
                    y_pos = 120;
                    tail_head = 0;
                    tail_count = 0;
                    tail_last_x = 160;
                    tail_last_y = 120;
                    bomb_hit = 0;
                    current_state = STATE_START;
                    
                }
                break;

        }

       

        pixel_buffer_start = *(pixel_ctrl_ptr + 1);  // new back buffer

        // background updates based on states

        if (current_state == STATE_START) {
            draw_background(start);

        } else if (current_state == STATE_PLAY) {
            draw_background(play);
            service_fx();

            add_object();
            physics();
            service_fx();

            collisions();
            service_fx();
            

            if (pom_hit) {

                // freeze gameplay & run explosion animation
                bomb_explosion(pom_cx, pom_cy, pixel_ctrl_ptr, 0xf515);
                audio_playback_mono(angel_combo_5_packed, angel_combo_5_packed_len, 1, 1);

                service_fx();
                wait_for_vsync();
                service_fx();
                pixel_buffer_start = *(pixel_ctrl_ptr + 1);
                pom_hit = 0;
                miss_count = 0;
            }

            
            if (bomb_hit) {
                if (score > high_score) {
                    high_score = score;
                }

                bomb_explosion(bomb_cx, bomb_cy, pixel_ctrl_ptr, 0xFFFF);
                audio_playback_mono(Bomb_explode, Bomb_explode_len, 1, 1);
                audio_playback_mono(Game_over, Game_over_len, 1, 1);

                pom_hit = 0;

                current_state = STATE_GAMEOVER;

                pixel_buffer_start = *(pixel_ctrl_ptr + 1);
                continue;
            }

            if (miss_count >= 5) {
                miss_count = 0;
                if (score > high_score) {
                    high_score = score;
                }
                audio_playback_mono(Game_over, Game_over_len, 1, 1);
                current_state = STATE_GAMEOVER;
            }



            drawAllObjects();
            // update score display
            draw_score_top(score);
            draw_miss_count(miss_count);
            service_fx();
            

            push_tail(x_pos, y_pos);
            draw_tail();
            service_fx();
            draw_cursor(x_pos, y_pos);
            service_fx();
            //FOR SLICING
            //check_tail_distance_sound();


        } else if (current_state == STATE_GAMEOVER) {
            draw_background(gameover);
            draw_gameover_scores(score, high_score);
            

        }
    
        // WAIT FOR VSYNC
        service_fx();
        wait_for_vsync();  // swap front and back buffers on VGA vertical sync
        service_fx();

        pixel_buffer_start = *(pixel_ctrl_ptr + 1);
    }

    return 0;
}