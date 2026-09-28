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

/* ===================== GLOBALS ===================== */
static volatile uint32_t g_msTick = 0;
static volatile uint8_t g_buttonEvent = 0;
static volatile uint32_t g_lastButtonTick = 0xFFFFFFCEU;

static uint8_t state = 0;

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


/* ===================== EXTI13 ===================== */
static void exti13_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;

    SYSCFG->EXTICR[3] &= ~(SYSCFG_EXTICR4_EXTI13);

    EXTI->IMR  |= (1U << BTN);
    EXTI->FTSR |= (1U << BTN);

    EXTI->PR = (1U << BTN);

    NVIC_EnableIRQ(EXTI15_10_IRQn);
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

static void lcd_display_status(void)
{
    lcd_cursor(0, 0);
    lcd_clear_line();

    lcd_cursor(0, 0);
    lcd_text("St:");
    lcd_character((char)('0' + state));
    lcd_text(" D:");
    lcd_write_number(get_current_duty());
    lcd_character('%');

    lcd_cursor(1, 0);
    lcd_clear_line();

    lcd_cursor(1, 0);
    lcd_text("Tick:");
    lcd_write_number(g_msTick);
}
/* ===================== MAIN ===================== */
int main(void)
{ 
		uint32_t led2Tick = now();
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

    lcd_initialize();
    lcd_display_status();
		    uint32_t led1Tick = now();
    
    uint32_t lcdTick = now();

    update_pwm_duty();

    while (1)
    {
        uint32_t currentTick = now();

        if (g_buttonEvent != 0)
        {
            __disable_irq();
            g_buttonEvent = 0;
            __enable_irq();

            state++;

            if (state >= 3)
                state = 0;

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
