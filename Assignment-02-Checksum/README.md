# HW2 - Bytewise vs. Wordwise Checksum

ARM assembly assignment for comparing bytewise and wordwise checksum implementations over a fixed-size memory buffer.

## Project Details

| Item | Value |
| --- | --- |
| Target | STM32F401RBTx |
| Main source | [HW2_Checksum.s](HW2_Checksum.s) |
| Keil project | [HW2_Checksum.uvprojx](HW2_Checksum.uvprojx) |
| Main output name | `HW2_Checksum` |
| Output directory | `Objects/` |
| Listing directory | `Listings/` |
| Proteus simulation | Not found in this folder |

## What It Implements

The program allocates a 1024-byte buffer, fills it with a repeating byte pattern, computes a checksum using bytewise XOR, computes another checksum using wordwise XOR, and branches to `Ok_Loop` or `Error_Loop` based on the comparison result.

## How to Open

1. Open [HW2_Checksum.uvprojx](HW2_Checksum.uvprojx) in Keil uVision.
2. Confirm the target device is `STM32F401RBTx`.
3. Build the project from Keil.
4. Debug the program in Keil and inspect `res1` and `res2` if needed.

## Notes

- The source file is referenced by the Keil project as `.\HW2_Checksum.s`; keep it in this directory.
- Existing generated files under `Objects/` and `Listings/` are build artifacts and were left untouched.
- No Proteus project file or simulation image was found for this assignment.
