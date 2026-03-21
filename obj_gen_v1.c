// The fruit/bomb movement and generation module
// Purpose: module has two structs that contain location, speed, direction, etc.
// - Fruits
// - Bomb
// Includes random fruit generator within an overall weighted random generator
// generates a random fruit from the fruit struct
// then less often generates bombs
// considers gravity to animate objects being thrown onto screen and falling off
// sends input to collision and VGA module
// objects are generated at random locations along x axis at random speeds

#include <stdio.h>
#include <stdlib.h>

#define VGA 0xFF203020
#define GRAVITY 2


// defining structs/types

typedef enum {
  APPLE,
  LEMON,
  BANANA,
  PINEAPPLE,
  WATERMELON,
  STRAWBERRY,
  POMEGRANATE,
  BOMB
} Type;

typedef struct {
  // type of object for different actions/vga
  Type type;
  // direction/velocity
  int vx, vy;
  // position
  int x, y;
  // check location onscreen
  int onScreen;  // 1 if active
  // mass for gravity
  int mass;
} Object;

int main() {
  // random generation logic (result is sent to collision and vga)
}

void randomGenerator(Object* obj) {
  // Randomize start location
  obj->x =
      rand() % 300 +
      10;  // random value 0-299 (within vga, not at edge) then +10 to avoid 0
           // edge giving object in 10-209 range (10 offset from each side)
  obj->y = 239;  // objects start at bottom

  // Randomize upward velocity
  obj->vy =
      -(rand() % 5 + 5) /
      (obj->mass);  // gives velocity 5-9 (open to change upon testing)
                    // negative since moving up screen (decreasing y direction)

  // Randomize horizontal velocity
  obj->vx =
      rand() % 4 -
      2;  // gives value -2, -1, 0, 1, 2 for left and right movement on screen

  // Weighted random generator for fruit or bomb
  // start with 60/30/10 (fruit/pom/bomb) split and test
  int chance = rand() % 100;  // number 0-99

  // chance of fruit
  if (chance < 70) {
    obj->type = rand() % 6;  // random index 0-5 for the first 6 fruit types
    obj->mass = 2;
  } else if (chance < 85) {
    obj->type =
        POMEGRANATE;  // If not less than 60 but less than 70 we hit pom range
    obj->mass = 1;
  } else {
    obj->type = BOMB;  // otherwise we have a bomb
    obj->mass = 3;
  }

  obj->onScreen =
      1;  // tells us an object is onscreen for later detection and such
}

void physics(Object* obj) {
  if (!obj->onScreen) {
    return;
  }

  // update position
  obj->x += obj->vx;
  obj->y += obj->vy;

  // apply force of gravity Fg = mg to vertical velocity
  obj->vy += (GRAVITY * (obj->mass));

  // when object is off screen
  if (obj->y > 240) {
    obj->onScreen = 0;
  }
}