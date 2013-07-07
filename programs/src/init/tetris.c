#include "tetris.h"

#include "vga.h"
#include "rand.h"
#include "../ulib.h"
#include "../inttypes.h"

#define BOARD_X 10
#define BOARD_Y 20

#define BOARD_ORIGIN_X 30
#define BOARD_ORIGIN_Y 2

#define TICK_DURATION 5000
#define TICKS_PER_SEC 1

#define SCREEN_X_TO_BOARD_X(SCREEN_X) ((SCREEN_X-BOARD_ORIGIN_X)/2)
#define SCREEN_Y_TO_BOARD_Y(SCREEN_Y) (SCREEN_Y - BOARD_ORIGIN_Y)

#define NEXT_PIECE_ORIGIN_X (BOARD_ORIGIN_X-17)
#define NEXT_PIECE_ORIGIN_Y BOARD_ORIGIN_Y
#define NEXT_PIECE_SIZE_X 12
#define NEXT_PIECE_SIZE_Y 4

#define MAX_VGA_X (BOARD_ORIGIN_X+(BOARD_X*2))
#define MAX_VGA_Y (BOARD_ORIGIN_Y+BOARD_Y)

#define NUM_PIECES 7

typedef enum
{
	P_I = 0,
	P_J,
	P_L,
	P_O,
	P_S,
	P_T,
	P_Z,
	P_E,
} piece_t;

typedef struct
{
	int8_t arr_x[4];
	int8_t arr_y[4];
} block_map_t;

typedef enum
{
	SPAWN_BLOCK = 0,
	SHIFT_DOWN_BLOCKS,
	LINE_CHECK,
	LINE_CHECK_FLASH,
	PAUSE,
	GAME_OVER
} state_t;

static block_map_t block_offsets[4][NUM_PIECES] =
{
	{ // Rotation 0
		{ .arr_x = { -1,  0, 1, 2 }, .arr_y = { 0, 0, 0, 0 } }, // I
		{ .arr_x = { -1, 0, 1, 1 }, .arr_y = { 0, 0, 0, 1 } }, // J
		{ .arr_x = { -1, -1, 0, 1 }, .arr_y = { 0, 1, 0, 0 } }, // L
		{ .arr_x = { 0, 1, 0, 1 }, .arr_y = { 0, 0, 1, 1 } }, // O
		{ .arr_x = { -1, 0, 0, 1 }, .arr_y = { 1, 1, 0, 0 } }, // S
		{ .arr_x = { -1, 0, 0, 1 }, .arr_y = { 0, 0, 1, 0 } }, // T
		{ .arr_x = { -1, 0, 0, 1 }, .arr_y = { 0, 0, 1, 1 } }, // Z
	},
	{ // Rotation 1
		{ .arr_x = { 0, 0, 0, 0 }, .arr_y = { -1, 0, 1, 2 } }, // I
		{ .arr_x = { 0, 0, 0, -1 }, .arr_y = { -1, 0, 1, 1 } }, // J
		{ .arr_x = { -1, 0, 0, 0 }, .arr_y = { -1, -1, 0, 1 } }, // L
		{ .arr_x = { 0, 1, 0, 1 }, .arr_y = { 0, 0, 1, 1 } }, // O
		{ .arr_x = { -1, -1, 0, 0 }, .arr_y = { -1, 0, 0, 1 } }, // S
		{ .arr_x = { 0, 0, -1, 0 }, .arr_y = { -1, 0, 0, 1 } }, // T
		{ .arr_x = { 0, 0, 1, 1 }, .arr_y = { 1, 0, 0, -1 } }, // Z
	},
	{ // Rotation 2
		{ .arr_x = { -1, 0, 1, 2 }, .arr_y = { 0, 0, 0, 0 } }, // I
		{ .arr_x = { 1, 0, -1, -1 }, .arr_y = { 0, 0, 0, -1 } }, // J
		{ .arr_x = { 1, 1, 0, -1 }, .arr_y = { 0, -1, 0, 0 } }, // L
		{ .arr_x = { 0, 1, 0, 1 }, .arr_y = { 0, 0, 1, 1 } }, // O
		{ .arr_x = { -1, 0, 0, 1 }, .arr_y = { 1, 1, 0, 0 } }, // S
		{ .arr_x = { -1, 0, 0, 1 }, .arr_y = { 0, 0, -1, 0 } }, // T
		{ .arr_x = { -1, 0, 0, 1 }, .arr_y = { 0, 0, 1, 1 } }, // Z
	},
	{ // Rotation 3
		{ .arr_x = { 0, 0, 0, 0 }, .arr_y = { -1, 0, 1, 2 } }, // I
		{ .arr_x = { 0, 0, 0, 1 }, .arr_y = { 1, 0, -1, -1 } }, // J
		{ .arr_x = { 1, 0, 0, 0 }, .arr_y = { 1, 1, 0, -1 } }, // L
		{ .arr_x = { 0, 1, 0, 1 }, .arr_y = { 0, 0, 1, 1 } }, // O
		{ .arr_x = { -1, -1, 0, 0 }, .arr_y = { -1, 0, 0, 1 } }, // S
		{ .arr_x = { 0, 0, 1, 0 }, .arr_y = { -1, 0, 0, 1 } }, // T
		{ .arr_x = { 0, 0, 1, 1 }, .arr_y = { 1, 0, 0, -1 } }, // Z
	}
};

static color_t piece_colors[NUM_PIECES] = 
{
	{ .fg = FG_BLACK, .bg = BG_GREEN, .blink = 0 },
	{ .fg = FG_BLACK, .bg = BG_CYAN, .blink = 0 },
	{ .fg = FG_BLACK, .bg = BG_BLUE, .blink = 0 },
	{ .fg = FG_BLACK, .bg = BG_RED, .blink = 0 },
	{ .fg = FG_BLACK, .bg = BG_MAGENTA, .blink = 0 },
	{ .fg = FG_BLACK, .bg = BG_GREEN, .blink = 0 },
	{ .fg = FG_BLACK, .bg = BG_BROWN, .blink = 0 },
};

static uint8_t gameboard[BOARD_X][BOARD_Y];
static uint8_t quit = 0;
static state_t state = SPAWN_BLOCK;
static state_t state_pause_prev = SPAWN_BLOCK;
static piece_t current_piece = P_I;
static uint8_t current_rotation = 0;
static piece_t next_piece = P_I;
static int32_t piece_origin_x = 0;
static int32_t piece_origin_y = 0;
static uint32_t ticks = 0;
static color_t background_color = { .fg = FG_BLACK, .bg = BG_BLACK, .blink = 0 };

static void draw_game_board()
{
	uint16_t x = BOARD_ORIGIN_X;
	uint16_t y = BOARD_ORIGIN_Y;

	for (uint32_t bx = 0; bx < BOARD_X; ++bx)
	{
		for (uint32_t by = 0; by < BOARD_Y; ++by)
		{
			const uint32_t off1 = (y+by)*MAX_X + (x+bx);
			const uint32_t off2 = off1+1;
			video[off1].c = ' ';
			video[off1].color = piece_colors[gameboard[bx][by]];
			video[off2].c = ' ';
			video[off2].color = piece_colors[gameboard[bx][by]];
		}
	}
}

static void draw_game_borders()
{
	color_t border_color = { .fg = FG_BLACK, .bg = BG_WHITE, .blink = 0 };

	// Draw top and bottom border
	const uint32_t max_x = BOARD_ORIGIN_X + BOARD_X*2;
	for (uint32_t x = BOARD_ORIGIN_X-1; x < max_x+1; ++x)
	{
		const uint32_t off1 = (BOARD_ORIGIN_Y-1)*MAX_X + x;
		video[off1].c = ' ';
		video[off1].color = border_color;

		const uint32_t off2 = (MAX_VGA_Y)*MAX_X + x;
		video[off2].c = ' ';
		video[off2].color = border_color;
	}

	// Draw left and right border
	for (uint32_t y = BOARD_ORIGIN_Y-1; y < MAX_VGA_Y+1; ++y)
	{
		const uint32_t off1 = y*MAX_X + (BOARD_ORIGIN_X - 1);
		video[off1].c = ' ';
		video[off1].color = border_color;

		const uint32_t off2 = y*MAX_X + max_x;
		video[off2].c = ' ';
		video[off2].color = border_color;
	}

	// Draw the next piece borders
	const uint32_t max_next_x = NEXT_PIECE_ORIGIN_X+NEXT_PIECE_SIZE_X+1;
	for (uint32_t x = NEXT_PIECE_ORIGIN_X-1; x < max_next_x+1; ++x)
	{
		const uint32_t off1 = (NEXT_PIECE_ORIGIN_Y-1)*MAX_X + x;
		video[off1].c = ' ';
		video[off1].color = border_color;

		const uint32_t off2 = (NEXT_PIECE_ORIGIN_Y+NEXT_PIECE_SIZE_Y+1)*MAX_X + x;
		video[off2].c = ' ';
		video[off2].color = border_color;
	}

	const uint32_t max_next_y = NEXT_PIECE_ORIGIN_Y+NEXT_PIECE_SIZE_Y+1;
	for (uint32_t y = NEXT_PIECE_ORIGIN_Y-1; y < max_next_y+1; ++y)
	{
		const uint32_t off1 = y*MAX_X + NEXT_PIECE_ORIGIN_X-1;
		video[off1].c = ' ';
		video[off1].color = border_color;

		const uint32_t off2 = y*MAX_X + max_next_x;
		video[off2].c = ' ';
		video[off2].color = border_color;
	}
}

static void draw_piece(int32_t ox, int32_t oy, uint8_t rotation, piece_t piece, color_t color)
{
	for (uint32_t i = 0; i < 4; ++i)
	{
		const int32_t px = block_offsets[rotation][piece].arr_x[i]*2 + ox;
		const int32_t py = block_offsets[rotation][piece].arr_y[i] + oy;
		const int32_t off1 = py*MAX_X + px;
		video[off1].c = ' ';
		video[off1].color = color;

		const int32_t off2 = off1+1;
		video[off2].c = ' ';
		video[off2].color = color;
	}
}

static void draw_current_piece()
{
	draw_piece(piece_origin_x, piece_origin_y, current_rotation, current_piece, piece_colors[current_piece]);
}

static void choose_piece()
{
	next_piece = P_S;//rand() % NUM_PIECES;
}

static uint8_t check_collision(int32_t ox, int32_t oy, uint8_t rotation, piece_t piece)
{
	for (uint32_t i = 0; i < 4; ++i)
	{
		const int32_t px = block_offsets[rotation][piece].arr_x[i] + ox;
		const int32_t py = block_offsets[rotation][piece].arr_y[i] + oy;

		if (px < 0 || 
			px >= BOARD_X || 
			py < 0 || 
			py >= BOARD_Y ||
			gameboard[px][py] != P_E)
		{
			return 1;
		}
	}

	return 0;
}

static uint8_t shift_down()
{
	const int32_t adjusted_origin_x = SCREEN_X_TO_BOARD_X(piece_origin_x);
	const int32_t adjusted_origin_y = SCREEN_Y_TO_BOARD_Y(piece_origin_y);
	if (check_collision(adjusted_origin_x, adjusted_origin_y+1, current_rotation, current_piece))
	{
		return 1;
	}

	++piece_origin_y;

	return 0;
}

static uint8_t quit_pressed = 0;
static uint8_t rotate_pressed = 0;
static uint8_t left_pressed = 0;
static uint8_t right_pressed = 0;
static uint8_t drop_pressed = 0;
static uint8_t pause_pressed = 0;

static void update_keys()
{
	quit_pressed = 0;
	left_pressed = 0;
	right_pressed = 0;
	drop_pressed = 0;
	rotate_pressed = 0;
	pause_pressed = 0;

	while (key_available())
	{
		const uint8_t key = get_key();

		switch (key)
		{
			case 'q':
			case 'Q':
				quit_pressed = 1;
				break;
			case 'a':
			case 'A':
				left_pressed = 1;
				break;
			case 'd':
			case 'D':
				right_pressed = 1;
				break;
			case 'p':
			case 'P':
				pause_pressed = 1;
				break;
			case 'w':
			case 'W':
				rotate_pressed = 1;
				break;
			case ' ':
				drop_pressed = 1;
				break;
		}
	}
}

static uint8_t handle_actions()
{
	const int32_t adjusted_origin_x = SCREEN_X_TO_BOARD_X(piece_origin_x);
	const int32_t adjusted_origin_y = SCREEN_Y_TO_BOARD_Y(piece_origin_y);

	if (left_pressed &&
			!check_collision(adjusted_origin_x-1, adjusted_origin_y, current_rotation, current_piece))
	{
		piece_origin_x -= 2;
	}
	else if (right_pressed &&
			!check_collision(adjusted_origin_x+1, adjusted_origin_y, current_rotation, current_piece))
	{
		piece_origin_x += 2;
	}
	else if (rotate_pressed &&
			!check_collision(adjusted_origin_x, adjusted_origin_y, (current_rotation+1)&3, current_piece))
	{
		current_rotation = (current_rotation + 1) & 3;
	}
	else if (drop_pressed)
	{
		// Figure out how far the piece can drop
		int32_t drop_amount = 1;
		while (!check_collision(adjusted_origin_x, adjusted_origin_y+drop_amount, current_rotation, current_piece))
		{
			++drop_amount;
		}

		piece_origin_y += drop_amount-1;

		return 1;
	}

	return 0;
}

static void place_current_block_on_board()
{
	const int32_t ox = SCREEN_X_TO_BOARD_X(piece_origin_x);
	const int32_t oy = SCREEN_Y_TO_BOARD_Y(piece_origin_y);

	for (uint32_t i = 0; i < 4; ++i)
	{
		const int32_t px = block_offsets[current_rotation][current_piece].arr_x[i];
		const int32_t py = block_offsets[current_rotation][current_piece].arr_y[i];

		gameboard[px+ox][py+oy] = current_piece;
	}
}

static void update()
{
	update_keys();
	if (quit_pressed)
	{
		quit = 1;
		return;
	}

	if (pause_pressed)
	{
		if (state == PAUSE)
		{
			state = state_pause_prev;
		}
		else
		{
			state_pause_prev = state;
			state = PAUSE;
		}
	}

	switch (state)
	{
		case SPAWN_BLOCK:
			current_piece = next_piece;
			choose_piece();

			// Set the piece origin
			piece_origin_x = BOARD_ORIGIN_X + BOARD_X;
			piece_origin_y = BOARD_ORIGIN_Y + 10;

			const int32_t bx = SCREEN_X_TO_BOARD_X(piece_origin_x);
			const int32_t by = SCREEN_Y_TO_BOARD_Y(piece_origin_y);
			if (check_collision(bx, by, current_rotation, current_piece))
			{
				// Can't place the new piece
				state = GAME_OVER;
			}
			else
			{
				// Can place the new piece
				draw_current_piece();
				state = SHIFT_DOWN_BLOCKS;
				ticks = 0;
			}
			break;
		case SHIFT_DOWN_BLOCKS:
			// Erase the piece at the current location
			draw_piece(piece_origin_x, piece_origin_y, current_rotation, current_piece, background_color);
			if (handle_actions())
			{
				place_current_block_on_board();
				state = LINE_CHECK;
			}

/*			++ticks;
			if (ticks >= TICKS_PER_SEC)
			{
				ticks = 0;

				// Shift the current piece down
				if (shift_down())
				{
					// Collided
					place_current_block_on_board();
					state = LINE_CHECK;
				}
			}
			*/
			draw_current_piece();
			break;
		case LINE_CHECK:
			state = SPAWN_BLOCK;
			break;
		case LINE_CHECK_FLASH:
			break;
		case PAUSE:
			// Do nothing
			break;
		case GAME_OVER:
			break;
	}
}

static void render()
{

}

void tetris()
{
	clear_screen();

	draw_game_borders();
//	draw_game_board();

	choose_piece();

	// Initialize the game board
	for (uint32_t y = 0; y < BOARD_Y; ++y)
	{
		for (uint32_t x = 0; x < BOARD_X; ++x)
		{
			gameboard[x][y] = P_E;
		}
	}

	// Enter game loop
	while (!quit)
	{
		update();
		render();

		msleep(TICK_DURATION);
	}
}
