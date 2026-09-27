# HW6 - ADC Buffering, DMA-Style Processing, UART, and LCD Status

STM32F401 assignment using CMSIS C to process ADC samples in a 256-sample circular buffer, report statistics over USART2, display status on an LCD, and preserve the timer, PWM, EXTI button, and LED behavior from earlier assignments. A Proteus project is included.

## Project Details

| Item | Value |
| --- | --- |
| Target | STM32F401RETx |
| Main source | [hw6.c](hw6.c) |
| Keil project | [HW6_keil.uvprojx](HW6_keil.uvprojx) |
| Proteus project | [hw6.pdsprj](hw6.pdsprj) |
| Existing HEX output | [Objects/HW6_keil.hex](Objects/HW6_keil.hex) |
| Main output name | `HW6_keil` |
| Output directory | `Objects/` |
| Listing directory | `Listings/` |

## What It Implements

The firmware configures the STM32F401 system clock, TIM2 millisecond timing, TIM3 PWM on PA6, EXTI13 button input, GPIOB LEDs, USART2 reporting on PA2/PA3, and LCD output. ADC samples from an LM35-style input on PA0 are inserted into a 256-sample circular buffer and processed in half-buffer/full-buffer blocks to calculate average, maximum, sample count, and temperature.

The source notes that this is a Proteus-compatible version: ADC samples are read using software-triggered conversions and inserted into the same buffer that would be used by the required DMA workflow, because Proteus may not reliably emulate ADC-to-DMA transfers for this MCU.

## How to Open

### Keil uVision

1. Open [HW6_keil.uvprojx](HW6_keil.uvprojx) in Keil uVision.
2. Confirm the target device is `STM32F401RETx`.
3. Ensure the required STM32F4 Keil device pack and CMSIS dependencies are installed.
4. Build the project from Keil.

### Proteus

1. Open [hw6.pdsprj](hw6.pdsprj) from this directory.
2. Keep [Objects/HW6_keil.hex](Objects/HW6_keil.hex) available unless you rebuild and relink the simulation firmware manually.
3. If Proteus asks for missing firmware paths, point it to the generated Keil output in `Objects/`.

## Portability Notes

- The Keil project references `hw6.c` as `.\hw6.c`; keep it in this directory.
- The Keil project references STM32Cube files through local absolute paths under `C:\Users\ASUS\STM32Cube\Repository\STM32Cube_FW_F4_V1.28.0\...`. On another machine, these paths may need to be updated in Keil.
- The ADC input is documented in the source as PA0 / ADC1 channel 0 for the included Proteus schematic.
- No screenshots or simulation result images were found in this folder.
