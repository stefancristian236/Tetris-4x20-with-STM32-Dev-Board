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
    uint32_t t_repeat;
    uint16_t repeat_ms;
    uint8_t repeating;
} Btn;

static Btn B[4] =
{
    { BUTT_LEFT_PORT,   BUTT_LEFT_PIN,   BTN_IDLE, 0, 0, BUTTON_REPEAT_MS, 0 },
    { BUTT_RIGHT_PORT,  BUTT_RIGHT_PIN,  BTN_IDLE, 0, 0, BUTTON_REPEAT_MS, 0 },
    { BUTT_ROTATE_PORT, BUTT_ROTATE_PIN, BTN_IDLE, 0, 0, 0, 0 },
    { BUTT_DROP_PORT,   BUTT_DROP_PIN,   BTN_IDLE, 0, 0, BUTTON_DROP_REPEAT_MS, 0 }
};

void butt_Init(void)
{
    for (int i = 0; i < 4; i++)
    {
        B[i].phase = BTN_IDLE;
        B[i].t_start = 0;
        B[i].t_repeat = 0;
        B[i].repeating = 0;
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
                b->repeating = 0;
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
                b->t_repeat = now;
                b->repeating = 0;
                return 1;
            }
            break;

        case BTN_FIRED:
            if (!pressed)
            {
                b->phase = BTN_IDLE;
                b->repeating = 0;
            }
            else if (b->repeat_ms > 0)
            {
                uint32_t wait_ms = b->repeating ? b->repeat_ms : BUTTON_REPEAT_START_MS;

                if ((now - b->t_repeat) >= wait_ms)
                {
                    b->t_repeat = now;
                    b->repeating = 1;
                    return 1;
                }
            }
            break;
    }

    return 0;
}

uint8_t read_butt_Left(void)   { return poll(&B[0]); }
uint8_t read_butt_Right(void)  { return poll(&B[1]); }
uint8_t read_butt_Rotate(void) { return poll(&B[2]); }
uint8_t read_butt_Drop(void)   { return poll(&B[3]); }
