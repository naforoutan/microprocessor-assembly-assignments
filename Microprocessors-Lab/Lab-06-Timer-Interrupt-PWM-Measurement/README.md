# Lab 06 - Timer Interrupt and PWM Frequency Measurement

STM32F401 lab project using timer interrupts, GPIO EXTI input capture logic, ADC reporting, UART communication, and a PC COM application. The firmware measures a PWM signal by timing edges and reporting frequency/duty-cycle data over serial.

## Project Details

| Item | Value |
| --- | --- |
| Target | STM32F401RETx |
| Main source | [STM32-Keil/labadc/Core/Src/main.c](STM32-Keil/labadc/Core/Src/main.c) |
| CubeMX file | [STM32-Keil/labadc/labadc.ioc](STM32-Keil/labadc/labadc.ioc) |
| Keil project | [STM32-Keil/labadc/MDK-ARM/labadc.uvprojx](STM32-Keil/labadc/MDK-ARM/labadc.uvprojx) |
| Existing HEX output | [STM32-Keil/labadc/MDK-ARM/labadc/labadc.hex](STM32-Keil/labadc/MDK-ARM/labadc/labadc.hex) |
| PC app project | [PC-COM-App/COM/COM.csproj](PC-COM-App/COM/COM.csproj) |
| PC app solution | [PC-COM-App/COM.slnx](PC-COM-App/COM.slnx) |
| Python serial logger | [PC-COM-App/serial_logger.py](PC-COM-App/serial_logger.py) |

## What It Implements

The firmware generates PWM on TIM1 and measures the signal through PA0 EXTI using TIM2 as a high-resolution counter. TIM3 is used as a periodic interrupt source. The project also includes ADC reporting, USART2 serial communication, and the existing motor-control command protocol.

## Opening the Project

1. Open [STM32-Keil/labadc/MDK-ARM/labadc.uvprojx](STM32-Keil/labadc/MDK-ARM/labadc.uvprojx) in Keil uVision.
2. Open [STM32-Keil/labadc/labadc.ioc](STM32-Keil/labadc/labadc.ioc) in STM32CubeMX if pin or peripheral configuration needs to be inspected.
3. Open [PC-COM-App/COM.slnx](PC-COM-App/COM.slnx) or [PC-COM-App/COM/COM.csproj](PC-COM-App/COM/COM.csproj) in Visual Studio for the PC serial application.

## Notes

- The source comments document the required hardware connection: PA8 PWM output to PA0 EXTI input.
- Keep the `STM32-Keil/labadc/` structure intact because the Keil project uses relative paths.
- Generated Keil and Visual Studio temporary files should remain ignored, but the existing HEX file is preserved because it may be useful for reproducing the firmware state.
