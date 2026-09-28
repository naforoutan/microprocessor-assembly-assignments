# Lab 01 - GPIO Blink

Basic STM32F401 lab project using HAL/CubeMX-generated firmware to toggle GPIOC pin 3 with a delay. A Proteus project is included.

## Project Details

| Item | Value |
| --- | --- |
| Target | STM32F401RETx |
| Main source | [lab1/Core/Src/main.c](lab1/Core/Src/main.c) |
| CubeMX file | [lab1/lab1.ioc](lab1/lab1.ioc) |
| Keil project | [lab1/MDK-ARM/lab1.uvprojx](lab1/MDK-ARM/lab1.uvprojx) |
| Additional Keil project | [kambiz.uvprojx](kambiz.uvprojx) |
| Proteus project | [New Project.pdsprj](New%20Project.pdsprj) |
| Existing HEX output | [lab1/MDK-ARM/lab1/lab1.hex](lab1/MDK-ARM/lab1/lab1.hex) |

## Notes

- The CubeMX/Keil project structure under `lab1/` is preserved because the Keil project uses relative paths such as `../Core/Src/main.c`.
- The firmware toggles GPIOC pin 3 in the main loop.
- Open the Proteus project from this folder if simulation is needed.
