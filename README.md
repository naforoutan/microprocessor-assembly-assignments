# Microprocessor Assembly Assignments

University microprocessor coursework prepared for GitHub publication. The current workspace contains two preserved assignment projects using Keil uVision, STM32F401 targets, ARM assembly/CMSIS C, and Proteus where available.

The original project layouts are intentionally preserved so Keil and Proteus relative references remain valid.

## Repository Contents

| Project | Topic | Target | Tools | Directory |
| --- | --- | --- | --- | --- |
| HW2 | Bytewise vs. wordwise checksum in ARM assembly | STM32F401RBTx | Keil uVision, ARM assembly | [Assignment-02-Checksum](Assignment-02-Checksum/) |
| HW3 | STM32F401 GPIO, SysTick timing, button input, LEDs, and LCD output | STM32F401RETx | Keil uVision, CMSIS C, Proteus | [Assignment-03-GPIO-LCD](Assignment-03-GPIO-LCD/) |

## Opening Projects

1. Open the assignment directory.
2. Open the `.uvprojx` file in Keil uVision.
3. Verify the installed Keil device pack matches the project target.
4. Build from Keil without changing source file locations.
5. For Proteus simulations, open the `.pdsprj` file from its original directory.

## Preservation Notes

- Assembly source, C source, Keil project files, Proteus project files, RTE configuration files, and existing build outputs were not modified.
- The HW3 Proteus project is binary and appears to contain embedded firmware/source references. Its `.pdsprj` file and existing HEX output are preserved.
- HW3 Keil project files reference STM32Cube files using local absolute paths under `C:\Users\ASUS\STM32Cube\Repository\...`; another machine may need those paths repaired in Keil.
- No Keil or Proteus build/simulation was performed in this environment.

## Suggested GitHub Repository Metadata

Suggested name: `Microprocessors`

Suggested description: `STM32F401 microprocessor coursework with Keil uVision, ARM assembly/CMSIS C, and Proteus simulations.`

If these folders are later split into two course repositories, use a companion repository named `Microprocessors-Lab` for lab-only experiments.
