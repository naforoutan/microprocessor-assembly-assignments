# Lab 03 - UART Motor PWM Control

STM32F401 lab project for controlling DC motor speed and direction using PWM and a UART frame protocol. A C# WinForms COM application archive has been extracted and preserved.

## Project Details

| Item | Value |
| --- | --- |
| Target | STM32F401RETx |
| Main source | [Core/Src/main.c](Core/Src/main.c) |
| CubeMX file | [lab2.ioc](lab2.ioc) |
| Keil project | [MDK-ARM/lab2.uvprojx](MDK-ARM/lab2.uvprojx) |
| Existing HEX output | [MDK-ARM/lab2/lab2.hex](MDK-ARM/lab2/lab2.hex) |
| PC app project | [PC-COM-App/COM/COM/COM.csproj](PC-COM-App/COM/COM/COM.csproj) |
| Original PC app archive | [Original-Archives/COM (1).zip](<Original-Archives/COM (1).zip>) |

## What It Implements

The firmware defines a UART frame protocol with start/end bytes, CRC, device addressing, motor speed commands, direction control, status, and echo commands. It uses TIM1 PWM channels and GPIO direction pins for two motors.

## Notes

- The Keil project remains in `MDK-ARM/` and references source files through relative paths.
- No Proteus project file was found directly in this lab folder.
