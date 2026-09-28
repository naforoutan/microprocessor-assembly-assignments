# Lab 02 - UART Serial Communication and PC COM App

Lab project containing an STM32F401 UART firmware project, a Proteus simulation, and a C# WinForms serial COM application.

## Project Details

| Item | Value |
| --- | --- |
| Target | STM32F401RETx |
| STM32 source | [STM32-Keil/lab2/Core/Src/main.c](STM32-Keil/lab2/Core/Src/main.c) |
| CubeMX file | [STM32-Keil/lab2/lab2.ioc](STM32-Keil/lab2/lab2.ioc) |
| Keil project | [STM32-Keil/lab2/MDK-ARM/lab2.uvprojx](STM32-Keil/lab2/MDK-ARM/lab2.uvprojx) |
| Existing HEX output | [STM32-Keil/lab2/MDK-ARM/lab2/lab2.hex](STM32-Keil/lab2/MDK-ARM/lab2/lab2.hex) |
| Proteus project | [prot-lab2.pdsprj](prot-lab2.pdsprj) |
| PC app project | [PC-COM-App/COM/COM.csproj](PC-COM-App/COM/COM.csproj) |
| Original archive | [Original-Archives/lab2-stm32-keil.zip](Original-Archives/lab2-stm32-keil.zip) |

## Notes

- The STM32 project was extracted from the original archive and the archive was preserved.
- The PC app is a C# WinForms serial-port tool.
- Keep the STM32 project structure under `STM32-Keil/lab2/` intact because the Keil project uses relative CubeMX paths.
