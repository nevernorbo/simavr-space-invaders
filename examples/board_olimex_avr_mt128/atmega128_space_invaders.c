#undef F_CPU
#define F_CPU 16000000
#include "avr_mcu_section.h"
AVR_MCU(F_CPU, "atmega128");

#define __AVR_ATmega128__ 1
#include <avr/io.h>
#include <util/delay.h>

// GENERAL INIT - USED BY ALMOST EVERYTHING ----------------------------------

static void port_init()
{
	PORTA = 0b00011111;
	DDRA = 0b01000000; // buttons & led
	PORTB = 0b00000000;
	DDRB = 0b00000000;
	PORTC = 0b00000000;
	DDRC = 0b11110111; // lcd
	PORTD = 0b11000000;
	DDRD = 0b00001000;
	PORTE = 0b00100000;
	DDRE = 0b00110000; // buzzer
	PORTF = 0b00000000;
	DDRF = 0b00000000;
	PORTG = 0b00000000;
	DDRG = 0b00000000;
}

// SOUND GENERATOR -----------------------------------------------------------

typedef struct
{
	int freq;
	int length;
} tune_t;

static tune_t TUNE_START[] = {{2000, 40}, {0, 0}};
static tune_t TUNE_LEVELUP[] = {{3000, 20}, {0, 0}};
static tune_t TUNE_GAMEOVER[] = {{1000, 200}, {1500, 200}, {2000, 400}, {0, 0}};

static void play_note(int freq, int len)
{
	for (int l = 0; l < len; ++l)
	{
		int i;
		PORTE = (PORTE & 0b11011111) | 0b00010000; // set bit4 = 1; set bit5 = 0
		for (i = freq; i; i--)
			;
		PORTE = (PORTE | 0b00100000) & 0b11101111; // set bit4 = 0; set bit5 = 1
		for (i = freq; i; i--)
			;
	}
}

static void play_tune(tune_t *tune)
{
	while (tune->freq != 0)
	{
		play_note(tune->freq, tune->length);
		++tune;
	}
}

// BUTTON HANDLING -----------------------------------------------------------

#define BUTTON_NONE 0
#define BUTTON_CENTER 1
#define BUTTON_LEFT 2
#define BUTTON_RIGHT 3
#define BUTTON_UP 4
static int button_accept = 1;

static int button_pressed()
{
	// right
	if (!(PINA & 0b00000001) & button_accept)
	{					   // check state of button 1 and value of button_accept
		button_accept = 0; // button is pressed
		return BUTTON_RIGHT;
	}

	// up
	if (!(PINA & 0b00000010) & button_accept)
	{					   // check state of button 2 and value of button_accept
		button_accept = 0; // button is pressed
		return BUTTON_UP;
	}

	// center
	if (!(PINA & 0b00000100) & button_accept)
	{					   // check state of button 3 and value of button_accept
		button_accept = 0; // button is pressed
		return BUTTON_CENTER;
	}

	// left
	if (!(PINA & 0b00010000) & button_accept)
	{					   // check state of button 5 and value of button_accept
		button_accept = 0; // button is pressed
		return BUTTON_LEFT;
	}

	return BUTTON_NONE;
}

static void button_unlock()
{
	// check state of all buttons
	if (
		((PINA & 0b00000001) | (PINA & 0b00000010) | (PINA & 0b00000100) | (PINA & 0b00001000) | (PINA & 0b00010000)) == 31)
		button_accept = 1; // if all buttons are released button_accept gets value 1
}

// LCD HELPERS ---------------------------------------------------------------

#define CLR_DISP 0x00000001
#define DISP_ON 0x0000000C
#define DISP_OFF 0x00000008
#define CUR_HOME 0x00000002
#define CUR_OFF 0x0000000C
#define CUR_ON_UNDER 0x0000000E
#define CUR_ON_BLINK 0x0000000F
#define CUR_LEFT 0x00000010
#define CUR_RIGHT 0x00000014
#define CG_RAM_ADDR 0x00000040
#define DD_RAM_ADDR 0x00000080
#define DD_RAM_ADDR2 0x000000C0

#define GET_BASE_ADDRESS(row) ((row) == 0 ? DD_RAM_ADDR : DD_RAM_ADDR2)

// #define		ENTRY_INC	    0x00000007	//LCD increment
// #define		ENTRY_DEC	    0x00000005	//LCD decrement
// #define		SH_LCD_LEFT	  0x00000010	//LCD shift left
// #define		SH_LCD_RIGHT	0x00000014	//LCD shift right
// #define		MV_LCD_LEFT	  0x00000018	//LCD move left
// #define		MV_LCD_RIGHT	0x0000001C	//LCD move right

static void lcd_delay(unsigned int b)
{
	volatile unsigned int a = b;
	while (a)
		a--;
}

static void lcd_pulse()
{
	PORTC = PORTC | 0b00000100; // set E to high
	lcd_delay(1400);			// delay ~110ms
	PORTC = PORTC & 0b11111011; // set E to low
}

static void lcd_send(int command, unsigned char a)
{
	unsigned char data;

	data = 0b00001111 | a;				 // get high 4 bits
	PORTC = (PORTC | 0b11110000) & data; // set D4-D7
	if (command)
		PORTC = PORTC & 0b11111110; // set RS port to 0 -> display set to command mode
	else
		PORTC = PORTC | 0b00000001; // set RS port to 1 -> display set to data mode
	lcd_pulse();					// pulse to set D4-D7 bits

	data = a << 4;						 // get low 4 bits
	PORTC = (PORTC & 0b00001111) | data; // set D4-D7
	if (command)
		PORTC = PORTC & 0b11111110; // set RS port to 0 -> display set to command mode
	else
		PORTC = PORTC | 0b00000001; // set RS port to 1 -> display set to data mode
	lcd_pulse();					// pulse to set d4-d7 bits
}

static void lcd_send_command(unsigned char a)
{
	lcd_send(1, a);
}

static void lcd_send_data(unsigned char a)
{
	lcd_send(0, a);
}

static void lcd_init()
{
	// LCD initialization
	// step by step (from Gosho) - from DATASHEET

	PORTC = PORTC & 0b11111110;

	lcd_delay(10000);

	PORTC = 0b00110000; // set D4, D5 port to 1
	lcd_pulse();		// high->low to E port (pulse)
	lcd_delay(1000);

	PORTC = 0b00110000; // set D4, D5 port to 1
	lcd_pulse();		// high->low to E port (pulse)
	lcd_delay(1000);

	PORTC = 0b00110000; // set D4, D5 port to 1
	lcd_pulse();		// high->low to E port (pulse)
	lcd_delay(1000);

	PORTC = 0b00100000; // set D4 to 0, D5 port to 1
	lcd_pulse();		// high->low to E port (pulse)

	lcd_send_command(0x28);		// function set: 4 bits interface, 2 display lines, 5x8 font
	lcd_send_command(DISP_OFF); // display off, cursor off, blinking off
	lcd_send_command(CLR_DISP); // clear display
	lcd_send_command(0x06);		// entry mode set: cursor increments, display does not shift

	lcd_send_command(DISP_ON);	// Turn ON Display
	lcd_send_command(CLR_DISP); // Clear Display
}

static void lcd_send_text(char *str)
{
	while (*str)
		lcd_send_data(*str++);
}

static void lcd_send_line1(char *str)
{
	lcd_send_command(DD_RAM_ADDR);
	lcd_send_text(str);
}

static void lcd_send_line2(char *str)
{
	lcd_send_command(DD_RAM_ADDR2);
	lcd_send_text(str);
}

// THE GAME ==================================================================

/* Sprites (characters) */

#define CHARMAP_SIZE 8
#define PLAYER_SPRITE 0

static unsigned char CHARMAP[CHARMAP_SIZE][8] = {
	{
		0b00000, // Player
		0b00000,
		0b00000,
		0b00000,
		0b00000,
		0b00000,
		0b00100,
		0b01110,
	},
	{
		0b01110, // Enemy
		0b10101,
		0b01010,
		0b00000,
		0b00000,
		0b00000,
		0b00000,
		0b00000,
	},
	{
		0b11111, // Enemy 2
		0b10101,
		0b01110,
		0b00000,
		0b00000,
		0b00000,
		0b00000,
		0b00000,
	},
	{
		0b11111, // Enemy 3
		0b01010,
		0b10101,
		0b00000,
		0b00000,
		0b00000,
		0b00000,
		0b00000,
	},
};

#define PLAYER_BULLET_SLOT 0
#define PLAYER_SLOT 1
#define DYNAMIC_SLOT_START_INDEX 2

static unsigned char STORED_CHARMAP[CHARMAP_SIZE][8]; // Used for dynamically storing custom characters

/* Character Utils */

#define CHECK_BIT(binary_num, pos) ((binary_num) & (1 << (pos)))

int is_char_equal(const unsigned char a[8], const unsigned char b[8])
{
	for (int i = 0; i < 8; i++)
	{
		if (a[i] != b[i])
		{
			return 0;
		}
	}
	return 1;
}

void store_character(unsigned char character[8], int slot)
{
	lcd_send_command(CG_RAM_ADDR + slot * 8);
	for (int c = 0; c < 8; c++)
	{
		lcd_send_data(character[c]);
		STORED_CHARMAP[slot][c] = character[c];
	}
}

void render_custom_character(int slot, int row, int column)
{
	lcd_send_command(GET_BASE_ADDRESS(row) + column);
	lcd_send_data(slot);
}

void merge_characters(unsigned char character[8], unsigned char other_character[8])
{
	for (int c = 0; c < 8; c++)
	{
		character[c] |= other_character[c];
	}
	return character;
}

void shift_character_down(unsigned char character[8], unsigned char shifted_character[8])
{
	for (int i = 0; i < 4; i++)
	{
		shifted_character[i] = 0;
	}

	for (int c = 4; c < 8; c++)
	{
		shifted_character[c] = character[c - 4];
	}
}

unsigned int render_character_if_stored(unsigned char character[8], int row, int column)
{
	for (int i = 0; i < CHARMAP_SIZE; i++)
	{
		if (is_char_equal(character, STORED_CHARMAP[i]))
		{
			render_custom_character(i, row, column);
			return 1;
		}
	}
	return 0;
}

static void characters_init()
{
	for (int c = 0; c < CHARMAP_SIZE; ++c)
	{
		lcd_send_command(CG_RAM_ADDR + c * 8);
		for (int r = 0; r < 8; ++r)
		{
			lcd_send_data(CHARMAP[c][r]);
			STORED_CHARMAP[c][r] = CHARMAP[c][r];
		}
	}
}

/* Type definitions */

enum game_state_t
{
	START_SCREEN,
	PLAYING,
	GAME_OVER,
	VICTORY
};

struct player_t
{
	unsigned int sprite;
	unsigned int column;
};

struct enemy_t
{
	unsigned int sprite;
	unsigned int alive;
	unsigned int column;
	int half_row; // There are 4 half rows on the playfield, signed integer!
};

struct bullet_t
{
	unsigned int active;
	unsigned int y_direction; // up or down
	unsigned int column;
	unsigned int y_position;
};

/* Global variables */

#define ENEMY_COUNT 18
#define PLAYFIELD_ROWS 2
#define PLAYFIELD_COLUMNS 16

#define PLAYFIELD_START_X 0
#define PLAYFIELD_END_X 15

// Used for rendering, stores four bits that correspond to 0b<bullet><bottom enemy><top enemy><player>
unsigned int playfield[PLAYFIELD_ROWS][PLAYFIELD_COLUMNS];
// Bit positions for the playfield
#define PLAYER_BIT 0
#define TOP_ENEMY_BIT 1
#define BOTTOM_ENEMY_BIT 2
#define BULLET_BIT 3

unsigned int rerender = 0;
unsigned int dynamic_slot = DYNAMIC_SLOT_START_INDEX;

struct player_t player;
struct bullet_t player_bullet;
struct enemy_t enemies[ENEMY_COUNT];
enum game_state_t game_state = START_SCREEN;
unsigned score = 0;
unsigned enemy_direction = 1;

/* Position management */

void init_positions()
{
	for (int i = 0; i < PLAYFIELD_ROWS; i++)
	{
		for (int j = 0; j < PLAYFIELD_COLUMNS; j++)
		{
			playfield[i][j] = 0b0000; // bullet, bottom enemy, top enemy, player
		}
	}
}

void update_positions()
{
	init_positions();

	playfield[1][player.column] |= 0b0001;

	for (int i = 0; i < ENEMY_COUNT; i++)
	{
		if (enemies[i].alive && enemies[i].half_row >= 0)
		{
			if (enemies[i].half_row % 2 == 0)
			{
				playfield[enemies[i].half_row / 2][enemies[i].column] |= 0b0010;
			}
			else
			{
				playfield[enemies[i].half_row / 2][enemies[i].column] |= 0b0100;
			}
		}
	}

	if (player_bullet.active)
	{
		playfield[player_bullet.y_position / 8][player_bullet.column] |= 0b1000;
	}
}

/* Rendering */

enum render_method_t
{
	PLAYER,
	PLAYER_BULLET,
	DYNAMIC
};

void render_positions()
{
	for (int i = 0; i < PLAYFIELD_ROWS; i++)
	{
		for (int j = PLAYFIELD_START_X; j < PLAYFIELD_COLUMNS; j++)
		{
			if (playfield[i][j] == 0)
			{
				lcd_send_command(GET_BASE_ADDRESS(i) + j);
				lcd_send_data(' ');
			}

			enum render_method_t render_method = DYNAMIC;
			unsigned char char_to_render[8] = {0, 0, 0, 0, 0, 0, 0, 0};

			// Player
			if (i == 1 && CHECK_BIT(playfield[i][j], PLAYER_BIT))
			{
				merge_characters(char_to_render, CHARMAP[player.sprite]);
				render_method = PLAYER;
			}

			// First enemy
			if (CHECK_BIT(playfield[i][j], TOP_ENEMY_BIT))
			{
				// Find which enemy is at this position
				for (int enemy_idx = 0; enemy_idx < ENEMY_COUNT; enemy_idx++)
				{
					if (
						enemies[enemy_idx].alive &&
						enemies[enemy_idx].column == j &&
						enemies[enemy_idx].half_row / 2 == i &&
						enemies[enemy_idx].half_row % 2 == 0) // top half
					{
						merge_characters(char_to_render, CHARMAP[enemies[enemy_idx].sprite]);
						break;
					}
				}
			}

			// Second enemy
			if (CHECK_BIT(playfield[i][j], BOTTOM_ENEMY_BIT))
			{
				// Find which enemy is at this position
				for (int enemy_idx = 0; enemy_idx < ENEMY_COUNT; enemy_idx++)
				{
					if (enemies[enemy_idx].alive &&
						enemies[enemy_idx].column == j &&
						enemies[enemy_idx].half_row / 2 == i &&
						enemies[enemy_idx].half_row % 2 == 1) // bottom half
					{
						unsigned char shifted_character[8];
						shift_character_down(CHARMAP[enemies[enemy_idx].sprite], shifted_character);
						merge_characters(char_to_render, shifted_character);

						break;
					}
				}
			}

			// Bullet
			if (CHECK_BIT(playfield[i][j], BULLET_BIT))
			{
				if (player_bullet.active && player_bullet.y_position != 8) // Workaround to skip the render on row change (would cause an annoying artifact)
				{
					int bullet_row = player_bullet.y_position % 8;
					unsigned char bullet_map[8] = {0, 0, 0, 0, 0, 0, 0, 0};
					bullet_map[bullet_row] = 0b00100;

					merge_characters(char_to_render, bullet_map);
					render_method = PLAYER_BULLET;
				}
			}

			// Render character if it's already stored
			if (render_character_if_stored(char_to_render, i, j))
			{
				continue;
			}

			// Prevent dynamic slot from being out of bounds and start on the first dynamic index
			if (dynamic_slot == CHARMAP_SIZE)
			{
				dynamic_slot = DYNAMIC_SLOT_START_INDEX;
			}

			switch (render_method)
			{
			case PLAYER:
				store_character(char_to_render, PLAYER_SLOT);
				render_custom_character(PLAYER_SLOT, i, j);
				break;
			case PLAYER_BULLET:
				store_character(char_to_render, PLAYER_BULLET_SLOT);
				render_custom_character(PLAYER_BULLET_SLOT, i, j);
				break;
			default:
				store_character(char_to_render, dynamic_slot);
				render_custom_character(dynamic_slot, i, j);
				dynamic_slot++;
				break;
			}
		}
	}
}

/* Player */

void player_init()
{
	player.sprite = PLAYER_SPRITE;
	player.column = 7;
}

void handle_input(int btn)
{
	// Movement
	if (btn == BUTTON_LEFT && player.column > PLAYFIELD_START_X)
	{
		move_player(-1);
	}
	else if (btn == BUTTON_RIGHT && player.column < PLAYFIELD_END_X)
	{
		move_player(1);
	}

	// Shooting
	if (btn == BUTTON_UP)
	{
		player_shoot();
	}
}

void move_player(int direction)
{
	player.column += direction;
	rerender = 1;
}

/* Bullet */

void player_bullet_init()
{
	player_bullet.active = 0;
}

void player_shoot()
{
	if (player_bullet.active)
		return;

	player_bullet.active = 1;
	player_bullet.column = player.column;
	player_bullet.y_position = 13; // Directly above player
	player_bullet.y_direction = -1;
}

unsigned int check_enemy_hit()
{
	for (int i = 0; i < ENEMY_COUNT; i++)
	{
		if (enemies[i].alive && enemies[i].column == player_bullet.column && enemies[i].half_row * 2 == player_bullet.y_position / 2)
		{
			enemy_killed(&enemies[i]);
			player_bullet.active = 0;
			return 1;
		}
	}
	return 0;
}

void player_bullets_move()
{
	if (player_bullet.active)
	{
		rerender = 1;

		if (check_enemy_hit())
		{
			return;
		}

		// Bullet is going offscreen
		if (player_bullet.y_position == 0)
		{
			player_bullet.active = 0;
			return;
		}

		// Bullet going from second to first row
		if (player_bullet.y_position > 0 && player_bullet.y_position % 8 == 0)
		{
			player_bullet.y_position = 8;
		}

		player_bullet.y_position += player_bullet.y_direction;
	}
}

/* Enemies */
void enemies_init()
{
	for (int i = 0; i < ENEMY_COUNT; i++)
	{
		enemies[i].alive = 1;
		enemies[i].sprite = (i % 3) + 1;
		enemies[i].column = (i % (ENEMY_COUNT / 2)) + PLAYFIELD_START_X;
		enemies[i].half_row = i / (ENEMY_COUNT / 2);
	}
}

void enemies_move()
{
	unsigned int wrap = 0;
	for (int i = 0; i < ENEMY_COUNT / 2; i++)
	{
		int j = ENEMY_COUNT / 2 - (i + 1); // Here to find the enemy that causes wrapping faster by scanning from both edges

		if (enemies[i].alive && ((enemy_direction == -1 && enemies[i].column == PLAYFIELD_START_X) ||
								 (enemy_direction == 1 && enemies[i].column == PLAYFIELD_END_X)))
		{
			wrap = 1;
			break;
		}

		if (enemies[j].alive && ((enemy_direction == -1 && enemies[j].column == PLAYFIELD_START_X) ||
								 (enemy_direction == 1 && enemies[j].column == PLAYFIELD_END_X)))
		{
			wrap = 1;
			break;
		}
	}

	if (wrap)
	{
		for (int j = 0; j < ENEMY_COUNT; j++)
		{
			enemies[j].half_row += 1;
			check_for_game_over(&enemies[j]);
		}
		enemy_direction *= -1;
	}
	else
	{
		for (int j = 0; j < ENEMY_COUNT; j++)
		{
			enemies[j].column += enemy_direction;
			check_for_game_over(&enemies[j]);
		}
	}

	rerender = 1;
}

void enemy_killed(struct enemy_t *enemy)
{
	enemy->alive = 0;
	score++;
	if (score == ENEMY_COUNT)
	{
		game_state = VICTORY;
	}
}

/* Game state */

void game_state_init()
{
	game_state = PLAYING;
	score = 0;
	enemy_direction = 1;
}

void check_for_game_over(struct enemy_t *enemy)
{
	if (enemy->alive && enemy->half_row + 1 == PLAYFIELD_ROWS * 2)
	{
		game_state = GAME_OVER;
	}
}

/* Start screen */
void render_start_screen()
{
	lcd_send_command(DD_RAM_ADDR);
	lcd_send_text("[Space Invaders]");
	lcd_send_line2("       ");
	lcd_send_data(PLAYER_SPRITE);
}

/* Initialize the game */
void init_game()
{
	game_state_init();
	player_init();
	player_bullet_init();
	enemies_init();
	update_positions(); // also initalizes the positions
	render_positions();
}

int main()
{
	port_init();
	lcd_init();
	characters_init();

	render_start_screen();

	while (1) // Program loop
	{
		while (button_pressed() != BUTTON_CENTER)
		{
			button_unlock();
		}

		init_game();
		long int delay = 0;

		while (1) // Game loop
		{
			if (delay == 300000)
			{
				enemies_move();
				delay = 0;
			}
			delay++;

			handle_input(button_pressed());
			player_bullets_move();

			if (rerender)
			{
				update_positions();
				render_positions();
				rerender = 0;
			}

			if (game_state == GAME_OVER || game_state == VICTORY)
			{
				break;
			}

			button_unlock();
		}

		if (game_state == GAME_OVER)
		{
			lcd_send_line1("    GAME OVER");
		}
		else if (game_state == VICTORY)
		{
			lcd_send_line1("    VICTORY");
		}
	}
}
