#include "opcodes.hpp"
#include <assert.h>
#include <stdio.h>

Opcode g_opcode_table[0x100];
Opcode g_prefix_opcode_table[0x100];

void fill_opcode_table()
{
    g_opcode_table[0x00] = { OPERAND_NONE, "NOP", 4 };
    g_opcode_table[0x01] = { OPERAND_IMMEDIATE_16, "LD BC,%u", 12 };
    g_opcode_table[0x02] = { OPERAND_NONE, "LD (BC),A", 8 };
    g_opcode_table[0x03] = { OPERAND_NONE, "INC BC", 8 };
    g_opcode_table[0x04] = { OPERAND_NONE, "INC B", 4 };
    g_opcode_table[0x05] = { OPERAND_NONE, "DEC B", 4 };
    g_opcode_table[0x06] = { OPERAND_IMMEDIATE_8, "LD B,%u", 8 };
    g_opcode_table[0x07] = { OPERAND_NONE, "RLCA", 4 };
    g_opcode_table[0x08] = { OPERAND_ADDRESS, "LD (0x%x),SP", 20 };
    g_opcode_table[0x09] = { OPERAND_NONE, "ADD HL,BC", 8 };
    g_opcode_table[0x0A] = { OPERAND_NONE, "LD A,(BC)", 8 };
    g_opcode_table[0x0B] = { OPERAND_NONE, "DEC BC", 8 };
    g_opcode_table[0x0C] = { OPERAND_NONE, "INC C", 4 };
    g_opcode_table[0x0D] = { OPERAND_NONE, "DEC C", 4 };
    g_opcode_table[0x0E] = { OPERAND_IMMEDIATE_8, "LD C,%u", 8 };
    g_opcode_table[0x0F] = { OPERAND_NONE, "RRCA", 4 };

    g_opcode_table[0x10] = { OPERAND_NONE, "STOP 0", 4 };
    g_opcode_table[0x11] = { OPERAND_IMMEDIATE_16, "LD DE,%u", 12 };
    g_opcode_table[0x12] = { OPERAND_NONE, "LD (DE),A", 8 };
    g_opcode_table[0x13] = { OPERAND_NONE, "INC DE", 8 };
    g_opcode_table[0x14] = { OPERAND_NONE, "INC D", 4 };
    g_opcode_table[0x15] = { OPERAND_NONE, "DEC D", 4 };
    g_opcode_table[0x16] = { OPERAND_IMMEDIATE_8, "LD D,%u", 8 };
    g_opcode_table[0x17] = { OPERAND_NONE, "RLA", 4 };
    g_opcode_table[0x18] = { OPERAND_RELATIVE, "JR %d", 12};
    g_opcode_table[0x19] = { OPERAND_NONE, "ADD HL,DE", 8 };
    g_opcode_table[0x1A] = { OPERAND_NONE, "LD A,(DE)", 8 };
    g_opcode_table[0x1B] = { OPERAND_NONE, "DEC DE", 8 };
    g_opcode_table[0x1C] = { OPERAND_NONE, "INC E", 4 };
    g_opcode_table[0x1D] = { OPERAND_NONE, "DEC E", 4 };
    g_opcode_table[0x1E] = { OPERAND_IMMEDIATE_8, "LD E,%u", 8 };
    g_opcode_table[0x1F] = { OPERAND_NONE, "RRA", 4 };

    g_opcode_table[0x20] = { OPERAND_RELATIVE, "JR NZ,%d", 0 };
    g_opcode_table[0x21] = { OPERAND_IMMEDIATE_16, "LD HL,%u", 12 };
    g_opcode_table[0x22] = { OPERAND_NONE, "LD (HL+),A", 8 };
    g_opcode_table[0x23] = { OPERAND_NONE, "INC HL", 8 };
    g_opcode_table[0x24] = { OPERAND_NONE, "INC H", 4 };
    g_opcode_table[0x25] = { OPERAND_NONE, "DEC H", 4 };
    g_opcode_table[0x26] = { OPERAND_IMMEDIATE_8, "LD H,%u", 8 };
    g_opcode_table[0x27] = { OPERAND_NONE, "RLA", 4 };
    g_opcode_table[0x28] = { OPERAND_RELATIVE, "JR Z,%d", 0 };
    g_opcode_table[0x29] = { OPERAND_NONE, "ADD HL,HL", 8 };
    g_opcode_table[0x2A] = { OPERAND_NONE, "LD A,(HL+)", 8 };
    g_opcode_table[0x2B] = { OPERAND_NONE, "DEC HL", 8 };
    g_opcode_table[0x2C] = { OPERAND_NONE, "INC L", 4 };
    g_opcode_table[0x2D] = { OPERAND_NONE, "DEC L", 4 };
    g_opcode_table[0x2E] = { OPERAND_IMMEDIATE_8, "LD L,%u", 8 };
    g_opcode_table[0x2F] = { OPERAND_NONE, "CPL", 4 };

    g_opcode_table[0x30] = { OPERAND_RELATIVE, "JR NC,%d", 0 };
    g_opcode_table[0x31] = { OPERAND_IMMEDIATE_16, "LD SP,%u", 12 };
    g_opcode_table[0x32] = { OPERAND_NONE, "LD (HL-),A", 8 };
    g_opcode_table[0x33] = { OPERAND_NONE, "INC SP", 8 };
    g_opcode_table[0x34] = { OPERAND_NONE, "INC (HL)", 12 };
    g_opcode_table[0x35] = { OPERAND_NONE, "DEC (HL)", 12 };
    g_opcode_table[0x36] = { OPERAND_IMMEDIATE_8, "LD (HL),%u", 12 };
    g_opcode_table[0x37] = { OPERAND_NONE, "SCF", 4 };
    g_opcode_table[0x38] = { OPERAND_RELATIVE, "JR C,%d", 0 };
    g_opcode_table[0x39] = { OPERAND_NONE, "ADD HL,SP", 8 };
    g_opcode_table[0x3A] = { OPERAND_NONE, "LD A,(HL-)", 8 };
    g_opcode_table[0x3B] = { OPERAND_NONE, "DEC SP", 8 };
    g_opcode_table[0x3C] = { OPERAND_NONE, "INC A", 4 };
    g_opcode_table[0x3D] = { OPERAND_NONE, "DEC A", 4 };
    g_opcode_table[0x3E] = { OPERAND_IMMEDIATE_8, "LD A,%u", 8 };
    g_opcode_table[0x3F] = { OPERAND_NONE, "CCF", 4 };

    g_opcode_table[0x40] = { OPERAND_NONE, "LD B,B", 4 };
    g_opcode_table[0x41] = { OPERAND_NONE, "LD B,C", 4 };
    g_opcode_table[0x42] = { OPERAND_NONE, "LD B,D", 4 };
    g_opcode_table[0x43] = { OPERAND_NONE, "LD B,E", 4 };
    g_opcode_table[0x44] = { OPERAND_NONE, "LD B,H", 4 };
    g_opcode_table[0x45] = { OPERAND_NONE, "LD B,L", 4 };
    g_opcode_table[0x46] = { OPERAND_NONE, "LD B,(HL)", 8 };
    g_opcode_table[0x47] = { OPERAND_NONE, "LD B,A", 4 };
    g_opcode_table[0x48] = { OPERAND_NONE, "LD C,B", 4 };
    g_opcode_table[0x49] = { OPERAND_NONE, "LD C,C", 4 };
    g_opcode_table[0x4A] = { OPERAND_NONE, "LD C,D", 4 };
    g_opcode_table[0x4B] = { OPERAND_NONE, "LD C,E", 4 };
    g_opcode_table[0x4C] = { OPERAND_NONE, "LD C,H", 4 };
    g_opcode_table[0x4D] = { OPERAND_NONE, "LD C,L", 4 };
    g_opcode_table[0x4E] = { OPERAND_NONE, "LD C,(HL)", 8 };
    g_opcode_table[0x4F] = { OPERAND_NONE, "LD C,A", 4 };

    g_opcode_table[0x50] = { OPERAND_NONE, "LD D,B", 4 };
    g_opcode_table[0x51] = { OPERAND_NONE, "LD D,C", 4 };
    g_opcode_table[0x52] = { OPERAND_NONE, "LD D,D", 4 };
    g_opcode_table[0x53] = { OPERAND_NONE, "LD D,E", 4 };
    g_opcode_table[0x54] = { OPERAND_NONE, "LD D,H", 4 };
    g_opcode_table[0x55] = { OPERAND_NONE, "LD D,L", 4 };
    g_opcode_table[0x56] = { OPERAND_NONE, "LD D,(HL)", 8 };
    g_opcode_table[0x57] = { OPERAND_NONE, "LD D,A", 4 };
    g_opcode_table[0x58] = { OPERAND_NONE, "LD E,B", 4 };
    g_opcode_table[0x59] = { OPERAND_NONE, "LD E,C", 4 };
    g_opcode_table[0x5A] = { OPERAND_NONE, "LD E,D", 4 };
    g_opcode_table[0x5B] = { OPERAND_NONE, "LD E,E", 4 };
    g_opcode_table[0x5C] = { OPERAND_NONE, "LD E,H", 4 };
    g_opcode_table[0x5D] = { OPERAND_NONE, "LD E,L", 4 };
    g_opcode_table[0x5E] = { OPERAND_NONE, "LD E,(HL)", 8 };
    g_opcode_table[0x5F] = { OPERAND_NONE, "LD E,A", 4 };

    g_opcode_table[0x60] = { OPERAND_NONE, "LD H,B", 4 };
    g_opcode_table[0x61] = { OPERAND_NONE, "LD H,C", 4 };
    g_opcode_table[0x62] = { OPERAND_NONE, "LD H,D", 4 };
    g_opcode_table[0x63] = { OPERAND_NONE, "LD H,E", 4 };
    g_opcode_table[0x64] = { OPERAND_NONE, "LD H,H", 4 };
    g_opcode_table[0x65] = { OPERAND_NONE, "LD H,L", 4 };
    g_opcode_table[0x66] = { OPERAND_NONE, "LD H,(HL)", 8 };
    g_opcode_table[0x67] = { OPERAND_NONE, "LD H,A", 4 };
    g_opcode_table[0x68] = { OPERAND_NONE, "LD L,B", 4 };
    g_opcode_table[0x69] = { OPERAND_NONE, "LD L,C", 4 };
    g_opcode_table[0x6A] = { OPERAND_NONE, "LD L,D", 4 };
    g_opcode_table[0x6B] = { OPERAND_NONE, "LD L,E", 4 };
    g_opcode_table[0x6C] = { OPERAND_NONE, "LD L,H", 4 };
    g_opcode_table[0x6D] = { OPERAND_NONE, "LD L,L", 4 };
    g_opcode_table[0x6E] = { OPERAND_NONE, "LD L,(HL)", 8 };
    g_opcode_table[0x6F] = { OPERAND_NONE, "LD L,A", 4 };

    g_opcode_table[0x70] = { OPERAND_NONE, "LD (HL),B", 8 };
    g_opcode_table[0x71] = { OPERAND_NONE, "LD (HL),C", 8 };
    g_opcode_table[0x72] = { OPERAND_NONE, "LD (HL),D", 8 };
    g_opcode_table[0x73] = { OPERAND_NONE, "LD (HL),E", 8 };
    g_opcode_table[0x74] = { OPERAND_NONE, "LD (HL),H", 8 };
    g_opcode_table[0x75] = { OPERAND_NONE, "LD (HL),L", 8 };
    g_opcode_table[0x76] = { OPERAND_NONE, "HALT", 4 };
    g_opcode_table[0x77] = { OPERAND_NONE, "LD (HL),A", 8 };
    g_opcode_table[0x78] = { OPERAND_NONE, "LD A,B", 4 };
    g_opcode_table[0x79] = { OPERAND_NONE, "LD A,C", 4 };
    g_opcode_table[0x7A] = { OPERAND_NONE, "LD A,D", 4 };
    g_opcode_table[0x7B] = { OPERAND_NONE, "LD A,E", 4 };
    g_opcode_table[0x7C] = { OPERAND_NONE, "LD A,H", 4 };
    g_opcode_table[0x7D] = { OPERAND_NONE, "LD A,L", 4 };
    g_opcode_table[0x7E] = { OPERAND_NONE, "LD A,(HL)", 8 };
    g_opcode_table[0x7F] = { OPERAND_NONE, "LD A,A", 4 };

    g_opcode_table[0x80] = { OPERAND_NONE, "ADD A,B", 4 };
    g_opcode_table[0x81] = { OPERAND_NONE, "ADD A,C", 4 };
    g_opcode_table[0x82] = { OPERAND_NONE, "ADD A,D", 4 };
    g_opcode_table[0x83] = { OPERAND_NONE, "ADD A,E", 4 };
    g_opcode_table[0x84] = { OPERAND_NONE, "ADD A,H", 4 };
    g_opcode_table[0x85] = { OPERAND_NONE, "ADD A,L", 4 };
    g_opcode_table[0x86] = { OPERAND_NONE, "ADD A,(HL)", 8 };
    g_opcode_table[0x87] = { OPERAND_NONE, "ADD A,A", 4 };
    g_opcode_table[0x88] = { OPERAND_NONE, "ADC A,B", 4 };
    g_opcode_table[0x89] = { OPERAND_NONE, "ADC A,C", 4 };
    g_opcode_table[0x8A] = { OPERAND_NONE, "ADC A,D", 4 };
    g_opcode_table[0x8B] = { OPERAND_NONE, "ADC A,E", 4 };
    g_opcode_table[0x8C] = { OPERAND_NONE, "ADC A,H", 4 };
    g_opcode_table[0x8D] = { OPERAND_NONE, "ADC A,L", 4 };
    g_opcode_table[0x8E] = { OPERAND_NONE, "ADC A,(HL)", 8 };
    g_opcode_table[0x8F] = { OPERAND_NONE, "ADC A,A", 4 };

    g_opcode_table[0x90] = { OPERAND_NONE, "SUB A,B", 4 };
    g_opcode_table[0x91] = { OPERAND_NONE, "SUB A,C", 4 };
    g_opcode_table[0x92] = { OPERAND_NONE, "SUB A,D", 4 };
    g_opcode_table[0x93] = { OPERAND_NONE, "SUB A,E", 4 };
    g_opcode_table[0x94] = { OPERAND_NONE, "SUB A,H", 4 };
    g_opcode_table[0x95] = { OPERAND_NONE, "SUB A,L", 4 };
    g_opcode_table[0x96] = { OPERAND_NONE, "SUB A,(HL)", 8 };
    g_opcode_table[0x97] = { OPERAND_NONE, "SUB A", 4 };
    g_opcode_table[0x98] = { OPERAND_NONE, "SBC A,B", 4 };
    g_opcode_table[0x99] = { OPERAND_NONE, "SBC A,C", 4 };
    g_opcode_table[0x9A] = { OPERAND_NONE, "SBC A,D", 4 };
    g_opcode_table[0x9B] = { OPERAND_NONE, "SBC A,E", 4 };
    g_opcode_table[0x9C] = { OPERAND_NONE, "SBC A,H", 4 };
    g_opcode_table[0x9D] = { OPERAND_NONE, "SBC A,L", 4 };
    g_opcode_table[0x9E] = { OPERAND_NONE, "SBC A,(HL)", 8 };
    g_opcode_table[0x9F] = { OPERAND_NONE, "SBC A,A", 4 };

    g_opcode_table[0xA0] = { OPERAND_NONE, "AND B", 4 };
    g_opcode_table[0xA1] = { OPERAND_NONE, "AND C", 4 };
    g_opcode_table[0xA2] = { OPERAND_NONE, "AND D", 4 };
    g_opcode_table[0xA3] = { OPERAND_NONE, "AND E", 4 };
    g_opcode_table[0xA4] = { OPERAND_NONE, "AND H", 4 };
    g_opcode_table[0xA5] = { OPERAND_NONE, "AND L", 4 };
    g_opcode_table[0xA6] = { OPERAND_NONE, "AND (HL)", 8 };
    g_opcode_table[0xA7] = { OPERAND_NONE, "AND A", 4 };
    g_opcode_table[0xA8] = { OPERAND_NONE, "XOR B", 4 };
    g_opcode_table[0xA9] = { OPERAND_NONE, "XOR C", 4 };
    g_opcode_table[0xAA] = { OPERAND_NONE, "XOR D", 4 };
    g_opcode_table[0xAB] = { OPERAND_NONE, "XOR E", 4 };
    g_opcode_table[0xAC] = { OPERAND_NONE, "XOR H", 4 };
    g_opcode_table[0xAD] = { OPERAND_NONE, "XOR L", 4 };
    g_opcode_table[0xAE] = { OPERAND_NONE, "XOR (HL)", 8 };
    g_opcode_table[0xAF] = { OPERAND_NONE, "XOR A", 4 };

    g_opcode_table[0xB0] = { OPERAND_NONE, "OR B", 4 };
    g_opcode_table[0xB1] = { OPERAND_NONE, "OR C", 4 };
    g_opcode_table[0xB2] = { OPERAND_NONE, "OR D", 4 };
    g_opcode_table[0xB3] = { OPERAND_NONE, "OR E", 4 };
    g_opcode_table[0xB4] = { OPERAND_NONE, "OR H", 4 };
    g_opcode_table[0xB5] = { OPERAND_NONE, "OR L", 4 };
    g_opcode_table[0xB6] = { OPERAND_NONE, "OR (HL)", 8 };
    g_opcode_table[0xB7] = { OPERAND_NONE, "OR A", 4 };
    g_opcode_table[0xB8] = { OPERAND_NONE, "CP B", 4 };
    g_opcode_table[0xB9] = { OPERAND_NONE, "CP C", 4 };
    g_opcode_table[0xBA] = { OPERAND_NONE, "CP D", 4 };
    g_opcode_table[0xBB] = { OPERAND_NONE, "CP E", 4 };
    g_opcode_table[0xBC] = { OPERAND_NONE, "CP H", 4 };
    g_opcode_table[0xBD] = { OPERAND_NONE, "CP L", 4 };
    g_opcode_table[0xBE] = { OPERAND_NONE, "CP (HL)", 8 };
    g_opcode_table[0xBF] = { OPERAND_NONE, "CP A", 4 };

    g_opcode_table[0xC0] = { OPERAND_NONE, "RET NZ", 0 };
    g_opcode_table[0xC1] = { OPERAND_NONE, "POP BC", 12 };
    g_opcode_table[0xC2] = { OPERAND_ADDRESS, "JP NZ,0x%04x", 0 };
    g_opcode_table[0xC3] = { OPERAND_ADDRESS, "JP 0x%04x", 16 };
    g_opcode_table[0xC4] = { OPERAND_ADDRESS, "CALL NZ,0%04x", 0 };
    g_opcode_table[0xC5] = { OPERAND_NONE, "PUSH BC", 16 };
    g_opcode_table[0xC6] = { OPERAND_IMMEDIATE_8, "ADD A,%u", 8 };
    g_opcode_table[0xC7] = { OPERAND_NONE, "RST 0x00", 16 };
    g_opcode_table[0xC8] = { OPERAND_NONE, "RET Z", 0 };
    g_opcode_table[0xC9] = { OPERAND_NONE, "RET", 16 };
    g_opcode_table[0xCA] = { OPERAND_ADDRESS, "JP Z,0x%04x", 0 };
    g_opcode_table[0xCB] = { OPERAND_IMMEDIATE_8, "PREFIX CB: %u", 0 };
    g_opcode_table[0xCC] = { OPERAND_ADDRESS, "CALL Z,0x%04x", 0 };
    g_opcode_table[0xCD] = { OPERAND_ADDRESS, "CALL 0x%04x", 24 };
    g_opcode_table[0xCE] = { OPERAND_NONE, "ADC A,%u", 8 };
    g_opcode_table[0xCF] = { OPERAND_NONE, "RST 0x08", 16 };

    g_opcode_table[0xD0] = { OPERAND_NONE, "RET NC", 0 };
    g_opcode_table[0xD1] = { OPERAND_NONE, "POP DE", 12 };
    g_opcode_table[0xD2] = { OPERAND_ADDRESS, "JP NC,0x%04x", 0 };
    g_opcode_table[0xD3] = { OPERAND_NONE, "INVALID", 0 };
    g_opcode_table[0xD4] = { OPERAND_ADDRESS, "CALL NC,0%04x", 0 };
    g_opcode_table[0xD5] = { OPERAND_NONE, "PUSH DE", 16 };
    g_opcode_table[0xD6] = { OPERAND_IMMEDIATE_8, "SUB %u", 8 };
    g_opcode_table[0xD7] = { OPERAND_NONE, "RST 0x10", 16 };
    g_opcode_table[0xD8] = { OPERAND_NONE, "RET C", 0 };
    g_opcode_table[0xD9] = { OPERAND_NONE, "RETI", 16 };
    g_opcode_table[0xDA] = { OPERAND_ADDRESS, "JP C,0x%04x", 0 };
    g_opcode_table[0xDB] = { OPERAND_NONE, "INVALID", 0 };
    g_opcode_table[0xDC] = { OPERAND_ADDRESS, "CALL C,0x%04x", 0 };
    g_opcode_table[0xDD] = { OPERAND_NONE, "INVALID", 0 };
    g_opcode_table[0xDE] = { OPERAND_NONE, "SBC A,%u", 8 };
    g_opcode_table[0xDF] = { OPERAND_NONE, "RST 0x18", 16 };

    g_opcode_table[0xE0] = { OPERAND_IMMEDIATE_8, "LDH (0x%02x),A", 12 };
    g_opcode_table[0xE1] = { OPERAND_NONE, "POP HL", 12 };
    g_opcode_table[0xE2] = { OPERAND_NONE, "LDH (C),A", 8 };
    g_opcode_table[0xE3] = { OPERAND_NONE, "INVALID", 0 };
    g_opcode_table[0xE4] = { OPERAND_NONE, "INVALID", 0 };
    g_opcode_table[0xE5] = { OPERAND_NONE, "PUSH HL", 16 };
    g_opcode_table[0xE6] = { OPERAND_IMMEDIATE_8, "AND %u", 8 };
    g_opcode_table[0xE7] = { OPERAND_NONE, "RST 0x20", 16 };
    g_opcode_table[0xE8] = { OPERAND_RELATIVE, "ADD SP,%d", 16 };
    g_opcode_table[0xE9] = { OPERAND_NONE, "JP (HL)", 4 };
    g_opcode_table[0xEA] = { OPERAND_ADDRESS, "LD (0x%04x),A", 16 };
    g_opcode_table[0xEB] = { OPERAND_NONE, "INVALID", 0 };
    g_opcode_table[0xEC] = { OPERAND_NONE, "INVALID", 0 };
    g_opcode_table[0xED] = { OPERAND_NONE, "INVALID", 0 };
    g_opcode_table[0xEE] = { OPERAND_NONE, "XOR %u", 8 };
    g_opcode_table[0xEF] = { OPERAND_NONE, "RST 0x28", 16 };

    g_opcode_table[0xF0] = { OPERAND_IMMEDIATE_8, "LDH A,(0x%02x)", 12 };
    g_opcode_table[0xF1] = { OPERAND_NONE, "POP AF", 12 };
    g_opcode_table[0xF2] = { OPERAND_NONE, "LDH A,(C)", 8 };
    g_opcode_table[0xF3] = { OPERAND_NONE, "DI", 4 };
    g_opcode_table[0xF4] = { OPERAND_NONE, "INVALID", 0 };
    g_opcode_table[0xF5] = { OPERAND_NONE, "PUSH AF", 16 };
    g_opcode_table[0xF6] = { OPERAND_IMMEDIATE_8, "OR %u", 8 };
    g_opcode_table[0xF7] = { OPERAND_NONE, "RST 0x30", 16 };
    g_opcode_table[0xF8] = { OPERAND_RELATIVE, "LD HL,SP+%d", 12 };
    g_opcode_table[0xF9] = { OPERAND_NONE, "LD SP,HL", 8 };
    g_opcode_table[0xFA] = { OPERAND_ADDRESS, "LD A,(0x%04x)", 16 };
    g_opcode_table[0xFB] = { OPERAND_NONE, "EI", 4 };
    g_opcode_table[0xFC] = { OPERAND_NONE, "INVALID", 0 };
    g_opcode_table[0xFD] = { OPERAND_NONE, "INVALID", 0 };
    g_opcode_table[0xFE] = { OPERAND_IMMEDIATE_8, "CP %u", 8 };
    g_opcode_table[0xFF] = { OPERAND_NONE, "RST 0x38", 16 };

    char regs[][5] = { "B", "C", "D", "E", "H", "L", "(HL)", "A" };

    char instrs[][5] = { "RLC", "RRC", "RL", "RR", "SLA", "SRA", "SWAP", "SRL" };

    for (int i = 0; i < 8; i++) {
        for (int reg_id = 0; reg_id < 8; reg_id++) {
            char label[20];
            snprintf(label, 20, "%s %s", instrs[i], regs[reg_id]);
            g_prefix_opcode_table[i*8+reg_id] = { OPERAND_NONE, label, (reg_id == 6) ? (uint8_t)16 : (uint8_t)8 };
        }
    }

    for (int bit = 0; bit < 8; bit++) {
        for (int reg_id = 0; reg_id < 8; reg_id++) {
            char label[20];
            snprintf(label, 20, "BIT %d,%s", bit, regs[reg_id]);
            g_prefix_opcode_table[0x40 + 8*bit + reg_id] = { OPERAND_NONE, label, (reg_id == 6) ? (uint8_t)12: (uint8_t)8 };
        }
    }

    for (int bit = 0; bit < 8; bit++) {
        for (int reg_id = 0; reg_id < 8; reg_id++) {
            char label[20];
            snprintf(label, 20, "RES %d,%s", bit, regs[reg_id]);
            g_prefix_opcode_table[0x80 + 8*bit + reg_id] = { OPERAND_NONE, label, (reg_id == 6) ? (uint8_t)16: (uint8_t)8 };
        }
    }

    for (int bit = 0; bit < 8; bit++) {
        for (int reg_id = 0; reg_id < 8; reg_id++) {
            char label[20];
            snprintf(label, 20, "SET %d,%s", bit, regs[reg_id]);
            g_prefix_opcode_table[0xC0 + 8*bit + reg_id] = { OPERAND_NONE, label, (reg_id == 6) ? (uint8_t)16: (uint8_t)8 };
        }
    }
}

uint8_t get_operand_num_bytes(OperandType operand)
{
    switch(operand) {
    case OPERAND_NONE:
        return 0;

    case OPERAND_IMMEDIATE_8:
    case OPERAND_RELATIVE:
        return 1;

    case OPERAND_IMMEDIATE_16:
    case OPERAND_ADDRESS:
        return 2;
    }

    assert(0);
    return 255;
}
