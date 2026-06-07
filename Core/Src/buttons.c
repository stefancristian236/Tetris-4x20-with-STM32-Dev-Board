#include "buttons.h"

static uint32_t tick_left = 0;
static uint32_t tick_right = 0;
static uint32_t rotate = 0;
static uint32_t drop = 0;
static uint8_t rotate_lock = 0;

void butt_Init(void) {

}

//buttons set on active low
//not press = 0, press = 1
//keeps updating the movemen as we hold the button down
uint8_t read_butt_Left(void) {
    if (HAL_GPIO_ReadPin(BTN_PORT, BTN_LEFT_PIN) == GPIO_PIN_RESET) {
        if ((HAL_GetTick() - tick_left) > 100) {
            tick_left = HAL_GetTick();
            return 1;
        }
    }
}

uint8_t read_butt_Right(void) {
    if (HAL_GPIO_ReadPin(BTN_PORT, BTN_RIGHT_PIN) == GPIO_PIN_RESET) {
        if ((HAL_GetTick() - tick_right) > 100) {
            tick_right = HAL_GetTick();
            return 1;
        }
    }
}

uint8_t read_butt_Drop(void) {
    if (HAL_GPIO_ReadPin(BTN_PORT, BTN_DROP_PIN) == GPIO_PIN_RESET) {
        if ((HAL_GetTick() - drop) > 50) {
            drop = HAL_GetTick();
            return 1;
        }
    }
}

//single push button
//no need to continue the movement while is being pressed
uint8_t read_butt_Rotate(void) {
    if (HAL_GPIO_ReadPin(BTN_PORT, BTN_ROTATE_PIN) == GPIO_PIN_RESET) {
        if (!rotate_lock && (HAL_GetTick() - rotate) > 50) {
            rotate_lock = 1;
            rotate = HAL_GetTick();
            return 1;
        }
    } else {
        rotate_lock = 0;
    }
    return 0;
}