#ifndef OPCODES_HPP
#define OPCODES_HPP
#include <stdint.h>

enum OperandType {
    OPERAND_NONE,
    OPERAND_ADDRESS,
    OPERAND_IMMEDIATE_8,
    OPERAND_IMMEDIATE_16,
    OPERAND_RELATIVE
};

struct Opcode {
    OperandType operand;
    const char* format_str;
    uint8_t cycles;
};

extern Opcode g_opcode_table[0x100];
extern Opcode g_prefix_opcode_table[0x100];

uint8_t get_operand_num_bytes(OperandType opcode);
void fill_opcode_table();

#endif // OPCODES_HPP
