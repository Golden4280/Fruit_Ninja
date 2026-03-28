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
#define LED_BASE    0xFF200000
#define AUDIO_BASE  0xFF203040

struct audio_t {
    volatile unsigned int control;
    volatile unsigned char rarc;
    volatile unsigned char ralc;
    volatile unsigned char warc;
    volatile unsigned char walc;
    volatile unsigned int ldata;
    volatile unsigned int rdata;
};

volatile struct audio_t *audiop = (struct audio_t *)AUDIO_BASE;

/* Sound data arrays */
const int *gameovers_sound = gameovers;
const int gameovers_sound_len = sizeof(gameovers) / sizeof(gameovers[0]);

const int *apple_sound = Red_Apples;
const int apple_sound_len = sizeof(Red_Apples) / sizeof(Red_Apples[0]);

const int *lemon_sound = lemons;
const int lemon_sound_len = sizeof(lemons) / sizeof(lemons[0]);

const int *banana_sound = bananas;
const int banana_sound_len = sizeof(bananas) / sizeof(bananas[0]);

const int *pineapple_sound = pineapples;
const int pineapple_sound_len = sizeof(pineapples) / sizeof(pineapples[0]);

const int *watermelon_sound = watermelons;
const int watermelon_sound_len = sizeof(watermelons) / sizeof(watermelons[0]);

const int *strawberry_sound = strawberrys;
const int strawberry_sound_len = sizeof(strawberrys) / sizeof(strawberrys[0]);

const int *pomegranate_sound = pomegranates;
const int pomegranate_sound_len = sizeof(pomegranates) / sizeof(pomegranates[0]);

const int *bomb_sound = bombs;
const int bomb_sound_len = sizeof(bombs) / sizeof(bombs[0]);

const int *start_sound = starts;
const int start_sound_len = sizeof(starts) / sizeof(starts[0]);

const int *play_sound_data = plays;
const int play_sound_len = sizeof(plays) / sizeof(plays[0]);

const int *slice_sound = slice;
const int slice_sound_len = sizeof(slice) / sizeof(slice[0]);

void delay(void) {
    volatile int i;
    for (i = 0; i < 500000; i++);
}

void audio_reset(void) {
    audiop->control = 0x8;
    audiop->control = 0x0;
}

void audio_playback_mono(const int *samples, int n, int step, int replicate) {
    int i, r;

    audio_reset();

    for (i = 0; i < n; i += step) {
        for (r = 0; r < replicate; r++) {
            while (audiop->warc == 0);
            audiop->ldata = samples[i];
            audiop->rdata = samples[i];
        }
    }
}

void blink_led(volatile int *led_ptr, int pattern) {
    *led_ptr = pattern;
    delay();
    *led_ptr = 0x0;
    delay();
}

int main(void) {
    volatile int *switch_ptr = (int *)SWITCH_BASE;
    volatile int *led_ptr = (int *)LED_BASE;

    while (1) {
        int sw = *switch_ptr & 0x1FF;

        if (sw & 0x001) {
            blink_led(led_ptr, 0x001);
            audio_playback_mono(slice_sound, slice_sound_len, 1, 1);
            delay();
        }
        else if (sw & 0x002) {
            blink_led(led_ptr, 0x002);
            audio_playback_mono(apple_sound, apple_sound_len, 1, 1);
            delay();
        }
        else if (sw & 0x004) {
            blink_led(led_ptr, 0x004);
            audio_playback_mono(lemon_sound, lemon_sound_len, 1, 1);
            delay();
        }
        else if (sw & 0x008) {
            blink_led(led_ptr, 0x008);
            audio_playback_mono(banana_sound, banana_sound_len, 1, 1);
            delay();
        }
        else if (sw & 0x010) {
            blink_led(led_ptr, 0x010);
            audio_playback_mono(pineapple_sound, pineapple_sound_len, 1, 1);
            delay();
        }
        else if (sw & 0x020) {
            blink_led(led_ptr, 0x020);
            audio_playback_mono(watermelon_sound, watermelon_sound_len, 1, 1);
            delay();
        }
        else if (sw & 0x040) {
            blink_led(led_ptr, 0x040);
            audio_playback_mono(strawberry_sound, strawberry_sound_len, 1, 1);
            delay();
        }
        else if (sw & 0x080) {
            blink_led(led_ptr, 0x080);
            audio_playback_mono(pomegranate_sound, pomegranate_sound_len, 1, 1);
            delay();
        }
        else if (sw & 0x100) {
            blink_led(led_ptr, 0x100);
            audio_playback_mono(bomb_sound, bomb_sound_len, 1, 1);
            delay();
        }
    }

    return 0;
}