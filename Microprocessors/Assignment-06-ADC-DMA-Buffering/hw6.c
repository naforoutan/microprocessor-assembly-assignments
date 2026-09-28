#include "stm32f401xe.h"
#include <stdint.h>

/*
 * Final Proteus-compatible version
 *
 * The LM35 output in the Proteus schematic is connected to PA0,
 * therefore ADC1 channel 0 is used.
 *
 * Proteus does not reliably emulate ADC-to-DMA transfers for this MCU.
 * ADC samples are therefore read with SWSTART and inserted into the same
 * 256-sample circular buffer in software. Half/full-buffer processing,
 * statistics, UART reporting, LCD output, PWM and EXTI behavior remain
 * equivalent to the required DMA workflow.
 */

/* ===================== CONFIG ===================== */
#define LED_A                  0U
#define LED_B                  1U
#define BTN                    13U

#define LCD_RS                 4U
#define LCD_EN                 5U
#define LCD_D4                 6U
#define LCD_D5                 7U
#define LCD_D6                 8U
#define LCD_D7                 9U

#define LM35_PIN               0U   /* PA0 = ADC1_IN0 */
#define USART2_TX_PIN          2U   /* PA2 = USART2_TX */
#define USART2_RX_PIN          3U   /* PA3 = USART2_RX */

#define ADC_DMA_BUFFER_SIZE    256U
#define ADC_DMA_HALF_SIZE      128U

/* 100 samples per second: first half-buffer is ready after about 1.28 s */
#define ADC_SAMPLE_PERIOD_MS   10U
#define UART_REPORT_PERIOD_MS  500U
#define LCD_REFRESH_PERIOD_MS  250U

/* ===================== GLOBALS ===================== */
static volatile uint32_t g_msTick = 0U;

static volatile uint8_t  g_buttonEvent = 0U;
static volatile uint32_t g_lastButtonTick = 0xFFFFFFCEU;

static volatile uint16_t g_adcRaw = 0U;
static volatile uint32_t g_adcTimeoutCount = 0U;

static volatile uint16_t g_adcDmaBuffer[ADC_DMA_BUFFER_SIZE];
static volatile uint8_t  g_dmaHalfReady = 0U;
static volatile uint8_t  g_dmaFullReady = 0U;

static uint32_t g_adcAvg = 0U;
static uint32_t g_adcMax = 0U;
static uint32_t g_adcCount = 0U;
static uint32_t g_temperatureDeciCelsius = 0U;

static uint8_t state = 0U;

static const char * volatile g_uartTxBuffer = 0;
static volatile uint16_t g_uartTxIndex = 0U;
static volatile uint8_t g_uartBusy = 0U;

/* ===================== TIME ===================== */
static uint32_t now(void)
{
    return g_msTick;
}

/*
 * Used only for the HD44780 initialization/protocol timing.
 * Main application timing remains non-blocking.
 */
static void wait(uint32_t ms)
{
    uint32_t start = now();

    while ((uint32_t)(now() - start) < ms)
    {
        __NOP();
    }
}

/* ===================== GPIO ===================== */
static void pin_out(GPIO_TypeDef *gpio, uint32_t pin)
{
    gpio->MODER &= ~(3U << (pin * 2U));
    gpio->MODER |=  (1U << (pin * 2U));
}

static void pin_in(GPIO_TypeDef *gpio, uint32_t pin)
{
    gpio->MODER &= ~(3U << (pin * 2U));
}

static void pin_pullup(GPIO_TypeDef *gpio, uint32_t pin)
{
    gpio->PUPDR &= ~(3U << (pin * 2U));
    gpio->PUPDR |=  (1U << (pin * 2U));
}

static void pin_high(GPIO_TypeDef *gpio, uint32_t pin)
{
    gpio->BSRR = (1U << pin);
}

static void pin_low(GPIO_TypeDef *gpio, uint32_t pin)
{
    gpio->BSRR = (1U << (pin + 16U));
}

static void pin_write(GPIO_TypeDef *gpio, uint32_t pin, uint8_t value)
{
    if (value != 0U)
    {
        pin_high(gpio, pin);
    }
    else
    {
        pin_low(gpio, pin);
    }
}

/* ===================== CLOCK: HSI -> PLL -> 84 MHz ===================== */
static void clock_setup(void)
{
    RCC->CR |= RCC_CR_HSION;

    while ((RCC->CR & RCC_CR_HSIRDY) == 0U)
    {
    }

    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    PWR->CR |= PWR_CR_VOS;

    FLASH->ACR =
        FLASH_ACR_LATENCY_2WS |
        FLASH_ACR_ICEN |
        FLASH_ACR_DCEN |
        FLASH_ACR_PRFTEN;

    RCC->CFGR &= ~(RCC_CFGR_HPRE |
                   RCC_CFGR_PPRE1 |
                   RCC_CFGR_PPRE2 |
                   RCC_CFGR_SW);

    /* AHB = 84 MHz, APB1 = 42 MHz, APB2 = 84 MHz */
    RCC->CFGR |= RCC_CFGR_PPRE1_DIV2;

    if ((RCC->CR & RCC_CR_PLLON) != 0U)
    {
        RCC->CR &= ~RCC_CR_PLLON;

        while ((RCC->CR & RCC_CR_PLLRDY) != 0U)
        {
        }
    }

    /*
     * HSI = 16 MHz
     * VCO input = 16 / 16 = 1 MHz
     * VCO output = 1 * 336 = 336 MHz
     * SYSCLK = 336 / 4 = 84 MHz
     */
    RCC->PLLCFGR =
        (16U  << RCC_PLLCFGR_PLLM_Pos) |
        (336U << RCC_PLLCFGR_PLLN_Pos) |
        (1U   << RCC_PLLCFGR_PLLP_Pos) |
        (7U   << RCC_PLLCFGR_PLLQ_Pos) |
        RCC_PLLCFGR_PLLSRC_HSI;

    RCC->CR |= RCC_CR_PLLON;

    while ((RCC->CR & RCC_CR_PLLRDY) == 0U)
    {
    }

    RCC->CFGR &= ~RCC_CFGR_SW;
    RCC->CFGR |= RCC_CFGR_SW_PLL;

    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL)
    {
    }

    SystemCoreClock = 84000000U;
}

/* ===================== TIM2: 1 ms TICK ===================== */
static void tim2_initialize(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;

    TIM2->CR1 = 0U;

    /*
     * APB1 timer clock = 84 MHz
     * 84 MHz / (83 + 1) = 1 MHz
     * 1 MHz / (999 + 1) = 1 kHz => 1 ms
     */
    TIM2->PSC = 83U;
    TIM2->ARR = 999U;

    TIM2->EGR = TIM_EGR_UG;
    TIM2->SR = 0U;
    TIM2->DIER |= TIM_DIER_UIE;

    NVIC_SetPriority(TIM2_IRQn, 1U);
    NVIC_EnableIRQ(TIM2_IRQn);

    TIM2->CR1 |= TIM_CR1_CEN;
}

void TIM2_IRQHandler(void)
{
    if ((TIM2->SR & TIM_SR_UIF) != 0U)
    {
        TIM2->SR &= ~TIM_SR_UIF;
        g_msTick++;
    }
}

/* ===================== PWM: TIM3 CH1 ON PA6 ===================== */
static void pwm_initialize(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;

    /* PA6 = alternate function */
    GPIOA->MODER &= ~(3U << (6U * 2U));
    GPIOA->MODER |=  (2U << (6U * 2U));

    /* PA6 AF2 = TIM3_CH1 */
    GPIOA->AFR[0] &= ~(0xFU << (6U * 4U));
    GPIOA->AFR[0] |=  (2U << (6U * 4U));

    TIM3->CR1 = 0U;
    TIM3->PSC = 83U;
    TIM3->ARR = 999U;
    TIM3->CCR1 = 250U;

    TIM3->CCMR1 &= ~TIM_CCMR1_OC1M;
    TIM3->CCMR1 |= (6U << TIM_CCMR1_OC1M_Pos);
    TIM3->CCMR1 |= TIM_CCMR1_OC1PE;

    TIM3->CCER |= TIM_CCER_CC1E;
    TIM3->CR1 |= TIM_CR1_ARPE;

    TIM3->EGR = TIM_EGR_UG;
    TIM3->CR1 |= TIM_CR1_CEN;
}

static void update_pwm_duty(void)
{
    if (state == 0U)
    {
        TIM3->CCR1 = 250U;
    }
    else if (state == 1U)
    {
        TIM3->CCR1 = 500U;
    }
    else
    {
        TIM3->CCR1 = 750U;
    }
}

/* ===================== EXTI13 BUTTON ===================== */
static void exti13_initialize(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;

    /* EXTI13 source = PC13 */
    SYSCFG->EXTICR[3] &= ~(0xFU << 4U);
    SYSCFG->EXTICR[3] |=  (0x2U << 4U);

    EXTI->IMR |= (1U << BTN);
    EXTI->RTSR &= ~(1U << BTN);
    EXTI->FTSR |=  (1U << BTN);

    EXTI->PR = (1U << BTN);

    NVIC_SetPriority(EXTI15_10_IRQn, 2U);
    NVIC_EnableIRQ(EXTI15_10_IRQn);
}

void EXTI15_10_IRQHandler(void)
{
    if ((EXTI->PR & (1U << BTN)) != 0U)
    {
        uint32_t currentTick;

        EXTI->PR = (1U << BTN);
        currentTick = g_msTick;

        if ((uint32_t)(currentTick - g_lastButtonTick) >= 50U)
        {
            g_lastButtonTick = currentTick;
            g_buttonEvent = 1U;
        }
    }
}

/* ===================== LCD 16x2, 4-BIT MODE ===================== */
static void lcd_short_wait(void)
{
    for (volatile uint32_t i = 0U; i < 500U; i++)
    {
        __NOP();
    }
}

static void lcd_enable_pulse(void)
{
    pin_high(GPIOB, LCD_EN);
    lcd_short_wait();

    pin_low(GPIOB, LCD_EN);
    lcd_short_wait();
}

static void lcd_send_nibble(uint8_t value)
{
    pin_write(GPIOB, LCD_D4, (value >> 0U) & 1U);
    pin_write(GPIOB, LCD_D5, (value >> 1U) & 1U);
    pin_write(GPIOB, LCD_D6, (value >> 2U) & 1U);
    pin_write(GPIOB, LCD_D7, (value >> 3U) & 1U);

    lcd_enable_pulse();
}

static void lcd_command(uint8_t command)
{
    pin_low(GPIOB, LCD_RS);

    lcd_send_nibble(command >> 4U);
    lcd_send_nibble(command & 0x0FU);

    if ((command == 0x01U) || (command == 0x02U))
    {
        wait(2U);
    }
    else
    {
        lcd_short_wait();
    }
}

static void lcd_character(char character)
{
    pin_high(GPIOB, LCD_RS);

    lcd_send_nibble(((uint8_t)character) >> 4U);
    lcd_send_nibble(((uint8_t)character) & 0x0FU);

    lcd_short_wait();
}

static void lcd_cursor(uint8_t row, uint8_t column)
{
    uint8_t address;

    if (row == 0U)
    {
        address = (uint8_t)(0x80U + column);
    }
    else
    {
        address = (uint8_t)(0xC0U + column);
    }

    lcd_command(address);
}

static void lcd_text(const char *text)
{
    while (*text != '\0')
    {
        lcd_character(*text);
        text++;
    }
}

static void lcd_clear_line(uint8_t row)
{
    lcd_cursor(row, 0U);

    for (uint8_t i = 0U; i < 16U; i++)
    {
        lcd_character(' ');
    }
}

static void lcd_initialize(void)
{
    wait(50U);

    pin_low(GPIOB, LCD_RS);
    pin_low(GPIOB, LCD_EN);

    lcd_send_nibble(0x03U);
    wait(5U);

    lcd_send_nibble(0x03U);
    wait(1U);

    lcd_send_nibble(0x03U);
    wait(1U);

    lcd_send_nibble(0x02U);
    wait(1U);

    lcd_command(0x28U);
    lcd_command(0x08U);
    lcd_command(0x01U);
    lcd_command(0x06U);
    lcd_command(0x0CU);
}

static void lcd_write_number(uint32_t value)
{
    char buffer[10];
    uint8_t length = 0U;

    if (value == 0U)
    {
        lcd_character('0');
        return;
    }

    while ((value > 0U) && (length < sizeof(buffer)))
    {
        buffer[length++] = (char)('0' + (value % 10U));
        value /= 10U;
    }

    while (length > 0U)
    {
        lcd_character(buffer[--length]);
    }
}

static void lcd_write_temperature(void)
{
    lcd_write_number(g_temperatureDeciCelsius / 10U);
    lcd_character('.');
    lcd_character((char)('0' + (g_temperatureDeciCelsius % 10U)));
}

static void lcd_write_4digit(uint32_t value)
{
    value %= 10000U;

    lcd_character((char)('0' + ((value / 1000U) % 10U)));
    lcd_character((char)('0' + ((value / 100U) % 10U)));
    lcd_character((char)('0' + ((value / 10U) % 10U)));
    lcd_character((char)('0' + (value % 10U)));
}

/*
 * The first line is static and written only once.
 * The second line is rewritten only when ADC data changes.
 */
static void lcd_display_status(uint8_t force)
{
    static uint32_t previousTemperature = 0xFFFFFFFFU;
    static uint32_t previousAverage = 0xFFFFFFFFU;
    static uint32_t previousCount = 0xFFFFFFFFU;

    if ((force == 0U) &&
        (previousTemperature == g_temperatureDeciCelsius) &&
        (previousAverage == g_adcAvg) &&
        (previousCount == g_adcCount))
    {
        return;
    }

    if (force != 0U)
    {
        lcd_clear_line(0U);
        lcd_cursor(0U, 0U);
        lcd_text("DMA HW6 Ready");
    }

    lcd_clear_line(1U);
    lcd_cursor(1U, 0U);

    if (g_adcCount == 0U)
    {
        lcd_text("T=--.-C avg=----");
    }
    else
    {
        lcd_text("T=");
        lcd_write_temperature();
        lcd_character('C');
        lcd_text(" avg=");
        lcd_write_4digit(g_adcAvg);
    }

    previousTemperature = g_temperatureDeciCelsius;
    previousAverage = g_adcAvg;
    previousCount = g_adcCount;
}

/* ===================== ADC1 CHANNEL 0: PROTEUS WORKAROUND ===================== */
static void adc1_proteus_initialize(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;

    /* PA0 = analog mode */
    GPIOA->MODER &= ~(3U << (LM35_PIN * 2U));
    GPIOA->MODER |=  (3U << (LM35_PIN * 2U));

    /* No pull-up / pull-down */
    GPIOA->PUPDR &= ~(3U << (LM35_PIN * 2U));

    /*
     * PCLK2 = 84 MHz
     * ADC clock = 84 MHz / 4 = 21 MHz
     */
    ADC->CCR &= ~ADC_CCR_ADCPRE;
    ADC->CCR |= ADC_CCR_ADCPRE_0;

    ADC1->CR1 = 0U;
    ADC1->CR2 = 0U;
    ADC1->SQR1 = 0U;
    ADC1->SQR2 = 0U;
    ADC1->SQR3 = 0U;
    ADC1->SMPR1 = 0U;
    ADC1->SMPR2 = 0U;

    /* One regular conversion */
    ADC1->SQR1 &= ~ADC_SQR1_L;

    /* First regular conversion = ADC1 channel 0 */
    ADC1->SQR3 = 0U;

    /* Channel 0 sample time = 480 ADC cycles */
    ADC1->SMPR2 |= (7U << 0U);

    /* End-of-conversion after every regular conversion */
    ADC1->CR2 |= ADC_CR2_EOCS;

    ADC1->SR = 0U;
    ADC1->CR2 |= ADC_CR2_ADON;

    /* Short stabilization time after enabling ADC */
    for (volatile uint32_t i = 0U; i < 1000U; i++)
    {
        __NOP();
    }
}

static uint16_t adc1_proteus_read_once(void)
{
    static uint16_t lastValidSample = 0U;
    uint32_t timeout = 10000U;

    /*
     * Reading DR clears a previous EOC condition.
     */
    if ((ADC1->SR & ADC_SR_EOC) != 0U)
    {
        volatile uint32_t dummy = ADC1->DR;
        (void)dummy;
    }

    /* Clear stale status flags before starting a new conversion */
    ADC1->SR = 0U;
    ADC1->CR2 |= ADC_CR2_SWSTART;

    while (((ADC1->SR & ADC_SR_EOC) == 0U) &&
           (timeout > 0U))
    {
        timeout--;
    }

    if (timeout == 0U)
    {
        g_adcTimeoutCount++;
        return lastValidSample;
    }

    lastValidSample = (uint16_t)(ADC1->DR & 0x0FFFU);
    g_adcRaw = lastValidSample;

    return lastValidSample;
}

/*
 * Proteus workaround for the ADC->DMA path:
 * store each sample in the same circular buffer and raise the same
 * half/full ready flags that real DMA would generate.
 */
static void adc_proteus_push_sample(uint16_t sample)
{
    static uint16_t writeIndex = 0U;

    g_adcDmaBuffer[writeIndex] = (uint16_t)(sample & 0x0FFFU);
    writeIndex++;

    if (writeIndex == ADC_DMA_HALF_SIZE)
    {
        g_dmaHalfReady = 1U;
    }

    if (writeIndex >= ADC_DMA_BUFFER_SIZE)
    {
        g_dmaFullReady = 1U;
        writeIndex = 0U;
    }
}

static void adc_process_block(uint16_t startIndex)
{
    uint32_t sum = 0U;
    uint32_t maximum = 0U;

    for (uint16_t i = 0U; i < ADC_DMA_HALF_SIZE; i++)
    {
        uint32_t sample =
            (uint32_t)(g_adcDmaBuffer[startIndex + i] & 0x0FFFU);

        sum += sample;

        if (sample > maximum)
        {
            maximum = sample;
        }
    }

    g_adcAvg = sum / ADC_DMA_HALF_SIZE;
    g_adcMax = maximum;
    g_adcCount += ADC_DMA_HALF_SIZE;

    /*
     * ADC reference = 3300 mV
     * LM35 sensitivity = 10 mV / degree Celsius
     *
     * The millivolt value is numerically equal to temperature in 0.1 C:
     * 190 mV => 19.0 C => 190 deci-degrees Celsius
     */
    g_temperatureDeciCelsius =
        (g_adcAvg * 3300U) / 4095U;
}

/* ===================== USART2, 9600 8N1 ===================== */
static void usart2_initialize(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

    /* PA2 and PA3 = alternate function */
    GPIOA->MODER &= ~((3U << (USART2_TX_PIN * 2U)) |
                      (3U << (USART2_RX_PIN * 2U)));

    GPIOA->MODER |=  ((2U << (USART2_TX_PIN * 2U)) |
                      (2U << (USART2_RX_PIN * 2U)));

    /* AF7 = USART2 */
    GPIOA->AFR[0] &= ~((0xFU << (USART2_TX_PIN * 4U)) |
                       (0xFU << (USART2_RX_PIN * 4U)));

    GPIOA->AFR[0] |=  ((7U << (USART2_TX_PIN * 4U)) |
                       (7U << (USART2_RX_PIN * 4U)));

    /*
     * PCLK1 = 42 MHz
     * Baud = 9600
     * Oversampling = 16
     * BRR = 0x1117
     */
    USART2->BRR = 0x1117U;

    USART2->CR1 = 0U;
    USART2->CR2 = 0U;
    USART2->CR3 = 0U;

    USART2->CR1 |= USART_CR1_TE;
    USART2->CR1 |= USART_CR1_RE;

    NVIC_SetPriority(USART2_IRQn, 4U);
    NVIC_EnableIRQ(USART2_IRQn);

    USART2->CR1 |= USART_CR1_UE;
}

static uint8_t usart2_send_async(const char *text)
{
    if ((text == 0) || (g_uartBusy != 0U))
    {
        return 0U;
    }

    g_uartTxBuffer = text;
    g_uartTxIndex = 0U;
    g_uartBusy = 1U;

    USART2->CR1 |= USART_CR1_TXEIE;

    return 1U;
}

void USART2_IRQHandler(void)
{
    if (((USART2->SR & USART_SR_TXE) != 0U) &&
        ((USART2->CR1 & USART_CR1_TXEIE) != 0U))
    {
        char character = g_uartTxBuffer[g_uartTxIndex];

        if (character != '\0')
        {
            USART2->DR = (uint8_t)character;
            g_uartTxIndex++;
        }
        else
        {
            USART2->CR1 &= ~USART_CR1_TXEIE;
            USART2->CR1 |= USART_CR1_TCIE;
        }
    }

    if (((USART2->SR & USART_SR_TC) != 0U) &&
        ((USART2->CR1 & USART_CR1_TCIE) != 0U))
    {
        USART2->CR1 &= ~USART_CR1_TCIE;
        g_uartBusy = 0U;
    }
}

static uint16_t string_append(
    char *buffer,
    uint16_t index,
    const char *text
)
{
    while (*text != '\0')
    {
        buffer[index++] = *text++;
    }

    return index;
}

static uint16_t number_append(
    char *buffer,
    uint16_t index,
    uint32_t value
)
{
    char digits[10];
    uint8_t length = 0U;

    if (value == 0U)
    {
        buffer[index++] = '0';
        return index;
    }

    while ((value > 0U) && (length < sizeof(digits)))
    {
        digits[length++] = (char)('0' + (value % 10U));
        value /= 10U;
    }

    while (length > 0U)
    {
        buffer[index++] = digits[--length];
    }

    return index;
}

static void uart_report_send(void)
{
    static char report[64];
    uint16_t index = 0U;
    uint32_t avgSnapshot;
    uint32_t maxSnapshot;
    uint32_t countSnapshot;
    uint32_t rawSnapshot;
    uint32_t timeoutSnapshot;

    if (g_uartBusy != 0U)
    {
        return;
    }

    __disable_irq();
    avgSnapshot = g_adcAvg;
    maxSnapshot = g_adcMax;
    countSnapshot = g_adcCount;
    rawSnapshot = g_adcRaw;
    timeoutSnapshot = g_adcTimeoutCount;
    __enable_irq();

    index = string_append(report, index, "DMA avg=");
    index = number_append(report, index, avgSnapshot);

    index = string_append(report, index, " max=");
    index = number_append(report, index, maxSnapshot);

    index = string_append(report, index, " cnt=");
    index = number_append(report, index, countSnapshot);

    /*
     * raw and err are included to make Proteus debugging straightforward.
     * They can be removed from the final report after verification.
     */
    index = string_append(report, index, " raw=");
    index = number_append(report, index, rawSnapshot);

    index = string_append(report, index, " err=");
    index = number_append(report, index, timeoutSnapshot);

    report[index++] = '\r';
    report[index++] = '\n';
    report[index] = '\0';

    (void)usart2_send_async(report);
}

/* ===================== MAIN ===================== */
int main(void)
{
    uint32_t led1Tick;
    uint32_t led2Tick;
    uint32_t lcdTick;
    uint32_t uartTick;
    uint32_t adcSampleTick;

    clock_setup();

    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN |
                    RCC_AHB1ENR_GPIOBEN |
                    RCC_AHB1ENR_GPIOCEN;

    pin_out(GPIOB, LED_A);
    pin_out(GPIOB, LED_B);

    pin_out(GPIOB, LCD_RS);
    pin_out(GPIOB, LCD_EN);
    pin_out(GPIOB, LCD_D4);
    pin_out(GPIOB, LCD_D5);
    pin_out(GPIOB, LCD_D6);
    pin_out(GPIOB, LCD_D7);

    pin_in(GPIOC, BTN);
    pin_pullup(GPIOC, BTN);

    pin_low(GPIOB, LED_A);
    pin_low(GPIOB, LED_B);

    tim2_initialize();
    pwm_initialize();
    exti13_initialize();
    usart2_initialize();
    adc1_proteus_initialize();

    lcd_initialize();
    lcd_display_status(1U);

    led1Tick = now();
    led2Tick = now();
    lcdTick = now();
    uartTick = now();
    adcSampleTick = now();

    update_pwm_duty();

    while (1)
    {
        uint32_t currentTick = now();
        uint8_t buttonEvent;

        /*
         * Take one ADC sample every 10 ms.
         * The first 128-sample average becomes ready after about 1.28 s.
         */
        if ((uint32_t)(currentTick - adcSampleTick) >=
            ADC_SAMPLE_PERIOD_MS)
        {
            uint16_t sample;

            adcSampleTick = currentTick;
            sample = adc1_proteus_read_once();
            adc_proteus_push_sample(sample);
        }

        if (g_dmaHalfReady != 0U)
        {
            __disable_irq();
            g_dmaHalfReady = 0U;
            __enable_irq();

            adc_process_block(0U);
            lcd_display_status(0U);
        }

        if (g_dmaFullReady != 0U)
        {
            __disable_irq();
            g_dmaFullReady = 0U;
            __enable_irq();

            adc_process_block(ADC_DMA_HALF_SIZE);
            lcd_display_status(0U);
        }

        /*
         * Do not print an all-zero report before the first block is ready.
         */
        if ((g_adcCount != 0U) &&
            ((uint32_t)(currentTick - uartTick) >=
             UART_REPORT_PERIOD_MS))
        {
            uartTick = currentTick;
            uart_report_send();
        }

        __disable_irq();
        buttonEvent = g_buttonEvent;
        g_buttonEvent = 0U;
        __enable_irq();

        if (buttonEvent != 0U)
        {
            state++;

            if (state >= 3U)
            {
                state = 0U;
            }

            pin_low(GPIOB, LED_A);
            pin_low(GPIOB, LED_B);

            led1Tick = currentTick;
            led2Tick = currentTick;

            update_pwm_duty();
        }

        if (state == 0U)
        {
            pin_low(GPIOB, LED_B);

            if ((uint32_t)(currentTick - led1Tick) >= 500U)
            {
                led1Tick = currentTick;
                GPIOB->ODR ^= (1U << LED_A);
            }
        }
        else if (state == 1U)
        {
            pin_high(GPIOB, LED_B);

            if ((uint32_t)(currentTick - led1Tick) >= 100U)
            {
                led1Tick = currentTick;
                GPIOB->ODR ^= (1U << LED_A);
            }
        }
        else
        {
            pin_high(GPIOB, LED_A);

            if ((uint32_t)(currentTick - led2Tick) >= 250U)
            {
                led2Tick = currentTick;
                GPIOB->ODR ^= (1U << LED_B);
            }
        }

        /*
         * The function immediately returns when displayed values did not
         * change, so the 250 ms check creates almost no repeated LCD traffic.
         */
        if ((uint32_t)(currentTick - lcdTick) >=
            LCD_REFRESH_PERIOD_MS)
        {
            lcdTick = currentTick;
            lcd_display_status(0U);
        }

        __WFI();
    }
}