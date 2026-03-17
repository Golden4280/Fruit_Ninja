#include <stdio.h>

#define LEDR_BASE 0xFF200000
#define SWITCH_BASE 0xFF200040
#define KEY_BASE 0xFF200050

enum States {
    STATE_START,
    STATE_PLAY,
    STATE_FRUIT,
    STATE_BOMB,
    STATE_GAMEOVER
};

volatile enum States current_state = STATE_START;

void delay(void){
    volatile int i;
    for (i = 0; i < 500000; i++);
}

int main(void){
    volatile int *LEDR_ptr = (int *) LEDR_BASE;
    volatile int *switch_ptr = (int *) SWITCH_BASE;
    volatile int *KEY_ptr = (int *) KEY_BASE;

    // enable KEY0 interrupt mask
    KEY_ptr[2] = 0x1;

    // clear any old edge bits
    KEY_ptr[3] = 0xF;

    int click_start, hit_fruit, hit_bomb;
    int fruit_count = 0;

    while (1){

        int sw = *switch_ptr & 0x1F;

        hit_fruit = sw & 0x1; // SW0
        hit_bomb  = (sw >> 1) & 0x1; // SW1

        int edge = KEY_ptr[3];
        click_start = edge & 0x1; // KEY0 press

        KEY_ptr[3] = edge;

        if (current_state == STATE_START) {
            *LEDR_ptr = ((fruit_count & 0x1F) << 5) | 0x01;

            if (click_start) {
                current_state = STATE_PLAY;
            }
        }
        else if (current_state == STATE_PLAY) {
            *LEDR_ptr = ((fruit_count & 0x1F) << 5) | 0x02;

            if (hit_fruit) {
                fruit_count++;
                current_state = STATE_FRUIT;
            }
            else if (hit_bomb) {
                current_state = STATE_BOMB;
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

            if (click_start) {
				fruit_count = 0;  
                current_state = STATE_START;
            }
        }
    }
}