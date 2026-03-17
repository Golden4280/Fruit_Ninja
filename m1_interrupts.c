#include <stdio.h>

// m1 for ece243 project
// interupt version

// fsm through start-play-miss-fruit-bomb-gameover
// led shows state
// switches for fruit/bomb/miss
// key for start

// addresses interupts
#define LED 0xff200000
#define SW 0xff200040
#define KEY 0xff200050

// states for fsm
#define START 0x0     // ledr1
#define PLAY 0x1      // ledr2
#define FRUIT 0x2     // ledr3
#define BOMB 0x4      // ledr4
#define GAMEOVER 0x8  // ledr5

// global variables
volatile int high_score = 0;
volatile int state = START;  // start as default idle state
volatile int score = 0;      // output score starts at 0

// helper functions and isr
static void handler(void) __attribute__((interrupt("machine")));
void KEY_ISR(void);
void set_KEY(void);

int main() {
  volatile int* led_ptr = (int*)LED;
  volatile int* sw_ptr = (int*)SW;

  // setup interrupts
  set_KEY();

  int mstatus_value, mtvec_value, mie_value;

  mtvec_value = (int)&handler;  // set trap handler address
  __asm__ volatile("csrw mtvec, %0" ::"r"(mtvec_value));

  mstatus_value = 0b1000;  // interupt bit mask, disable all interupts
  __asm__ volatile("csrc mstatus, %0" ::"r"(mstatus_value));

  // disable all currently enabled interrupts
  __asm__ volatile("csrr %0, mie" : "=r"(mie_value));
  __asm__ volatile("csrc mie, %0" ::"r"(mie_value));
  mie_value = (1 << 18);  // shifts 1 18 bits for irq 18 (keys)

  __asm__ volatile("csrs mie, %0" ::"r"(mie_value));
  __asm__ volatile("csrs mstatus, %0" ::"r"(mstatus_value));

  // play loop
  while (1) {
    int slice = *sw_ptr;

    if (state == PLAY) {
      // bitwise and to determine slice
      if (slice & 0x1) {
        // miss
        state = PLAY;
      } else if (slice & 0x2) {
        // fruit
        state = FRUIT;
      } else if (slice & 0x4) {
        state = BOMB;
      }

      if (state == FRUIT) {
        // incr score and reset to play
        score++;
        state = PLAY;
      } else if (state == BOMB) {
        // store score in global high score if new max
        if (score > high_score) {
          high_score = score;
        }
        // set to gameover and reset score
        state = GAMEOVER;
        score = 0;
        // display score on led
      }

      *led_ptr = state;
    }
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
  int pressed;

  volatile int* key_ptr = KEY;
  pressed = *(key_ptr + 3);  // read edge capture to see if key is pressed
  *(key_ptr + 3) = pressed;  // clear edge capture for next thing

  if (pressed) {
    if (state == START) {
      state = PLAY;
    } else if (state == GAMEOVER) {
      state = START;
      score = 0;  // reset score
    }
  }
}

void set_KEY(void) {
  volatile int* KEY_ptr = (int*)KEY;
  *(KEY_ptr + 3) = 0xF;  // clear EdgeCapture register
  *(KEY_ptr + 2) = 0xF;  // enable interrupts for all KEYs
}