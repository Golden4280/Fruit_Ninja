// collisions module
// takes all x coordinate input from mouse position
// loops through coordinations of each object
// if equal then check if bomb set state to gameover
//
// else check pomogrante +2
// if neither than incrememtn +1

// scoring module

// return global boolean

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
#define CHAR_X_1 (134/8)
#define CHAR_Y_1 (140/16)
#define CHAR_X_2 (134/8)
#define CHAR_Y_2 (160/16)


void draw_score_top(int score) {
    char buffer[4];
    // format as 3 digits, padded with zeros
    sprintf(buffer, "%03d", score);

    // Clear old score area (3 digits)
    clear_text_area(0, 0, 3);

    // Draw new score at top-left
    write_string(0, 0, buffer);
}


void draw_gameover_scores(int score, int high_score) {
    char line1[20];
    char line2[20];

    sprintf(line1, "SCORE: %03d", score);
    sprintf(line2, "HIGH SCORE:  %03d", high_score);

    // Clear a region in the middle of the screen
    
    clear_text_area(10, 8, 40);
    clear_text_area(10, 10, 40);


    write_string(CHAR_X_1, CHAR_Y_1, line1);
    write_string(CHAR_X_2, CHAR_Y_2, line2);
}








// inside state condition





// bomb exploding function
// start point takes in bomb start coord and adds offset to start from center
// draws white lines from bomb
// calls delay

void explosion(Object *bo) {

}


// DRAW LINE FUNCTION
void draw_line(int x0, int y0, int x1, int y1, short int line_color) 
{

	// used to determine how to navigate line
	bool is_steep = abs(y1-y0) > abs(x1-x0);
	
	// for calulation
	if (is_steep) {
		// complete algorithm vertically instead of horizonatally
		swap(&x0, &y0);
		swap(&x1, &y1);
	
	}
	if (x0 > x1) {
		// point 1 ahead of point 2 then swap
		// drawing L -> R always
		swap(&x0, &x1);
		swap(&y0, &y1);
	}

	// total distance covered
	int deltax = x1 - x0;
	// verified x before, ensure y is aboslute for up/down
	int deltay = abs(y1 - y0); // why absolute here?
	
	// round to nearest pixel
	// used to determine when we "enter" a new pixel
	int error = -(deltax/2);
	int y = y0;
	// which direction to move in y
	// if point 1 > then we move down else move up
	int y_step;
	if (y0 > y1) 
	{
	y_step = -1;
	} else {
		y_step = 1;
	}
	
	for (int x = x0; x <= x1; x++) {
        if (x0 >= 0 && x0 < 320 && y0 >= 0 && y0 < 240) {

            if (is_steep) {
		        plot_pixel(y, x, line_color);
		    } else {
		        plot_pixel(x, y, line_color);
		    }

        }
		
		// increment error for pixel calculation
		error = error + deltay;
	
		// when we meet threshold, increment y to next
		// reset error tracker for next loop
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




// FUNCTION END









// 
void collisions() {

    // // use x_pos and y_pos 
    // // exit while once bomb is hit
    // while (bomb_hit == 0) {
    // actually continuously call functin in play and with each call, check all opbject s and positions
        for (int i = 0; i < MAX_OBJECTS; i++) {
            // increment through the entire object (48x48 square)
            if (objects[i].onScreen) {
            
                for (int y = 0; y < obj_h; y++) {
                    int y1 = objects[i].y + y;

                    for (int x = 0; x < obj_w; x++) {
                        int x1 = objects[i].x + x;
                    
                        unsigned short colour = objects[i].image[y * (objects[i].w) + x];
                        if (colour != TRANSPARENT) {
                            if ((x1 == x_pos) && (y1 == y_pos)) {
                            
                                // check if pom
                                if (objects[i].type == POMEGRANATE) {
                                    score += 2;
                                    
                                    // also play sound
                                } else if (objects[i].type == BOMB) {
                                    // if a bomb is hit, stop checking and return so that we can go to collisions
                                    bomb_hit = 1;
                                    return;
                                } else {
                                // if any other furit increment by 1
                                    score += 1;
                                    
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

// draw score function

// start display at 000
// decode score function
// draw score every loop if collision occurs
// if score %10 = 0 then only change ones

// dimensions of score sprites
#define NUM_W 16 
#define NUM_H 32

// location of score starting point and distance between pixels 
// 100s at 10, 10
// 10s at 10 + 16 + 2, 10
// 1s at 10 + 2*16 + 2*2, 10
// 2 rep number spacing
#define HUND_START 10 
#define TEN_START 28
#define ONE_START 46

#define SPACING 2
#define SC_X_1 134
#define SC_Y_1 140
#define SC_X_2 134
#define SC_Y_2 160

// write score as 000 before play

// take num and return the number to be printed
const unsigned short* decode_score(int num) {
    switch (num) 
        case 0: return _0;
        case 1: return _1;
        case 2: return _2;
        case 3: return _3;
        case 4: return _4;
        case 5: return _5;
        case 6: return _6;
        case 7: return _7;
        case 8: return _8;
        case 9: return _9;
        default: return _0;
}

// potential error is redrawing the same digtis over eachother
void update_score() {

    // no update if no change check
    if (score == old_score) { return; }

    // isolate digits from score
    int ones = score % 10;
    int tens = (score / 10) % 10;
    int hunds = (score / 100) % 10;

    // isolate digits from old scroe
    int old_ones = old_score % 10; // not actually neeeded
    int old_tens = (old_score / 10) % 10;
    int old_hunds = (old_score / 100) % 10;

    // alwasy update ones 
    // update ones
    draw_object(ONE_START, SCORE_Y, NUM_W, NUM_H, (decode_score(ones)));

    // if 10s and 100s dont change then dont update display
    // only called if at least one hit so ones alwasy increments
    if (tens != old_tens) {
        // update tens
        draw_object(TEN_START, SCORE_Y, NUM_W, NUM_H, (decode_score(tens)));

    // only change 10s and 1s
    } 

    if (hunds != old_hunds) {

        // update huns
        draw_object(HUND_START, SCORE_Y, NUM_W, NUM_H, (decode_score(hunds)));

    }

    // update old for next time
    old_score = score;
}

// takes some score and displays it center screen
void gameover_score(int sc, int x, int y) {

    // get digits
    int ones = sc % 10;
    int tens = (sc / 10) % 10;
    int hunds = (sc / 100) % 10;

    // get start points of each score
    int one_st = x + 2*(NUM_W + SPACING);
    int ten_st = x + NUM_W + SPACING;
    int hund_st = x;

    // DRAW OBEJCT FOR ALL THREE
    draw_object(one_st, y, NUM_W, NUM_H, (decode_score(ones)));
    draw_object(ten_st, y, NUM_W, NUM_H, (decode_score(tens)));
    draw_object(hund_st, y, NUM_W, NUM_H, (decode_score(hunds)));
}


// OLD MAIN JIC


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

                        update_cursor_position(dx, dy); //move cursor

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

    // update score display
    update_score();

    push_tail(x_pos, y_pos);
    draw_tail();
    draw_cursor(x_pos, y_pos);

    wait_for_vsync();  // swap front and back buffers on VGA vertical sync

    pixel_buffer_start = *(pixel_ctrl_ptr + 1);
  }
}
}


