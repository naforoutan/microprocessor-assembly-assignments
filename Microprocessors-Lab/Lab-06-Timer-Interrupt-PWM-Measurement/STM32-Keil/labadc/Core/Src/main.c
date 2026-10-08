/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : PWM Frequency Measurement using GPIO EXTI Interrupt
  * @note           : HARDWARE CONNECTION: PA8 (PWM output) ---> PA0 (EXTI input)
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include <string.h>
#include <stdio.h>

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim2;  // TIM2 for high-resolution timing
TIM_HandleTypeDef htim3;  // ADDED: to satisfy linker reference from stm32f4xx_it.c
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

// ==================== PWM Measurement using EXTI ====================
// Using PA0 as EXTI input for frequency measurement
// Hardware: Connect PA8 (PWM output) to PA0 (EXTI input)

volatile uint32_t lastRisingTime = 0;
volatile uint32_t currentRisingTime = 0;
volatile uint32_t periodTicks = 0;
volatile uint32_t pwmFrequency = 0;
volatile uint32_t pulseWidthTicks = 0;
volatile uint32_t pwmDutyCycle = 0;
volatile uint32_t fallingEdgeTime = 0;
volatile uint8_t edgeState = 0;  // 0: wait rising, 1: wait falling, 2: wait next rising

// TIM2 clock = 84MHz (no prescaler)
#define TIMER_CLOCK_HZ  84000000UL

// TIM1 configuration for 20kHz PWM
#define TIM1_PRESCALER     84 - 1
#define TIM1_PERIOD        50 - 1

// ==================== Motor Control Variables ====================
typedef enum {
    MOTOR_STOP = 0,
    MOTOR_CW,
    MOTOR_CCW
} MotorDirection_t;

typedef struct {
    TIM_HandleTypeDef *htim;
    uint32_t channel;
    uint8_t currentSpeed;
    MotorDirection_t direction;
    uint8_t motorId;
} Motor_t;

// Protocol definitions
#define SOF             0xAA
#define EOF_BYTE        0x55
#define DEVICE_ADDR     0x02
#define BROADCAST_ADDR  0xFF

#define CMD_SET_PWM         0x01
#define CMD_MOTOR_CONTROL   0x10
#define CMD_GET_STATUS      0x20
#define CMD_ECHO            0x30
#define CMD_ADC_REPORT      0x40
#define CMD_FREQ_REPORT     0x50

#define STATUS_SUCCESS      0x01
#define STATUS_IGNORED      0x02
#define STATUS_ERROR        0xFF

// Motor pins
#define MOTOR1_IN1_PIN  GPIO_PIN_0
#define MOTOR1_IN2_PIN  GPIO_PIN_1
#define MOTOR1_IN_PORT  GPIOB

#define MOTOR2_IN1_PIN  GPIO_PIN_2
#define MOTOR2_IN2_PIN  GPIO_PIN_10
#define MOTOR2_IN_PORT  GPIOB

// Global variables
Motor_t motor1, motor2;
uint8_t rxByte;
uint8_t rxBuffer[128];
volatile uint16_t rxIndex = 0;
volatile uint8_t uartState = 0;
volatile uint8_t frameReady = 0;
volatile uint8_t validAddress = 0;
uint32_t lastAdcReportTime = 0;
uint16_t lastAdcValue = 0;

// Function prototypes
uint8_t CalcCRC(uint8_t *data, uint8_t len);
void Motor_SetDirection(Motor_t *motor, MotorDirection_t dir);
void Motor_SetSpeed(Motor_t *motor, uint8_t speed);
void Motor_Init(Motor_t *motor, TIM_HandleTypeDef *htim, uint32_t channel, uint8_t id);
void SendFrame(uint8_t transmitter_addr, uint8_t receiver_addr, uint8_t cmd, uint8_t len, uint8_t *data);
void SendSimpleResponse(uint8_t transmitter_addr, uint8_t receiver_addr, uint8_t cmd, uint8_t status);
void ProcessReceivedFrame(void);
uint16_t ADC_ReadOnce(void);
uint8_t ADC_To_Speed(uint16_t adcValue);
void MX_TIM1_Init_Complete(void);
void MX_TIM2_Init_Complete(void);

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM1_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM3_Init(void)
{
    __HAL_RCC_TIM3_CLK_ENABLE();

    htim3.Instance = TIM3;
    htim3.Init.Prescaler = 8399;
    htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim3.Init.Period = 9;
    htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
    {
        Error_Handler();
    }
}


/* USER CODE BEGIN 0 */

// ==================== CRC Functions ====================
uint8_t CalcCRC(uint8_t *data, uint8_t len)
{
    uint8_t crc = 0;
    for (uint8_t i = 0; i < len; i++)
    {
        crc ^= data[i];
    }
    return crc;
}

// ==================== ADC Functions ====================
uint16_t ADC_ReadOnce(void)
{
    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 20);
    uint16_t value = HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);
    return value;
}

uint8_t ADC_To_Speed(uint16_t adcValue)
{
    if (adcValue > 4095) adcValue = 4095;
    return (uint8_t)((uint32_t)adcValue * 255 / 4095);
}

// ==================== TIM2 Initialization (for high-resolution timing) ====================
void MX_TIM2_Init_Complete(void)
{
    __HAL_RCC_TIM2_CLK_ENABLE();
    
    htim2.Instance = TIM2;
    htim2.Init.Prescaler = 0;           // No prescaler -> 84MHz
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = 0xFFFFFFFF;     // 32-bit maximum
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    
    if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
    {
        Error_Handler();
    }
    
    // Start the timer
    if (HAL_TIM_Base_Start(&htim2) != HAL_OK)
    {
        Error_Handler();
    }
}

// ==================== TIM1 PWM Initialization ====================
void MX_TIM1_Init_Complete(void)
{
    HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_1);
    HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_2);
    
    htim1.Instance = TIM1;
    htim1.Init.Prescaler = TIM1_PRESCALER;
    htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim1.Init.Period = TIM1_PERIOD;
    htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim1.Init.RepetitionCounter = 0;
    htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    
    if (HAL_TIM_PWM_Init(&htim1) != HAL_OK)
    {
        Error_Handler();
    }
    
    TIM_MasterConfigTypeDef sMasterConfig = {0};
    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
    {
        Error_Handler();
    }
    
    TIM_OC_InitTypeDef sConfigOC = {0};
    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.Pulse = 0;
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
    sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
    sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
    
    if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
    {
        Error_Handler();
    }
    if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
    {
        Error_Handler();
    }
    
    TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};
    sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_ENABLE;
    sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_ENABLE;
    sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
    sBreakDeadTimeConfig.DeadTime = 0;
    sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
    sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
    sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_ENABLE;
    if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig) != HAL_OK)
    {
        Error_Handler();
    }
}

// ==================== Motor Control Functions ====================
void Motor_SetDirection(Motor_t *motor, MotorDirection_t dir)
{
    GPIO_TypeDef *port;
    uint16_t in1_pin, in2_pin;
    
    if (motor->motorId == 0) {
        port = MOTOR1_IN_PORT;
        in1_pin = MOTOR1_IN1_PIN;
        in2_pin = MOTOR1_IN2_PIN;
    } else {
        port = MOTOR2_IN_PORT;
        in1_pin = MOTOR2_IN1_PIN;
        in2_pin = MOTOR2_IN2_PIN;
    }
    
    switch(dir) {
        case MOTOR_STOP:
            HAL_GPIO_WritePin(port, in1_pin, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(port, in2_pin, GPIO_PIN_RESET);
            break;
        case MOTOR_CW:
            HAL_GPIO_WritePin(port, in1_pin, GPIO_PIN_SET);
            HAL_GPIO_WritePin(port, in2_pin, GPIO_PIN_RESET);
            break;
        case MOTOR_CCW:
            HAL_GPIO_WritePin(port, in1_pin, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(port, in2_pin, GPIO_PIN_SET);
            break;
    }
    motor->direction = dir;
}

void Motor_SetSpeed(Motor_t *motor, uint8_t speed)
{
    uint32_t arr = __HAL_TIM_GET_AUTORELOAD(motor->htim);
    uint32_t pwmValue = (uint32_t)speed * (arr + 1) / 255;
    __HAL_TIM_SET_COMPARE(motor->htim, motor->channel, pwmValue);
    motor->currentSpeed = speed;
}

void Motor_Init(Motor_t *motor, TIM_HandleTypeDef *htim, uint32_t channel, uint8_t id)
{
    motor->htim = htim;
    motor->channel = channel;
    motor->motorId = id;
    motor->currentSpeed = 0;
    motor->direction = MOTOR_STOP;
    
    Motor_SetDirection(motor, MOTOR_STOP);
    Motor_SetSpeed(motor, 0);
}

// ==================== Frame Functions ====================
void SendFrame(uint8_t transmitter_addr, uint8_t receiver_addr, uint8_t cmd,
               uint8_t len, uint8_t *data)
{
    uint8_t frame[160];
    uint8_t i = 0;
    
    frame[i++] = SOF;
    frame[i++] = transmitter_addr;
    frame[i++] = receiver_addr;
    frame[i++] = cmd;
    frame[i++] = len;
    
    for (uint8_t j = 0; j < len; j++) {
        frame[i++] = data[j];
    }
    
    uint8_t crcLen = 4 + len;
    uint8_t crcValue = CalcCRC(&frame[1], crcLen);
    frame[i++] = crcValue;
    frame[i++] = EOF_BYTE;
    
    HAL_UART_Transmit(&huart2, frame, i, 100);
}

void SendSimpleResponse(uint8_t transmitter_addr, uint8_t receiver_addr, 
                        uint8_t cmd, uint8_t status)
{
    SendFrame(transmitter_addr, receiver_addr, cmd, 1, &status);
}

// ==================== Frame Processing ====================
void ProcessReceivedFrame(void)
{
    if (rxIndex < 7) return;
    if (rxBuffer[0] != SOF) return;
    if (rxBuffer[rxIndex - 1] != EOF_BYTE) return;
    
    uint8_t addr_tx = rxBuffer[1];
    uint8_t addr_rx = rxBuffer[2];
    uint8_t cmd = rxBuffer[3];
    uint8_t len = rxBuffer[4];
    
    if (rxIndex != (uint16_t)(len + 7)) return;
    
    if (addr_rx != DEVICE_ADDR && addr_rx != BROADCAST_ADDR) return;
    
    uint8_t crc_calc = CalcCRC(&rxBuffer[1], 4 + len);
    uint8_t crc_rx = rxBuffer[5 + len];
    
    if (crc_calc != crc_rx) return;
    
    uint8_t *data = &rxBuffer[5];
    uint8_t mode_manual = (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_12) == GPIO_PIN_SET) ? 1 : 0;
    
    switch(cmd) 
    {
        case CMD_SET_PWM:
            if (mode_manual && len == 2) {
                uint8_t motor_id = data[0];
                uint8_t speed = data[1];
                
                if (motor_id == 0) {
                    Motor_SetSpeed(&motor1, speed);
                    SendSimpleResponse(DEVICE_ADDR, addr_tx, cmd, STATUS_SUCCESS);
                }
                else if (motor_id == 1) {
                    Motor_SetSpeed(&motor2, speed);
                    SendSimpleResponse(DEVICE_ADDR, addr_tx, cmd, STATUS_SUCCESS);
                }
            } else if (!mode_manual) {
                SendSimpleResponse(DEVICE_ADDR, addr_tx, cmd, STATUS_IGNORED);
            }
            break;
            
        case CMD_MOTOR_CONTROL:
            if (mode_manual && len == 3) {
                uint8_t motor_id = data[0];
                uint8_t speed = data[1];
                uint8_t direction = data[2];
                
                Motor_t *motor;
                if (motor_id == 0) motor = &motor1;
                else if (motor_id == 1) motor = &motor2;
                else break;
                
                Motor_SetSpeed(motor, speed);
                
                MotorDirection_t dir;
                switch(direction) {
                    case 0: dir = MOTOR_STOP; break;
                    case 1: dir = MOTOR_CW; break;
                    case 2: dir = MOTOR_CCW; break;
                    default: dir = MOTOR_STOP;
                }
                Motor_SetDirection(motor, dir);
                
                uint8_t response[3] = {motor->currentSpeed, (uint8_t)motor->direction, STATUS_SUCCESS};
                SendFrame(DEVICE_ADDR, addr_tx, cmd, 3, response);
            } else if (!mode_manual) {
                uint8_t response[3] = {0, 0, STATUS_IGNORED};
                SendFrame(DEVICE_ADDR, addr_tx, cmd, 3, response);
            }
            break;
            
        case CMD_GET_STATUS:
            {
                uint8_t status[7];
                status[0] = motor1.currentSpeed;
                status[1] = (uint8_t)motor1.direction;
                status[2] = motor2.currentSpeed;
                status[3] = (uint8_t)motor2.direction;
                status[4] = mode_manual ? 0x01 : 0x00;
                status[5] = (uint8_t)(pwmFrequency >> 8);
                status[6] = (uint8_t)(pwmFrequency & 0xFF);
                SendFrame(DEVICE_ADDR, addr_tx, cmd, 7, status);
            }
            break;
            
        case CMD_ECHO:
            SendFrame(DEVICE_ADDR, addr_tx, cmd, len, data);
            break;
            
        default:
            {
                uint8_t err = STATUS_ERROR;
                SendSimpleResponse(DEVICE_ADDR, addr_tx, 0xFF, err);
            }
            break;
    }
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_ADC1_Init();
    MX_USART2_UART_Init();

				
		/* USER CODE BEGIN 2 */

		// Initialize TIM2 for high-resolution timing (MUST be called first)
		MX_TIM2_Init_Complete();

		// Initialize TIM1 for PWM generation
		MX_TIM1_Init_Complete();

		// Initialize TIM3 for 1ms interrupt
		MX_TIM3_Init();

		// Enable TIM3 interrupt in NVIC
		HAL_NVIC_SetPriority(TIM3_IRQn, 1, 0);
		HAL_NVIC_EnableIRQ(TIM3_IRQn);

		// Start TIM3 with interrupt
		HAL_TIM_Base_Start_IT(&htim3);

		// Initialize motors with TIM1
		Motor_Init(&motor1, &htim1, TIM_CHANNEL_1, 0);
		Motor_Init(&motor2, &htim1, TIM_CHANNEL_2, 1);

		// Start PWM on TIM1 (PA8)
		HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
		HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);

		// Start UART reception
		HAL_UART_Receive_IT(&huart2, &rxByte, 1);

		// Enable EXTI interrupt for PA0
		HAL_NVIC_SetPriority(EXTI0_IRQn, 0, 0);
		HAL_NVIC_EnableIRQ(EXTI0_IRQn);

		// Send ready message
		char readyMsg[] = "STM32F4 Motor Controller Ready - PWM Freq Measurement using EXTI\r\n";
		HAL_UART_Transmit(&huart2, (uint8_t*)readyMsg, strlen(readyMsg), 100);

		char connMsg[] = "HARDWARE: Connect PA8 (PWM output) ---> PA0 (EXTI input)\r\n";
		HAL_UART_Transmit(&huart2, (uint8_t*)connMsg, strlen(connMsg), 100);

		char timMsg[] = "TIM2 initialized with 84MHz counter for accurate measurement\r\n";
		HAL_UART_Transmit(&huart2, (uint8_t*)timMsg, strlen(timMsg), 100);

		char tim3Msg[] = "TIM3 started in interrupt mode at 1kHz\r\n";
		HAL_UART_Transmit(&huart2, (uint8_t*)tim3Msg, strlen(tim3Msg), 100);

		/* USER CODE END 2 */


    /* Infinite loop */
    while (1)
    {
				HAL_Delay(1);
        /* USER CODE BEGIN 3 */
        uint8_t mode_manual = (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_12) == GPIO_PIN_SET) ? 1 : 0;
        
        uint16_t adc = ADC_ReadOnce();
        lastAdcValue = adc;
        uint8_t adcSpeed = ADC_To_Speed(adc);
        
        if (!mode_manual)
        {
            Motor_SetSpeed(&motor1, adcSpeed);
            Motor_SetSpeed(&motor2, adcSpeed);
            Motor_SetDirection(&motor1, MOTOR_CW);
            Motor_SetDirection(&motor2, MOTOR_CW);
        }
        
        if (HAL_GetTick() - lastAdcReportTime >= 1000)
        {
            lastAdcReportTime = HAL_GetTick();
            
            // ADC report
            uint8_t adcPayload[3] = { 
                (uint8_t)(adc >> 8),
                (uint8_t)(adc & 0xFF),
                mode_manual ? 0x01 : 0x00
            };
            SendFrame(DEVICE_ADDR, BROADCAST_ADDR, CMD_ADC_REPORT, 3, adcPayload);
            
            // Send frequency and duty cycle via UART
            char msg[100];
            sprintf(msg, "PWM Freq: %lu Hz, Duty: %lu%%, Period Ticks: %lu\r\n", 
                    (unsigned long)pwmFrequency, 
                    (unsigned long)pwmDutyCycle,
                    (unsigned long)periodTicks);
            HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), 100);
            
            // Frequency report frame
            uint8_t freqPayload[4] = {
                (uint8_t)(pwmFrequency >> 24),
                (uint8_t)(pwmFrequency >> 16),
                (uint8_t)(pwmFrequency >> 8),
                (uint8_t)(pwmFrequency & 0xFF)
            };
            SendFrame(DEVICE_ADDR, BROADCAST_ADDR, CMD_FREQ_REPORT, 4, freqPayload);
        }
        
        if (frameReady)
        {
            frameReady = 0;
            ProcessReceivedFrame();
            rxIndex = 0;
            uartState = 0;
            validAddress = 0;
        }
        
        HAL_Delay(10);
        /* USER CODE END 3 */
    }
}

// ==================== System Clock Configuration ====================
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLM = 16;
    RCC_OscInitStruct.PLL.PLLN = 336;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
    RCC_OscInitStruct.PLL.PLLQ = 7;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
    {
        Error_Handler();
    }
}

// ==================== GPIO Initialization ====================
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_10, GPIO_PIN_RESET);
    
    // Mode select pin (PB12)
    GPIO_InitStruct.Pin = GPIO_PIN_12;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    
    // Motor direction pins
    GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    
    // ===== EXTI INPUT FOR FREQUENCY MEASUREMENT =====
    // PA0 as input with EXTI interrupt on rising edge
    GPIO_InitStruct.Pin = GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;  // Start with rising edge
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

// ==================== ADC Initialization ====================
static void MX_ADC1_Init(void)
{
    ADC_ChannelConfTypeDef sConfig = {0};

    hadc1.Instance = ADC1;
    hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
    hadc1.Init.Resolution = ADC_RESOLUTION_12B;
    hadc1.Init.ScanConvMode = DISABLE;
    hadc1.Init.ContinuousConvMode = DISABLE;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.NbrOfConversion = 1;
    hadc1.Init.DMAContinuousRequests = DISABLE;
    hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    if (HAL_ADC_Init(&hadc1) != HAL_OK)
    {
        Error_Handler();
    }

    sConfig.Channel = ADC_CHANNEL_0;
    sConfig.Rank = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
    {
			
        Error_Handler();
    }
}

// ==================== UART Initialization ====================
static void MX_USART2_UART_Init(void)
{
    huart2.Instance = USART2;
    huart2.Init.BaudRate = 115200;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart2) != HAL_OK)
    {
        Error_Handler();
    }
}

// Empty init functions for CubeMX compatibility
static void MX_TIM1_Init(void) {}
static void MX_TIM2_Init(void) {}

/* USER CODE BEGIN 4 */

// ==================== UART Callback ====================
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2)
    {
        if (uartState == 0)
        {
            if (rxByte == SOF)
            {
                uartState = 1;
                rxIndex = 0;
                validAddress = 0;
                rxBuffer[rxIndex++] = rxByte;
            }
        }
        else if (uartState == 1)
        {
            if (rxIndex < sizeof(rxBuffer))
            {
                rxBuffer[rxIndex++] = rxByte;
                
                if (rxIndex == 3 && !validAddress)
                {
                    uint8_t addr_rx = rxBuffer[2];
                    if (addr_rx != DEVICE_ADDR && addr_rx != BROADCAST_ADDR)
                    {
                        uartState = 0;
                        rxIndex = 0;
                        validAddress = 0;
                        HAL_UART_Receive_IT(&huart2, &rxByte, 1);
                        return;
                    }
                    validAddress = 1;
                }
                
                if (validAddress && rxIndex >= 7)
                {
                    uint8_t len = rxBuffer[4];
                    uint16_t expectedLen = (uint16_t)(len + 7);
                    
                    if (rxIndex == expectedLen && rxByte == EOF_BYTE)
                    {
                        frameReady = 1;
                        uartState = 0;
                    }
                }
            }
            else
            {
                uartState = 0;
                rxIndex = 0;
                validAddress = 0;
            }
        }
        
        HAL_UART_Receive_IT(&huart2, &rxByte, 1);
    }
}

// ==================== EXTI GPIO Callback (Main frequency measurement) ====================
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == GPIO_PIN_0)
    {
        // Read TIM2 counter value (84MHz, 32-bit)
        uint32_t currentTime = TIM2->CNT;
        
        if (edgeState == 0)
        {
            // First rising edge detected
            lastRisingTime = currentTime;
            
            // Change EXTI to falling edge
            HAL_GPIO_DeInit(GPIOA, GPIO_PIN_0);
            GPIO_InitTypeDef GPIO_InitStruct = {0};
            GPIO_InitStruct.Pin = GPIO_PIN_0;
            GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
            GPIO_InitStruct.Pull = GPIO_PULLDOWN;
            HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
            HAL_NVIC_SetPriority(EXTI0_IRQn, 0, 0);
            HAL_NVIC_EnableIRQ(EXTI0_IRQn);
            edgeState = 1;
        }
        else if (edgeState == 1)
        {
            // Falling edge detected (end of high pulse)
            fallingEdgeTime = currentTime;
            
            // Calculate pulse width (handling 32-bit overflow)
            if (fallingEdgeTime >= lastRisingTime) {
                pulseWidthTicks = fallingEdgeTime - lastRisingTime;
            } else {
                pulseWidthTicks = (0xFFFFFFFF - lastRisingTime) + fallingEdgeTime;
            }
            
            // Change EXTI to rising edge
            HAL_GPIO_DeInit(GPIOA, GPIO_PIN_0);
            GPIO_InitTypeDef GPIO_InitStruct = {0};
            GPIO_InitStruct.Pin = GPIO_PIN_0;
            GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
            GPIO_InitStruct.Pull = GPIO_PULLDOWN;
            HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
            HAL_NVIC_SetPriority(EXTI0_IRQn, 0, 0);
            HAL_NVIC_EnableIRQ(EXTI0_IRQn);
            edgeState = 2;
        }
        else if (edgeState == 2)
        {
            // Next rising edge detected (full period)
            currentRisingTime = currentTime;
            
            // Calculate period (handling 32-bit overflow)
            if (currentRisingTime >= lastRisingTime) {
                periodTicks = currentRisingTime - lastRisingTime;
            } else {
                periodTicks = (0xFFFFFFFF - lastRisingTime) + currentRisingTime;
            }
            
            // Calculate frequency and duty cycle
            if (periodTicks > 0) {
                pwmFrequency = TIMER_CLOCK_HZ / periodTicks;
                pwmDutyCycle = (pulseWidthTicks * 100) / periodTicks;
            }
            
            // Update for next cycle
            lastRisingTime = currentRisingTime;
            edgeState = 1;  // Go back to waiting for falling edge
        }
    }
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM3)
    {
        static uint32_t lastTick = 0;
        uint32_t currentTick = HAL_GetTick();
        
        if (currentTick != lastTick) {
            char msg[50];
            sprintf(msg, "Delay Mode: %lu\r\n", (unsigned long)currentTick);
            HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), 100);
            lastTick = currentTick;
        }
    }
}


void Error_Handler(void)
{
    __disable_irq();
    while (1)
    {
    }
}

/* USER CODE END 4 */

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
}
#endif