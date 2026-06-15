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
    CMD_ROTATE
} TetrisCommand;

void Tetris_Init(void);
void Tetris_Update(TetrisCommand cmd);
void Tetris_DrawTerminal(void);
void Tetris_DrawLCD(void);

extern uint32_t last_fall_time;
#define FALL_DELAY_MS 500

#endif