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
// but for now just hex display