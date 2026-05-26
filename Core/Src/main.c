#include "main.h"
#include "usb_device.h"
#include <stdio.h>
#include <string.h>

//defininre de variabile
#define SW_VERSION  11 
#define clock_period 150 

TIM_HandleTypeDef  htim2;
UART_HandleTypeDef huart1;

uint8_t rx_data;
char rx_buffer[20];
uint8_t rx_index = 0;
volatile uint8_t play_morse_flag = 0;

volatile uint8_t pwm_counter = 0;
volatile uint8_t duty_R = 0;
volatile uint8_t duty_G = 0;
volatile uint8_t duty_B = 100;  
uint8_t state = 0;         

//definirea de functii prototip
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_TIM2_Init(void);
void Error_Handler(void);

#ifdef __GNUC__
#define PUTCHAR_PROTOTYPE  int __io_putchar(int ch)
#else
#define PUTCHAR_PROTOTYPE  int fputc(int ch, FILE *f)
#endif

PUTCHAR_PROTOTYPE {
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 0xFFFF);
    return ch;
}

int _write(int fd, char *ptr, int len) {
    HAL_UART_Transmit(&huart1, (uint8_t *)ptr, len, HAL_MAX_DELAY);
    return len;
}

//citim ce avem in rx, daca e Morse jucan else continuam sa citim
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART1) {
        if (rx_data == '\n' || rx_data == '\r') {
            rx_buffer[rx_index] = '\0';
            if (strcmp(rx_buffer, "Morse") == 0) {
                play_morse_flag = 1;
            }
            rx_index = 0;
        } else {
            if (rx_index < sizeof(rx_buffer) - 1) {
                rx_buffer[rx_index++] = (char)rx_data;
            }
        }
        HAL_UART_Receive_IT(&huart1, &rx_data, 1);
    }
}

//pwm citim din TIM2
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM2) {
        if (pwm_counter == 0) {
            if (duty_R > 0) HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
            if (duty_G > 0) HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);
            if (duty_B > 0) HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_SET);
        }

        if (pwm_counter >= duty_R) HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
        if (pwm_counter >= duty_G) HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET);
        if (pwm_counter >= duty_B) HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET);

        pwm_counter++;
        if (pwm_counter >= 100) {
            pwm_counter = 0;
        }
    }
}

//mimic un hashmap din cpp
typedef struct {
    char letter;
    const char *code;
} Dictionary;

Dictionary alphabet[] = {
    {'h', "...."},
    {'e', "."},
    {'l', ".-.."},
    {'o', "---"},
    {'2', "..---"},
    {'3', "...--"}
};

typedef struct {
    uint16_t duration;
    uint8_t color_mode;
} MorseCommand;

MorseCommand seq[150];
int sq_len = 0;

void command(uint16_t dur, uint8_t color) {
    seq[sq_len].duration = dur;
    seq[sq_len].color_mode = color;
    sq_len++;
}

//explicat mai jos la play_loop()
void morseBlink(const char* str) {
    sq_len = 0;
    for (int i = 0; str[i] != '\0'; ++i) {
        char c = str[i];

        if (c == ' ') {
            command(4 * clock_period, 0);
            continue;
        }

        const char* code = "";
        for (int j = 0; j < 6; j++) {
            if (alphabet[j].letter == c) {
                code = alphabet[j].code;
                break;
            }
        }

        for (int k = 0; code[k] != '\0'; ++k) {
            if (code[k] == '.') {
                command(clock_period, 1);
            } else if (code[k] == '-') {
                command(3 * clock_period, 2);
            }

            command(clock_period, 0);
        }
    }
}

//main loop, face switch intre culorile pentru cod
//red 1t dot, green 3t dash, spatiu 4t  0 0 0
void play_loop() {
    for (int i = 0; i < sq_len; ++i) {
        switch (seq[i].color_mode){
            case 1:
                duty_R = 100;
                duty_G = 0;
                duty_B = 0;
            break;
            case 2:
                duty_R = 0;
                duty_G = 100;
                duty_B = 0;
            break;
            case 0:
                duty_R = 0;
                duty_G = 0;
                duty_B = 0;
            break;
        break;
        }
        for(volatile uint32_t d = 0; d < (seq[i].duration * 1000); d++) {
    __NOP(); 
}
    }
    duty_R = 33;
    duty_G = 33;
    duty_B = 34;
}

int main(void) {
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART1_UART_Init();
    MX_USB_DEVICE_Init();
    
    MX_TIM2_Init();

    HAL_TIM_Base_Start_IT(&htim2);

    printf("Code version %d.%d starting...\r\n", SW_VERSION / 10, SW_VERSION % 10);
    printf("Stare initiala: Albastru (R 0%%, G 0%%, B 100%%)\r\n");

    const char *text = "hello 23";
    morseBlink(text);

    HAL_UART_Receive_IT(&huart1, &rx_data, 1);

    while (1) {
        //daca flag-ul se seteaza facem automat secventa Morse
        if (play_morse_flag == 1) {
            printf("Playing Morse sequence...\r\n");
            morseBlink("hello 23");

            play_loop();
            play_morse_flag = 0;
        }

        //debouncer
        if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET) {
            HAL_Delay(25);
            if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET) {
                while (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET);
                state++;
                if (state > 4) {
                    state = 0;
                }
                //fsm
                switch (state) {
                    case 0:
                        duty_R = 0; duty_G = 0; duty_B = 100;
                        break;
                    case 1:
                        duty_R = 80; duty_G = 50; duty_B = 0;
                        break;
                    case 2:
                        duty_R = 50; duty_G = 0; duty_B = 50;
                        break;
                    case 3:
                        duty_R = 0; duty_G = 0; duty_B = 0;
                        break;
                    case 4: 
                        play_loop();
                        break;  
                }

                printf("R %d%%, G %d%%, B %d%%\r\n", duty_R, duty_G, duty_B);
            }
        }
    }
}

//fisiere generare de CubeMX
void SystemClock_Config(void) {
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

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK  | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) Error_Handler();

    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USB;
    PeriphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_PLL_DIV1_5;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) Error_Handler();
}

static void MX_TIM2_Init(void) {
    TIM_ClockConfigTypeDef  sClockSourceConfig = {0};
    TIM_MasterConfigTypeDef sMasterConfig = {0};

    htim2.Instance = TIM2;
    htim2.Init.Prescaler = 71;   
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = 99;   
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&htim2) != HAL_OK) Error_Handler();

    sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
    if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK) Error_Handler();

    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK) Error_Handler();
}


static void MX_USART1_UART_Init(void) {
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 9600;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart1) != HAL_OK) Error_Handler();
}

static void MX_GPIO_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0 | GPIO_PIN_1, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_RESET);

    GPIO_InitStruct.Pin  = GPIO_PIN_13;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

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

void Error_Handler(void) {
    __disable_irq();
    while (1) {}
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line) {
    printf("Assert failed: file %s, line %lu\r\n", file, (unsigned long)line);
}
#endif