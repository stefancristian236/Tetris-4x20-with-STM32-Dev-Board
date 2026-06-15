#include "tetris.h"
#include "i2c-lcd.h" 
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "stm32f1xx_hal.h"

extern UART_HandleTypeDef huart1; 

uint8_t board[BOARD_H][BOARD_W];
uint32_t last_fall_time = 0;

int8_t piece_x, piece_y, piece_type, piece_rot;

const uint16_t SHAPES[7][4] = {
    {0x0F00, 0x2222, 0x00F0, 0x4444}, // I
    {0x44C0, 0x8E00, 0x6440, 0x0E20}, // J
    {0x4460, 0x0E80, 0xC440, 0x2E00}, // L
    {0xCC00, 0xCC00, 0xCC00, 0xCC00}, // O
    {0x06C0, 0x8C40, 0x6C00, 0x4620}, // S
    {0x0E40, 0x4C40, 0x4E00, 0x4640}, // T
    {0x0C60, 0x4C80, 0xC600, 0x2640}  // Z
};

bool get_shape_cell(int type, int rot, int x, int y) {
    return (SHAPES[type][rot] & (1 << (15 - (y * 4 + x)))) != 0;
}

bool check_collision(int px, int py, int rot) {
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

void spawn_piece(void) {
    piece_type = rand() % 7;
    piece_rot = 0;
    piece_x = BOARD_W / 2 - 2;
    piece_y = -2;
    if (check_collision(piece_x, piece_y, piece_rot)) {
        memset(board, 0, sizeof(board)); 
    }
}

void lock_piece(void) {
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            if (get_shape_cell(piece_type, piece_rot, x, y) && (piece_y + y >= 0)) {
                board[piece_y + y][piece_x + x] = 1;
            }
        }
    }
    for (int y = BOARD_H - 1; y >= 0; y--) {
        bool full = true;
        for (int x = 0; x < BOARD_W; x++) {
            if (!board[y][x]) { full = false; break; }
        }
        if (full) {
            for (int r = y; r > 0; r--) memcpy(board[r], board[r - 1], BOARD_W);
            memset(board[0], 0, BOARD_W);
            y++; 
        }
    }
    spawn_piece();
}

void Tetris_Init(void) {
    memset(board, 0, sizeof(board));
    spawn_piece();
}

void Tetris_Update(TetrisCommand cmd) {
    switch (cmd) {
        case CMD_LEFT:  if (!check_collision(piece_x - 1, piece_y, piece_rot)) piece_x--; break;
        case CMD_RIGHT: if (!check_collision(piece_x + 1, piece_y, piece_rot)) piece_x++; break;
        case CMD_DOWN:  
            if (!check_collision(piece_x, piece_y + 1, piece_rot)) {
                piece_y++; 
            } else {
                lock_piece();
            }
            break;
        case CMD_ROTATE: 
            if (!check_collision(piece_x, piece_y, (piece_rot + 1) % 4)) 
                piece_rot = (piece_rot + 1) % 4; 
            break;
        case CMD_NONE: default: break;
    }
}

void Tetris_DrawTerminal(void) {
    char buf[1024] = "\033[2J\033[H"; 
    strcat(buf, "TETRIS 4x20 STM32\r\n<!========!>\r\n"); 
    
    for (int y = 0; y < BOARD_H; y++) {
        strcat(buf, "<!");
        for (int x = 0; x < BOARD_W; x++) {
            bool is_piece = false;
            if (x >= piece_x && x < piece_x + 4 && y >= piece_y && y < piece_y + 4) {
                is_piece = get_shape_cell(piece_type, piece_rot, x - piece_x, y - piece_y);
            }
            strcat(buf, (board[y][x] || is_piece) ? "[]" : "  ");
        }
        strcat(buf, "!>\r\n");
    }
    strcat(buf, "<!========!>\r\n");
    
    HAL_UART_Transmit(&huart1, (uint8_t*)buf, strlen(buf), 100);
}

bool get_pixel(int x, int y) {
    if (x < 0 || x >= BOARD_W || y < 0 || y >= BOARD_H) return false;
    if (board[y][x]) return true; 
    
    if (x >= piece_x && x < piece_x + 4 && y >= piece_y && y < piece_y + 4) {
        return get_shape_cell(piece_type, piece_rot, x - piece_x, y - piece_y);
    }
    return false;
}

void Tetris_DrawLCD(void) {
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