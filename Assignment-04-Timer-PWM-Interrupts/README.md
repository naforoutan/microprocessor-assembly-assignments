# HW4 - Timer, PWM, External Interrupt, and LCD Status

STM32F401 assignment using CMSIS C to configure timer-based millisecond timing, PWM output, external interrupt button handling, LEDs, and LCD status output. A Proteus project is included.

## Project Details

| Item | Value |
| --- | --- |
| Target | STM32F401RETx |
| Main source | [hw4.c](hw4.c) |
| Keil project | [HW4_keil.uvprojx](HW4_keil.uvprojx) |
| Proteus project | [hw4.pdsprj](hw4.pdsprj) |
| Existing HEX output | [Objects/HW4_keil.hex](Objects/HW4_keil.hex) |
| Main output name | `HW4_keil` |
| Output directory | `Objects/` |
| Listing directory | `Listings/` |

## What It Implements

The firmware configures the STM32F401 system clock, uses TIM2 as a millisecond time base, uses TIM3 channel 1 on PA6 for PWM duty cycles, handles a debounced button event through EXTI13, controls GPIOB LEDs, and updates a character LCD with state, PWM duty, and tick information.

## How to Open

### Keil uVision

1. Open [HW4_keil.uvprojx](HW4_keil.uvprojx) in Keil uVision.
2. Confirm the target device is `STM32F401RETx`.
3. Ensure the required STM32F4 Keil device pack and CMSIS dependencies are installed.
4. Build the project from Keil.

### Proteus

1. Open [hw4.pdsprj](hw4.pdsprj) from this directory.
2. Keep [Objects/HW4_keil.hex](Objects/HW4_keil.hex) available unless you rebuild and relink the simulation firmware manually.
3. If Proteus asks for missing firmware paths, point it to the generated Keil output in `Objects/`.

## Portability Notes

- The Keil project references `hw4.c` as `.\hw4.c`; keep it in this directory.
- The Keil project references STM32Cube files through local absolute paths under `C:\Users\ASUS\STM32Cube\Repository\STM32Cube_FW_F4_V1.28.0\...`. On another machine, these paths may need to be updated in Keil.
- No screenshots or simulation result images were found in this folder.
