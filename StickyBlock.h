#ifndef STICKYBLOCK_H
#define STICKYBLOCK_H

#include "Velocity.h"

typedef struct {
    int x;
    int y;
    int height;
    int width;
    Velocity velocity;
} StickyBlock;

#endif
