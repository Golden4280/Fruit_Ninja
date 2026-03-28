int main(void) {
    srand(1);
    volatile int *LEDR_ptr = (int *)LEDR_BASE;
    volatile int *ps2_ptr = (int *)PS2_BASE;
    volatile int *pixel_ctrl_ptr = (int*)VGA;

    unsigned char byte;//Stores one byte read from the PS/2 port

    //double buffer
    *(pixel_ctrl_ptr + 1) = (int)&Buffer1;//buffer1 is the back buffer
    wait_for_vsync();//Swap buffers at the next vertical sync
    pixel_buffer_start = *pixel_ctrl_ptr; // Get the address of the current front buffer
    clear_screen();

    *(pixel_ctrl_ptr + 1) = (int)&Buffer2; //buffer2 is the back buffer
    pixel_buffer_start = *(pixel_ctrl_ptr + 1);//drawing happens in buffer 2
    clear_screen();

    draw_background(start);

    init_mouse(ps2_ptr);//initialize mouse
    //NOTHING CAN CHANGE TO BOMB OR HIT FRUIT RN 

    // INITIALIZE OBJECT ARRAY
    for (int i = 0; i < MAX_OBJECTS; i++) {
        objects[i].onScreen = 0;
    }

    while (1) {
        //FSM
        if (current_state == STATE_START) {
            *LEDR_ptr = 0x01; // LED 0 on = waiting on start screen

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

                    count = 0;// reset packet count

                    if (left || right) {//any click from start goes to play
                        current_state = STATE_PLAY;
                        break;
                    }
                }
            }
        }

        else if (current_state == STATE_PLAY) {
            *LEDR_ptr = 0x02; // LED 1 on = playing

            

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
                    //ps2 is SIGNED, signed is stored in bit 4 of byte 1
                    int dx = (int)packet[1]; //x movement 
                    if (packet[0] & 0x10) {
                        dx = dx - 256;
                    }
                    int dy = (int)packet[2]; //y movement
                    if(packet[0] & 0x20){
                        dy = dy - 256;
                    }

                    //first byte has overflow x = bit6 and overflowy = bit7
                    //if overflow happens, ignore that by setting it to 0
                    if (packet[0] & 0x40) dx = 0;
                    if (packet[0] & 0x80) dy = 0;

                    count = 0;//reset packet count

                    update_cursor_position(dx, dy);//move cursor

                    // rewritten play state i think 
                    // during play we consistently check if a collision occured
                    // collision automatically returns when bomb is set to 1
                    // loop in play runs until bomb is set to 1
                    // this changes a global variable in main
                    // which then acts to change state to gameover
                    // in gameover we print old score and new score center screen   
                    while (bomb_hit == 0) {
                        // while in play continue checking collisions
                        collisions();
                        current_state = STATE_PLAY;
                    }

                    current_state = STATE_GAMEOVER;

                }
            }
        }

        else if (current_state == STATE_GAMEOVER) {

            if (score > high_score) {
                score = high_score;
            }

            *LEDR_ptr = 0x10; // LED 4 on = game over

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
                else if (count == 3) {. //full packet
                    int left  =  packet[0] & 0x1;//bit 0 of first byte (left mouse button, if pressed becomes 1)
                    int right = (packet[0] >> 1) & 0x1; //bit 1 of first byte (right click)

                    count = 0; // reset packet count

                    if (left || right) {//any click from game over resets and goes back to start
                        reset_game();
                        current_state = STATE_START;
                        break;
                    }
                }
            }
        }

        pixel_buffer_start = *(pixel_ctrl_ptr + 1);  // new back buffer

        // background updates based on states

        if (current_state == STATE_START) {
            draw_background(start);

        } else if (current_state == STATE_PLAY) {
            draw_background(play);

            add_object();
            physics();
            drawAllObjects();

            // update score display
            update_score();

            push_tail(x_pos, y_pos);
            draw_tail();
            draw_cursor(x_pos, y_pos);


        } else if (current_state == STATE_GAMEOVER) {
            draw_background(gameover);

            gameover_score(score, SC_X_1, SC_Y_1);
            gameover_score(high_score, SC_X_2, SC_Y_2);
            score = 0;
            old_score = -1;

        }
    
        // WAIT FOR VSYNC

        wait_for_vsync();  // swap front and back buffers on VGA vertical sync

        pixel_buffer_start = *(pixel_ctrl_ptr + 1);
    }

    return 0;
}