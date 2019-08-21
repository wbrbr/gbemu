#include "cpu.hpp"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

Cpu::Cpu()
{
    pc = 0x100;
    sp = 0xFFFE;
    memset(memory, 0, sizeof(memory));
    memset(regs, 0, sizeof(regs));
}

void Cpu::load(const char* path)
{
    FILE* fp = fopen(path, "r");
    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    assert(size <= 0x8000); // for now
    if (fread(memory, 1, size, fp) != size) {
        perror("fread: ");
        exit(1);
    }

    assert(memory[0x104] == 0xCE && memory[0x105] == 0xED);
    char title[17];
    memcpy(title, memory + 0x134, 16);
    title[16] = 0;

    puts(title);

    assert(memory[0x147] == 0 && memory[0x148] == 0);
}

uint16_t Cpu::af()
{
    uint8_t f = (c << 4) | (h << 5) | (n << 6) | (z << 7);
    return (regs[REG_A] << 8) | f;
}

uint16_t Cpu::bc()
{
    return regs[REG_C] | (regs[REG_B] << 8);
}

uint16_t Cpu::de()
{
    return regs[REG_E] | (regs[REG_D] << 8);
}

uint16_t Cpu::hl()
{
    return regs[REG_L] | (regs[REG_H] << 8);
}

SideEffects Cpu::cycle()
{
    uint8_t instr = memory[pc];
    pc++;
    SideEffects eff;
    eff.cycles = 0;

    switch(instr) {
        case 0x00: // NOP
            eff.cycles = 4;
            break;

        case 0x01: // LD BC, d16
            regs[REG_C] = memory[pc++];
            regs[REG_B] = memory[pc++];
            eff.cycles = 12;
            break;

        case 0x02: // LD (BC), A
            memory[bc()] = regs[REG_A];
            eff.cycles = 8;
            break;

        case 0x03: // INC BC
        {
            
            uint16_t v = bc()+1;
            regs[REG_B] = v & 0xFF;
            regs[REG_C] = v >> 8;
            eff.cycles = 8;
            break;
        }

        case 0x04: // INC B
            z = regs[REG_B] == 0xff;
            h = ((regs[REG_B] & 0xf) == 0xf);
            n = 0;
            regs[REG_B]++;
            eff.cycles = 4;
            break;

        case 0x05: // DEC B
            z = regs[REG_B] == 1;
            h = (regs[REG_B] & 0xF) == 0;
            n = 1;
            regs[REG_B]--;
            eff.cycles = 4;
            break;

        case 0x06: // LD B,d8
            regs[REG_B] = memory[pc++];
            eff.cycles = 8;
            break;

        case 0x0e: // LD C,d8
            regs[REG_C] = memory[pc++];
            eff.cycles = 8;
            break;

        case 0x20: // JR NZ,r8
            if (!z) {
                pc += (int8_t)memory[pc]+1;
                eff.cycles = 12;
            } else {
                pc++;
                eff.cycles = 8;
            }
            break;

        case 0x21: // LD HL,d16
            regs[REG_L] = memory[pc++];
            regs[REG_H] = memory[pc++];
            eff.cycles = 12;
            break;

        case 0x32: // LDD (HL),A
        {
            memory[hl()] = regs[REG_A];
            uint16_t v = hl()-1;
            regs[REG_L] = v & 0xFF;
            regs[REG_H] = v >> 8;
            eff.cycles = 8;
            break;
        }

        case 0xaf: // XOR A
            regs[REG_A] = 0;
            z = 1;
            c = 0;
            n = 0;
            h = 0;
            eff.cycles = 4;
            break;

        case 0xc3: // JP a16
            pc = memory[pc] | (memory[pc+1] << 8);
            eff.cycles = 16;
            break;

        default:
            fprintf(stderr, "Unknown instruction: %x\n", instr);
            exit(1);
    }

    assert(eff.cycles > 0);
    return eff;
}

void Cpu::disas(uint16_t addr, char* buf)
{
    uint8_t instr = memory[addr];
    uint8_t op8 = memory[addr+1];
    uint16_t op16 = (memory[addr+2] << 8) | memory[addr+1];
    switch(instr) {
        case 0x0:
            sprintf(buf, "NOP");
            break;

        case 0x1:
            sprintf(buf, "LD BC,0x%04x", op16);
            break;

        case 0x2:
            sprintf(buf, "LD (BC),A");
            break;

        case 0x3:
            sprintf(buf, "INC BC");
            break;

        case 0x4:
            sprintf(buf, "INC B");
            break;

        case 0x5:
            sprintf(buf, "DEC B");
            break;

        case 0x6:
            sprintf(buf, "LD B,%02x", op8);
            break;

        case 0x7:
            sprintf(buf, "RLCA");
            break;

        case 0x8:
            sprintf(buf, "LD (%04x),SP", op16);
            break;

        case 0x9:
            sprintf(buf, "ADD HL,BC");
            break;

        case 0xa:
            sprintf(buf, "LD A,(BC)");
            break;

        case 0xb:
            sprintf(buf, "DEC BC");
            break;

        case 0xc:
            sprintf(buf, "INC C");
            break;

        case 0xd:
            sprintf(buf, "DEC C");
            break;

        case 0xe:
            sprintf(buf, "LD C,%02x", op8);
            break;

        case 0xf:
            sprintf(buf, "RRCA");
            break;

        case 0x10:
            sprintf(buf, "STOP 0");
            break;

        case 0x11:
            sprintf(buf, "LD DE,%04x", op16);
            break;

        case 0x12:
            sprintf(buf, "LD (DE),A");
            break;

        case 0x13:
            sprintf(buf, "INC DE");
            break;

        case 0x14:
            sprintf(buf, "INC D");
            break;

        case 0x15:
            sprintf(buf, "DEC D");
            break;

        case 0x16:
            sprintf(buf, "LD D,%02x", op8);
            break;

        case 0x17:
            sprintf(buf, "RLA");
            break;

        case 0x18:
            sprintf(buf, "JR %d", (int8_t)op8);
            break;

        case 0x19:
            sprintf(buf, "ADD HL,DE");
            break;

        case 0x1a:
            sprintf(buf, "LD A,(DE)");
            break;

        case 0x1b:
            sprintf(buf, "DEC DE");
            break;

        case 0x1c:
            sprintf(buf, "INC E");
            break;

        case 0x1d:
            sprintf(buf, "DEC E");
            break;

        case 0x1e:
            sprintf(buf, "LD E,%02x", op8);
            break;

        case 0x1f:
            sprintf(buf, "RRA");
            break;
            
        case 0x20:
            sprintf(buf, "JR NZ,%d", (int8_t)op8);
    }
}
