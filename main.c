#include "player.h"
#include "platform.h"
#include "toolbox.h"
#include <stdio.h>
#include <signal.h>

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;

#define MEM_PAL  0x05000000

unsigned short input_cur = 0x03FF;
unsigned short input_prev = 0x03FF;

volatile u16* ScanlineCounter = (volatile u16*)0x4000006;

typedef struct obj_attrs {
	uint16 attr0;
	uint16 attr1;
	uint16 attr2;
	uint16 pad;
} __attribute__((packed, aligned(4))) obj_attrs;

typedef uint32    tile_4bpp[8];
typedef tile_4bpp tile_block[512];

#define oam_mem            ((volatile obj_attrs *)MEM_OAM)
#define tile_mem           ((volatile tile_block *)MEM_VRAM)
#define object_palette_mem ((volatile rgb15 *)(MEM_PAL + 0x200))

typedef uint16 rgb15;

// Set the position of an object to specified x and y coordinates
static inline void set_object_position(volatile obj_attrs *object, int x, int y) {
	object->attr0 = (object->attr0 & ~OBJECT_ATTR0_Y_MASK) | (y & OBJECT_ATTR0_Y_MASK);
	object->attr1 = (object->attr1 & ~OBJECT_ATTR1_X_MASK) | (x & OBJECT_ATTR1_X_MASK);
}

void WaitForVblank(void) {
	while(*ScanlineCounter < 160) {}
}

void vid_vsync() {
    while(REG_VCOUNT >= 160);
    while(REG_VCOUNT < 160);
}

inline void key_poll() {
    input_prev = input_cur;
    input_cur = REG_KEY_INPUT | KEY_MASK;
}

inline unsigned short wasKeyPressed(unsigned short key_code) {
    return (key_code) & (~input_cur & input_prev);
}

inline unsigned short wasKeyReleased(unsigned short key_code) {
    return  (key_code) & (input_cur & ~input_prev);
}

inline unsigned short getKeyState(unsigned short key_code) {
    return !(key_code & (input_cur) );
}

static inline rgb15 RGB15(int r, int g, int b) {
	return r | (g << 5) | (b << 10);
}

int main() {
    volatile uint16 *player_tile_mem = (uint16 *)tile_mem[4][1];
	volatile uint16 *small_enemy_mem   = (uint16 *)tile_mem[4][3];
    volatile uint16 *platform_mem   = (uint16 *)tile_mem[4][4];

	for (int i = 0; i < 2 * (sizeof(tile_4bpp) / 2); ++i)
		player_tile_mem[i] = 0x1111;
    for (int i = 0; i < (sizeof(tile_4bpp) / 2); ++i)
		small_enemy_mem[i] = 0x3333;
    for (int i = 0; i < 4 * (sizeof(tile_4bpp) / 2); ++i)
		platform_mem[i] = 0x2222;

	// Color pallet move me
	object_palette_mem[1] = RGB15(0x1F, 0x1F, 0x1F); // White
	object_palette_mem[2] = RGB15(0x00, 0x80, 0xFF); // Blue
    object_palette_mem[3] = RGB15(0xFF, 0x00, 0x7F); // Pink

    int frame = 0;

    // Sprites
	volatile obj_attrs *player_attrs = &oam_mem[0];
	player_attrs->attr0 = 0x8000;
	player_attrs->attr1 = 0x0;
	player_attrs->attr2 = 1;

    volatile obj_attrs *small_enemy_attrs = &oam_mem[1];
	small_enemy_attrs->attr0 = 0;
	small_enemy_attrs->attr1 = 0x2000;
	small_enemy_attrs->attr2 = 3;

    volatile obj_attrs *platform_attrs = &oam_mem[2];
	platform_attrs->attr0 = 0x4000;
	platform_attrs->attr1 = 0x4000;
	platform_attrs->attr2 = 4;

    // small enemt stuff make struct
    const int sm_enemy_width = 8, sm_enemy_height = 8;
	int sm_enemy_velocity_x = 2, sm_enemy_velocity_y = 1;
	int sm_enemy_x = SCREEN_WIDTH - sm_enemy_width, sm_enemy_y = SCREEN_HEIGHT - sm_enemy_height;
    int sm_enemy_velocity = -2;

    Platform platform;
    platform.x = 0;
    platform.y = 100;
    platform.height = 8;
    platform.width = 32;

    Player player;
    player.height = 16;
    player.width = 8;
    player.x = 0;
    player.y = SCREEN_HEIGHT - player.height;
    player.x_velocity = 0;
    player.y_velocity = 0;

    int gravity = 3;

    // Draw init move me
	set_object_position(player_attrs, player.x, player.y);
    set_object_position(small_enemy_attrs, sm_enemy_x, sm_enemy_y);
    set_object_position(platform_attrs, platform.x, platform.y);

    // Set video mode move to function
    REG_DISPCNT = DCNT_OBJ | 0x0040;

    while(1) {
        vid_vsync();

        if((frame & 7) == 0)
            key_poll();

        // Key inputs: move or clean I guess?
        if (getKeyState(KEY_RIGHT)) {
            player.x_velocity = 2;
        } else if (getKeyState(KEY_LEFT)) {
            player.x_velocity = -2;
        } else {
            player.x_velocity = 0;
        }
        if (wasKeyPressed(KEY_A)) {
            player.y_velocity = -9;
        }

        // Enemy
        if (sm_enemy_x + sm_enemy_velocity <= 0) {
            sm_enemy_velocity = 2;
        } else if (sm_enemy_x + sm_enemy_width + sm_enemy_velocity >= SCREEN_WIDTH) {
            sm_enemy_velocity = -2;
        }

        sm_enemy_x += sm_enemy_velocity;

        // Gravity
        if (player.y_velocity < gravity) {
            player.y_velocity += gravity;
        }

        // Check bounds: move
        if (player.x + player.x_velocity <= 0 || player.x + player.width + player.x_velocity >= SCREEN_WIDTH) {
            player.x_velocity = 0;
        }
        if (player.y + player.height + player.y_velocity >= SCREEN_HEIGHT) {
            player.y_velocity = 0;
            player.y = SCREEN_HEIGHT - player.height;
        } else if (player.y + player.y_velocity <= 0) {
            player.y_velocity = 0;
        }

        if (player.x + player.x_velocity <= platform.x + platform.width && player.y + player.y_velocity <= platform.y + platform.height && player.y + player.height + player.y_velocity >= platform.y) {
            if (player.x > platform.x + platform.width) { // TODO: Collides with left side
                player.x_velocity = 0;    
            }
            player.x_velocity = 0;
            player.y_velocity = 0;
            //player.x = platform.x +platform.width;
        }

        // if (player.x + player.width + player.x_velocity >= platform.x && player.x + player.x_velocity <= platform.x + platform.width) {
        //     if (player.y > platform.y + platform.height && player.y + player.y_velocity < platform.y + platform.height) {
        //         player.y_velocity = 0;
        //     } else if (player.y + player.height < platform.y && player.y + player.height + player.y_velocity > platform.y) {
        //         player.y_velocity = 0;
        //     }
        // }

        player.x += player.x_velocity;
        player.y += player.y_velocity;

        set_object_position(player_attrs, player.x, player.y);
        set_object_position(small_enemy_attrs, sm_enemy_x, sm_enemy_y);

        frame++;
    }

    return 0;
}