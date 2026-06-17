#ifndef BUTTONS_H
#define BUTTONS_H

#include "stm32f1xx_hal.h"
#include <stdint.h>

#define BUTT_LEFT_PORT    GPIOA
#define BUTT_LEFT_PIN     GPIO_PIN_4

#define BUTT_RIGHT_PORT   GPIOA
#define BUTT_RIGHT_PIN    GPIO_PIN_1

#define BUTT_ROTATE_PORT  GPIOA
#define BUTT_ROTATE_PIN   GPIO_PIN_3

#define BUTT_DROP_PORT    GPIOA
#define BUTT_DROP_PIN     GPIO_PIN_2

#define DEBOUNCE_MS 20U
#define BUTTON_REPEAT_START_MS 220U
#define BUTTON_REPEAT_MS 90U
#define BUTTON_DROP_REPEAT_MS 55U

void butt_Init(void);

uint8_t read_butt_Left(void);
uint8_t read_butt_Right(void);
uint8_t read_butt_Rotate(void);
uint8_t read_butt_Drop(void);

#endif
