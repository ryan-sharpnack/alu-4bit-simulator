# Project 02: 4-Bit ALU Simulator

## Technical Objective
A pure C simulation that replicates a physical 4-bit Arithmetic Logic Unit (ALU) using software-defined logic gates and bitwise operations.

## The "Why": Engineering Value & Threat Impact
*   **Operational Risk / Threat Model:** Hardware-level understanding is critical; malicious actors can exploit microarchitectural flaws or side-channels (like Meltdown/Spectre) if software engineers treat physical processors as black boxes.
*   **Engineering Mastery:** Proves absolute control over bitwise manipulation, boolean algebra, and the mechanical transition from software commands to electrical-equivalent logic (AND, OR, NOT, XOR) without relying on high-level mathematical operators.
*   **Defensive Utility:** Provides a base-level understanding of hardware states, which is foundational for reverse engineering binary malware and identifying low-level hardware tampering.

## Architecture & System Boundary
*   **Language & Toolchain:** C / GCC compiler with standard GNU library tools.
*   **Operating System Focus:** Cross-platform CLI execution (Linux/macOS terminal compatibility).
*   **Core APIs/Primitives Used:** Bitwise primitives (`&`, `|`, `~`, `^`, `<<`, `>>`) acting as simulated physical hardware transistors.

## Technical Execution (What & How)
*   **Logic Gate Simulation:** Modeled physical AND/XOR hardware gates using bitwise operations to calculate manual binary addition.
*   **Flags & Overflow Handling:** Tracked CPU state registers manually by calculating and setting the Carry-Out, Zero, and Negative flag bits after every operation.
*   **Control Signals:** Implemented an opcode selection line to dynamically route 4-bit input buses into different arithmetic or logical paths.

## How to Build & Run Locally
```bash
# Compile the C source code with strict error checking warnings enabled
gcc src/alu_sim.c -o alu_sim -Wall -Wextra

# Execute the binary simulator
./alu_sim
```
