#include "tetris.h"
#include "i2c-lcd.h" 
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "stm32f1xx_hal.h"

extern UART_HandleTypeDef huart1; 

#define PIECE_COUNT 7U
#define LEVEL_LINES 5U
#define SOFT_DROP_POINTS 1U
#define HARD_DROP_POINTS 2U

static uint8_t board[BOARD_H][BOARD_W];
uint32_t last_fall_time = 0;

static int8_t piece_x, piece_y, piece_type, piece_rot;
static uint8_t piece_bag[PIECE_COUNT];
static uint8_t bag_index = PIECE_COUNT;
static uint8_t next_piece = 0;
static uint32_t score = 0;
static uint16_t lines = 0;
static uint8_t level = 1;
static bool paused = false;
static bool game_over = false;

static const uint16_t SHAPES[7][4] = {
    {0x0F00, 0x2222, 0x00F0, 0x4444}, // I
    {0x44C0, 0x8E00, 0x6440, 0x0E20}, // J
    {0x4460, 0x0E80, 0xC440, 0x2E00}, // L
    {0xCC00, 0xCC00, 0xCC00, 0xCC00}, // O
    {0x06C0, 0x8C40, 0x6C00, 0x4620}, // S
    {0x0E40, 0x4C40, 0x4E00, 0x4640}, // T
    {0x0C60, 0x4C80, 0xC600, 0x2640}  // Z
};

static bool get_shape_cell(int type, int rot, int x, int y) {
    return (SHAPES[type][rot] & (1 << (15 - (y * 4 + x)))) != 0;
}

static void refill_bag(void) {
    for (uint8_t i = 0; i < PIECE_COUNT; i++) {
        piece_bag[i] = i;
    }

    for (int i = PIECE_COUNT - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        uint8_t tmp = piece_bag[i];
        piece_bag[i] = piece_bag[j];
        piece_bag[j] = tmp;
    }

    bag_index = 0;
}

static uint8_t draw_from_bag(void) {
    if (bag_index >= PIECE_COUNT) {
        refill_bag();
    }

    return piece_bag[bag_index++];
}

static bool check_collision(int px, int py, int rot) {
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            if (get_shape_cell(piece_type, rot, x, y)) {
                int nx = px + x;
                int ny = py + y;
                if (nx < 0 || nx >= BOARD_W || ny >= BOARD_H) return true; 
                if (ny >= 0 && board[ny][nx]) return true;                 
            }
        }
    }
    return false;
}

static void spawn_piece(void) {
    piece_type = (int8_t)next_piece;
    next_piece = draw_from_bag();
    piece_rot = 0;
    piece_x = BOARD_W / 2 - 2;
    piece_y = -2;

    if (check_collision(piece_x, piece_y, piece_rot)) {
        game_over = true;
    }
}

static uint8_t clear_full_lines(void) {
    uint8_t cleared = 0;

    for (int y = BOARD_H - 1; y >= 0; y--) {
        bool full = true;

        for (int x = 0; x < BOARD_W; x++) {
            if (!board[y][x]) {
                full = false;
                break;
            }
        }

        if (full) {
            for (int r = y; r > 0; r--) {
                memcpy(board[r], board[r - 1], BOARD_W);
            }
            memset(board[0], 0, BOARD_W);
            cleared++;
            y++;
        }
    }

    return cleared;
}

static void award_line_score(uint8_t cleared) {
    static const uint16_t line_scores[5] = { 0, 100, 300, 500, 800 };

    if (cleared == 0) {
        return;
    }

    lines += cleared;
    score += (uint32_t)line_scores[cleared] * level;

    uint16_t new_level = 1U + (lines / LEVEL_LINES);
    level = (new_level > 99U) ? 99U : (uint8_t)new_level;
}

static void lock_piece(void) {
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            if (get_shape_cell(piece_type, piece_rot, x, y) && (piece_y + y >= 0)) {
                board[piece_y + y][piece_x + x] = 1;
            }
        }
    }

    award_line_score(clear_full_lines());
    spawn_piece();
}

static bool move_piece(int8_t dx, int8_t dy) {
    if (!check_collision(piece_x + dx, piece_y + dy, piece_rot)) {
        piece_x += dx;
        piece_y += dy;
        return true;
    }

    return false;
}

static bool rotate_piece(void) {
    static const int8_t kicks[] = { 0, -1, 1, -2, 2 };
    int8_t next_rot = (piece_rot + 1) % 4;

    for (uint8_t i = 0; i < sizeof(kicks) / sizeof(kicks[0]); i++) {
        if (!check_collision(piece_x + kicks[i], piece_y, next_rot)) {
            piece_x += kicks[i];
            piece_rot = next_rot;
            return true;
        }
    }

    return false;
}

static uint8_t drop_distance(void) {
    uint8_t distance = 0;

    while (!check_collision(piece_x, piece_y + distance + 1, piece_rot)) {
        distance++;
    }

    return distance;
}

static bool active_piece_cell(int x, int y) {
    if (game_over) {
        return false;
    }

    if (x >= piece_x && x < piece_x + 4 && y >= piece_y && y < piece_y + 4) {
        return get_shape_cell(piece_type, piece_rot, x - piece_x, y - piece_y);
    }

    return false;
}



static void lcd_write_line(uint8_t row, const char *text) {
    lcd_put_cur(row, 0);

    for (uint8_t i = 0; i < 20; i++) {
        char ch = (text[i] != '\0') ? text[i] : ' ';
        lcd_send_data(ch);
        if (text[i] == '\0') {
            for (i = i + 1; i < 20; i++) {
                lcd_send_data(' ');
            }
            break;
        }
    }
}

void Tetris_Init(void) {
    memset(board, 0, sizeof(board));
    srand((unsigned int)(HAL_GetTick() ^ 0xA5A5U));
    bag_index = PIECE_COUNT;
    next_piece = draw_from_bag();
    score = 0;
    lines = 0;
    level = 1;
    paused = false;
    game_over = false;
    last_fall_time = HAL_GetTick();
    spawn_piece();
}

void Tetris_Update(TetrisCommand cmd) {
    if (cmd == CMD_RESTART) {
        Tetris_Init();
        return;
    }

    if (cmd == CMD_PAUSE) {
        if (!game_over) {
            paused = !paused;
        }
        return;
    }

    if (game_over || paused) {
        return;
    }

    switch (cmd) {
        case CMD_LEFT:
            move_piece(-1, 0);
            break;
        case CMD_RIGHT:
            move_piece(1, 0);
            break;
        case CMD_DOWN:  
            if (move_piece(0, 1)) {
                score += SOFT_DROP_POINTS;
            } else {
                lock_piece();
            }
            break;
        case CMD_FALL:
            if (!move_piece(0, 1)) {
                lock_piece();
            }
            break;
        case CMD_ROTATE: 
            rotate_piece();
            break;
        case CMD_HARD_DROP: {
            uint8_t distance = drop_distance();
            piece_y += distance;
            score += (uint32_t)distance * HARD_DROP_POINTS;
            lock_piece();
            break;
        }
        case CMD_NONE: default: break;
    }
}

void Tetris_GetStats(TetrisStats *stats) {
    if (stats == NULL) {
        return;
    }

    stats->score = score;
    stats->lines = lines;
    stats->level = level;
    stats->next_piece = next_piece;
    stats->paused = paused;
    stats->game_over = game_over;
}

uint32_t Tetris_GetFallDelay(void) {
    uint32_t reduction = (uint32_t)(level - 1U) * 45U;

    if (reduction >= (FALL_DELAY_MS - MIN_FALL_DELAY_MS)) {
        return MIN_FALL_DELAY_MS;
    }

    return FALL_DELAY_MS - reduction;
}

bool Tetris_IsPaused(void) {
    return paused;
}

bool Tetris_IsGameOver(void) {
    return game_over;
}

static bool get_pixel(int x, int y) {
    if (x < 0 || x >= BOARD_W || y < 0 || y >= BOARD_H) return false;
    if (board[y][x]) return true; 

    return active_piece_cell(x, y);
}

void Tetris_DrawLCD(void) {
    if (game_over) {
        char text[32];
        lcd_write_line(0, "     GAME OVER      ");
        snprintf(text, sizeof(text), "Scor %lu", (unsigned long)score);
        lcd_write_line(1, text);
        snprintf(text, sizeof(text), "Linie %u Nivel %u", (unsigned)lines, (unsigned)level);
        lcd_write_line(2, text);
        lcd_write_line(3, " Apasa RST  ");
        return;
    }

    for (int r = 0; r < 4; r++) {           
        lcd_put_cur(r, 0);                  
        for (int c = 0; c < 20; c++) {      
            
            int board_x = r * 2;            
            int board_y = c;                
            
            bool top_block = get_pixel(board_x, board_y);
            bool bot_block = get_pixel(board_x + 1, board_y);
            
            if (top_block && !bot_block) {
                lcd_send_data(1);          
            } else if (!top_block && bot_block) {
                lcd_send_data(2);           
            } else if (top_block && bot_block) {
                lcd_send_data(3);           
            } else {
                lcd_send_data(' ');        
            }
        }
    }
}
