# Microprocessors Lab

Laboratory assignments and experiments for the Microprocessors Lab course. Projects are preserved in their original STM32CubeMX, Keil uVision, Proteus, and Visual Studio structures where changing internals could break builds or simulations.

## Repository Contents

| Lab | Topic | Target / Platform | Tools | Directory |
| --- | --- | --- | --- | --- |
| Lab 01 | GPIO output blink / basic STM32 setup | STM32F401RETx | STM32CubeMX, Keil uVision, Proteus | [Lab-01-GPIO-Blink](Lab-01-GPIO-Blink/) |
| Lab 02 | UART serial communication with PC COM application | STM32F401RETx, Windows | STM32CubeMX, Keil uVision, Proteus, C# WinForms | [Lab-02-UART-Serial-COM](Lab-02-UART-Serial-COM/) |
| Lab 03 | UART protocol for DC motor PWM and direction control | STM32F401RETx, Windows | STM32CubeMX, Keil uVision, C# WinForms | [Lab-03-UART-Motor-PWM](Lab-03-UART-Motor-PWM/) |
| Lab 04 | ADC-based motor speed/reporting with UART protocol | STM32F401RETx, Windows | STM32CubeMX, Keil uVision, C# WinForms | [Lab-04-ADC-Motor-Control](Lab-04-ADC-Motor-Control/) |
| Lab 05 | PWM frequency measurement with EXTI interrupt | STM32F401RETx, Windows | STM32CubeMX, Keil uVision, C# WinForms | [Lab-05-Interrupt-Control](Lab-05-Interrupt-Control/) |

## Opening Projects

1. Open the lab directory.
2. For STM32 firmware, open the `.uvprojx` file from the listed `MDK-ARM` folder in Keil uVision.
3. If an `.ioc` file is present, it can be opened in STM32CubeMX.
4. For Proteus simulations, open the `.pdsprj` file from its current directory.
5. For PC serial tools, open the `.csproj` or `.slnx` file in Visual Studio.

## Preservation Notes

- Existing source files, Keil project files, CubeMX files, Proteus project files, Visual Studio project files, archives, and required HEX files were preserved.
- Original zip submissions are kept in `Original-Archives/` where present.
- Keil and Proteus were not run in this environment; only file/link/reference checks were performed.
