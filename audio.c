#include <stdio.h>

#include "Banana.h"
#include "Bomb.h"
#include "gameover.h"
#include "Lemon.h"
#include "Pineapple.h"
#include "play.h"
#include "Pomegranate.h"
#include "Red_Apple.h"
#include "start.h"
#include "Strawberry.h"
#include "Watermelon.h"

#define SWITCH_BASE 0xFF200040
#define AUDIO_BASE  0xFF203040

// Sound data arrays taken from the header files
const short *miss_sound = gameover;
const int miss_sound_len = sizeof(gameover) / sizeof(gameover[0]);

const short *apple_sound = Red_Apple;
const int apple_sound_len = sizeof(Red_Apple) / sizeof(Red_Apple[0]);

const short *lemon_sound = Lemon;
const int lemon_sound_len = sizeof(Lemon) / sizeof(Lemon[0]);

const short *banana_sound = Banana;
const int banana_sound_len = sizeof(Banana) / sizeof(Banana[0]);

const short *pineapple_sound = Pineapple;
const int pineapple_sound_len = sizeof(Pineapple) / sizeof(Pineapple[0]);

const short *watermelon_sound = Watermelon;
const int watermelon_sound_len = sizeof(Watermelon) / sizeof(Watermelon[0]);

const short *strawberry_sound = Strawberry;
const int strawberry_sound_len = sizeof(Strawberry) / sizeof(Strawberry[0]);

const short *pomegranate_sound = Pomegranate;
const int pomegranate_sound_len = sizeof(Pomegranate) / sizeof(Pomegranate[0]);

const short *bomb_sound = Bomb;
const int bomb_sound_len = sizeof(Bomb) / sizeof(Bomb[0]);

// Optional extra sounds from your headers
const short *start_sound = start;
const int start_sound_len = sizeof(start) / sizeof(start[0]);

const short *play_screen_sound = play;
const int play_screen_sound_len = sizeof(play) / sizeof(play[0]);

// Reset and enable audio device
void audio_init(volatile int *audio_ptr) {
    *audio_ptr = 0x8;
    *audio_ptr = 0x0;
}

// Write one sample to both left and right channels
// Wait until there is space in the audio FIFO
void audio_write_sample(volatile int *audio_ptr, int sample24) {
    while (1) {
        int fifospace = *(audio_ptr + 1);
        int wsrc = (fifospace >> 16) & 0xFF;
        int wslc = (fifospace >> 24) & 0xFF;

        // Only write if both channels have space
        if (wsrc > 0 && wslc > 0) {
            *(audio_ptr + 2) = sample24; // left channel
            *(audio_ptr + 3) = sample24; // right channel
            break;
        }
    }
}

// Play an entire sound
void play_sound(volatile int *audio_ptr, const short sound[], int length) {
    int i;
    for (i = 0; i < length; i++) {
        // Convert 16-bit sample to 24-bit and send it
        audio_write_sample(audio_ptr, ((int)sound[i]) << 8);
    }
}

// Simple delay so sounds do not overlap instantly
void delay(void) {
    volatile int i;
    for (i = 0; i < 500000; i++);
}

int main(void) {
    volatile int *switch_ptr = (int *)SWITCH_BASE;
    volatile int *audio_ptr = (int *)AUDIO_BASE;

    // Initialize audio hardware
    audio_init(audio_ptr);

    while (1) {
        // Read switches
        int sw = *switch_ptr & 0x1FF;

        // Play one sound depending on which switch is on
        if (sw & 0x001) {
            play_sound(audio_ptr, miss_sound, miss_sound_len);
            delay();
        }
        else if (sw & 0x002) {
            play_sound(audio_ptr, apple_sound, apple_sound_len);
            delay();
        }
        else if (sw & 0x004) {
            play_sound(audio_ptr, lemon_sound, lemon_sound_len);
            delay();
        }
        else if (sw & 0x008) {
            play_sound(audio_ptr, banana_sound, banana_sound_len);
            delay();
        }
        else if (sw & 0x010) {
            play_sound(audio_ptr, pineapple_sound, pineapple_sound_len);
            delay();
        }
        else if (sw & 0x020) {
            play_sound(audio_ptr, watermelon_sound, watermelon_sound_len);
            delay();
        }
        else if (sw & 0x040) {
            play_sound(audio_ptr, strawberry_sound, strawberry_sound_len);
            delay();
        }
        else if (sw & 0x080) {
            play_sound(audio_ptr, pomegranate_sound, pomegranate_sound_len);
            delay();
        }
        else if (sw & 0x100) {
            play_sound(audio_ptr, bomb_sound, bomb_sound_len);
            delay();
        }
    }

    return 0;
}