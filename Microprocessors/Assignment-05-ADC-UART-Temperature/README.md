# HW5 - ADC, UART, Temperature, PWM, and LCD Status

STM32F401 assignment using CMSIS C to combine timer-based scheduling, PWM output, EXTI button handling, ADC temperature measurement, USART2 reporting, LEDs, and LCD output. A Proteus project is included.

## Project Details

| Item | Value |
| --- | --- |
| Target | STM32F401RETx |
| Main source | [hw5.c](hw5.c) |
| Keil project | [HW5_keil.uvprojx](HW5_keil.uvprojx) |
| Proteus project | [hw5.pdsprj](hw5.pdsprj) |
| Existing HEX output | [Objects/HW5_keil.hex](Objects/HW5_keil.hex) |
| Main output name | `HW5_keil` |
| Output directory | `Objects/` |
| Listing directory | `Listings/` |

## What It Implements

The firmware configures the STM32F401 system clock, uses TIM2 as a millisecond time base, uses TIM3 channel 1 on PA6 for PWM duty cycles, handles a debounced button event through EXTI13, samples an LM35-style analog temperature input on PA1 using ADC1, sends periodic reports over USART2 on PA2/PA3, controls GPIOB LEDs, and displays state, duty, temperature, and raw ADC data on an LCD.

## How to Open

### Keil uVision

1. Open [HW5_keil.uvprojx](HW5_keil.uvprojx) in Keil uVision.
2. Confirm the target device is `STM32F401RETx`.
3. Ensure the required STM32F4 Keil device pack and CMSIS dependencies are installed.
4. Build the project from Keil.

### Proteus

1. Open [hw5.pdsprj](hw5.pdsprj) from this directory.
2. Keep [Objects/HW5_keil.hex](Objects/HW5_keil.hex) available unless you rebuild and relink the simulation firmware manually.
3. If Proteus asks for missing firmware paths, point it to the generated Keil output in `Objects/`.

## Portability Notes

- The Keil project references `hw5.c` as `.\hw5.c`; keep it in this directory.
- The Keil project references STM32Cube files through local absolute paths under `C:\Users\ASUS\STM32Cube\Repository\STM32Cube_FW_F4_V1.28.0\...`. On another machine, these paths may need to be updated in Keil.
- No screenshots or simulation result images were found in this folder.
