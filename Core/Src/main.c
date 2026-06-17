/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"
#include "tetris.h"
#include "buttons.h"
#include "i2c-lcd.h"   
#include <string.h>
#include <stdio.h>

I2C_HandleTypeDef hi2c1;
TIM_HandleTypeDef htim2;
UART_HandleTypeDef huart1;

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_TIM2_Init(void);
static void MX_I2C1_Init(void);
void Error_Handler(void);

void load_custom_chars(void) {
    uint8_t top[8]  = {0x1F, 0x1F, 0x1F, 0x1F, 0x00, 0x00, 0x00, 0x00};
    uint8_t bot[8]  = {0x00, 0x00, 0x00, 0x00, 0x1F, 0x1F, 0x1F, 0x1F};
    uint8_t full[8] = {0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F};

    lcd_send_cmd(0x40 + (1 * 8)); for(int i=0; i<8; i++) lcd_send_data(top[i]);
    lcd_send_cmd(0x40 + (2 * 8)); for(int i=0; i<8; i++) lcd_send_data(bot[i]);
    lcd_send_cmd(0x40 + (3 * 8)); for(int i=0; i<8; i++) lcd_send_data(full[i]);
}

static TetrisCommand Read_Inputs(void) {
    if (read_butt_Left()) return CMD_LEFT;
    if (read_butt_Right()) return CMD_RIGHT;
    if (read_butt_Rotate()) return CMD_ROTATE;
    if (read_butt_Drop()) return CMD_DOWN;
    
    return CMD_NONE;
}

uint8_t rx_data;
static volatile TetrisCommand pending_uart_cmd = CMD_NONE;

static TetrisCommand Command_From_UART(uint8_t ch) {
    switch (ch) {
        case 'a':
        case 'A':
            return CMD_LEFT;
        case 'd':
        case 'D':
            return CMD_RIGHT;
        case 'w':
        case 'W':
            return CMD_ROTATE;
        case 's':
        case 'S':
            return CMD_DOWN;
        case 'r':
        case 'R':
            return CMD_RESTART;
        default:
            return CMD_NONE;
    }
}

static TetrisCommand Pop_UART_Command(void) {
    TetrisCommand cmd;

    __disable_irq();
    cmd = pending_uart_cmd;
    pending_uart_cmd = CMD_NONE;
    __enable_irq();

    return cmd;
}

static void Apply_Command(TetrisCommand cmd, uint8_t *needs_draw) {
    if (cmd == CMD_NONE) {
        return;
    }

    Tetris_Update(cmd);
    *needs_draw = 1;

    if (cmd == CMD_DOWN || cmd == CMD_HARD_DROP ||
        cmd == CMD_PAUSE || cmd == CMD_RESTART) {
        last_fall_time = HAL_GetTick();
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART1) {
        TetrisCommand cmd = Command_From_UART(rx_data);

        if (cmd != CMD_NONE) {
            pending_uart_cmd = cmd;
        }

        HAL_UART_Receive_IT(&huart1, &rx_data, 1);
    }
}

int main(void)
{
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  butt_Init();
  MX_USART1_UART_Init(); 
  MX_I2C1_Init();        
  MX_TIM2_Init();

  lcd_init();          
  lcd_clear();
  load_custom_chars();  
  
  Tetris_Init();        
  HAL_UART_Receive_IT(&huart1, &rx_data, 1);
  Tetris_DrawLCD();

  while (1)
  {
    static uint32_t last_led_toggle = 0;
    uint8_t needs_draw = 0;
    uint32_t now = HAL_GetTick();

    if ((now - last_led_toggle) >= 500U) {
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
        last_led_toggle = now;
    }

    Apply_Command(Read_Inputs(), &needs_draw);
    Apply_Command(Pop_UART_Command(), &needs_draw);

    if (!Tetris_IsPaused() && !Tetris_IsGameOver() &&
        (now - last_fall_time) >= Tetris_GetFallDelay()) {
        Tetris_Update(CMD_FALL);
        needs_draw = 1;
        last_fall_time = now;
    }

    if (needs_draw) {
        Tetris_DrawLCD();
    }

    HAL_Delay(10);
  }
}

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef     RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef     RCC_ClkInitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInit   = {0};

    RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState            = RCC_HSE_ON;
    RCC_OscInitStruct.HSEPredivValue      = RCC_HSE_PREDIV_DIV1;
    RCC_OscInitStruct.HSIState            = RCC_HSI_ON;
    RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource       = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLMUL          = RCC_PLL_MUL9;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) Error_Handler();

    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                     | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) Error_Handler();

    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USB;
    PeriphClkInit.UsbClockSelection    = RCC_USBCLKSOURCE_PLL_DIV1_5;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) Error_Handler();
}

static void MX_I2C1_Init(void)
{
    hi2c1.Instance             = I2C1;
    hi2c1.Init.ClockSpeed      = 100000;
    hi2c1.Init.DutyCycle       = I2C_DUTYCYCLE_2;
    hi2c1.Init.OwnAddress1     = 0;
    hi2c1.Init.AddressingMode  = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2     = 0;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode   = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c1) != HAL_OK) Error_Handler();
}

static void MX_TIM2_Init(void)
{
    TIM_ClockConfigTypeDef  sClockSourceConfig = {0};
    TIM_MasterConfigTypeDef sMasterConfig      = {0};
    TIM_OC_InitTypeDef      sConfigOC          = {0};

    htim2.Instance               = TIM2;
    htim2.Init.Prescaler         = 7199;   
    htim2.Init.CounterMode       = TIM_COUNTERMODE_UP;
    htim2.Init.Period            = 99;    
    htim2.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&htim2) != HAL_OK) Error_Handler();

    sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
    if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK) Error_Handler();

    if (HAL_TIM_OC_Init(&htim2) != HAL_OK) Error_Handler();

    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterSlaveMode     = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK) Error_Handler();

    sConfigOC.OCMode     = TIM_OCMODE_TIMING;
    sConfigOC.Pulse      = 0;
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
    if (HAL_TIM_OC_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_1) != HAL_OK) Error_Handler();

    HAL_TIM_MspPostInit(&htim2);
}

static void MX_USART1_UART_Init(void)
{
    huart1.Instance          = USART1;
    huart1.Init.BaudRate     = 115200;
    huart1.Init.WordLength   = UART_WORDLENGTH_8B;
    huart1.Init.StopBits     = UART_STOPBITS_1;
    huart1.Init.Parity       = UART_PARITY_NONE;
    huart1.Init.Mode         = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart1) != HAL_OK) Error_Handler();
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0 | GPIO_PIN_1, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET);

    GPIO_InitStruct.Pin   = GPIO_PIN_13;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP; 
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    GPIO_InitStruct.Pin  = GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin   = GPIO_PIN_0 | GPIO_PIN_1;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_InitStruct.Pin   = GPIO_PIN_8;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) {}
}
