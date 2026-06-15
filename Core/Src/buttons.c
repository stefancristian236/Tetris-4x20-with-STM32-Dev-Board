#include "buttons.h"
#include "stm32f1xx_hal.h"
#include <stdint.h>

typedef enum {
    BTN_IDLE,
    BTN_DEBOUNCING,
    BTN_FIRED
} BtnPhase;

typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
    BtnPhase phase;
    uint32_t t_start;
} Btn;

static Btn B[4] =
{
    { BUTT_LEFT_PORT,   BUTT_LEFT_PIN,   BTN_IDLE, 0 },
    { BUTT_RIGHT_PORT,  BUTT_RIGHT_PIN,  BTN_IDLE, 0 },
    { BUTT_ROTATE_PORT, BUTT_ROTATE_PIN, BTN_IDLE, 0 },
    { BUTT_DROP_PORT,   BUTT_DROP_PIN,   BTN_IDLE, 0 }
};

void butt_Init(void)
{
    for (int i = 0; i < 4; i++)
    {
        B[i].phase = BTN_IDLE;
        B[i].t_start = 0;
    }
}

static uint8_t poll(Btn *b)
{
    if (b == NULL) return 0;

    uint8_t pressed = (HAL_GPIO_ReadPin(b->port, b->pin) == GPIO_PIN_RESET);
    uint32_t now = HAL_GetTick();

    switch (b->phase)
    {
        case BTN_IDLE:
            if (pressed)
            {
                b->phase = BTN_DEBOUNCING;
                b->t_start = now;
            }
            break;

        case BTN_DEBOUNCING:
            if (!pressed)
            {
                b->phase = BTN_IDLE;
            }
            else if ((now - b->t_start) >= DEBOUNCE_MS)
            {
                b->phase = BTN_FIRED;
                return 1;
            }
            break;

        case BTN_FIRED:
            if (!pressed)
            {
                b->phase = BTN_IDLE;
            }
            break;
    }

    return 0;
}

uint8_t read_butt_Left(void)   { return poll(&B[0]); }
uint8_t read_butt_Right(void)  { return poll(&B[1]); }
uint8_t read_butt_Rotate(void) { return poll(&B[2]); }
uint8_t read_butt_Drop(void)   { return poll(&B[3]); }