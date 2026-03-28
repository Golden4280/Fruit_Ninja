
#include "start_sound.h"
static int Game_start_packed_len = sizeof(Game_start_packed) / sizeof(Game_start_packed[0]);

//int samples_n = 77920;

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


void 
audio_playback_mono(int *Game_start_packed, int n, int step, int replicate) {
            int i;

            audiop->control = 0x8; // clear the output FIFOs
            audiop->control = 0x0; // resume input conversion
            for (i = 0; i < n; i+=step) {
              // output data if there is space in the output FIFOs
              for (int r=0; r < replicate; r++) {
				while(audiop->warc == 0);
                audiop->ldata = Game_start_packed[i];
                audiop->rdata = Game_start_packed[i];
			  }
			}
	}	


int 
main(void) {
   audio_playback_mono(Game_start_packed, Game_start_packed_len, 1, 1);
   while (1);
}

