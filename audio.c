#include <stdio.h>

#include "bananas.h"
#include "bombs.h"
#include "gameovers.h"
#include "lemons.h"
#include "pineapples.h"
#include "plays.h"
#include "pomegranates.h"
#include "Red_Apples.h"
#include "slice.h"
#include "starts.h"
#include "strawberrys.h"
#include "watermelons.h"

#define SWITCH_BASE 0xFF200040
#define AUDIO_BASE  0xFF203040

// Sound data arrays
const short *miss_sound = gameovers;
const int miss_sound_len = sizeof(gameovers) / sizeof(gameovers[0]);

const short *apple_sound = Red_Apples;
const int apple_sound_len = sizeof(Red_Apples) / sizeof(Red_Apples[0]);

const short *lemon_sound = lemons;
const int lemon_sound_len = sizeof(lemons) / sizeof(lemons[0]);

const short *banana_sound = bananas;
const int banana_sound_len = sizeof(bananas) / sizeof(bananas[0]);

const short *pineapple_sound = pineapples;
const int pineapple_sound_len = sizeof(pineapples) / sizeof(pineapples[0]);

const short *watermelon_sound = watermelons;
const int watermelon_sound_len = sizeof(watermelons) / sizeof(watermelons[0]);

const short *strawberry_sound = strawberrys;
const int strawberry_sound_len = sizeof(strawberrys) / sizeof(strawberrys[0]);

const short *pomegranate_sound = pomegranates;
const int pomegranate_sound_len = sizeof(pomegranates) / sizeof(pomegranates[0]);

const short *bomb_sound = bombs;
const int bomb_sound_len = sizeof(bombs) / sizeof(bombs[0]);

// Extra sounds if you want them later
const short *start_sound = starts;
const int start_sound_len = sizeof(starts) / sizeof(starts[0]);

const short *play_sound_data = plays;
const int play_sound_len = sizeof(plays) / sizeof(plays[0]);

const short *slice_sound = slice;
const int slice_sound_len = sizeof(slice) / sizeof(slice[0]);

// Reset and enable audio device
void audio_init(volatile int *audio_ptr) {
    *audio_ptr = 0x8;
    *audio_ptr = 0x0;
}

// Write one sample to both left and right channels
void audio_write_sample(volatile int *audio_ptr, int sample24) {
    while (1) {
        int fifospace = *(audio_ptr + 1);
        int wsrc = (fifospace >> 16) & 0xFF;
        int wslc = (fifospace >> 24) & 0xFF;

        if (wsrc > 0 && wslc > 0) {
            *(audio_ptr + 2) = sample24; // left
            *(audio_ptr + 3) = sample24; // right
            break;
        }
    }
}

// Play an entire sound
void play_sound(volatile int *audio_ptr, const short sound[], int length) {
    int i;
    for (i = 0; i < length; i++) {
        audio_write_sample(audio_ptr, ((int)sound[i]) << 8);
    }
}

// Small delay
void delay(void) {
    volatile int i;
    for (i = 0; i < 500000; i++);
}

int main(void) {
    volatile int *switch_ptr = (int *)SWITCH_BASE;
    volatile int *audio_ptr = (int *)AUDIO_BASE;

    audio_init(audio_ptr);

    while (1) {
        int sw = *switch_ptr & 0x1FF;

        if (sw & 0x001) {
            play_sound(audio_ptr, miss_sound, miss_sound_len);
            delay();
        } else if (sw & 0x002) {
            play_sound(audio_ptr, apple_sound, apple_sound_len);
            delay();
        } else if (sw & 0x004) {
            play_sound(audio_ptr, lemon_sound, lemon_sound_len);
            delay();
        } else if (sw & 0x008) {
            play_sound(audio_ptr, banana_sound, banana_sound_len);
            delay();
        } else if (sw & 0x010) {
            play_sound(audio_ptr, pineapple_sound, pineapple_sound_len);
            delay();
        } else if (sw & 0x020) {
            play_sound(audio_ptr, watermelon_sound, watermelon_sound_len);
            delay();
        } else if (sw & 0x040) {
            play_sound(audio_ptr, strawberry_sound, strawberry_sound_len);
            delay();
        } else if (sw & 0x080) {
            play_sound(audio_ptr, pomegranate_sound, pomegranate_sound_len);
            delay();
        } else if (sw & 0x100) {
            play_sound(audio_ptr, bomb_sound, bomb_sound_len);
            delay();
        }
    }

    return 0;
}