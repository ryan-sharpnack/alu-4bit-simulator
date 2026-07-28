# Project 02: 4-Bit ALU Simulator

## Technical Objective
A pure C simulation that replicates a physical 4-bit Arithmetic Logic Unit (ALU) using software-defined logic gates and bitwise operations. The simulator computes results and reports the same status flags a real CPU would set after each operation.

## The "Why": Engineering Value & Threat Impact
*   **Operational Risk / Threat Model:** Hardware-level understanding is critical; malicious actors can exploit microarchitectural flaws or side-channels (like Meltdown/Spectre) if software engineers treat physical processors as black boxes.
*   **Engineering Mastery:** Proves absolute control over bitwise manipulation, boolean algebra, and the mechanical transition from software commands to electrical-equivalent logic (AND, OR, NOT, XOR) without relying on high-level mathematical operators.
*   **Defensive Utility:** Provides a base-level understanding of hardware states and CPU status flags, foundational for reverse engineering binary malware and identifying low-level hardware tampering.

## Architecture & System Boundary
*   **Language & Toolchain:** C (C11) / GCC compiler with standard GNU library tools.
*   **Operating System Focus:** Cross-platform CLI execution (Linux/macOS terminal compatibility).
*   **Core APIs/Primitives Used:** Bitwise primitives (`&`, `|`, `~`, `^`, `<<`, `>>`) acting as simulated physical hardware transistors — no built-in `+`/`-` operators are used anywhere in the gate or adder logic itself.

## Technical Execution (What & How)
*   **Logic Gate Simulation:** Modeled all four physical gates (AND, OR, NOT, XOR) from bitwise primitives, then composed them into a full-adder circuit (`sum = A⊕B⊕Cin`, `carry = (A·B) + (Cin·(A⊕B))`) to perform ripple-carry binary addition one bit at a time.
*   **Two's-Complement Subtraction:** Reuses the same adder circuit for subtraction by inverting B through the NOT gate and feeding the "+1" directly into the adder's carry-in line — the same technique real ALU hardware uses to avoid a second dedicated circuit.
*   **Selectable Logic Operations:** AND, OR, XOR, and NOT are also exposed directly as opcodes in their own right, not just used internally by the adder.
*   **Flags & Overflow Handling:** Tracks CPU status flags — Carry, Zero, Negative, **and Overflow** — recalculating each from the adder's internal carry chain after every operation. Overflow is derived as `carry-into-MSB XOR carry-out-of-MSB`, which correctly distinguishes true signed overflow (e.g., `5 + 3 = 8`, outside the 4-bit signed range of -8..+7) from a result that is simply negative.
*   **Control Signals:** Implemented an opcode selection line to dynamically route 4-bit input buses into different arithmetic or logical paths.

## Opcode Reference
| Opcode | Operation        |
|--------|-------------------|
| 0      | ADD               |
| 1      | SUB               |
| 2      | AND               |
| 3      | OR                |
| 4      | XOR               |
| 5      | NOT (B ignored)   |

## How to Build & Run Locally
```bash
# Compile the C source code with strict error checking warnings enabled
gcc src/alu_sim.c -o alu_sim -Wall -Wextra -Wstrict-prototypes -std=c11

# Run the automated test suite (no arguments)
./alu_sim

# Run a single operation directly: <A> <B> <opcode>
# Example below computes 5 + 3 (opcode 0 = ADD)
./alu_sim 0101 0011 0
```

Example output:
```
Opcode: ADD
  A = 0101 (5)
  B = 0011 (3)
  Result = 1000 (8)
  Flags  = Overflow:1 Negative:1 Zero:0 Carry:0
```
