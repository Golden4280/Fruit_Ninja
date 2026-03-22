#include <stdio.h>


// CODE STARTS HERE

#define VGA 0xff203020
// #define pixel_buffer_start 0x08000000
#define ROWS 240
#define COLS 320
#define pCOLS 512
#define TRANSPARENT 0x0000
volatile int pixel_buffer_start;  // global variable
short int Buffer1[240][512];      // 240 rows, 512 (320 + padding) columns
short int Buffer2[240][512];

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

// DRAWING FRUITS AND BOMBS
// for drawing each iteration of the object
// takes in cornor x and y position
// then object size (48x48 pixels always but can make more dynamic if we want
// to) iterates through whole object array takes pointer to specific image array
// (which we define when object is gen)

void draw_object(int x0, int y0, int w, int h, const unsigned short* obj) {
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

void draw_background(const unsigned short* bg) {
  for (int y = 0; y < ROWS; y++) {
    for (int x = 0; x < COLS; x++) {
      unsigned short colour = bg[y * COLS + x];
      plot_pixel(x, y, colour);
    }
  }
}

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

// TESTING FUNCTIONS

// GRAVITY MOVEMENT
void physics(Object* obj) {
  if (!obj->onScreen) {
    return;
  }

  // update position
  obj->x += obj->vx;
  obj->y += obj->vy;

  // apply force of gravity Fg = mg to vertical velocity
  obj->vy += obj->g;

  // when object is off screen
  if (obj->y > 240 + obj->h) {
    obj->onScreen = 0;
  }
}

int main() {
  volatile int* pixel_ctrl_ptr = (int*)VGA;
  // declare other variables(not shown)
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

  // init sprite

  Object bomb = {.type = BOMB,
                 .vx = 1,
                 .vy = -5.0f,
                 .x = 50,
                 .y = 239,
                 .w = BOMB_WIDTH,
                 .h = BOMB_HEIGHT,
                 .g = 0.2f,
                 .onScreen = 1,
                 .image = Bomb};

  while (1) {
    pixel_buffer_start = *(pixel_ctrl_ptr + 1);  // new back buffer
    draw_background(play);
    draw_object(bomb.x, bomb.y, bomb.w, bomb.h, bomb.image);

    physics(&bomb);

    wait_for_vsync();  // swap front and back buffers on VGA vertical sync
  }
}

// TESTING ENDED

typedef struct {
  float x;
  float y;
  float dx;
  float dy;
  int w;
  int h;
  float g;
  int onScreen;
  const unsigned short* image;
} Sprite;

void update_sprite(Sprite* s) {
  s->x += s->dx;
  s->y += s->dy;
}

void bounce_edges(Sprite* s) {
  if (s->x < 0 || s->x + s->w >= 320) s->dx = -s->dx;

  if (s->y < 0 || s->y + s->h >= 240) s->dy = -s->dy;
}
