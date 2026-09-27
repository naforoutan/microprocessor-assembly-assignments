#include "stm32f401xe.h"
#include <stdint.h>

/* ===================== CONFIG ===================== */
#define LED_A 0
#define LED_B 1
#define BTN   13

#define LCD_RS 4
#define LCD_EN 5
#define LCD_D4 6
#define LCD_D5 7
#define LCD_D6 8
#define LCD_D7 9

#define LM35_PIN       1U
#define USART2_TX_PIN  2U
#define USART2_RX_PIN  3U

/* ===================== GLOBALS ===================== */
static volatile uint32_t g_msTick = 0;
static volatile uint8_t g_buttonEvent = 0;
static volatile uint32_t g_lastButtonTick = 0xFFFFFFCEU;

static volatile uint16_t g_adcRaw = 0U;

static uint32_t g_temperatureMilliVolt = 0U;
static uint32_t g_temperatureCelsius = 0U;
static const char * volatile g_uartTxBuffer = 0;
static volatile uint16_t g_uartTxIndex = 0U;
static volatile uint8_t g_uartBusy = 0U;

static volatile uint8_t g_adcBusy = 0U;

static uint8_t state = 0;
static uint32_t g_temperatureDeciCelsius = 0U;
/* ===================== TIM2 ISR ===================== */

static uint32_t now(void)
{
    return g_msTick;
}

static void wait(uint32_t ms)
{
    uint32_t start = now();

    while ((now() - start) < ms)
    {
        __NOP();
    }
}


/* ===================== GPIO ===================== */
static void pin_out(GPIO_TypeDef *g, int p)
{
    g->MODER &= ~(3U << (p * 2));
    g->MODER |=  (1U << (p * 2));
}

static void pin_in(GPIO_TypeDef *g, int p)
{
    g->MODER &= ~(3U << (p * 2));
}

static void up(GPIO_TypeDef *g, int p)
{
    g->PUPDR &= ~(3U << (p * 2));
    g->PUPDR |=  (1U << (p * 2));
}

static void hi(GPIO_TypeDef *port, uint32_t pin)
{
    port->BSRR = (1U << pin);
}

static void lo(GPIO_TypeDef *port, uint32_t pin)
{
    port->BSRR = (1U << (pin + 16U));
}

static void write_pin(GPIO_TypeDef *port, uint32_t pin, uint8_t value)
{
    if (value != 0U)
        hi(port, pin);
    else
        lo(port, pin);
}

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

    RCC->CFGR |= RCC_CFGR_PPRE1_DIV2;

    if ((RCC->CR & RCC_CR_PLLON) != 0U)
    {
        RCC->CR &= ~RCC_CR_PLLON;

        while ((RCC->CR & RCC_CR_PLLRDY) != 0U)
        {
        }
    }

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


static void pwm_initialize(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;

    /* PA6 = Alternate Function */
    GPIOA->MODER &= ~(3U << (6U * 2U));
    GPIOA->MODER |=  (2U << (6U * 2U));

    /* PA6 AF2 = TIM3_CH1 */
    GPIOA->AFR[0] &= ~(0xFU << (6U * 4U));
    GPIOA->AFR[0] |=  (2U << (6U * 4U));

    TIM3->CR1 = 0;

    /*
     * Timer clock = 84 MHz
     * 84 MHz / 84 = 1 MHz
     * 1 MHz / 1000 = 1 kHz
     */
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
    if (state == 0)
        TIM3->CCR1 = 250U;
    else if (state == 1)
        TIM3->CCR1 = 500U;
    else
        TIM3->CCR1 = 750U;
}

static void tim2_initialize(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;

    TIM2->CR1 = 0;

    /*
     * APB1 timer clock = 84 MHz
     * 84 MHz / 84 = 1 MHz
     * 1 MHz / 1000 = 1 kHz = 1 ms
     */
    TIM2->PSC = 83U;
    TIM2->ARR = 999U;

    TIM2->EGR = TIM_EGR_UG;
    TIM2->SR = 0;

    TIM2->DIER |= TIM_DIER_UIE;

    NVIC_SetPriority(TIM2_IRQn, 1);
    NVIC_EnableIRQ(TIM2_IRQn);

    TIM2->CR1 |= TIM_CR1_CEN;
}

void TIM2_IRQHandler(void)
{
    if ((TIM2->SR & TIM_SR_UIF) != 0)
    {
        TIM2->SR &= ~TIM_SR_UIF;
        g_msTick++;
    }
}

static void exti13_initialize(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;

    /*
     * EXTICR[3] controls EXTI12 to EXTI15.
     * EXTI13 is located in bits 7:4.
     * Value 2 selects Port C.
     */
    SYSCFG->EXTICR[3] &= ~(0xFU << 4);
    SYSCFG->EXTICR[3] |=  (0x2U << 4);

    EXTI->IMR |= (1U << BTN);

    EXTI->RTSR &= ~(1U << BTN);
    EXTI->FTSR |=  (1U << BTN);

    EXTI->PR = (1U << BTN);

    NVIC_SetPriority(EXTI15_10_IRQn, 2);
    NVIC_EnableIRQ(EXTI15_10_IRQn);
}

void EXTI15_10_IRQHandler(void)
{
    if ((EXTI->PR & (1U << BTN)) != 0)
    {
        EXTI->PR = (1U << BTN);

        uint32_t currentTick = g_msTick;

        if ((uint32_t)(currentTick - g_lastButtonTick) >= 50U)
        {
            g_lastButtonTick = currentTick;
            g_buttonEvent = 1;
        }
    }
}

static void lcd_short_wait(void)
{
    for (volatile uint32_t i = 0; i < 500U; i++)
    {
        __NOP();
    }
}

static void lcd_enable_pulse(void)
{
    hi(GPIOB, LCD_EN);
    lcd_short_wait();

    lo(GPIOB, LCD_EN);
    lcd_short_wait();
}

static void lcd_send_nibble(uint8_t value)
{
    write_pin(GPIOB, LCD_D4, (value >> 0U) & 1U);
    write_pin(GPIOB, LCD_D5, (value >> 1U) & 1U);
    write_pin(GPIOB, LCD_D6, (value >> 2U) & 1U);
    write_pin(GPIOB, LCD_D7, (value >> 3U) & 1U);

    lcd_enable_pulse();
}

static void lcd_command(uint8_t command)
{
    lo(GPIOB, LCD_RS);

    lcd_send_nibble(command >> 4U);
    lcd_send_nibble(command & 0x0FU);

    if ((command == 0x01U) || (command == 0x02U))
        wait(2U);
    else
        lcd_short_wait();
}

static void lcd_character(char character)
{
    hi(GPIOB, LCD_RS);

    lcd_send_nibble(((uint8_t)character) >> 4U);
    lcd_send_nibble(((uint8_t)character) & 0x0FU);

    lcd_short_wait();
}

static void lcd_cursor(uint8_t row, uint8_t column)
{
    uint8_t address;

    if (row == 0U)
        address = 0x80U + column;
    else
        address = 0xC0U + column;

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

static void lcd_clear_line(void)
{
    for (uint8_t i = 0; i < 16U; i++)
    {
        lcd_character(' ');
    }
}

static void lcd_initialize(void)
{
    wait(50U);

    lo(GPIOB, LCD_RS);
    lo(GPIOB, LCD_EN);

    lcd_send_nibble(0x03U);
    wait(5U);

    lcd_send_nibble(0x03U);
    wait(5U);

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
    uint8_t length = 0;

    if (value == 0)
    {
        lcd_character('0');
        return;
    }

    while (value > 0 && length < sizeof(buffer))
    {
        buffer[length++] = (char)('0' + (value % 10U));
        value /= 10U;
    }

    while (length > 0)
    {
        lcd_character(buffer[--length]);
    }
}

static uint8_t get_current_duty(void)
{
    if (state == 0)
        return 25;

    if (state == 1)
        return 50;

    return 75;
}


static void adc1_initialize(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;

    /* PA1 = Analog mode */
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
    ADC1->SMPR2 = 0U;

    /* Single conversion, software trigger, EOC after conversion */
    ADC1->CR2 |= ADC_CR2_EOCS;

    /* One conversion: ADC channel 1 */
    ADC1->SQR3 = 1U;

    /* Channel 1 sample time = 56 cycles */
    ADC1->SMPR2 |= ADC_SMPR2_SMP1_1 |
                   ADC_SMPR2_SMP1_0;

    ADC1->SR = 0U;

    ADC1->CR2 |= ADC_CR2_ADON;
}

static void adc1_start_conversion(void)
{
    if (g_adcBusy != 0U)
        return;

    g_adcBusy = 1U;

    ADC1->SR = 0U;
    ADC1->CR2 |= ADC_CR2_SWSTART;
}


static void temperature_update(uint16_t adcRaw)
{
    g_temperatureMilliVolt =
        ((uint32_t)adcRaw * 3300U) / 4095U;

    g_temperatureDeciCelsius = g_temperatureMilliVolt;
    g_temperatureCelsius = g_temperatureDeciCelsius / 10U;
}
static void adc1_process_conversion(void)
{
    if ((ADC1->SR & ADC_SR_EOC) != 0U)
    {
        g_adcRaw = (uint16_t)ADC1->DR;
        g_adcBusy = 0U;

        temperature_update(g_adcRaw);
    }

    if ((ADC1->SR & ADC_SR_OVR) != 0U)
    {
        ADC1->SR &= ~ADC_SR_OVR;
        g_adcBusy = 0U;
    }
}
static void usart2_initialize(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

    /* PA2 and PA3 = Alternate Function */
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
     * PCLK1 = 42MHz
     * Baud = 9600
     * Oversampling = 16
     * BRR = 0x1117
     */
    USART2->BRR = 0x1117U;

    USART2->CR1 = 0U;
    USART2->CR2 = 0U;
    USART2->CR3 = 0U;

    /* 8 data bits, no parity, 1 stop bit */
    USART2->CR1 |= USART_CR1_TE;
    USART2->CR1 |= USART_CR1_RE;

    NVIC_SetPriority(USART2_IRQn, 4U);
    NVIC_EnableIRQ(USART2_IRQn);

    USART2->CR1 |= USART_CR1_UE;
}

static uint8_t usart2_send_async(const char *text)
{
    if ((text == 0) || (g_uartBusy != 0U))
        return 0U;

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
static uint16_t number_4digit_append(
    char *buffer,
    uint16_t index,
    uint32_t value
)
{
    buffer[index++] = (char)('0' + ((value / 1000U) % 10U));
    buffer[index++] = (char)('0' + ((value / 100U) % 10U));
    buffer[index++] = (char)('0' + ((value / 10U) % 10U));
    buffer[index++] = (char)('0' + (value % 10U));

    return index;
}


static void uart_report_send(void)
{
    uint16_t adcRawSnapshot;
    uint32_t temperatureSnapshot;

    static char report[48];
    uint16_t index = 0U;

    if (g_uartBusy != 0U)
        return;

    __disable_irq();
    adcRawSnapshot = g_adcRaw;
    temperatureSnapshot = g_temperatureDeciCelsius;
    __enable_irq();

    index = string_append(report, index, "T=");
    index = number_append(report, index, temperatureSnapshot / 10U);
    report[index++] = '.';
    index = number_append(report, index, temperatureSnapshot % 10U);

    index = string_append(report, index, " raw=");
    index = number_4digit_append(report, index, adcRawSnapshot);

    index = string_append(report, index, " st=");
    index = number_append(report, index, state);

    index = string_append(report, index, " duty=");
    index = number_append(report, index, get_current_duty());

    report[index++] = '\r';
    report[index++] = '\n';
    report[index] = '\0';

    usart2_send_async(report);
}

static void lcd_write_temperature(void)
{
    lcd_write_number(g_temperatureDeciCelsius / 10U);
    lcd_character('.');
    lcd_write_number(g_temperatureDeciCelsius % 10U);
}

static void lcd_write_4digit(uint32_t value)
{
    lcd_character((char)('0' + ((value / 1000U) % 10U)));
    lcd_character((char)('0' + ((value / 100U) % 10U)));
    lcd_character((char)('0' + ((value / 10U) % 10U)));
    lcd_character((char)('0' + (value % 10U)));
}

static void lcd_display_status(void)
{
    lcd_cursor(0U, 0U);
    lcd_clear_line();

    lcd_cursor(0U, 0U);
    lcd_text("St:");
    lcd_character((char)('0' + state));
    lcd_text(" D:");
    lcd_write_number(get_current_duty());

    lcd_cursor(1U, 0U);
    lcd_clear_line();

    lcd_cursor(1U, 0U);
    lcd_text("T=");
    lcd_write_temperature();
    lcd_character('C');

    lcd_text(" r=");
    lcd_write_4digit(g_adcRaw);
}


/* ===================== MAIN ===================== */
int main(void)
{ 
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
    up(GPIOC, BTN);

    lo(GPIOB, LED_A);
    lo(GPIOB, LED_B);

    tim2_initialize();
    pwm_initialize();
    exti13_initialize();

		adc1_initialize();
		usart2_initialize();

		
    lcd_initialize();
    lcd_display_status();
		
		uint32_t led1Tick = now();
		uint32_t led2Tick = now();
    uint32_t lcdTick = now();
		
		uint32_t adcTick = now() - 200U;
		uint32_t uartTick = now();

    update_pwm_duty();

    while (1)
    {
        uint32_t currentTick = now();
			
				adc1_process_conversion();
			
				if ((currentTick - adcTick) >= 200U)
				{
					adcTick = currentTick;
					adc1_start_conversion();
				}
					
				if ((currentTick - uartTick) >= 500U)
				{
						uartTick = currentTick;
						uart_report_send();
				}
				uint8_t buttonEvent;

				__disable_irq();
				buttonEvent = g_buttonEvent;
				g_buttonEvent = 0U;
				__enable_irq();

				if (buttonEvent != 0U)
				{
						state++;

						if (state >= 3U)
								state = 0U;

						lo(GPIOB, LED_A);
						lo(GPIOB, LED_B);

						led1Tick = currentTick;
						led2Tick = currentTick;
						lcdTick = currentTick;

						update_pwm_duty();
						lcd_display_status();
				}
        if (state == 0)
        {
            lo(GPIOB, LED_B);

            if ((currentTick - led1Tick) >= 500U)
            {
                led1Tick = currentTick;
                GPIOB->ODR ^= (1U << LED_A);
            }
        }
        else if (state == 1)
        {
            hi(GPIOB, LED_B);

            if ((currentTick - led1Tick) >= 100U)
            {
                led1Tick = currentTick;
                GPIOB->ODR ^= (1U << LED_A);
            }
        }
        else
				{
						hi(GPIOB, LED_A);

						if ((currentTick - led2Tick) >= 250U)
						{
								led2Tick = currentTick;
								GPIOB->ODR ^= (1U << LED_B);
						}
				}

        if ((currentTick - lcdTick) >= 250U)
        {
            lcdTick = currentTick;
            lcd_display_status();
        }

        __WFI();
    }
}
