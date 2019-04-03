#include "Player.h"
#include "toolbox.h"
#include "Platform.h"
#include "Velocity.h"
#include "StickyBlock.h"
#include "ObjectAttributes.h"
#include <stdio.h>
#include <signal.h>

unsigned short input_cur = 0x03FF;
unsigned short input_prev = 0x03FF;

volatile uint16* ScanlineCounter = (volatile uint16*)0x4000006;

// Set the position of an object to specified x and y coordinates
static inline void set_object_position(volatile ObjectAttributes *object, int x, int y) {
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

static inline void setColorPallet() {
    object_palette_mem[1] = RGB15(0x1F, 0x1F, 0x1F); // White
	object_palette_mem[2] = RGB15(0x00, 0x80, 0xFF); // Blue
    object_palette_mem[3] = RGB15(0xFF, 0x00, 0x7F); // Pink
}

static inline void writePlayerTilesToMemory() {
    volatile uint16 *player_tile_mem = (uint16 *)tile_mem[4][1];

    for (int i = 0; i < 2 * (sizeof(tile_4bpp) / 2); ++i)
		player_tile_mem[i] = 0x1111;
}

static inline void writeEnemyTilesToMemory() {
    volatile uint16 *small_enemy_mem   = (uint16 *)tile_mem[4][3];

    for (int i = 0; i < (sizeof(tile_4bpp) / 2); ++i)
		small_enemy_mem[i] = 0x3333;
}

static inline void writePlatformTilesToMemory() {
    volatile uint16 *platform_mem   = (uint16 *)tile_mem[4][4];

    for (int i = 0; i < 4 * (sizeof(tile_4bpp) / 2); ++i)
		platform_mem[i] = 0x2222;
}

int main() {
    int frame = 0;
    
    setColorPallet();

    writePlayerTilesToMemory();
    writeEnemyTilesToMemory();
    writePlatformTilesToMemory();

    // Sprites
	volatile ObjectAttributes *player_attrs = &oam_mem[0];
	player_attrs->attr0 = 0x8000;
	player_attrs->attr1 = 0x0;
	player_attrs->attr2 = 1;

    volatile ObjectAttributes *small_enemy_attrs = &oam_mem[1];
	small_enemy_attrs->attr0 = 0;
	small_enemy_attrs->attr1 = 0x2000;
	small_enemy_attrs->attr2 = 3;

    volatile ObjectAttributes *platform_attrs = &oam_mem[2];
	platform_attrs->attr0 = 0x4000;
	platform_attrs->attr1 = 0x4000;
	platform_attrs->attr2 = 4;

    StickyBlock stickyBlock;
    stickyBlock.width = 8;
    stickyBlock.height = 8;
    stickyBlock.x = SCREEN_WIDTH - stickyBlock.width;
    stickyBlock.y = SCREEN_HEIGHT - stickyBlock.height;
    stickyBlock.velocity.x = -1;
    stickyBlock.velocity.y = 0;

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
    player.velocity.x = 0;
    player.velocity.y = 0;
    player.jump_velocity = -9;
    player.move_speed = 2;

    int gravity = 3;

	set_object_position(player_attrs, player.x, player.y);
    set_object_position(small_enemy_attrs, stickyBlock.x, stickyBlock.y);
    set_object_position(platform_attrs, platform.x, platform.y);

    REG_DISPCNT = DCNT_OBJ | 0x0040;

    while(1) {
        vid_vsync();

        if((frame & 7) == 0)
            key_poll();

        if (getKeyState(KEY_RIGHT)) {
            player.velocity.x = player.move_speed;
        } else if (getKeyState(KEY_LEFT)) {
            player.velocity.x = player.move_speed * -1;
        } else {
            player.velocity.x = 0;
        }
        if (wasKeyPressed(KEY_A)) {
            player.velocity.y = player.jump_velocity;
        }

        // Enemy
        if (stickyBlock.x + stickyBlock.velocity.x <= 0) {
            stickyBlock.velocity.x = 1;
        } else if (stickyBlock.x + stickyBlock.width + stickyBlock.velocity.x >= SCREEN_WIDTH) {
            stickyBlock.velocity.x = -1;
        }
        stickyBlock.x += stickyBlock.velocity.x;

        // Gravity
        if (player.velocity.y < gravity) {
            player.velocity.y += gravity;
        }

        // Check floor / ceiling bounds: move
        if (player.x + player.velocity.x <= 0 || player.x + player.width + player.velocity.x >= SCREEN_WIDTH) {
            player.velocity.x = 0;
        }
        if (player.y + player.height + player.velocity.y >= SCREEN_HEIGHT) {
            player.velocity.y = 0;
            player.y = SCREEN_HEIGHT - player.height;
        } else if (player.y + player.velocity.y <= 0) {
            player.velocity.y = 0;
        }

        // Check player / platform collisions
        if (player.x + player.width > platform.x && player.x < platform.x + platform.width && player.y + player.height + player.velocity.y >= platform.y && player.y + player.height - player.velocity.y < platform.y) { // Top Collision
            player.velocity.y = 0;
            player.y = platform.y - player.height;    
        }

        if (player.x + player.width > platform.x && player.x < platform.x + platform.width && player.y + player.velocity.y <= platform.y + platform.height && player.y - player.velocity.y > platform.y + platform.height) { // Bottom Collision
            player.velocity.y = gravity;
            player.y = platform.y + platform.height;    
        }

        if (player.y + player.height > platform.y && player.y < platform.y && player.x + player.velocity.x < platform.x + platform.width) { // Right
            player.velocity.x = 0;
            player.x = platform.x + platform.width;
        }

        // TODO: Left(need to add platform to test on)

        // Check player / enemy collision

        player.x += player.velocity.x;
        player.y += player.velocity.y;

        set_object_position(player_attrs, player.x, player.y);
        set_object_position(small_enemy_attrs, stickyBlock.x, stickyBlock.y);

        frame++;
    }

    return 0;
}