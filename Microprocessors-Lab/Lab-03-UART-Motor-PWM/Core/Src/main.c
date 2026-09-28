/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : DC Motor Control with PWM and Direction via UART
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "usart.h"
#include "gpio.h"
#include "tim.h"
#include <string.h>

/* USER CODE BEGIN 0 */

// ==================== Protocol Definitions ====================
#define SOF             0xAA
#define EOF_BYTE        0x55
#define DEVICE_ADDR     0x02    // This device address (STM32)

// Protocol commands
#define CMD_SET_PWM         0x01    // Simple PWM setting (speed only)
#define CMD_MOTOR_CONTROL   0x10    // Full motor control (speed + direction)
#define CMD_GET_STATUS      0x20    // Get motor status
#define CMD_ECHO            0x30    // Echo command back

// Motor direction states
typedef enum {
    MOTOR_STOP = 0,
    MOTOR_CW,      // Clockwise
    MOTOR_CCW      // Counter-Clockwise
} MotorDirection_t;

// Motor structure
typedef struct {
    TIM_HandleTypeDef *htim;
    uint32_t channel;
    uint8_t currentSpeed;        // 0-255
    MotorDirection_t direction;
    uint8_t motorId;
} Motor_t;

// Motor direction control pins (use different pins, not PA8/PA9)
// Example: Use PB pins for direction control
#define MOTOR1_IN1_PIN  GPIO_PIN_8
#define MOTOR1_IN2_PIN  GPIO_PIN_9
#define MOTOR1_IN_PORT  GPIOB   // Changed to GPIOB

#define MOTOR2_IN1_PIN  GPIO_PIN_10
#define MOTOR2_IN2_PIN  GPIO_PIN_11
#define MOTOR2_IN_PORT  GPIOB   // Changed to GPIOB

// Global variables
Motor_t motor1, motor2;
uint8_t rxByte;
uint8_t rxBuffer[64];
uint8_t rxIndex = 0;
uint8_t frameReady = 0;

// Function prototypes
uint8_t CalcCRC(uint8_t *data, uint8_t len);
void Motor_SetDirection(Motor_t *motor, MotorDirection_t dir);
void Motor_SetSpeed(Motor_t *motor, uint8_t speed);
void Motor_Init(Motor_t *motor, TIM_HandleTypeDef *htim, uint32_t channel, uint8_t id);
void SendFrame(uint8_t transmitter_addr, uint8_t receiver_addr, uint8_t cmd, uint8_t data_len, uint8_t *data, uint8_t data_count);
void SendSimpleResponse(uint8_t transmitter_addr, uint8_t receiver_addr, uint8_t cmd, uint8_t status);
void ProcessReceivedFrame(void);
static uint16_t map_0_255_to_arr(uint8_t val, uint16_t arrMax);
void Error_Handler(void);  // Add this prototype

// ==================== CRC Functions ====================
/*
 * Calculate CRC using XOR over all bytes
 */
uint8_t CalcCRC(uint8_t *data, uint8_t len)
{
    uint8_t crc = 0;
    for (uint8_t i = 0; i < len; i++)
    {
        crc ^= data[i];
    }
    return crc;
}

// ==================== Motor Control Functions ====================
/*
 * Set motor direction
 * Parameters:
 * - motor: pointer to motor structure
 * - dir: direction (STOP, CW, CCW)
 */
void Motor_SetDirection(Motor_t *motor, MotorDirection_t dir)
{
    GPIO_TypeDef *port;
    uint16_t in1_pin, in2_pin;
    
    // Select appropriate pins based on motor number
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

/*
 * Set motor speed using PWM
 * Parameters:
 * - motor: pointer to motor structure
 * - speed: 0-255 value to set
 */
void Motor_SetSpeed(Motor_t *motor, uint8_t speed)
{
    uint16_t arrMax = __HAL_TIM_GET_AUTORELOAD(motor->htim);
    // Convert 0-255 value to 0-ARR range
    uint16_t pwmValue = (uint16_t)((uint32_t)speed * arrMax / 255);
    __HAL_TIM_SET_COMPARE(motor->htim, motor->channel, pwmValue);
    motor->currentSpeed = speed;
}

/*
 * Initialize motor structure and pins
 */
void Motor_Init(Motor_t *motor, TIM_HandleTypeDef *htim, uint32_t channel, uint8_t id)
{
    motor->htim = htim;
    motor->channel = channel;
    motor->motorId = id;
    motor->currentSpeed = 0;
    motor->direction = MOTOR_STOP;
    
    // Stop motor initially
    Motor_SetDirection(motor, MOTOR_STOP);
    Motor_SetSpeed(motor, 0);
}

// ==================== Frame Transmission Function ====================
/*
 * Send a communication frame
 * Frame format:
 * | SOF | Addr_Tx | Addr_Rx | Cmd | Len | Data... | CRC | EOF |
 */
void SendFrame(uint8_t transmitter_addr,
               uint8_t receiver_addr,
               uint8_t cmd,
               uint8_t data_len,
               uint8_t *data,
               uint8_t data_count)
{
    uint8_t frame[32];
    uint8_t i = 0;
    
    frame[i++] = SOF;
    frame[i++] = transmitter_addr;
    frame[i++] = receiver_addr;
    frame[i++] = cmd;
    frame[i++] = data_len;
    
    // Copy data bytes
    for (uint8_t j = 0; j < data_count; j++) {
        frame[i++] = data[j];
    }
    
    // Calculate CRC over (Address_Tx to last data byte)
    uint8_t crcLen = 4 + data_count; // Address_Tx, Address_Rx, Cmd, Len + data
    uint8_t crcValue = CalcCRC(&frame[1], crcLen);
    frame[i++] = crcValue;
    frame[i++] = EOF_BYTE;
    
    HAL_UART_Transmit(&huart2, frame, i, 100);
}

/*
 * Helper function to send simple response with status byte
 */
void SendSimpleResponse(uint8_t transmitter_addr, uint8_t receiver_addr, 
                        uint8_t cmd, uint8_t status)
{
    SendFrame(transmitter_addr, receiver_addr, cmd, 1, &status, 1);
}

// ==================== Frame Processing Function ====================
/*
 * Process received UART frame and execute commands
 */
void ProcessReceivedFrame(void)
{
    // Check minimum frame length
    if (rxIndex < 9) return;
    
    // Check SOF and EOF
    if (rxBuffer[0] != SOF || rxBuffer[rxIndex-1] != EOF_BYTE) return;
    
    uint8_t addr_tx   = rxBuffer[1];
    uint8_t addr_rx   = rxBuffer[2];
    uint8_t cmd       = rxBuffer[3];
    uint8_t len       = rxBuffer[4];
    
    // Check destination address
    if (addr_rx != DEVICE_ADDR) return;
    
    // Calculate and verify CRC
    uint8_t crcLen = 4 + len; // Address_Tx + Address_Rx + Cmd + Len + data
    uint8_t crc_calc = CalcCRC(&rxBuffer[1], crcLen);
    uint8_t crc_rx = rxBuffer[5 + len]; // CRC is after data bytes
    
    if (crc_calc != crc_rx) return;
    
    // Process based on command
    switch(cmd) 
    {
        case CMD_SET_PWM:  // Simple PWM command (speed only)
            if (len == 2) {
                uint8_t data_addr = rxBuffer[5];
                uint8_t data_val  = rxBuffer[6];
                
                if (data_addr == 0) {
                    Motor_SetSpeed(&motor1, data_val);
                    SendSimpleResponse(DEVICE_ADDR, addr_tx, cmd, 0x01); // Success
                }
                else if (data_addr == 1) {
                    Motor_SetSpeed(&motor2, data_val);
                    SendSimpleResponse(DEVICE_ADDR, addr_tx, cmd, 0x01);
                }
            }
            break;
            
        case CMD_MOTOR_CONTROL:  // Full motor control (speed + direction)
            if (len == 3) {
                uint8_t motor_id   = rxBuffer[5];
                uint8_t speed      = rxBuffer[6];
                uint8_t direction  = rxBuffer[7];
                
                Motor_t *motor;
                if (motor_id == 0) motor = &motor1;
                else if (motor_id == 1) motor = &motor2;
                else break;
                
                // Set speed
                Motor_SetSpeed(motor, speed);
                
                // Set direction
                MotorDirection_t dir;
                switch(direction) {
                    case 0: dir = MOTOR_STOP; break;
                    case 1: dir = MOTOR_CW; break;
                    case 2: dir = MOTOR_CCW; break;
                    default: dir = MOTOR_STOP;
                }
                Motor_SetDirection(motor, dir);
                
                // Send response with new status
                uint8_t response[3] = {motor->currentSpeed, (uint8_t)motor->direction, 0x01};
                SendFrame(DEVICE_ADDR, addr_tx, cmd, 3, response, 3);
            }
            break;
            
        case CMD_GET_STATUS:  // Get motor status
            {
                uint8_t status[5];
                status[0] = motor1.currentSpeed;
                status[1] = (uint8_t)motor1.direction;
                status[2] = motor2.currentSpeed;
                status[3] = (uint8_t)motor2.direction;
                status[4] = 0xAA; // Valid flag
                SendFrame(DEVICE_ADDR, addr_tx, cmd, 5, status, 5);
            }
            break;
            
        case CMD_ECHO:  // Echo back (for testing)
            SendFrame(DEVICE_ADDR, addr_tx, cmd, len, &rxBuffer[5], len);
            break;
            
        default:
            // Unknown command
            uint8_t err = 0xFF;
            SendSimpleResponse(DEVICE_ADDR, addr_tx, 0xFF, err);
            break;
    }
}

/*
 * Map a value from range 0–255 to range 0–ARR
 * arrMax is the timer auto-reload value (e.g., 999)
 */
static uint16_t map_0_255_to_arr(uint8_t val, uint16_t arrMax)
{
    return (uint16_t)((uint32_t)val * arrMax / 255);
}

/* USER CODE END 0 */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
    /* MCU Initialization ------------------------------------------------------*/
    HAL_Init();
    SystemClock_Config();
    
    /* Initialize peripherals */
    MX_GPIO_Init();
    MX_USART2_UART_Init();
    MX_TIM1_Init();
    
    /* USER CODE BEGIN 2 */
    
    // Initialize motors
    Motor_Init(&motor1, &htim1, TIM_CHANNEL_1, 0);
    Motor_Init(&motor2, &htim1, TIM_CHANNEL_2, 1);
    
    // Start PWM
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
    
    // Start UART reception
    HAL_UART_Receive_IT(&huart2, &rxByte, 1);
    
    // Send ready message
    uint8_t readyMsg[] = "STM32 Motor Controller Ready\r\n";
    HAL_UART_Transmit(&huart2, readyMsg, sizeof(readyMsg)-1, 100);
    
    /* USER CODE END 2 */
    
    /* Infinite loop */
    while (1)
    {
        if (frameReady)
        {
            frameReady = 0;
            ProcessReceivedFrame();
            rxIndex = 0;  // Reset buffer for next frame
        }
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
    
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                   RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
    {
        Error_Handler();
    }
}

/* USER CODE BEGIN 4 */
/**
  * @brief  UART Receive Complete Callback
  * @param  huart: UART handle
  * @retval None
  */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2)
    {
        static uint8_t lastByte = 0;
        
        // Start new frame when SOF is received
        if (rxByte == SOF && lastByte != SOF)
        {
            rxIndex = 0;
        }
        
        if (rxIndex < sizeof(rxBuffer))
        {
            rxBuffer[rxIndex++] = rxByte;
            
            // Frame complete when EOF is received
            if (rxByte == EOF_BYTE && rxIndex >= 9)
            {
                frameReady = 1;
            }
        }
        else
        {
            // Buffer overflow - reset
            rxIndex = 0;
        }
        
        lastByte = rxByte;
        
        // Continue receiving
        HAL_UART_Receive_IT(&huart2, &rxByte, 1);
    }
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
    // Disable interrupts
    __disable_irq();
    
    // Infinite loop
    while(1)
    {
        // Toggle an LED or do something to indicate error
        // You can add LED blinking code here if you have an LED connected
    }
}
/* USER CODE END 4 */

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
}
#endif /* USE_FULL_ASSERT */