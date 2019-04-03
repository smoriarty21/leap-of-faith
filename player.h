#ifndef PLAYER_H
#define PLAYER_H

#include "Velocity.h"

typedef struct {
    int x;
    int y;
    int height;
    int width;
    int jump_velocity;
    int move_speed;
    Velocity velocity;
} Player;

#endif