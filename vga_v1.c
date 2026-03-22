// trying to get display working
// vga takes input from the following modules
// - fruit and bomb (produced by collision, fruit and bomb slicing animation)
// - scoring (display current and high score)
// - object generation (fruits and bombs)
// - miss (draw slice)

#include <stdio.h>

#define VGA 0xff203020
#define pixel_buffer_start 0x08000000
#define ROWS 240
#define COLS 320
#define pCOLS 512
#define TRANSPARENT 0x0000

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

void draw_object(int x0, int y0, int w, int h, const short* obj) {
  for (int y = 0; y < h; y++) {
    // given start coordinate at y0
    int y1 = y0 + y;
    // if we are in bounds keep drawing
    if (y1 < 0 || y1 >= ROWS) continue;

    for (int x = 0; x < w; x++) {
      int x1 = x0 + x;
      // stay in bounds
      if (x1 < 0 || x1 >= COLS) continue;

      // maps to location of pixel in array
      unsigned short colour = obj[y * w + x];
      if (colour != TRANSPARENT) {
        plot_pixel(x1, y1, colour);
      }
    }
  }
}

// called during different states
// draws different backgrounds for start, play and gameover

void draw_background(const unsigned short* bg) {
  for (int y = 0; y < ROWS; y++) {
    for (int x = 0; x < COLS; x++) {
      unsigned short colour = bg[y * COLS + x];
      plot_pixel(x, y, colour);
    }
  }
}