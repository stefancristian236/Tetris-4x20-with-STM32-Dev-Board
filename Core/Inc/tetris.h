#ifndef TETRIS_H
#define TETRIS_H

#include <stdint.h>
#include <stdbool.h>
 
#define BOARD_W 8
#define BOARD_H 20

typedef enum {
    CMD_NONE = 0,
    CMD_LEFT,
    CMD_RIGHT,
    CMD_DOWN,
    CMD_ROTATE,
    CMD_HARD_DROP,
    CMD_FALL,
    CMD_PAUSE,
    CMD_RESTART
} TetrisCommand;

typedef struct {
    uint32_t score;
    uint16_t lines;
    uint8_t level;
    uint8_t next_piece;
    bool paused;
    bool game_over;
} TetrisStats;

void Tetris_Init(void);
void Tetris_Update(TetrisCommand cmd);
void Tetris_DrawTerminal(void);
void Tetris_DrawLCD(void);
void Tetris_GetStats(TetrisStats *stats);
uint32_t Tetris_GetFallDelay(void);
bool Tetris_IsPaused(void);
bool Tetris_IsGameOver(void);

extern uint32_t last_fall_time;
#define FALL_DELAY_MS 650U
#define MIN_FALL_DELAY_MS 120U

#endif
