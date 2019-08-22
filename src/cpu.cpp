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
        fprintf(stderr, "Forbidden access to %04x (pc = %04x)", a, pc);
        return 0xFF;
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
                fprintf(stderr, "Unsupported I/O read: %04x (pc = %04x)\n", a, pc);
                return 0xFF;
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
        if (!pp->oamaccess() return;
        ppu->oam[a - 0xFE00] = v;
        return;
    }
    if (a <= 0xFEFF) {
        fprintf(stderr, "Forbidden access to %04x (pc = %04x)", a, pc);
        return;
    }

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

        case 0x32: // LDD (HL),A
        {
            memw(hl(), regs[REG_A]);
            uint16_t v = hl()-1;
            regs[REG_L] = v & 0xFF;
            regs[REG_H] = v >> 8;
            eff.cycles = 8;
            break;
        }

        case 0x3e: // LD A,d8
            regs[REG_A] = mem(pc++);
            eff.cycles = 8;
            break;

        case 0xaf: // XOR A
            regs[REG_A] = 0;
            z = 1;
            c = 0;
            n = 0;
            h = 0;
            eff.cycles = 4;
            break;

        case 0xc3: // JP a16
            pc = mem(pc) | (mem(pc+1) << 8);
            eff.cycles = 16;
            break;

        case 0xe0: // LD ($ff00+a8),A
            memw(0xff00 + mem(pc++), regs[REG_A]);
            eff.cycles = 12;
            break;

        case 0xf0: // LD A,($ff00+a8)
            regs[REG_A] = mem(0xff00+mem(pc++));
            eff.cycles = 12;
            break;

        case 0xf2: // LD A,(C)
            regs[REG_A] = mem(regs[REG_C]);
            eff.cycles = 8;
            break;

        case 0xf3: // DI
            ime = false;
            eff.cycles = 4;
            break;

        case 0xfb: // EI
            ime = true;
            eff.cycles = 4;
            break;
            // FIXME: bug here: the next instruction cannot be interrupted on the GB
        
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
