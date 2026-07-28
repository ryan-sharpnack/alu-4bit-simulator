#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ==========================================
// CONFIGURATION CONSTANTS
// ==========================================
// Centralizing the bit-width means the loop, the mask, and the sign-bit
// position all derive from one source of truth. If this ever became an
// 8-bit ALU, this is the only line that changes.
#define BIT_WIDTH   4
#define NIBBLE_MASK 0x0F

// Opcode identifiers — named instead of magic numbers so run_alu() reads
// like an instruction decoder, not a coin flip. ADD/SUB drive the adder
// circuit; AND/OR/XOR/NOT expose the gate primitives directly, since a
// real ALU's "boolean algebra" and "arithmetic" capabilities are both
// selectable operations, not just internal plumbing.
#define OPCODE_ADD 0
#define OPCODE_SUB 1
#define OPCODE_AND 2
#define OPCODE_OR  3
#define OPCODE_XOR 4
#define OPCODE_NOT 5

// Bit positions within the packed status/flags register.
// Order chosen to mirror common CPU status registers (flags packed low-to-high).
#define FLAG_CARRY_BIT    0
#define FLAG_ZERO_BIT     1
#define FLAG_NEGATIVE_BIT 2
#define FLAG_OVERFLOW_BIT 3

// ==========================================
// 1. HARDWARE GATE SIMULATION PRIMITIVES
// ==========================================
unsigned char gate_and(unsigned char a, unsigned char b) { return a & b; }
unsigned char gate_or(unsigned char a, unsigned char b)  { return a | b; }
unsigned char gate_not(unsigned char a)                 { return (~a) & NIBBLE_MASK; }
unsigned char gate_xor(unsigned char a, unsigned char b) { return a ^ b; }

// ==========================================
// 2. CORE ALU: 4-BIT RIPPLE-CARRY ADDER
// ==========================================
// Simulates physical hardware addition bit-by-bit using full-adder logic:
//   sum       = A XOR B XOR Cin
//   carry_out = (A AND B) OR (Cin AND (A XOR B))
//
// carry_into_msb is captured separately from carry_out so the caller can
// compute the Overflow flag (see run_alu). Without capturing this
// intermediate value, it gets silently overwritten by the final carry_out
// before we ever get to compare the two.
unsigned char simulate_4bit_adder(unsigned char A, unsigned char B, unsigned char carry_in,
                                   unsigned char *carry_out, unsigned char *carry_into_msb) {
    unsigned char sum = 0;
    unsigned char c = carry_in;

    for (int i = 0; i < BIT_WIDTH; i++) {
        unsigned char bitA = (A >> i) & 1;
        unsigned char bitB = (B >> i) & 1;

        if (i == BIT_WIDTH - 1) {
            *carry_into_msb = c;
        }

        unsigned char bitSum = gate_xor(gate_xor(bitA, bitB), c);
        c = gate_or(gate_and(bitA, bitB), gate_and(c, gate_xor(bitA, bitB)));
        sum |= (bitSum << i);
    }

    *carry_out = c;
    return sum;
}

// ==========================================
// 3. MAIN ALU EXECUTION WRAPPER
// ==========================================
unsigned char run_alu(unsigned char A, unsigned char B, unsigned char opcode, unsigned char *flags) {
    unsigned char result = 0;
    unsigned char carry_out = 0;
    unsigned char carry_into_msb = 0;

    A &= NIBBLE_MASK;
    B &= NIBBLE_MASK;

    switch (opcode) {
        case OPCODE_ADD:
            result = simulate_4bit_adder(A, B, 0, &carry_out, &carry_into_msb);
            break;

        case OPCODE_SUB:
            // Two's complement subtraction: invert B (NOT gate) and feed the
            // "+1" directly into the adder's own carry_in line, rather than
            // doing a separate increment step. This is exactly how real
            // hardware reuses a single adder circuit for both operations.
            result = simulate_4bit_adder(A, gate_not(B), 1, &carry_out, &carry_into_msb);
            break;

        case OPCODE_AND:
            result = gate_and(A, B);
            break; // carry_out/carry_into_msb stay 0 — see note below

        case OPCODE_OR:
            result = gate_or(A, B);
            break;

        case OPCODE_XOR:
            result = gate_xor(A, B);
            break;

        case OPCODE_NOT:
            // Unary operation — B is accepted on the command line for a
            // consistent 3-argument interface but is not used here.
            result = gate_not(A);
            break;

        default:
            // Real hardware would trap this as an illegal instruction /
            // raise a fault. This simulator has no fault mechanism, so we
            // surface it loudly instead of silently returning a misleading
            // zero result with zero flags.
            fprintf(stderr, "ALU ERROR: unrecognized opcode %u\n", opcode);
            *flags = 0;
            return 0;
    }

    // --- SET HARDWARE CPU FLAGS ---
    // NOTE: for the bitwise operations above, carry_out and carry_into_msb
    // are left at their initialized value of 0. That's a deliberate
    // simplification of this simulator, not a universal hardware truth —
    // real CPUs vary on whether/how logical ops touch Carry. The one flag
    // this guarantees for logical ops is Overflow, which becomes 0 XOR 0 = 0,
    // correctly reflecting that overflow is an arithmetic-only concept.
    unsigned char zero_flag = (result == 0) ? 1 : 0;
    unsigned char negative_flag = (result >> (BIT_WIDTH - 1)) & 1;
    unsigned char overflow_flag = gate_xor(carry_into_msb, carry_out);

    // NOTE ON CARRY CONVENTION: for subtraction, carry_out = 1 means NO
    // borrow occurred (A >= B), carry_out = 0 means a borrow occurred.
    // This follows the ARM convention. x86's SUB instruction defines CF
    // with the opposite meaning (1 = borrow occurred). Neither is more
    // "correct" — it's a documented architecture choice, and this is ours.
    *flags = (overflow_flag << FLAG_OVERFLOW_BIT) |
             (negative_flag << FLAG_NEGATIVE_BIT) |
             (zero_flag     << FLAG_ZERO_BIT)     |
             (carry_out     << FLAG_CARRY_BIT);

    return result;
}

// ==========================================
// 4. CLI INPUT HANDLING
// ==========================================
// Converts a 4-character binary string ("0101") into its numeric value.
// Returns -1 on any malformed input so the caller can fail with a clear
// error instead of silently misinterpreting garbage as 0.
int parse_binary_nibble(const char *s) {
    if (s == NULL || strlen(s) != BIT_WIDTH) {
        return -1;
    }
    int value = 0;
    for (int i = 0; i < BIT_WIDTH; i++) {
        char c = s[i];
        if (c != '0' && c != '1') {
            return -1;
        }
        // Leftmost character is the most significant bit.
        value = (value << 1) | (c - '0');
    }
    return value;
}

// Renders a nibble back to a binary string for output, e.g. 8 -> "1000".
// Uses a static buffer since this is a single-threaded CLI tool printing
// one result at a time — not safe to hold two live calls simultaneously.
const char *nibble_to_binary_string(unsigned char v) {
    static char buf[BIT_WIDTH + 1];
    for (int i = 0; i < BIT_WIDTH; i++) {
        buf[BIT_WIDTH - 1 - i] = ((v >> i) & 1) ? '1' : '0';
    }
    buf[BIT_WIDTH] = '\0';
    return buf;
}

void print_usage(const char *prog_name) {
    fprintf(stderr, "Usage: %s <A> <B> <opcode>\n", prog_name);
    fprintf(stderr, "  A, B    4-bit binary strings, e.g. 0101\n");
    fprintf(stderr, "  opcode  0=ADD  1=SUB  2=AND  3=OR  4=XOR  5=NOT (B ignored)\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Run with no arguments to execute the automated test suite instead.\n");
    fprintf(stderr, "Example: %s 0101 0011 0    (5 + 3)\n", prog_name);
}

// ==========================================
// 5. AUTOMATED VERIFICATION SUITE
// ==========================================
static int g_tests_run = 0;
static int g_tests_failed = 0;

#define CHECK(label, actual, expected)                                       \
    do {                                                                     \
        g_tests_run++;                                                      \
        int _a = (actual);                                                  \
        int _e = (expected);                                                \
        if (_a == _e) {                                                     \
            printf("[PASS] %-28s (got %d)\n", label, _a);                   \
        } else {                                                            \
            printf("[FAIL] %-28s (got %d, expected %d)\n", label, _a, _e);   \
            g_tests_failed++;                                               \
        }                                                                    \
    } while (0)

int run_test_suite(void) {
    unsigned char flags = 0;
    unsigned char result = 0;

    printf("=== RUNNING ALU AUTOMATED TEST SUITE ===\n");

    // Test 1: 5 + 3 = 8 — overflows the signed 4-bit range (-8..+7).
    // Exposes whether Overflow is wired correctly; Negative alone would
    // mislabel this as "negative" when it's really "wrapped around."
    result = run_alu(5, 3, OPCODE_ADD, &flags);
    CHECK("5 + 3 -> result",   result,                              8);
    CHECK("5 + 3 -> carry",   (flags >> FLAG_CARRY_BIT)    & 1,     0);
    CHECK("5 + 3 -> zero",    (flags >> FLAG_ZERO_BIT)     & 1,     0);
    CHECK("5 + 3 -> negative",(flags >> FLAG_NEGATIVE_BIT) & 1,     1);
    CHECK("5 + 3 -> overflow",(flags >> FLAG_OVERFLOW_BIT) & 1,     1);

    // Test 2: 7 - 2 = 5
    result = run_alu(7, 2, OPCODE_SUB, &flags);
    CHECK("7 - 2 -> result",   result,                              5);
    CHECK("7 - 2 -> carry",   (flags >> FLAG_CARRY_BIT)    & 1,     1);
    CHECK("7 - 2 -> zero",    (flags >> FLAG_ZERO_BIT)     & 1,     0);
    CHECK("7 - 2 -> negative",(flags >> FLAG_NEGATIVE_BIT) & 1,     0);
    CHECK("7 - 2 -> overflow",(flags >> FLAG_OVERFLOW_BIT) & 1,     0);

    // Test 3: 4 - 4 = 0 (Zero flag)
    result = run_alu(4, 4, OPCODE_SUB, &flags);
    CHECK("4 - 4 -> result",   result,                              0);
    CHECK("4 - 4 -> carry",   (flags >> FLAG_CARRY_BIT)    & 1,     1);
    CHECK("4 - 4 -> zero",    (flags >> FLAG_ZERO_BIT)     & 1,     1);
    CHECK("4 - 4 -> negative",(flags >> FLAG_NEGATIVE_BIT) & 1,     0);
    CHECK("4 - 4 -> overflow",(flags >> FLAG_OVERFLOW_BIT) & 1,     0);

    // Test 4: 2 - 7 = -5 — borrow occurs, result is genuinely negative.
    // Contrast against Test 1: here Negative=1 AND Overflow=0, proving
    // the two flags really do measure different things.
    result = run_alu(2, 7, OPCODE_SUB, &flags);
    CHECK("2 - 7 -> result",   result,                              11);
    CHECK("2 - 7 -> carry",   (flags >> FLAG_CARRY_BIT)    & 1,     0);
    CHECK("2 - 7 -> zero",    (flags >> FLAG_ZERO_BIT)     & 1,     0);
    CHECK("2 - 7 -> negative",(flags >> FLAG_NEGATIVE_BIT) & 1,     1);
    CHECK("2 - 7 -> overflow",(flags >> FLAG_OVERFLOW_BIT) & 1,     0);

    // Tests 5-9: logic gate opcodes, exercised at full nibble width
    // (not just single bits) since that's the actual claim being tested.
    CHECK("1100 AND 1010",     run_alu(0xC, 0xA, OPCODE_AND, &flags), 0x8);
    CHECK("1100 OR  1010",     run_alu(0xC, 0xA, OPCODE_OR,  &flags), 0xE);
    CHECK("1111 XOR 0011",     run_alu(0xF, 0x3, OPCODE_XOR, &flags), 0xC);
    CHECK("NOT 0000",          run_alu(0x0, 0x0, OPCODE_NOT, &flags), 0xF);
    CHECK("NOT 1111",          run_alu(0xF, 0x0, OPCODE_NOT, &flags), 0x0);

    printf("\n=== TEST SUMMARY: %d/%d PASSED ===\n",
           g_tests_run - g_tests_failed, g_tests_run);

    return (g_tests_failed > 0) ? 1 : 0;
}

// ==========================================
// 6. ENTRY POINT
// ==========================================
int main(int argc, char *argv[]) {
    // No arguments: run the automated verification suite. This keeps the
    // regression tests reachable (`./alu_sim` with no args) without
    // requiring them every time someone wants to compute something.
    if (argc == 1) {
        return run_test_suite();
    }

    if (argc != 4) {
        print_usage(argv[0]);
        return 1;
    }

    int a_val = parse_binary_nibble(argv[1]);
    int b_val = parse_binary_nibble(argv[2]);
    char *endptr = NULL;
    long opcode_long = strtol(argv[3], &endptr, 10);

    if (a_val < 0) {
        fprintf(stderr, "ERROR: '%s' is not a valid 4-bit binary string.\n", argv[1]);
        return 1;
    }
    if (b_val < 0) {
        fprintf(stderr, "ERROR: '%s' is not a valid 4-bit binary string.\n", argv[2]);
        return 1;
    }
    if (endptr == argv[3] || *endptr != '\0' || opcode_long < OPCODE_ADD || opcode_long > OPCODE_NOT) {
        fprintf(stderr, "ERROR: '%s' is not a valid opcode (0-5).\n", argv[3]);
        return 1;
    }

    unsigned char A = (unsigned char)a_val;
    unsigned char B = (unsigned char)b_val;
    unsigned char opcode = (unsigned char)opcode_long;
    unsigned char flags = 0;

    unsigned char result = run_alu(A, B, opcode, &flags);

    static const char *opcode_names[] = { "ADD", "SUB", "AND", "OR", "XOR", "NOT" };

    printf("Opcode: %s\n", opcode_names[opcode]);
    printf("  A = %s (%u)\n", nibble_to_binary_string(A), A);
    if (opcode != OPCODE_NOT) {
        printf("  B = %s (%u)\n", nibble_to_binary_string(B), B);
    }
    printf("  Result = %s (%u)\n", nibble_to_binary_string(result), result);
    printf("  Flags  = Overflow:%d Negative:%d Zero:%d Carry:%d\n",
           (flags >> FLAG_OVERFLOW_BIT) & 1,
           (flags >> FLAG_NEGATIVE_BIT) & 1,
           (flags >> FLAG_ZERO_BIT)     & 1,
           (flags >> FLAG_CARRY_BIT)    & 1);

    return 0;
}
