#ifndef BUTTONS_H
#define BUTTONS_H

#include <stm32f1xx_hal.h>
#include <stdint.h>

#define BTN_PORT GPIOA
#define BTN_LEFT_PIN GPIO_PIN_1
#define BTN_RIGHT_PIN GPIO_PIN_2
#define BTN_ROTATE_PIN GPIO_PIN_3
#define BTN_DROP_PIN GPIO_PIN_4

void butt_Init(void);
uint8_t read_butt_Left(void);
uint8_t read_butt_Rotate(void);
uint8_t read_butt_Right(void);
uint8_t read_butt_Drop(void);

#endif