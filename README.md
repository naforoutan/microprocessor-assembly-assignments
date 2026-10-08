# Microprocessor Assembly Assignments

STM32F401 coursework and lab projects covering ARM assembly, GPIO, timers, PWM, interrupts, ADC, UART communication, LCD output, and PC-side serial tools. The repository keeps the original embedded-project structure so the Keil, Proteus, STM32CubeMX, and Visual Studio files remain usable.

## What's Included

- Main course assignments from HW2 through HW6 in [Microprocessors](Microprocessors/).
- Laboratory exercises from Lab 01 through Lab 05 in [Microprocessors-Lab](Microprocessors-Lab/).
- Keil uVision projects (`.uvprojx`) for STM32 firmware builds.
- Proteus simulation projects (`.pdsprj`) where available.
- STM32CubeMX/HAL and CMSIS support files needed by the preserved projects.
- C# WinForms COM-port utilities for UART-based labs.
- Existing HEX outputs used by several Proteus simulations.

## Project Map

| Area | Folder | Focus |
| --- | --- | --- |
| Course assignments | [Microprocessors](Microprocessors/) | ARM assembly and CMSIS C assignments for STM32F401 targets. |
| Lab exercises | [Microprocessors-Lab](Microprocessors-Lab/) | STM32CubeMX/HAL projects, UART tools, motor control, ADC, and interrupt experiments. |

## Main Assignments

| Project | Topic | Source / Project Files |
| --- | --- | --- |
| [HW2 - Checksum](Microprocessors/Assignment-02-Checksum/) | Bytewise and wordwise checksum comparison in ARM assembly. | `HW2_Checksum.s`, `HW2_Checksum.uvprojx` |
| [HW3 - GPIO LCD](Microprocessors/Assignment-03-GPIO-LCD/) | GPIO setup, SysTick timing, button input, LEDs, and character LCD output. | `hw3.c`, `HW3_CMSIS.uvprojx`, `hw3.pdsprj` |
| [HW4 - Timer PWM Interrupts](Microprocessors/Assignment-04-Timer-PWM-Interrupts/) | TIM2 timing, TIM3 PWM, EXTI13 button interrupt, LEDs, and LCD status. | `hw4.c`, `HW4_keil.uvprojx`, `hw4.pdsprj` |
| [HW5 - ADC UART Temperature](Microprocessors/Assignment-05-ADC-UART-Temperature/) | ADC temperature sampling, USART2 reports, PWM, EXTI, LEDs, and LCD output. | `hw5.c`, `HW5_keil.uvprojx`, `hw5.pdsprj` |
| [HW6 - ADC DMA Buffering](Microprocessors/Assignment-06-ADC-DMA-Buffering/) | ADC buffering, DMA-style block processing, UART statistics, PWM, EXTI, and LCD status. | `hw6.c`, `HW6_keil.uvprojx`, `hw6.pdsprj` |

## Lab Exercises

| Project | Topic | Source / Project Files |
| --- | --- | --- |
| [Lab 01 - GPIO Blink](Microprocessors-Lab/Lab-01-GPIO-Blink/) | Basic CubeMX/HAL firmware that toggles GPIOC pin 3. | `lab1/Core/Src/main.c`, `lab1/lab1.ioc`, `lab1/MDK-ARM/lab1.uvprojx` |
| [Lab 02 - UART Serial COM](Microprocessors-Lab/Lab-02-UART-Serial-COM/) | STM32 USART2 firmware, Proteus simulation, and C# serial COM app. | `STM32-Keil/lab2/Core/Src/main.c`, `prot-lab2.pdsprj`, `PC-COM-App/COM/COM.csproj` |
| [Lab 03 - UART Motor PWM](Microprocessors-Lab/Lab-03-UART-Motor-PWM/) | UART frame protocol for DC motor speed, direction, status, echo, and TIM1 PWM. | `Core/Src/main.c`, `MDK-ARM/lab2.uvprojx`, `PC-COM-App/COM/COM/COM.csproj` |
| [Lab 04 - ADC Motor Control](Microprocessors-Lab/Lab-04-ADC-Motor-Control/) | ADC-based speed/reporting workflow with UART protocol and motor PWM control. | `labadc/Core/Src/main.c`, `labadc/labadc.ioc`, `labadc/MDK-ARM/labadc.uvprojx` |
| [Lab 05 - Interrupt Control](Microprocessors-Lab/Lab-05-Interrupt-Control/) | PWM frequency and duty-cycle measurement using EXTI edge timing and UART reports. | `STM32-Keil/labadc/Core/Src/main.c`, `STM32-Keil/labadc/MDK-ARM/labadc.uvprojx` |

## Topics Covered

- ARM assembly routines and memory-buffer processing.
- STM32F401 clock, GPIO, SysTick, timer, PWM, EXTI, ADC, DMA-style buffering, and USART setup.
- Character LCD status interfaces.
- UART frame protocols for PC-to-board communication.
- Motor speed and direction control with PWM.
- Proteus-based firmware simulation workflows.

## Tools and Targets

| Tool / Platform | Used For |
| --- | --- |
| Keil uVision / MDK-ARM | Building and debugging STM32 firmware projects. |
| STM32CubeMX | Viewing or regenerating CubeMX-based lab configurations. |
| Proteus | Opening preserved simulation projects and loading existing HEX firmware. |
| Visual Studio | Building the C# WinForms COM-port helper applications. |
| STM32F401RBTx | Target used by HW2. |
| STM32F401RETx | Target used by the remaining assignment and lab firmware projects. |

## How to Use This Repository

1. Choose either [Microprocessors](Microprocessors/) or [Microprocessors-Lab](Microprocessors-Lab/).
2. Open the project folder and read its local `README.md` for exact file names and notes.
3. Open the `.uvprojx` file in Keil uVision to build or debug firmware.
4. Open `.ioc` files in STM32CubeMX only if you need to inspect or regenerate HAL configuration.
5. Open `.pdsprj` files in Proteus from their original folders so relative firmware paths remain valid.
6. Open `.csproj` or `.slnx` files in Visual Studio for the PC COM-port applications.

## Repository Notes

- Generated folders such as `Drivers/`, `RTE/`, `Core/`, `MDK-ARM/`, `Objects/`, and `Listings/` are intentionally kept because embedded IDE projects often depend on them.
- Some Keil projects may contain machine-specific STM32Cube paths such as `C:\Users\ASUS\STM32Cube\Repository\...`; update those paths in Keil if your local environment uses a different Cube repository location.
- Existing HEX files are preserved because Proteus simulations may depend on them even when the firmware has not been rebuilt locally.
- `Original-Archives/` folders keep the submitted zip archives for lab projects where they were available.
- Keil, Proteus, STM32CubeMX, and Visual Studio were not run here; this documentation is based on the repository contents.

## Quick Links

- [Main assignments index](Microprocessors/README.md)
- [Lab projects index](Microprocessors-Lab/README.md)
- [HW2 assembly source](Microprocessors/Assignment-02-Checksum/HW2_Checksum.s)
- [Latest main assignment source](Microprocessors/Assignment-06-ADC-DMA-Buffering/hw6.c)
