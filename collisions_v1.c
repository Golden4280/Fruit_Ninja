// collisions module
// takes all x coordinate input from mouse position
// loops through coordinations of each object
// if equal then check if bomb set state to gameover
//
// else check pomogrante +2
// if neither than incrememtn +1

// scoring module

// return global boolean

void collisions() {

    // // use x_pos and y_pos 
    // // exit while once bomb is hit
    // while (bomb_hit == 0) {
    // actually continuously call functin in play and with each call, check all opbject s and positions
        for (int i = 0; i < MAX_OBJECTS; i++) {
            // increment through the entire object (48x48 square)
            if (object[i].onScreen) {
            
                for (int y = 0; y < obj_h; y++) {
                    int y1 = objects[i].y + y;

                    for (int x = 0; x < obj_w; x++) {
                        int x1 = objects[i].x + x;
                    
                        unsigned short colour = obj[y * w + x];
                        if (colour != TRANSPARENT) {
                            if ((x1 == x_pos) && (y1 == y_pos)) {
                            
                                // check if pom
                                if (objects[i].type == POMEGRANATE) {
                                    score += 2;
                                    // also play sound
                                } else if (objects[i].type == BOMB) {
                                    // if a bomb is hit, stop checking and return so that we can go to collisions
                                    bomb_hit == 1;
                                    return;
                                } else {
                                // if any other furit increment by 1
                                    score += 1
                                }
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
#define SCORE_Y 10
 

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

void update_score() {

    // no update if no change check
    if (score == oldscore) { return; }

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

