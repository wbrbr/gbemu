#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "cpu.hpp"
#include "ppu.hpp"

Cpu::Cpu()
{
    ppu = nullptr;
    pc = 0x100;
    sp = 0xFFFE;
    ie = 0;
    if_ = 0;
    ime = true;
    memset(rom, 0, sizeof(rom));
    memset(wram, 0, sizeof(wram));
    memset(hram, 0, sizeof(hram));
    memset(regs, 0, sizeof(regs));
}

void Cpu::load(const char* path)
{
    FILE* fp = fopen(path, "r");
    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    assert(size <= 0x8000); // for now
    if (fread(rom, 1, size, fp) != size) {
        perror("fread: ");
        exit(1);
    }

    assert(rom[0x104] == 0xCE && rom[0x105] == 0xED);
    char title[17];
    memcpy(title, rom + 0x134, 16);
    title[16] = 0;

    puts(title);

    assert(rom[0x147] == 0 && rom[0x148] == 0);
}

uint8_t Cpu::mem(uint16_t a)
{
    if (a <= 0x7FFF) return rom[a];
    if (a <= 0x9FFF) {
        if (!ppu->vramaccess()) return 0xFF;
        return ppu->vram[a - 0x8000];
    }
    if (a <= 0xBFFF) {
        fprintf(stderr, "ERAM not supported\n");
        return 0xFF;
    }
    if (a <= 0xDFFF) return wram[a - 0xC000];
    if (a <= 0xFDFF) return wram[a - 0xE000];
    if (a <= 0xFE9F) {
        if (!ppu->oamaccess()) return 0xFF;
        return ppu->oam[a - 0xFE00];
    }
    if (a <= 0xFEFF) {
        fprintf(stderr, "Invalid memory read: %04x (pc = %04x)\n", a, pc);
        return 0;
    }
    if (a <= 0xFF7F) {
        switch(a) {
            case 0xFF0F: return if_;
            case 0xFF40: return ppu->lcdc;
            case 0xFF41: return ppu->stat;
            case 0xFF42: return ppu->scy;
            case 0xFF43: return ppu->scx;
            case 0xFF44: return ppu->ly;
            case 0xFF45: return ppu->lyc;
            case 0xFF47: return ppu->bgp;
            case 0xFF48: return ppu->obp0;
            case 0xFF49: return ppu->obp1;
            case 0xFF4A: return ppu->wy;
            case 0xFF4B: return ppu->wx;
            default:
                return 0xFF;
                fprintf(stderr, "Unsupported I/O read: %04x (pc = %04x)\n", a, pc);
        }
    }

    if (a <= 0xFFFE) return hram[a - 0xFF80];
    return ie;
}

void Cpu::memw(uint16_t a, uint8_t v)
{
    if (a <= 0x7FFF) return;
    if (a <= 0x9FFF) {
        if (!ppu->vramaccess()) return;
        ppu->vram[a - 0x8000] = v;
        return;
    }
    if (a <= 0xBFFF) {
        fprintf(stderr, "ERAM not supported\n");
        return;
    }
    if (a <= 0xDFFF) {
        wram[a - 0xC000] = v;
        return;
    }
    if (a <= 0xFDFF) {
        wram[a - 0xE000] = v;
        return;
    }
    if (a <= 0xFE9F) {
        if (!ppu->oamaccess()) return;
        ppu->oam[a - 0xFE00] = v;
        return;
    }
    if (a <= 0xFEFF) return;

    if (a <= 0xFF7F) {
        switch(a) {
            case 0xFF0F: if_ = v; break;
            case 0xFF40: ppu->lcdc = v; break;
            case 0xFF41: assert((v & 0xF) == 0); ppu->stat = v; break; // TODO: only change top bits
            case 0xFF42: ppu->scy = v; break;
            case 0xFF43: ppu->scx = v; break;
            case 0xFF45: ppu->lyc = v; break;
            case 0xFF46: fprintf(stderr, "OAM DMA not implemented\n"); break;
            case 0xFF47: ppu->bgp = v; break;
            case 0xFF48: ppu->obp0 = v; break;
            case 0xFF49: ppu->obp1 = v; break;
            case 0xFF4A: ppu->wy = v; break;
            case 0xFF4B: ppu->wx = v; break;
            default:
                fprintf(stderr, "Unsupported I/O write: %04x (pc = %04x)\n", a, pc);
                return;
        }
    }

    if (a <= 0xFFFE) {
        hram[a - 0xFF80] = v;
        return;
    }
    ie = v;
}

void Cpu::push(uint16_t v)
{
    sp -= 2;
    memw(sp, v & 0xFF);
    memw(sp+1, v >> 8);
}

uint8_t Cpu::pop8()
{
    uint8_t v = mem(sp);
    sp++;
    return v;
}

uint16_t Cpu::pop16()
{
    uint16_t v = mem(sp) | (mem(sp+1) << 8);
    sp += 2;
    return v;
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
    if (ime) {
        uint16_t int_handlers[] = {0x40, 0x48, 0x50, 0x58, 0x60};
        for (int i = 0; i <= 4; i++)
        {
            if (((ie & if_) >> i) & 1) {
                printf("interrupt %d\n", i);
                if_ &= ~(1 << i);
                ime = false;
                push(pc);
                pc = int_handlers[i];
                // FIXME: needs to increment cycles by 5
                break;
            }
        }
    }

    uint8_t instr = mem(pc);
    pc++;
    SideEffects eff;
    eff.cycles = 0;

    switch(instr) {
        case 0x00: // NOP
            eff.cycles = 4;
            break;

        case 0x01: // LD BC, d16
            regs[REG_C] = mem(pc++);
            regs[REG_B] = mem(pc++);
            eff.cycles = 12;
            break;

        case 0x02: // LD (BC), A
            memw(bc(), regs[REG_A]);
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
            regs[REG_B] = mem(pc++);
            eff.cycles = 8;
            break;

        case 0x09: // ADD HL,BC
        {
            c = hl() + bc() > 0xffff;
            h = (hl() & 0xfff) + (bc() & 0xfff) > 0xfff;
            uint16_t v = hl() + bc();
            regs[REG_L] = v & 0xff;
            regs[REG_H] = v >> 8;
            n = 0;
            eff.cycles = 8;
            break;
        }

        case 0x0a: // LD A,(BC)
            regs[REG_A] = mem(bc());
            eff.cycles = 8;
            break;

        case 0x0b: // DEC BC
        {
            uint16_t v = bc()-1;
            regs[REG_B] = v >> 8;
            regs[REG_C] = v & 0xFF;
            eff.cycles = 8;
            break;
        }


        case 0x0c: // INC C
            z = regs[REG_C] == 0xff;
            h = ((regs[REG_C] & 0xf) == 0xf);
            n = 0;
            regs[REG_C]++;
            eff.cycles = 4;
            break;

        case 0x0d: // DEC C
            z = regs[REG_C] == 1;
            h = (regs[REG_C] & 0xF) == 0;
            n = 1;
            regs[REG_C]--;
            eff.cycles = 4;
            break;

        case 0x0e: // LD C,d8
            regs[REG_C] = mem(pc++);
            eff.cycles = 8;
            break;

        case 0x11: // LD DE,d16
            regs[REG_E] = mem(pc++);
            regs[REG_D] = mem(pc++);
            eff.cycles = 12;
            break;

        case 0x12: // LD (DE),A
            memw(de(), regs[REG_A]);
            eff.cycles = 8;
            break;

        case 0x13: // INC DE
        {
            uint16_t v = de() + 1;
            regs[REG_E] = v & 0xff;
            regs[REG_D] = v >> 8;
            eff.cycles = 8;
            break;            
        }

        case 0x16: // LD D,d8
            regs[REG_D] = mem(pc++);
            eff.cycles = 8;
            break;

        case 0x1A: // LD A,(DE)
            regs[REG_A] = mem(de());
            eff.cycles = 8;
            break;

        case 0x18: // JR r8
            pc += (int8_t)mem(pc++);
            eff.cycles = 12;
            break;

        case 0x19: // ADD HL,DE
        {
            c = hl() + de() > 0xffff;
            h = (hl() & 0xfff) + (de() & 0xfff) > 0xfff;
            uint16_t v = hl() + de();
            regs[REG_L] = v & 0xff;
            regs[REG_H] = v >> 8;
            n = 0;
            eff.cycles = 8;
            break;
        }

        case 0x1c: // INC E
            h = (regs[REG_E] & 0xf) == 0xf;
            regs[REG_E]++;
            z = regs[REG_E] == 0;
            n = 0;
            eff.cycles = 4;
            break;

        case 0x20: // JR NZ,r8
            if (!z) {
                pc += (int8_t)mem(pc++);
                eff.cycles = 12;
            } else {
                pc++;
                eff.cycles = 8;
            }
            break;

        case 0x21: // LD HL,d16
            regs[REG_L] = mem(pc++);
            regs[REG_H] = mem(pc++);
            eff.cycles = 12;
            break;

        case 0x22: // LD (HL+),A
        {
            memw(hl(), regs[REG_A]);
            uint16_t v = hl()+1;
            regs[REG_L] = v & 0xff;
            regs[REG_H] = v >> 8;
            eff.cycles = 8;
            break;
        }

        case 0x23: // INC HL
        {
            uint16_t v = hl()+1;
            regs[REG_L] = v & 0xff;
            regs[REG_H] = v >> 8;
            eff.cycles = 8;
            break;
        }

        case 0x28: // JR Z,r8
            if (z) {
                pc += (int8_t)mem(pc++);
                eff.cycles = 12;
            } else {
                pc++;
                eff.cycles = 8;
            }
            break;

        case 0x2a: // LD A,(HL+)
        {
            regs[REG_A] = mem(hl());
            uint16_t v = hl()+1;
            regs[REG_L] = v & 0xFF;
            regs[REG_H] = v >> 8;
            eff.cycles = 8;
            break;
        }

        case 0x2c: // INC L
            h = (regs[REG_L] & 0xF) == 0xF;
            regs[REG_L]++;
            z = regs[REG_L] == 0;
            n = 0;
            eff.cycles = 4;
            break;

        case 0x2d: // DEC L
            n = 1;
            h = (regs[REG_L] & 0xF) == 0;
            regs[REG_L]--;
            z = regs[REG_L] == 0;
            eff.cycles = 4;
            break;

        case 0x2f: // CPL
            regs[REG_A] ^= 0xff;
            n = 1;
            h = 1;
            eff.cycles = 4;
            break;


        case 0x31: // LD SP,d16
            sp = mem(pc) | (mem(pc+1) << 8);
            pc += 2;
            eff.cycles = 12;
            break;


        case 0x32: // LDD (HL),A
        {
            memw(hl(), regs[REG_A]);
            uint16_t v = hl()-1;
            regs[REG_L] = v & 0xFF;
            regs[REG_H] = v >> 8;
            eff.cycles = 8;
            break;
        }

        case 0x34: // INC (HL)
        {
            uint8_t v = mem(hl());
            z = v == 0xff;
            h = ((v & 0xf) == 0xf);
            n = 0;
            memw(hl(), v+1);
            eff.cycles = 12;
            break;
        }

        case 0x35: // DEC (HL)
        {
            uint8_t v = mem(hl());
            z = v == 1;
            h = (v & 0xF) == 0;
            n = 1;
            memw(hl(), v-1);
            eff.cycles = 12;
            break;
        }

        case 0x36: // LD (HL),d8
            memw(mem(hl()), mem(pc++));
            eff.cycles = 12;
            break;

        case 0x3c: // INC A
            h = (regs[REG_A] & 0xF) == 0xF;
            regs[REG_A]++;
            z = regs[REG_A] == 0;
            n = 0;
            eff.cycles = 4;
            break;

        case 0x3d: // DEC A
            n = 1;
            h = (regs[REG_A] & 0xF) == 0;
            regs[REG_A]--;
            z = regs[REG_A] == 0;
            eff.cycles = 4;
            break;

        case 0x3e: // LD A,d8
            regs[REG_A] = mem(pc++);
            eff.cycles = 8;
            break;

        case 0x40: // LD B,B
            eff.cycles = 4;
            break;

        case 0x46: // LD B,(HL)
            regs[REG_B] = mem(hl());
            eff.cycles = 8;
            break;

        case 0x47: // LD B,A
            regs[REG_B] = regs[REG_A];
            eff.cycles = 4;
            break;

        case 0x4e: // LD C,(HL)
            regs[REG_C] = mem(hl());
            eff.cycles = 8;
            break;

        case 0x4f: // LD C,A
            regs[REG_C] = regs[REG_A];
            eff.cycles = 4;
            break;

        case 0x56: // LD D,(HL)
            regs[REG_D] = mem(hl());
            eff.cycles = 8;
            break;

        case 0x5e: // LD E,(HL)
            regs[REG_E] = mem(hl());
            eff.cycles = 8;
            break;

        case 0x5f: // LD E,A
            regs[REG_E] = regs[REG_A];
            eff.cycles = 4;
            break;

        case 0x60: // LD H,B
            regs[REG_H] = regs[REG_B];
            eff.cycles = 4;
            break;

        case 0x6f: // LD L,A
            regs[REG_L] = regs[REG_A];
            eff.cycles = 4;
            break;

        case 0x69: // LD L,C
            regs[REG_L] = regs[REG_C];
            eff.cycles = 4;
            break;

        case 0x77: // LD (HL),A
            memw(hl(), regs[REG_A]);
            eff.cycles = 8;
            break;

        case 0x78: // LD A,B
            regs[REG_A] = regs[REG_B];
            eff.cycles = 4;
            break;

        case 0x7c: // LD A,H
            regs[REG_A] = regs[REG_H];
            eff.cycles = 4;
            break;

        case 0x7d: // LD A,L
            regs[REG_A] = regs[REG_L];
            eff.cycles = 4;
            break;

        case 0x79: // LD A,C
            regs[REG_A] = regs[REG_C];
            eff.cycles = 4;
            break;

        case 0x7e: // LD A,(HL)
            regs[REG_A] = mem(hl());
            eff.cycles = 8;
            break;

        case 0x85: // ADD A,L
            h = (regs[REG_A] & 0xf) + (regs[REG_L] & 0xf) > 0xf;
            c = regs[REG_A] + regs[REG_L] > 0xff;
            regs[REG_A] += regs[REG_L];
            z = regs[REG_A] == 0;
            n = 0;
            eff.cycles = 4;
            break;

        case 0x87: // ADD A,A
            h = (regs[REG_A] & 0xf) + (regs[REG_A] & 0xf) > 0xf;
            c = regs[REG_A] + regs[REG_A] > 0xff;
            regs[REG_A] += regs[REG_A];
            z = regs[REG_A] == 0;
            n = 0;
            eff.cycles = 4;
            break;

        case 0xa1: // AND B
            regs[REG_A] &= regs[REG_B];
            z = regs[REG_A] == 0;
            h = 1;
            n = c = 0;
            eff.cycles = 4;
            break;

        case 0xa7: // AND A
            n = c = 0;
            h = 1;
            z = regs[REG_A] == 0;
            eff.cycles = 4;
            break;

        case 0xa9: // XOR C
            regs[REG_A] ^= regs[REG_C];
            z = regs[REG_A] == 0;
            n = h = c = 0;
            eff.cycles = 4;
            break;

        case 0xaf: // XOR A
            regs[REG_A] = 0;
            z = 1;
            c = 0;
            n = 0;
            h = 0;
            eff.cycles = 4;
            break;

        case 0xb0: // OR B
            regs[REG_A] |= regs[REG_B];
            n = c = h = 0;
            z = regs[REG_A] == 0;
            eff.cycles = 4;
            break;

        case 0xb1: // OR C
            regs[REG_A] = regs[REG_A] | regs[REG_C];
            n = c = h = 0;
            z = regs[REG_A] == 0;
            eff.cycles = 4;
            break;

        case 0xc0: // RET NZ
            if (!z) {
                pc = pop16();
                eff.cycles = 20;
            } else {
                eff.cycles = 8;
            }
            break;

        case 0xc1: // POP BC
            regs[REG_C] = pop8();
            regs[REG_B] = pop8();
            eff.cycles = 12;
            break;

        case 0xc2: // JP NZ,a16
            if (!z) {
                pc = mem(pc) | (mem(pc+1) << 8);
                eff.cycles = 16;
            } else {
                pc += 2;
                eff.cycles = 12;
            }
            break;

        case 0xc3: // JP a16
            pc = mem(pc) | (mem(pc+1) << 8);
            eff.cycles = 16;
            break;

        case 0xc5: // PUSH BC
            push(bc());
            eff.cycles = 16;
            break;

        case 0xc6: // ADD A,d8
        {
            uint8_t d8 = mem(pc++);
            h = (regs[REG_A] & 0xf) + (d8 & 0xf) > 0xf;
            c = regs[REG_A] + d8 > 0xff;
            regs[REG_A] += d8;
            z = regs[REG_A] == 0;
            n = 0;
            eff.cycles = 8;
            break;
        }

        case 0xc8: // RET Z
            if (z) {
                pc = pop16();
                eff.cycles = 20;
            } else {
                eff.cycles = 8;
            }
            break;

        case 0xc9: // RET
            pc = pop16();
            eff.cycles = 16;
            break;

        case 0xca: // JP Z,a16
            if (z) {
                pc = mem(pc) | (mem(pc+1) << 8);
                eff.cycles = 16;
            } else {
                pc += 2;
                eff.cycles = 12;
            }
            break;
        case 0xcb: // PREFIX
            execPrefix(eff);
            break;

        case 0xcd: // CALL a16
            push(pc+2);
            pc = mem(pc) | (mem(pc+1) << 8);
            eff.cycles = 16;
            break;

        case 0xd1: // POP DE
            regs[REG_E] = pop8();
            regs[REG_D] = pop8();
            eff.cycles = 12;
            break;

        case 0xd5: // PUSH DE
            push(de());
            eff.cycles = 16;
            break;

        case 0xd9: // RETI
            ime = true;
            pc = pop16();
            eff.cycles = 16;
            break;

        case 0xe0: // LD ($ff00+a8),A
            memw(0xff00 + mem(pc++), regs[REG_A]);
            eff.cycles = 12;
            break;

        case 0xe1: // POP HL
            regs[REG_L] = pop8();
            regs[REG_H] = pop8();
            eff.cycles = 12;
            break;

        case 0xe2: // LD ($ff00+C),A
            memw(0xff00+regs[REG_C], regs[REG_A]);
            eff.cycles = 8;
            break;

        case 0xe5: // PUSH HL
            push(hl());
            eff.cycles = 16;
            break;

        case 0xe6: // AND d8
            regs[REG_A] &= mem(pc++);
            z = regs[REG_A] == 0;
            n = 0;
            h = 1;
            c = 0;
            eff.cycles = 8;
            break;

        case 0xea: // LD (a16),A
            memw(mem(pc) | (mem(pc+1) << 8), regs[REG_A]);
            pc += 2;
            eff.cycles = 16;
            break;

        case 0xe9: // JP HL
            pc = hl();
            eff.cycles = 4;
            break;

        case 0xef: // RST $28
            push(pc);
            pc = 0x28;
            eff.cycles = 16;
            break;


        case 0xf0: // LD A,($ff00+a8)
            regs[REG_A] = mem(0xff00+mem(pc++));
            eff.cycles = 12;
            break;

        case 0xf1: // POP AF
        {
            uint8_t f = pop8();
            regs[REG_A] = pop8();
            z = f >> 7;
            n = (f >> 6) & 1;
            h = (f >> 5) & 1;
            c = (f >> 4) & 1;
            eff.cycles = 12;
            break;
        }

        case 0xf2: // LD A,(C)
            regs[REG_A] = mem(regs[REG_C]);
            eff.cycles = 8;
            break;

        case 0xf3: // DI
            ime = false;
            eff.cycles = 4;
            break;

        case 0xf5: // PUSH AF
            push(af());
            eff.cycles = 16;
            break;

        case 0xfb: // EI
            ime = true;
            eff.cycles = 4;
            break;
            // FIXME: bug here: the next instruction cannot be interrupted on the GB
        
        case 0xfa: // LD A,(a16)
            regs[REG_A] = mem(mem(pc) | (mem(pc+1) << 8));
            pc += 2;
            eff.cycles = 16;
            break;
        
        case 0xfe: // CP d8
        {
            uint8_t d8 = mem(pc++);
            c = d8 > regs[REG_A];
            z = d8 == regs[REG_A];
            n = 1;
            h = (d8 & 0xF) > (regs[REG_A] & 0xF);
            eff.cycles = 8;
            break;
        }

        default:
            fprintf(stderr, "Unknown instruction %02x at address %04x\n", instr, pc-1);
            exit(1);
    }

    assert(eff.cycles > 0);
    return eff;
}

void Cpu::disas(uint16_t addr, char* buf)
{
    uint8_t instr = mem(addr);
    uint8_t op8 = mem(addr+1);
    uint16_t op16 = (mem(addr+2) << 8) | mem(addr+1);
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

void Cpu::execPrefix(SideEffects& eff)
{
    uint8_t instr = mem(pc++);
    // TODO: algorithmic decoding
    switch(instr) {
        case 0x27: // SLA A
            c = (regs[REG_A] & (1 << 7)) > 0; // check if bit 7 is set
            regs[REG_A] <<= 1;
            z = regs[REG_A] == 0;
            eff.cycles = 8;
            break;

        case 0x37: // SWAP A
            regs[REG_A] = ((regs[REG_A] & 0x0f) << 4) | ((regs[REG_A] & 0xf0) >> 4);
            z = regs[REG_A] == 0;
            n = h = c = 0;
            eff.cycles = 8;
            break;

        case 0x50: // BIT 2,B
            z = (regs[REG_B] & (1 << 2)) == 0;
            n = 0;
            h = 1;
            eff.cycles = 8;
            break;

        case 0x58: // BIT 3,B
            z = (regs[REG_B] & (1 << 3)) == 0;
            n = 0;
            h = 1;
            eff.cycles = 8;
            break;

        case 0x60: // BIT 4,B
            z = (regs[REG_B] & (1 << 4)) == 0;
            n = 0;
            h = 1;
            eff.cycles = 8;
            break;

        case 0x68: // BIT 5,B
            z = (regs[REG_B] & (1 << 5)) == 0;
            n = 0;
            h = 1;
            eff.cycles = 8;
            break;

        case 0x7e: // BIT 7,(HL)
            z = (mem(hl()) & (1 << 7)) == 0;
            n = 0;
            h = 1;
            eff.cycles = 16;
            break;

        case 0x7f: // BIT 7,A
            z = (regs[REG_A] & (1 << 7)) == 0;
            n = 0;
            h = 1;
            eff.cycles = 8;
            break;

        case 0x87: // RES 0,A
            regs[REG_A] &= ~1;
            eff.cycles = 8;
            break;

        default:
            fprintf(stderr, "Unknown prefix instruction CB %02x (pc = %02x)\n", instr, pc-2);
            exit(1);
    }
}
