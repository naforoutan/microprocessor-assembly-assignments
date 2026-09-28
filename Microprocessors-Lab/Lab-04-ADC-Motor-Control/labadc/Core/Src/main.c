/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : DC Motor Control with PWM and Direction via UART using TIMER1
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
TIM_HandleTypeDef htim1;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM1_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
#include <string.h>

// ==================== Protocol Definitions ====================
#define SOF             0xAA
#define EOF_BYTE        0x55
#define DEVICE_ADDR     0x02
#define BROADCAST_ADDR  0xFF

// Protocol commands
#define CMD_SET_PWM         0x01
#define CMD_MOTOR_CONTROL   0x10
#define CMD_GET_STATUS      0x20
#define CMD_ECHO            0x30
#define CMD_ADC_REPORT      0x40

// Response status codes
#define STATUS_SUCCESS      0x01
#define STATUS_IGNORED      0x02
#define STATUS_ERROR        0xFF

// Motor direction states
typedef enum {
    MOTOR_STOP = 0,
    MOTOR_CW,
    MOTOR_CCW
} MotorDirection_t;

// Motor structure
typedef struct {
    TIM_HandleTypeDef *htim;
    uint32_t channel;
    uint8_t currentSpeed;
    MotorDirection_t direction;
    uint8_t motorId;
} Motor_t;

// UART frame state machine
typedef enum {
    UART_IDLE,
    UART_RECEIVING
} UART_State_t;

// Motor direction control pins
// Motor1: PB0/PB1
#define MOTOR1_IN1_PIN  GPIO_PIN_0
#define MOTOR1_IN2_PIN  GPIO_PIN_1
#define MOTOR1_IN_PORT  GPIOB

// Motor2: PB2/PB10
#define MOTOR2_IN1_PIN  GPIO_PIN_2
#define MOTOR2_IN2_PIN  GPIO_PIN_10
#define MOTOR2_IN_PORT  GPIOB

// TIM1 configuration for 20kHz PWM
// System clock: 84MHz (HSI 16MHz * 336 / 16 / 4 = 84MHz)
// Target PWM frequency: 20kHz
// Formula: Fpwm = Ftim / (Prescaler + 1) / (Period + 1)
// 84,000,000 / 84 / 50 = 20,000Hz
#define TIM1_PRESCALER     84 - 1    // 84MHz / 84 = 1MHz
#define TIM1_PERIOD        50 - 1    // 1MHz / 50 = 20kHz PWM frequency

// Global variables
Motor_t motor1, motor2;
uint8_t rxByte;
uint8_t rxBuffer[128];
volatile uint16_t rxIndex = 0;
volatile UART_State_t uartState = UART_IDLE;
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

// ==================== TIM1 MSP Initialization ====================
void HAL_TIM_PWM_MspInit(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM1)
    {
        // Enable TIM1 clock
        __HAL_RCC_TIM1_CLK_ENABLE();
        
        // Enable GPIOA clock for PA8 and PA9
        __HAL_RCC_GPIOA_CLK_ENABLE();
        
        // Configure PA8 as TIM1_CH1 (Alternate Function)
        GPIO_InitTypeDef GPIO_InitStruct = {0};
        GPIO_InitStruct.Pin = GPIO_PIN_8;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF1_TIM1;  // AF1 for TIM1 on STM32F4
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
        
        // Configure PA9 as TIM1_CH2 (Alternate Function)
        GPIO_InitStruct.Pin = GPIO_PIN_9;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    }
}

// ==================== TIM1 Configuration ====================
void MX_TIM1_Init_Complete(void)
{
    // Stop PWM if running
    HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_1);
    HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_2);
    
    // Configure TIM1
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
    
    // Master configuration for advanced timer
    TIM_MasterConfigTypeDef sMasterConfig = {0};
    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
    {
        Error_Handler();
    }
    
    // Configure PWM channels
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
    
    // Break and Dead-Time configuration for TIM1
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
    // Use ARR + 1 for accurate mapping 0-255 to 0-100% duty cycle
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
    // Minimum frame length: SOF(1)+Addr_Tx(1)+Addr_Rx(1)+Cmd(1)+Len(1)+CRC(1)+EOF(1) = 7
    if (rxIndex < 7) return;
    if (rxBuffer[0] != SOF) return;
    if (rxBuffer[rxIndex - 1] != EOF_BYTE) return;
    
    uint8_t addr_tx = rxBuffer[1];
    uint8_t addr_rx = rxBuffer[2];
    uint8_t cmd = rxBuffer[3];
    uint8_t len = rxBuffer[4];
    
    // Check if frame length matches expected length
    if (rxIndex != (uint16_t)(len + 7)) return;
    
    if (addr_rx != DEVICE_ADDR && addr_rx != BROADCAST_ADDR) return;
    
    uint8_t crc_calc = CalcCRC(&rxBuffer[1], 4 + len);
    uint8_t crc_rx = rxBuffer[5 + len];
    
    if (crc_calc != crc_rx) return;
    
    uint8_t *data = &rxBuffer[5];
    
    // Get current mode
    uint8_t mode_manual = (HAL_GPIO_ReadPin(MODE_SEL_Pin_GPIO_Port, MODE_SEL_Pin_Pin) == GPIO_PIN_SET) ? 1 : 0;
    
    switch(cmd) 
    {
        case CMD_SET_PWM:
            // Only apply in manual mode
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
                // In auto mode, ignore speed commands
                SendSimpleResponse(DEVICE_ADDR, addr_tx, cmd, STATUS_IGNORED);
            }
            break;
            
        case CMD_MOTOR_CONTROL:
            // Only apply in manual mode
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
                // In auto mode, ignore motor control commands
                uint8_t response[3] = {0, 0, STATUS_IGNORED};
                SendFrame(DEVICE_ADDR, addr_tx, cmd, 3, response);
            }
            break;
            
        case CMD_GET_STATUS:
            {
                uint8_t status[5];
                status[0] = motor1.currentSpeed;
                status[1] = (uint8_t)motor1.direction;
                status[2] = motor2.currentSpeed;
                status[3] = (uint8_t)motor2.direction;
                status[4] = mode_manual ? 0x01 : 0x00; // 0x01=Manual, 0x00=Auto
                SendFrame(DEVICE_ADDR, addr_tx, cmd, 5, status);
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
    /* USER CODE BEGIN 1 */

    /* USER CODE END 1 */

    HAL_Init();

    /* USER CODE BEGIN Init */

    /* USER CODE END Init */

    SystemClock_Config();

    /* USER CODE BEGIN SysInit */

    /* USER CODE END SysInit */

    MX_GPIO_Init();
    MX_ADC1_Init();
    MX_USART2_UART_Init();
    
    /* USER CODE BEGIN 2 */
    
    // Initialize TIM1 with full configuration
    MX_TIM1_Init_Complete();
    
    // Initialize motors with TIM1
    Motor_Init(&motor1, &htim1, TIM_CHANNEL_1, 0);
    Motor_Init(&motor2, &htim1, TIM_CHANNEL_2, 1);
    
    // Start PWM on TIM1
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
    
    // Start UART reception
    HAL_UART_Receive_IT(&huart2, &rxByte, 1);
    
    // Send ready message
    uint8_t readyMsg[] = "STM32F4 Motor Controller Ready - TIMER1 PWM @20kHz on PA8/PA9\r\n";
    HAL_UART_Transmit(&huart2, readyMsg, sizeof(readyMsg)-1, 100);
    
    /* USER CODE END 2 */

    /* Infinite loop */
    while (1)
    {
        /* USER CODE BEGIN 3 */
        uint8_t mode_manual = (HAL_GPIO_ReadPin(MODE_SEL_Pin_GPIO_Port, MODE_SEL_Pin_Pin) == GPIO_PIN_SET) ? 1 : 0;
        
        // Read ADC every iteration (for reporting in both modes)
        uint16_t adc = ADC_ReadOnce();
        lastAdcValue = adc;
        uint8_t adcSpeed = ADC_To_Speed(adc);
        
        if (mode_manual)
        {
            // Manual mode: speed is set only via UART commands
            // ADC is read only for reporting, not applied to motors
            // Motors keep their UART-set speeds
        }
        else
        {
            // Auto mode: speed is forced from ADC
            Motor_SetSpeed(&motor1, adcSpeed);
            Motor_SetSpeed(&motor2, adcSpeed);
            
            // Set both motors to forward direction in auto mode
            Motor_SetDirection(&motor1, MOTOR_CW);
            Motor_SetDirection(&motor2, MOTOR_CW);
        }
        
        // Send ADC report every 1 second (in BOTH modes)
        if (HAL_GetTick() - lastAdcReportTime >= 1000)
        {
            lastAdcReportTime = HAL_GetTick();
            uint8_t payload[3] = { 
                (uint8_t)(adc >> 8),     // ADC high byte
                (uint8_t)(adc & 0xFF),   // ADC low byte
                mode_manual ? 0x01 : 0x00  // Mode indicator
            };
            SendFrame(DEVICE_ADDR, BROADCAST_ADDR, CMD_ADC_REPORT, 3, payload);
        }
        
        // Process received UART frames
        if (frameReady)
        {
            frameReady = 0;
            ProcessReceivedFrame();
            rxIndex = 0;
            uartState = UART_IDLE;
            validAddress = 0;
        }
        
        HAL_Delay(10);
        /* USER CODE END 3 */
    }
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
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

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
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

/**
  * @brief TIM1 Initialization Function (called by CubeMX)
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{
    // Empty - TIM1 is fully initialized in MX_TIM1_Init_Complete()
    // This function exists only for CubeMX compatibility
}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
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

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* GPIO Ports Clock Enable */
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* Configure GPIO pin Output Level for motor direction pins only */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_10, GPIO_PIN_RESET);
    
    /* Do NOT write to PA8/PA9 here - they are managed by TIM1 MSP */

    /* Configure B1 button pin */
    GPIO_InitStruct.Pin = B1_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

    /* Configure MODE_SEL pin (PC13) as input with pull-up */
    GPIO_InitStruct.Pin = MODE_SEL_Pin_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(MODE_SEL_Pin_GPIO_Port, &GPIO_InitStruct);

    /* Configure motor direction pins (PB0, PB1, PB2, PB10) as outputs */
    GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    
    /* Note: PA8 and PA9 are configured as AF in HAL_TIM_PWM_MspInit */
    /* This is the correct place for F4 series - no GPIO manipulation here */
}

/* USER CODE BEGIN 4 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2)
    {
        // State machine for UART frame reception
        if (uartState == UART_IDLE)
        {
            if (rxByte == SOF)
            {
                // Start of frame detected
                uartState = UART_RECEIVING;
                rxIndex = 0;
                validAddress = 0;
                rxBuffer[rxIndex++] = rxByte;
            }
            // Ignore any other bytes when idle
        }
        else if (uartState == UART_RECEIVING)
        {
            // Store byte if buffer not full
            if (rxIndex < sizeof(rxBuffer))
            {
                rxBuffer[rxIndex++] = rxByte;
                
                // Check address on 3rd byte (index 2 = Addr_Rx)
                if (rxIndex == 3 && !validAddress)
                {
                    uint8_t addr_rx = rxBuffer[2];
                    // Drop frame if address doesn't match and is not broadcast
                    if (addr_rx != DEVICE_ADDR && addr_rx != BROADCAST_ADDR)
                    {
                        // Invalid address - drop this frame
                        uartState = UART_IDLE;
                        rxIndex = 0;
                        validAddress = 0;
                        HAL_UART_Receive_IT(&huart2, &rxByte, 1);
                        return;
                    }
                    validAddress = 1;
                }
                
                // Check for end of frame only if we have minimum length
                if (validAddress && rxIndex >= 7)
                {
                    // Only consider EOF if we have received the complete frame
                    // The complete frame length is: len + 7
                    uint8_t len = rxBuffer[4];
                    uint16_t expectedLen = (uint16_t)(len + 7);
                    
                    // If we have received the expected number of bytes and last byte is EOF
                    if (rxIndex == expectedLen && rxByte == EOF_BYTE)
                    {
                        frameReady = 1;
                        uartState = UART_IDLE;
                    }
                }
            }
            else
            {
                // Buffer overflow - reset
                uartState = UART_IDLE;
                rxIndex = 0;
                validAddress = 0;
            }
        }
        
        // Continue receiving
        HAL_UART_Receive_IT(&huart2, &rxByte, 1);
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
#endif /* USE_FULL_ASSERT */