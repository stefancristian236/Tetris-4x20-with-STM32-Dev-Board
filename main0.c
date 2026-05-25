/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body — RGB LED via PWM (TIM3) + State Machine (TIM2)
  ******************************************************************************
  * HARDWARE MAP (STM32F103):
  *   Red   LED → PB0  (TIM3_CH3)
  *   Green LED → PB1  (TIM3_CH4)
  *   Blue  LED → PA8  (GPIO, TIM1_CH1 requires advanced timer — kept as GPIO)
  *   Button    → PC13 (INPUT PULLUP)
  *   UART      → USART1
  *
  * PWM period = 99 → duty cycle 0–99 maps directly to 0–99%
  * TIM2 → base timer interrupt for state machine (period elapsed callback)
  * TIM3 → PWM generation for Red + Green channels
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "usb_device.h"

/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define SW_VERSION  10   // version 1.0
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
TIM_HandleTypeDef htim2;   // Base timer — period elapsed interrupt (state machine)
TIM_HandleTypeDef htim3;   // PWM timer — RGB LED channels

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
uint8_t uart_buf[1];
uint8_t ticks;
uint8_t SM_State;
uint8_t User_B_Pressed = 0;

/* RGB duty cycles (0–99) */
uint8_t pwm_red   = 0;
uint8_t pwm_green = 0;
uint8_t pwm_blue  = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM3_Init(void);   // NEW: PWM timer

/* USER CODE BEGIN PFP */
void RGB_SetColor(uint8_t red, uint8_t green, uint8_t blue);
/* USER CODE END PFP */

/* USER CODE BEGIN 0 */

/* ---- printf redirect to UART -------------------------------------------- */
#ifdef __GNUC__
#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
#define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif

PUTCHAR_PROTOTYPE
{
  HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 0xFFFF);
  return ch;
}

int _write(int fd, char *ptr, int len)
{
  HAL_UART_Transmit(&huart1, (uint8_t *)ptr, len, HAL_MAX_DELAY);
  return len;
}

/* ---- UART RX callback ---------------------------------------------------- */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (uart_buf[0] != '?')
  {
    uart_buf[0]++;
    HAL_UART_Transmit(&huart1, uart_buf, 1, 10);
  }
  else
  {
    printf("Sw Version %d.%d\r\n", SW_VERSION / 10, SW_VERSION % 10);
  }
  HAL_UART_Receive_IT(&huart1, uart_buf, 1);
}

/* ---- TIM2 Period Elapsed callback (state machine tick) ------------------- */
/*
 * FIX 1: Parameter renamed from *htim2 to *htim — avoids shadowing the global.
 * FIX 2: PWM start/set REMOVED from here — PWM is set once in main, not on every tick.
 *         This callback is only for state machine / blink logic.
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM2)
  {
    ticks++;
    /* Add periodic blink or animation logic here if needed */
    /* Example: toggle blue LED (GPIO) on each tick        */
    HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_8);
  }
}

/* ---- Helper: set RGB color via PWM --------------------------------------- */
/*
 * red, green, blue: 0–99 (maps to 0%–99% duty cycle)
 * Red   → TIM3_CH3 (PB0)
 * Green → TIM3_CH4 (PB1)
 * Blue  → PA8 GPIO only (TIM1_CH1 would need advanced timer init)
 */
void RGB_SetColor(uint8_t red, uint8_t green, uint8_t blue)
{
  pwm_red   = red;
  pwm_green = green;
  pwm_blue  = blue;

  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, red);    // PB0
  __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_4, green);  // PB1

  /* Blue on PA8 is GPIO — approximate: ON if blue > 50, OFF otherwise */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, (blue > 50) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/* USER CODE END 0 */

/* ---- Main ---------------------------------------------------------------- */
int main(void)
{
  HAL_Init();
  SystemClock_Config();

  MX_GPIO_Init();
  MX_USART1_UART_Init();
  MX_USB_DEVICE_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();   // Init PWM timer

  /* USER CODE BEGIN 2 */

  /* Start UART receive interrupt */
  HAL_UART_Receive_IT(&huart1, uart_buf, 1);

  /* Start TIM2 base interrupt (state machine ticks) */
  HAL_TIM_Base_Start_IT(&htim2);

  /*
   * FIX 3: PWM started ONCE here in main — not in the callback.
   *         TIM3 channels 3 and 4 drive PB0 (Red) and PB1 (Green).
   */
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3);  // Red  → PB0
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_4);  // Green → PB1

  /* Set initial color: 80% Red, 0% Green, 20% Blue */
  RGB_SetColor(80, 0, 20);

  /* USER CODE END 2 */

  /* Main loop */
  while (1)
  {
    /* Button debounce */
    if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET)
    {
      HAL_Delay(25);
      if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET)
      {
        while (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET);
        User_B_Pressed = 1;
        printf("USER\n\r");
      }
    }

    /* State machine */
    switch (SM_State)
    {
      case SM_START:
        printf("Code version %d.%d starting...\n\r", SW_VERSION / 10, SW_VERSION % 10);
        SM_State = SM_FAST_BLINK;
        break;

      case SM_FAST_BLINK:
        if (User_B_Pressed)
        {
          htim2.Init.Period = 300;
          if (HAL_TIM_Base_Init(&htim2) != HAL_OK) Error_Handler();
          User_B_Pressed = 0;
          SM_State = SM_SLOW_BLINK;
          /* Example: change color when switching state */
          RGB_SetColor(0, 80, 20);   // Green dominant in slow blink
        }
        break;

      case SM_SLOW_BLINK:
        if (User_B_Pressed)
        {
          htim2.Init.Period = 100;
          if (HAL_TIM_Base_Init(&htim2) != HAL_OK) Error_Handler();
          User_B_Pressed = 0;
          SM_State = SM_FAST_BLINK;
          RGB_SetColor(80, 0, 20);   // Red dominant in fast blink
        }
        break;

      default:
        SM_State = SM_START;
        break;
    }
  }
}

/* ---- Clock config -------------------------------------------------------- */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) Error_Handler();

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                   | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) Error_Handler();

  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USB;
  PeriphClkInit.UsbClockSelection    = RCC_USBCLKSOURCE_PLL_DIV1_5;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) Error_Handler();
}

/* ---- TIM2: base timer for period elapsed interrupt (state machine) ------- */
static void MX_TIM2_Init(void)
{
  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig     = {0};

  htim2.Instance               = TIM2;
  htim2.Init.Prescaler         = 7199;           // 72MHz / 7200 = 10kHz tick
  htim2.Init.CounterMode       = TIM_COUNTERMODE_UP;
  htim2.Init.Period            = 100;             // 10kHz / 100 = 100Hz → 10ms period
  htim2.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK) Error_Handler();

  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK) Error_Handler();

  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode     = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK) Error_Handler();
}

/* ---- TIM3: PWM for RGB LED (CH3=PB0 Red, CH4=PB1 Green) ----------------- */
/*
 * FIX 4: Separate timer for PWM — TIM3 configured in PWM mode.
 *         Prescaler + Period set for ~1kHz PWM frequency.
 *         Period = 99 → duty cycle values 0–99 = 0%–99%.
 *
 * PWM freq = 72MHz / (Prescaler+1) / (Period+1)
 *          = 72MHz / 720 / 100 = 1000Hz = 1kHz
 */
static void MX_TIM3_Init(void)
{
  TIM_OC_InitTypeDef sConfigOC = {0};

  htim3.Instance               = TIM3;
  htim3.Init.Prescaler         = 719;            // 72MHz / 720 = 100kHz
  htim3.Init.CounterMode       = TIM_COUNTERMODE_UP;
  htim3.Init.Period            = 99;             // 100kHz / 100 = 1kHz PWM
  htim3.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_PWM_Init(&htim3) != HAL_OK) Error_Handler();  // FIX: PWM_Init, not Base_Init

  /* FIX 5: OCMode = PWM1, not TIMING */
  sConfigOC.OCMode       = TIM_OCMODE_PWM1;
  sConfigOC.Pulse        = 0;                    // Start at 0% duty
  sConfigOC.OCPolarity   = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode   = TIM_OCFAST_DISABLE;

  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_3) != HAL_OK) Error_Handler();
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_4) != HAL_OK) Error_Handler();

  HAL_TIM_MspPostInit(&htim3);
}

/* ---- USART1 init --------------------------------------------------------- */
static void MX_USART1_UART_Init(void)
{
  huart1.Instance          = USART1;
  huart1.Init.BaudRate     = 9600;
  huart1.Init.WordLength   = UART_WORDLENGTH_8B;
  huart1.Init.StopBits     = UART_STOPBITS_1;
  huart1.Init.Parity       = UART_PARITY_NONE;
  huart1.Init.Mode         = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK) Error_Handler();
}

/* ---- GPIO init ----------------------------------------------------------- */
/*
 * FIX 6: PB0 and PB1 → GPIO_MODE_AF_PP (Alternate Function Push-Pull)
 *         Required for TIM3 PWM signal to physically appear on the pin.
 *         PA8 stays GPIO_MODE_OUTPUT_PP (no PWM timer available without TIM1 advanced setup).
 */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* Initial output levels */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0 | GPIO_PIN_1, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET);

  /* PC13 — User button, input with pull-up */
  GPIO_InitStruct.Pin  = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*
   * PB0 (TIM3_CH3 = Red), PB1 (TIM3_CH4 = Green)
   * Must be AF_PP for PWM output — not OUTPUT_PP!
   */
  GPIO_InitStruct.Pin   = GPIO_PIN_0 | GPIO_PIN_1;
  GPIO_InitStruct.Mode  = GPIO_MODE_AF_PP;        // FIX: was OUTPUT_PP
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;   // High speed for PWM
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* PA8 — Blue LED, plain GPIO (TIM1 advanced timer not configured here) */
  GPIO_InitStruct.Pin   = GPIO_PIN_8;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

/* ---- Error handler ------------------------------------------------------- */
void Error_Handler(void)
{
  __disable_irq();
  while (1) {}
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
  printf("Assert failed: file %s, line %lu\r\n", file, line);
}
#endif