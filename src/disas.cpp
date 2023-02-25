#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "disas.hpp"
#include "opcodes.hpp"
#include "util.hpp"

void disassemble(const Cpu& cpu, uint16_t& pc, char* buf, unsigned int buf_size)
{
    uint8_t instr = cpu.mem(pc);
    Opcode op = g_opcode_table[instr];
    uint8_t operand_size = get_operand_num_bytes(op.operand);

    switch (op.operand) {
    case OPERAND_NONE:
        snprintf(buf, buf_size, "%s", op.format_str);
        break;

    case OPERAND_IMMEDIATE_8:
        snprintf(buf, buf_size, op.format_str, cpu.mem(pc+1));
        break;

    case OPERAND_RELATIVE:
        snprintf(buf, buf_size, op.format_str, unsigned_to_signed(cpu.mem(pc+1)));
        break;

    case OPERAND_IMMEDIATE_16:
    case OPERAND_ADDRESS:
        snprintf(buf, buf_size, op.format_str, (uint16_t)cpu.mem(pc+1) | (uint16_t)(cpu.mem(pc+2) << 8));
        break;

    default:
        fprintf(stderr, "Unknown operand type\n");
        exit(1);
    }

    pc += 1 + operand_size;
}
