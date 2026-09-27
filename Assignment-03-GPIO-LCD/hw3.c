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

static volatile uint32_t tick = 0;
static uint8_t state = 0;

/* ===================== SYS TICK ===================== */
void SysTick_Handler(void)
{
    tick++;
}

static uint32_t now(void)
{
    return tick;
}

static void wait(uint32_t ms)
{
    uint32_t t = now();
    while ((now() - t) < ms);
}

/* ===================== GPIO CORE ===================== */
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

static void hi(GPIO_TypeDef *g, int p)
{
    g->BSRR = (1U << p);
}

static void lo(GPIO_TypeDef *g, int p)
{
    g->BSRR = (1U << (p + 16));
}

static void write_pin(GPIO_TypeDef *port, uint32_t pin, uint8_t value)
{
    if (value)
        hi(port, pin);
    else
        lo(port, pin);
}

/* ===================== BUTTON (REWORKED) ===================== */
static uint8_t read_button(void)
{
    static uint8_t locked = 0;

    uint8_t pressed = ((GPIOC->IDR & (1U << BTN)) == 0);

    if (!locked && pressed)
    {
        wait(40);
        if (((GPIOC->IDR & (1U << BTN)) == 0))
        {
            locked = 1;
            return 1;
        }
    }

    if (!pressed)
        locked = 0;

    return 0;
}

/* ===================== CLOCK ===================== */
static void clock_setup(void)
{
    /* Enable HSI and wait until it becomes ready */
    RCC->CR |= RCC_CR_HSION;
    while ((RCC->CR & RCC_CR_HSIRDY) == 0);

    /* Enable power controller and select voltage scale 2 */
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    PWR->CR |= PWR_CR_VOS;

    /* Configure Flash for 84 MHz */
    FLASH->ACR = FLASH_ACR_LATENCY_2WS |
                 FLASH_ACR_ICEN |
                 FLASH_ACR_DCEN |
                 FLASH_ACR_PRFTEN;

    /* AHB = 84 MHz, APB1 = 42 MHz, APB2 = 84 MHz */
    RCC->CFGR &= ~(RCC_CFGR_HPRE |
                   RCC_CFGR_PPRE1 |
                   RCC_CFGR_PPRE2);

    RCC->CFGR |= RCC_CFGR_PPRE1_DIV2;

    /* Disable PLL before changing its configuration */
    if ((RCC->CR & RCC_CR_PLLON) != 0)
    {
        RCC->CR &= ~RCC_CR_PLLON;

        while ((RCC->CR & RCC_CR_PLLRDY) != 0);
    }

    /*
     * HSI = 16 MHz
     * PLLM = 16
     * PLLN = 336
     * PLLP = 4
     * SYSCLK = (16 / 16) * 336 / 4 = 84 MHz
     */
    RCC->PLLCFGR =
        (16U  << RCC_PLLCFGR_PLLM_Pos) |
        (336U << RCC_PLLCFGR_PLLN_Pos) |
        (1U   << RCC_PLLCFGR_PLLP_Pos) |
        (7U   << RCC_PLLCFGR_PLLQ_Pos) |
        RCC_PLLCFGR_PLLSRC_HSI;

    /* Enable PLL and wait for lock */
    RCC->CR |= RCC_CR_PLLON;
    while ((RCC->CR & RCC_CR_PLLRDY) == 0);

    /* Select PLL as system clock */
    RCC->CFGR &= ~RCC_CFGR_SW;
    RCC->CFGR |= RCC_CFGR_SW_PLL;

    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);

    SystemCoreClock = 84000000U;
    SysTick_Config(SystemCoreClock / 1000U);
}

static void lcd_short_wait(void)
{
    for (volatile uint32_t i = 0; i < 500; i++)
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
    write_pin(GPIOB, LCD_D4, (value >> 0) & 1U);
    write_pin(GPIOB, LCD_D5, (value >> 1) & 1U);
    write_pin(GPIOB, LCD_D6, (value >> 2) & 1U);
    write_pin(GPIOB, LCD_D7, (value >> 3) & 1U);

    lcd_enable_pulse();
}

static void lcd_command(uint8_t command)
{
    lo(GPIOB, LCD_RS);

    lcd_send_nibble(command >> 4);
    lcd_send_nibble(command & 0x0F);

    if (command == 0x01 || command == 0x02)
        wait(2);
    else
        lcd_short_wait();
}

static void lcd_character(char character)
{
    hi(GPIOB, LCD_RS);

    lcd_send_nibble((uint8_t)character >> 4);
    lcd_send_nibble((uint8_t)character & 0x0F);

    lcd_short_wait();
}

static void lcd_cursor(uint8_t row, uint8_t column)
{
    uint8_t address;

    if (row == 0)
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
    for (uint8_t i = 0; i < 16; i++)
    {
        lcd_character(' ');
    }
}

static void lcd_initialize(void)
{
    wait(50);

    lo(GPIOB, LCD_RS);
    lo(GPIOB, LCD_EN);

    lcd_send_nibble(0x03);
    wait(5);

    lcd_send_nibble(0x03);
    wait(5);

    lcd_send_nibble(0x03);
    wait(1);

    lcd_send_nibble(0x02);
    wait(1);

    lcd_command(0x28);
    lcd_command(0x08);
    lcd_command(0x01);
    lcd_command(0x06);
    lcd_command(0x0C);
}

static void lcd_display_mode(uint8_t current_mode)
{
    lcd_cursor(0, 0);
    lcd_clear_line();

    lcd_cursor(0, 0);

    if (current_mode == 0)
        lcd_text("Mode : 0");
    else
        lcd_text("Mode : 1");

    lcd_cursor(1, 0);
    lcd_clear_line();

    lcd_cursor(1, 0);
    lcd_text("HW3 Ready");
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
	
		lcd_initialize();
		lcd_display_mode(state);

    uint32_t tA = now();
    uint32_t tB = now();

    while (1)
    {
        if (read_button())
				{
						state ^= 1U;

						lcd_display_mode(state);

						tA = now();
						tB = now();
				}

        if (state == 0)
        {
            lo(GPIOB, LED_B);

            if (now() - tA > 500)
            {
                tA = now();
                (GPIOB->ODR & (1 << LED_A)) ? lo(GPIOB, LED_A) : hi(GPIOB, LED_A);
            }
        }
        else
        {
            hi(GPIOB, LED_A);

            if (now() - tB > 250)
            {
                tB = now();
                (GPIOB->ODR & (1 << LED_B)) ? lo(GPIOB, LED_B) : hi(GPIOB, LED_B);
            }
        }
    }
}