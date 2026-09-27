# HW3 - STM32F401 GPIO, Button, LED, and LCD Control

STM32F401 assignment using CMSIS C to configure the system clock, SysTick timing, GPIO pins, button input, LED output, and a character LCD interface. A Proteus project is included.

## Project Details

| Item | Value |
| --- | --- |
| Target | STM32F401RETx |
| Main source | [hw3.c](hw3.c) |
| Keil project | [HW3_CMSIS.uvprojx](HW3_CMSIS.uvprojx) |
| Proteus project | [hw3.pdsprj](hw3.pdsprj) |
| Existing HEX output | [Objects/HW3_CMSIS.hex](Objects/HW3_CMSIS.hex) |
| Main output name | `HW3_CMSIS` |
| Output directory | `Objects/` |
| Listing directory | `Listings/` |

## What It Implements

The firmware configures an STM32F401 system clock using HSI and PLL, enables SysTick millisecond timing, reads a debounced button on GPIOC pin 13, controls LEDs on GPIOB pins 0 and 1, and writes status text to an LCD connected to GPIOB pins 4 through 9.

## How to Open

### Keil uVision

1. Open [HW3_CMSIS.uvprojx](HW3_CMSIS.uvprojx) in Keil uVision.
2. Confirm the target device is `STM32F401RETx`.
3. Ensure the required STM32F4 Keil device pack and CMSIS dependencies are installed.
4. Build the project from Keil.

### Proteus

1. Open [hw3.pdsprj](hw3.pdsprj) from this directory.
2. Keep [Objects/HW3_CMSIS.hex](Objects/HW3_CMSIS.hex) available unless you rebuild and relink the simulation firmware manually.
3. If Proteus asks for missing firmware paths, point it to the generated Keil output in `Objects/`.

## Portability Notes

- The Keil project references `hw3.c` as `.\hw3.c`; keep it in this directory.
- The Keil project also references STM32Cube files through local absolute paths under `C:\Users\ASUS\STM32Cube\Repository\STM32Cube_FW_F4_V1.28.0\...`. On another machine, these paths may need to be updated in Keil.
- Proteus workspace and autosave files are user-local and not required for the main project, but the main `.pdsprj` file is preserved.
- No screenshots or simulation result images were found in this folder.
