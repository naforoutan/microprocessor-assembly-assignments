# Lab 05 - PWM Frequency Measurement with EXTI Interrupt

STM32F401 lab project using GPIO EXTI interrupts to measure PWM frequency and duty cycle. It also keeps ADC, UART protocol, motor PWM, and C# COM app components from the previous lab workflow.

## Project Details

| Item | Value |
| --- | --- |
| Target | STM32F401RETx |
| Main source | [STM32-Keil/labadc/Core/Src/main.c](STM32-Keil/labadc/Core/Src/main.c) |
| CubeMX file | [STM32-Keil/labadc/labadc.ioc](STM32-Keil/labadc/labadc.ioc) |
| Keil project | [STM32-Keil/labadc/MDK-ARM/labadc.uvprojx](STM32-Keil/labadc/MDK-ARM/labadc.uvprojx) |
| Existing HEX output | [STM32-Keil/labadc/MDK-ARM/labadc/labadc.hex](STM32-Keil/labadc/MDK-ARM/labadc/labadc.hex) |
| PC app project | [PC-COM-App/COM/COM/COM.csproj](PC-COM-App/COM/COM/COM.csproj) |
| Original STM32 archive | [Original-Archives/interrupt-stm32-keil.zip](Original-Archives/interrupt-stm32-keil.zip) |
| Original PC app archive | [Original-Archives/COM (3).zip](<Original-Archives/COM (3).zip>) |

## What It Implements

The firmware measures a PWM signal by connecting PA8 PWM output to PA0 EXTI input, timing edges with TIM2, and reporting frequency/duty-cycle data over USART2. It also includes ADC reporting and motor-control protocol behavior.

## Notes

- The STM32 firmware was extracted from the original archive and the archive was preserved.
- The source comments document the required hardware connection: PA8 to PA0.
- Keep the `STM32-Keil/labadc/` structure intact because the Keil project uses relative paths.
