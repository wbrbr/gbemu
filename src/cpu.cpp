#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "cpu.hpp"
#include "ppu.hpp"

Mbc::~Mbc() {};

Mbc0::Mbc0()
{
    memset(rom, 0, sizeof(rom));
    memset(ram, 0, sizeof(ram));
}

void Mbc0::load(uint8_t* cartridge, unsigned int size)
{
    memcpy(rom, cartridge, size);
}

void Mbc0::reset()
{
    memset(ram, 0, sizeof(ram));
}

uint8_t Mbc0::mem(uint16_t a)
{
    if (a <= 0x7fff) return rom[a];
    if (a >= 0xa000 && a <= 0xbfff) return ram[a - 0xa000];
    fprintf(stderr, "Wrong MBC0 read: %04x\n", a);
    return 0xff;
}

void Mbc0::memw(uint16_t a, uint8_t v)
{
    if (a >= 0xa000 && a <= 0xbfff) ram[a - 0xa000] = v;
}

Mbc1::Mbc1()
{
    rom = (uint8_t*)calloc(1, 0x200000);
    reset();
}

void Mbc1::load(uint8_t* cartridge, unsigned int size)
{
    memcpy(rom, cartridge, size);
}

void Mbc1::reset()
{
    memset(ram, 0, sizeof(ram));
    ram_enabled = false;
    rom_bank = 1;
    ram_bank = 0;
    bank_mode = 0;
}

uint8_t Mbc1::mem(uint16_t a)
{
    if (a <= 0x3fff) return rom[a];
    if (a <= 0x7fff) {
        return rom[0x4000*rom_bank + a-0x4000];
    }
    if (ram_enabled && a >= 0xa000 && a <= 0xbfff) {
        return ram[0x2000*ram_bank + a - 0xa000];
    }
    return 0xff;
}

void Mbc1::memw(uint16_t a, uint8_t v)
{
    if (a <= 0x1fff) {
        ram_enabled = (v & 0xf) == 0xA;
        return;
    }
    if (a >= 0x2000 && a <= 0x3fff) {
        uint8_t low = v & 0b00011111;
        if (low == 0) low = 1;
        rom_bank &= 0b11100000;
        rom_bank |= low;
        return;
    }
    if (a >= 0x4000 && a <= 0x5fff) {
        puts("yay");
        if (bank_mode) {
            ram_bank = v & 3;
        } else {
            uint8_t hi = v & 3;
            rom_bank &= 0b00011111;
            rom_bank |= (hi << 5);
        }
        return;
    }
    if (a >= 0x6000 && a <= 0x7fff) {
        bank_mode = v & 1;
        return;
    }
    if (ram_enabled && a >= 0xa000 && a <= 0xbfff) {
        ram[rom_bank*0x2000 + a - 0xa000] = v;
        return;
    }
}


Cpu::Cpu(): serial(this)
{
    ppu = nullptr;
    mbc = nullptr;
    breakpoint = 0xffff;
    reset();
}

void Cpu::reset()
{
    pc = 0x100;
    sp = 0xFFFE;
    ie = 0;
    if_ = 0;
    ime = true;
    z = h = c = 1;
    n = 0;
    memset(wram, 0, sizeof(wram));
    memset(hram, 0, sizeof(hram));
    memset(regs, 0, sizeof(regs));
    serial = SerialController(this);
    if (mbc) mbc->reset();
}

Cpu::~Cpu()
{
    delete mbc;
}

void Cpu::load(const char* path)
{
    FILE* fp = fopen(path, "r");
    fseek(fp, 0, SEEK_END);
    size_t size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    uint8_t* cartridge = new uint8_t[size];
    if (fread(cartridge, 1, size, fp) != size) {
        perror("fread: ");
        exit(1);
    }

    assert(cartridge[0x104] == 0xCE && cartridge[0x105] == 0xED);
    char title[17];
    memcpy(title, cartridge + 0x134, 16);
    title[16] = 0;

    puts(title);

    switch(cartridge[0x147])
    {
        case 0x00:
            mbc = new Mbc0();
            break;

        case 0x01:
            mbc = new Mbc1();
            break;

        default:
            fprintf(stderr, "Unsupported MBC\n");
            exit(1);
    }
    mbc->load(cartridge, size);
}

uint8_t Cpu::mem(uint16_t a, bool bypass)
{
    if (a <= 0x7FFF) return mbc->mem(a);
    if (a <= 0x9FFF) {
        if (!bypass && !ppu->vramaccess()) return 0xFF;
        return ppu->vram[a - 0x8000];
    }
    if (a <= 0xBFFF) return mbc->mem(a);
    if (a <= 0xDFFF) return wram[a - 0xC000];
    if (a <= 0xFDFF) return wram[a - 0xE000];
    if (a <= 0xFE9F) {
        if (!bypass && !ppu->oamaccess()) return 0xFF;
        return ppu->oam[a - 0xFE00];
    }
    if (a <= 0xFEFF) {
        fprintf(stderr, "Invalid memory read: %04x (pc = %04x)\n", a, pc);
        return 0;
    }
    if (a <= 0xFF7F) {
        switch(a) {
            case 0xFF00: return joypad.joyp();
            case 0xFF01: return serial.sb;
            case 0xFF02: return serial.sc;
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

// return true if break, false otherwise
bool Cpu::memw(uint16_t a, uint8_t v)
{
    bool b = false;
    if (a <= 0x7FFF) return b;
    if (a <= 0x9FFF) {
        if (!ppu->vramaccess()) return b;
        ppu->vram[a - 0x8000] = v;
        return b;
    }
    if (a <= 0xBFFF) {
        mbc->memw(a, v);
        return b;
    }
    if (a <= 0xDFFF) {
        wram[a - 0xC000] = v;
        return b;
    }
    if (a <= 0xFDFF) {
        wram[a - 0xE000] = v;
        return b;
    }
    if (a <= 0xFE9F) {
        if (!ppu->oamaccess()) return b;
        ppu->oam[a - 0xFE00] = v;
        return b;
    }
    if (a <= 0xFEFF) return b;

    if (a <= 0xFF7F) {
        switch(a) {
            case 0xFF00: joypad.select_buttons = (v & (1 << 5)) == 0; break;
            case 0xFF01: serial.sb = v; puts("serial!!"); break;
            case 0xFF02: serial.sc = v; break;
            case 0xFF0F: if_ = v; break;
            case 0xFF40: ppu->lcdc = v; break;
            case 0xFF41: assert((v & 0xF) == 0); ppu->stat = v; break; // TODO: only change top bits
            case 0xFF42: ppu->scy = v; break;
            case 0xFF43: ppu->scx = v; break;
            case 0xFF45: ppu->lyc = v; break;
            case 0xFF46:
                // bug: the transfer should take 160 cycles
                for (uint8_t i = 0; i <= 0x9F; i++)
                {
                    uint16_t a = (v << 8) | i;
                    ppu->oam[i] = mem(a);
                }
                break;
            case 0xFF47: ppu->bgp = v; break;
            case 0xFF48: ppu->obp0 = v; break;
            case 0xFF49: ppu->obp1 = v; break;
            case 0xFF4A: ppu->wy = v; break;
            case 0xFF4B: ppu->wx = v; break;
            default:
                fprintf(stderr, "Unsupported I/O write: %04x (pc = %04x)\n", a, pc);
        }
        return b;
    }

    if (a <= 0xFFFE) {
        hram[a - 0xFF80] = v;
        return b;
    }
    ie = v;

    return b;
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
            regs[REG_B] = v >> 8;
            regs[REG_C] = v & 0xff;
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

        case 0x07: // RLCA
            c = (regs[REG_A] & (1 << 7)) > 0;
            regs[REG_A] <<= 1;
            regs[REG_A] |= c;
            z = n = h = 0;
            eff.cycles = 4;
            break;

            
        case 0x08: // LD (a16),SP
        {
            uint16_t a16 = mem(pc) | (mem(pc+1) << 8);
            memw(a16, sp & 0xff);
            memw(a16+1, sp >> 8);
            pc += 2;
            eff.cycles = 20;
            break;
        }

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

        case 0x0f: // RRCA
            c = regs[REG_A] & 1;
            regs[REG_A] >>= 1;
            z = h = n = 0;
            eff.cycles = 4;
            break;

        case 0x10: // STOP
            pc++;
            eff.cycles = 4;
            break;
            // TODO: wait for interrupt

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

        case 0x14: // INC D
            h = (regs[REG_D] & 0xf) == 0xf;
            regs[REG_D]++;
            z = regs[REG_D] == 0;
            n = 0;
            eff.cycles = 4;
            break;
            

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

        case 0x1b: // DEC DE
        {
            uint16_t v = de() - 1;
            regs[REG_E] = v & 0xff;
            regs[REG_D] = v >> 8;
            eff.cycles = 4;
            break;
        }

        case 0x1c: // INC E
            h = (regs[REG_E] & 0xf) == 0xf;
            regs[REG_E]++;
            z = regs[REG_E] == 0;
            n = 0;
            eff.cycles = 4;
            break;

        case 0x1d: // DEC E
            n = 1;
            h = (regs[REG_E] & 0xF) == 0;
            regs[REG_E]--;
            z = regs[REG_E] == 0;
            eff.cycles = 4;
            break;

        case 0x1e: // LD E,d8
            regs[REG_E] = mem(pc++);
            eff.cycles = 8;
            break;

        case 0x1f: // RRA
        {
            uint8_t tmp = c;
            c = regs[REG_A] & 1;
            regs[REG_A] >>= 1;
            regs[REG_A] |= (tmp << 7);
            z = n = h = 0;
            eff.cycles = 8;
            break;
        }

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

        case 0x24: // INC H
            h = (regs[REG_H] & 0xF) == 0xF;
            regs[REG_H]++;
            z = regs[REG_H] == 0;
            n = 0;
            eff.cycles = 4;
            break;

        case 0x25: // DEC H
            z = regs[REG_H] == 1;
            h = (regs[REG_H] & 0xF) == 0;
            n = 1;
            regs[REG_H]--;
            eff.cycles = 4;
            break;


        case 0x26: // LD H,d8
            regs[REG_H] = mem(pc++);
            eff.cycles = 8;
            break;
            
        case 0x27: // DAA
            daa();
            eff.cycles = 4;
            break;

        case 0x28: // JR Z,r8
            if (z) {
                pc += (int8_t)mem(pc++);
                eff.cycles = 12;
            } else {
                pc++;
                eff.cycles = 8;
            }
            break;

        case 0x29: // ADD HL,HL
        {
            c = hl() + hl() > 0xffff;
            h = (hl() & 0xfff) + (hl() & 0xfff) > 0xfff;
            uint16_t v = hl() + hl();
            regs[REG_L] = v & 0xff;
            regs[REG_H] = v >> 8;
            n = 0;
            eff.cycles = 8;
            break;
        }

        case 0x2a: // LD A,(HL+)
        {
            regs[REG_A] = mem(hl());
            uint16_t v = hl()+1;
            regs[REG_L] = v & 0xFF;
            regs[REG_H] = v >> 8;
            eff.cycles = 8;
            break;
        }

        case 0x2b: // DEC HL
        {
            uint16_t v = hl()-1;
            regs[REG_L] = v & 0xff;
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

        case 0x2e: // LD L,d8
            regs[REG_L] = mem(pc++);
            eff.cycles = 8;
            break;

        case 0x2f: // CPL
            regs[REG_A] ^= 0xff;
            n = 1;
            h = 1;
            eff.cycles = 4;
            break;

        case 0x30: // JR NC,r8
            if (!c) {
                pc += (int8_t)mem(pc++);
                eff.cycles = 12;
            } else {
                pc++;
                eff.cycles = 8;
            }
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

        case 0x33: // INC SP
            sp++; 
            eff.cycles = 8;
            break;

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
            memw(hl(), mem(pc++));
            eff.cycles = 12;
            break;

        case 0x38: // JR C,r8
            if (c) {
                pc += (int8_t)mem(pc++);
                eff.cycles = 12;
            } else {
                pc++;
                eff.cycles = 8;
            }
            break;

        case 0x39: // ADD HL,SP
        {
            c = hl() + sp > 0xffff;
            h = (hl() & 0xfff) + (sp & 0xfff) > 0xfff;
            uint16_t v = hl() + sp;
            regs[REG_L] = v & 0xff;
            regs[REG_H] = v >> 8;
            n = 0;
            eff.cycles = 8;
            break;
        }


        case 0x3a: // LD A,(HL-)
        {
            regs[REG_A] = mem(hl());
            uint16_t v = hl()-1;
            regs[REG_L] = v & 0xff;
            regs[REG_H] = v >> 8;
            eff.cycles = 8;
            break;
        }

        case 0x3b: // DEC SP
            sp--;
            eff.cycles = 8;
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

        case 0x54: // LD D,H
            regs[REG_D] = regs[REG_H];
            eff.cycles = 4;
            break;

        case 0x56: // LD D,(HL)
            regs[REG_D] = mem(hl());
            eff.cycles = 8;
            break;

        case 0x57: // LD D,A
            regs[REG_D] = regs[REG_A];
            eff.cycles = 4;
            break;

        case 0x5a: // LD E,D
            regs[REG_E] = regs[REG_D];
            eff.cycles = 4;
            break;

        case 0x5d: // LD E,L
            regs[REG_E] = regs[REG_L];
            eff.cycles = 4;
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

        case 0x62: // LD H,D
            regs[REG_H] = regs[REG_D];
            eff.cycles = 4;
            break;

        case 0x63: // LD H,E
            regs[REG_H] = regs[REG_E];
            eff.cycles = 4;
            break;

        case 0x64: // LD H,H
            eff.cycles = 4;
            break;

        case 0x65: // LD H,L
            regs[REG_H] = regs[REG_L];
            eff.cycles = 4;
            break;

        case 0x66: // LD H,(HL)
            regs[REG_H] = mem(hl());
            eff.cycles = 8;
            break;

        case 0x67: // LD H,A
            regs[REG_H] = regs[REG_A];
            eff.cycles = 4;
            break;

        case 0x68: // LD L,B
            regs[REG_L] = regs[REG_B];
            eff.cycles = 4;
            break;

        case 0x6a: // LD L,D
            regs[REG_L] = regs[REG_D];
            eff.cycles = 4;
            break;

        case 0x6b: // LD L,E
            regs[REG_L] = regs[REG_E];
            eff.cycles = 4;
            break;

        case 0x6c: // LD L,H
            regs[REG_L] = regs[REG_H];
            eff.cycles = 4;
            break;

        case 0x6d: // LD L,L
            eff.cycles = 4;
            break;

        case 0x6e: // LD L,(HL)
            regs[REG_L] = mem(hl());
            eff.cycles = 8;
            break;

        case 0x6f: // LD L,A
            regs[REG_L] = regs[REG_A];
            eff.cycles = 4;
            break;

        case 0x69: // LD L,C
            regs[REG_L] = regs[REG_C];
            eff.cycles = 4;
            break;

        case 0x70: // LD (HL),B
            memw(hl(), regs[REG_B]);
            eff.cycles = 8;
            break;

        case 0x71: // LD (HL),C
            memw(hl(), regs[REG_C]);
            eff.cycles = 8;
            break;

        case 0x72: // LD (HL),D
            memw(hl(), regs[REG_D]);
            eff.cycles = 8;
            break;

        case 0x73: // LD (HL),E
            memw(hl(), regs[REG_E]);
            eff.cycles = 8;
            break;

        case 0x77: // LD (HL),A
            memw(hl(), regs[REG_A]);
            eff.cycles = 8;
            break;

        case 0x78: // LD A,B
            regs[REG_A] = regs[REG_B];
            eff.cycles = 4;
            break;

        case 0x79: // LD A,C
            regs[REG_A] = regs[REG_C];
            eff.cycles = 4;
            break;

        case 0x7a: // LD A,D
            regs[REG_A] = regs[REG_E];
            eff.cycles = 4;
            break;

        case 0x7b: // LD A,E
            regs[REG_A] = regs[REG_E];
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

        case 0x7e: // LD A,(HL)
            regs[REG_A] = mem(hl());
            eff.cycles = 8;
            break;

        case 0x80: // ADD A,B
            h = (regs[REG_A] & 0xf) + (regs[REG_B] & 0xf) > 0xf;
            c = regs[REG_A] + regs[REG_B] > 0xff;
            regs[REG_A] += regs[REG_B];
            z = regs[REG_A] == 0;
            n = 0;
            eff.cycles = 4;
            break;

        case 0x82: // ADD A,D
            h = (regs[REG_A] & 0xf) + (regs[REG_D] & 0xf) > 0xf;
            c = regs[REG_A] + regs[REG_D] > 0xff;
            regs[REG_A] += regs[REG_D];
            z = regs[REG_A] == 0;
            n = 0;
            eff.cycles = 4;
            break;

        case 0x85: // ADD A,L
            h = (regs[REG_A] & 0xf) + (regs[REG_L] & 0xf) > 0xf;
            c = regs[REG_A] + regs[REG_L] > 0xff;
            regs[REG_A] += regs[REG_L];
            z = regs[REG_A] == 0;
            n = 0;
            eff.cycles = 4;
            break;

        case 0x86: // ADD A,(HL)
        {
            uint8_t v = mem(hl());
            h = (regs[REG_A] & 0xf) + (v & 0xf) > 0xf;
            c = regs[REG_A] + v > 0xff;
            regs[REG_A] += v;
            z = regs[REG_A] == 0;
            n = 0;
            eff.cycles = 4;
            break;
        }


        case 0x87: // ADD A,A
            h = (regs[REG_A] & 0xf) + (regs[REG_A] & 0xf) > 0xf;
            c = regs[REG_A] + regs[REG_A] > 0xff;
            regs[REG_A] += regs[REG_A];
            z = regs[REG_A] == 0;
            n = 0;
            eff.cycles = 4;
            break;

        case 0x89: // ADD A,C
            h = (regs[REG_A] & 0xf) + (regs[REG_C] & 0xf) > 0xf;
            c = regs[REG_A] + regs[REG_C] > 0xff;
            regs[REG_A] += regs[REG_C];
            z = regs[REG_A] == 0;
            n = 0;
            eff.cycles = 4;
            break;

        case 0x8e: // ADC A,(HL)
        {
            uint8_t v = mem(hl()) + c;
            h = (regs[REG_A] & 0xf) + (v & 0xf) > 0xf;
            c = regs[REG_A] + v > 0xff;
            regs[REG_A] += v;
            z = regs[REG_A] == 0;
            n = 0;
            eff.cycles = 8;
            break;
        }

        case 0x91: // SUB C
            c = regs[REG_C] > regs[REG_A];
            h = (regs[REG_C] & 0xf) > (regs[REG_A] & 0xf);
            n = 1;
            regs[REG_A] -= regs[REG_C];
            z = regs[REG_A] == 0;
            eff.cycles = 4;
            break;

        case 0x96: // SUB (HL)
        {
            uint8_t v = mem(hl());
            c = v > regs[REG_A];
            h = (v & 0xf) > (regs[REG_A] & 0xf);
            n = 1;
            regs[REG_A] -= v;
            z = regs[REG_A] == 0;
            eff.cycles = 8;
            break;
        }

        case 0x9c: // SBC A,H
        {
            uint8_t v = regs[REG_H] + c;
            c = v > regs[REG_A];
            h = (v & 0xf) > (regs[REG_A] & 0xf);
            n = 1;
            regs[REG_A] -= v;
            z = regs[REG_A] == 0;
            eff.cycles = 4;
            break;
        }
        
        case 0x9d: // SBC A,L
        {
            uint8_t v = regs[REG_L] + c;
            c = v > regs[REG_A];
            h = (v & 0xf) > (regs[REG_A] & 0xf);
            n = 1;
            regs[REG_A] -= v;
            z = regs[REG_A] == 0;
            eff.cycles = 4;
            break;
        }

        case 0x9e: // SBC A,(HL)
        {
            uint8_t v = mem(hl()) + c;
            c = v > regs[REG_A];
            h = (v & 0xf) > (regs[REG_A] & 0xf);
            n = 1;
            regs[REG_A] -= v;
            z = regs[REG_A] == 0;
            eff.cycles = 8;
            break;
        }

        case 0x9f: // SBC A,A
        {
            uint8_t v = regs[REG_A] + c;
            c = v > regs[REG_A];
            h = (v & 0xf) > (regs[REG_A] & 0xf);
            n = 1;
            regs[REG_A] -= v;
            z = regs[REG_A] == 0;
            eff.cycles = 4;
            break;
        }

        case 0xa0: // AND
            regs[REG_A] &= regs[REG_B];
            z = regs[REG_A] == 0;
            h = 1;
            n = c = 0;
            eff.cycles = 4;
            break;

        case 0xa1: // AND C
            regs[REG_A] &= regs[REG_C];
            z = regs[REG_A] == 0;
            h = 1;
            n = c = 0;
            eff.cycles = 4;
            break;

        case 0xa2: // AND D
            regs[REG_A] &= regs[REG_D];
            z = regs[REG_A] == 0;
            h = 1;
            n = c = 0;
            eff.cycles = 4;
            break;

        case 0xa3: // AND E
            regs[REG_A] &= regs[REG_E];
            z = regs[REG_A] == 0;
            h = 1;
            n = c = 0;
            eff.cycles = 4;
            break;

        case 0xa4: // AND H
            regs[REG_A] &= regs[REG_H];
            z = regs[REG_A] == 0;
            h = 1;
            n = c = 0;
            eff.cycles = 4;
            break;

        case 0xa5: // AND L
            regs[REG_A] &= regs[REG_L];
            z = regs[REG_A] == 0;
            h = 1;
            n = c = 0;
            eff.cycles = 4;
            break;

        case 0xa6: // AND (HL)
            regs[REG_A] &= mem(hl());
            z = regs[REG_A] == 0;
            h = 1;
            n = c = 0;
            eff.cycles = 8;
            break;

        case 0xa7: // AND A
            n = c = 0;
            h = 1;
            z = regs[REG_A] == 0;
            eff.cycles = 4;
            break;

        case 0xa8: // XOR B
            regs[REG_A] ^= regs[REG_C];
            z = regs[REG_A] == 0;
            n = h = c = 0;
            eff.cycles = 4;
            break;

        case 0xa9: // XOR C
            regs[REG_A] ^= regs[REG_C];
            z = regs[REG_A] == 0;
            n = h = c = 0;
            eff.cycles = 4;
            break;

        case 0xad: // XOR L
            regs[REG_A] ^= regs[REG_C];
            z = regs[REG_A] == 0;
            n = h = c = 0;
            eff.cycles = 4;
            break;

        case 0xae: // XOR (HL)
            regs[REG_A] ^= mem(hl());
            z = regs[REG_A] == 0;
            n = h = c = 0;
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

        case 0xb2: // OR D
            regs[REG_A] = regs[REG_A] | regs[REG_D];
            n = c = h = 0;
            z = regs[REG_A] == 0;
            eff.cycles = 4;
            break;

        case 0xb6: // OR (HL)
            regs[REG_A] |= mem(hl());
            z = regs[REG_A] == 0;
            n = h = c = 0;
            eff.cycles = 8;
            break;

        case 0xb7: // OR A
            z = regs[REG_A] == 0;
            n = h = c = 0;
            eff.cycles = 4;
            break;

        case 0xb9: // CP C
            c = regs[REG_C] > regs[REG_A];
            z = regs[REG_C] == regs[REG_A];
            n = 1;
            h = (regs[REG_C] & 0xF) > (regs[REG_A] & 0xF);
            eff.cycles = 4;
            break;

        case 0xbb: // CP E
            c = regs[REG_E] > regs[REG_A];
            z = regs[REG_E] == regs[REG_A];
            n = 1;
            h = (regs[REG_E] & 0xF) > (regs[REG_A] & 0xF);
            eff.cycles = 4;
            break;

        case 0xbc: // CP H
            c = regs[REG_H] > regs[REG_A];
            z = regs[REG_H] == regs[REG_A];
            n = 1;
            h = (regs[REG_H] & 0xF) > (regs[REG_A] & 0xF);
            eff.cycles = 4;
            break;

        case 0xbe: // CP (HL)
        {
            uint8_t v = mem(hl());
            c = v > regs[REG_A];
            z = v == regs[REG_A];
            n = 1;
            h = (v & 0xF) > (regs[REG_A] & 0xF);
            eff.cycles = 8;
            break;
        }


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

        case 0xc4: // CALL NZ,a16
            if (!z) {
                push(pc+2);
                pc = mem(pc) | (mem(pc+1) << 8);
                eff.cycles = 24;
            } else {
                pc += 2;
                eff.cycles = 12;
            }
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

        case 0xce: // ADC A,d8
        {
            uint8_t v = mem(pc++) + c;
            h = (regs[REG_A] & 0xf) + (v & 0xf) > 0xf;
            c = regs[REG_A] + v > 0xff;
            regs[REG_A] += v;
            z = regs[REG_A] == 0;
            n = 0;
            eff.cycles = 8;
            break;
        }

        case 0xd0: // RET NC
            if (!c) {
                pc = pop16();
                eff.cycles = 20;
            } else {
                eff.cycles = 8;
            }
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

        case 0xd6: // SUB d8
        {
            uint8_t d8 = mem(pc++);
            c = d8 > regs[REG_A];
            h = (d8 & 0xf) > (regs[REG_A] & 0xf);
            n = 1;
            regs[REG_A] -= d8;
            z = regs[REG_A] == 0;
            eff.cycles = 8;
            break;
        }

        case 0xd8: // RET C
            if (c) {
                pc = pop16();
                eff.cycles = 20;
                break;
            } else {
                eff.cycles = 8;
            }
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

        case 0xe8: // ADD SP,r8
        {
            int16_t r8 = (int8_t)mem(pc++);
            c = (sp & 0xff) + (r8 & 0xff) > 0xff;
            h = (sp & 0xf) + (r8 & 0xf) > 0xf;
            z = n = 0;
            sp += r8;
            eff.cycles = 16;
            break;
        }

        case 0xe9: // JP HL
            pc = hl();
            eff.cycles = 4;
            break;

        case 0xee: // XOR d8
            regs[REG_A] ^= mem(pc++);
            z = regs[REG_A] == 0;
            n = h = c = 0;
            eff.cycles = 8;
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

        case 0xf6: // OR d8
            regs[REG_A] |= mem(pc++);
            z = regs[REG_A] == 0;
            h = n = c = 0;
            eff.cycles = 8;
            break;

        case 0xf8: // LD HL,SP+r8
        {
            int16_t r8 = (int8_t)mem(pc++);
            c = (sp & 0xff) + (r8 & 0xff) > 0xff;
            h = (sp & 0xf) + (r8 & 0xf) > 0xf;
            z = n = 0;
            uint16_t v = sp+r8;
            regs[REG_L] = v & 0xff;
            regs[REG_H] = v >> 8;
            eff.cycles = 12;
            break;
        }

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

        case 0xf9: // LD SP,HL
            sp = hl();
            eff.cycles = 8;
            break;

        default:
            fprintf(stderr, "Unknown instruction %02x at address %04x\n", instr, pc-1);
            exit(1);
    }

    assert(eff.cycles > 0);

    // serial.exec(eff.cycles);

    if (pc == breakpoint) eff.break_ = true;
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
    

    if (instr >= 0x40 && instr <= 0x7f) { // BIT
        uint16_t off = instr - 0x40;
        uint8_t bit = off / 0x8;
        uint8_t reg = off % 0x8;
        uint8_t v;
        eff.cycles = 8;
        
        switch(reg) {
            case 0:
                v = regs[REG_B];
                break;

            case 1:
                v = regs[REG_C];
                break;

            case 2:
                v = regs[REG_D];
                break;

            case 3:
                v = regs[REG_E];
                break;

            case 4:
                v = regs[REG_H];
                break;

            case 5:
                v = regs[REG_L];
                break;

            case 6:
                v = mem(hl());
                eff.cycles = 16;
                break;

            case 7:
                v = regs[REG_A];
                break;
        }
        z = (v & (1 << bit)) == 0;
        n = 0;
        h = 1;
        return;
    }

    if (instr >= 0x80 && instr <= 0xbf) { // res
        uint16_t off = instr - 0x80;
        uint8_t bit = off / 0x8;
        uint8_t reg = off % 0x8;
        if (reg == 6) { // (HL)
            memw(hl(), mem(hl()) & ~(1 << bit));
            eff.cycles = 16;
        } else {
            uint8_t* v;
            switch(reg) {
                case 0:
                    v = &regs[REG_B];
                    break;

                case 1:
                    v = &regs[REG_C];
                    break;

                case 2:
                    v = &regs[REG_D];
                    break;

                case 3:
                    v = &regs[REG_E];
                    break;

                case 4:
                    v = &regs[REG_H];
                    break;

                case 5:
                    v = &regs[REG_L];
                    break;

                case 7:
                    v = &regs[REG_A];
                    break;
            }
            *v &= ~(1 << bit);
            eff.cycles = 8;
        }
        return;
    }

    if (instr >= 0xc0) { // SET
        uint16_t off = instr - 0xc0;
        uint8_t bit = off / 0x8;
        uint8_t reg = off % 0x8;
        if (reg == 6) { // (HL)
            memw(hl(), mem(hl()) | (1 << bit));
            eff.cycles = 16;
        } else {
            uint8_t* v;
            switch(reg) {
                case 0:
                    v = &regs[REG_B];
                    break;

                case 1:
                    v = &regs[REG_C];
                    break;

                case 2:
                    v = &regs[REG_D];
                    break;

                case 3:
                    v = &regs[REG_E];
                    break;

                case 4:
                    v = &regs[REG_H];
                    break;

                case 5:
                    v = &regs[REG_L];
                    break;

                case 7:
                    v = &regs[REG_A];
                    break;
            }
            *v |= 1 << bit;
            eff.cycles = 8;
        }
        return;
    }

    switch(instr) {

        case 0x08: // RRC B
            c = regs[REG_B] & 1;
            regs[REG_B] >>= 1;
            z = regs[REG_B] == 0;
            n = h = 0;
            eff.cycles = 8;
            break;

        case 0x19: // RR C
        {
            uint8_t tmp = c;
            c = regs[REG_C] & 1;
            regs[REG_C] >>= 1;
            regs[REG_C] |= (tmp << 7);
            z = regs[REG_C] == 0;
            n = h = 0;
            eff.cycles = 8;
            break;
        }

        case 0x1a: // RR D
        {
            uint8_t tmp = c;
            c = regs[REG_D] & 1;
            regs[REG_D] >>= 1;
            regs[REG_D] |= (tmp << 7);
            z = regs[REG_D] == 0;
            n = h = 0;
            eff.cycles = 8;
            break;
        }

        case 0x1b: // RR E
        {
            uint8_t tmp = c;
            c = regs[REG_E] & 1;
            regs[REG_E] >>= 1;
            regs[REG_E] |= (tmp << 7);
            z = regs[REG_E] == 0;
            n = h = 0;
            eff.cycles = 8;
            break;
        }

        case 0x0e: // RRC (HL)
        {
            c = mem(hl()) & 1;
            uint8_t v = mem(hl()) >> 1;
            memw(hl(), v);
            z = v == 0;
            n = h = 0;
            eff.cycles = 16;
            break;
        }

        case 0x1f: // RR A
        {
            uint8_t tmp = c;
            c = regs[REG_C] & 1;
            regs[REG_C] >>= 1;
            regs[REG_C] |= (tmp << 7);
            z = regs[REG_C] == 0;
            n = h = 0;
            eff.cycles = 8;
            break;
        }

        case 0x27: // SLA A
            c = (regs[REG_A] & (1 << 7)) > 0; // check if bit 7 is set
            regs[REG_A] <<= 1;
            z = regs[REG_A] == 0;
            eff.cycles = 8;
            break;

        case 0x30: // SWAP B
            regs[REG_B] = ((regs[REG_B] & 0x0f) << 4) | ((regs[REG_B] & 0xf0) >> 4);
            z = regs[REG_B] == 0;
            n = h = c = 0;
            eff.cycles = 8;
            break;

        case 0x33: // SWAP E
            regs[REG_E] = ((regs[REG_E] & 0x0f) << 4) | ((regs[REG_E] & 0xf0) >> 4);
            z = regs[REG_E] == 0;
            n = h = c = 0;
            eff.cycles = 8;
            break;

        case 0x37: // SWAP A
            regs[REG_A] = ((regs[REG_A] & 0x0f) << 4) | ((regs[REG_A] & 0xf0) >> 4);
            z = regs[REG_A] == 0;
            n = h = c = 0;
            eff.cycles = 8;
            break;

        case 0x38: // SRL B
            c = regs[REG_B] & 1;
            regs[REG_B] >>= 1;
            z = regs[REG_B] == 0;
            n = h = 0;
            eff.cycles = 8;
            break;

        case 0x3f: // SRL A
            c = regs[REG_A] & 1;
            regs[REG_A] >>= 1;
            z = regs[REG_A] == 0;
            n = h = 0;
            eff.cycles = 8;
            break;

        case 0x40: // BIT 0,B
            instr_bit(regs[REG_B], 0);
            eff.cycles = 8;
            break;

        case 0x41: // BIT 0,C
            instr_bit(regs[REG_C], 0);
            eff.cycles = 8;
            break;

        case 0x42: // BIT 0,D
            instr_bit(regs[REG_D], 0);
            eff.cycles = 8;
            break;

        case 0x43: // BIT 0,E
            instr_bit(regs[REG_E], 0);
            eff.cycles = 8;
            break;

        case 0x44: // BIT 0,H
            z = (regs[REG_H] & (1 << 0)) == 0;
            n = 0;
            h = 1;
            eff.cycles = 8;
            break;

        case 0x45: // BIT 0,L
            z = (regs[REG_L] & (1 << 0)) == 0;
            n = 0;
            h = 1;
            eff.cycles = 8;
            break;

        case 0x46: // BIT 0,(HL)
            z = (mem(hl()) & (1 << 0)) == 0;
            n = 0;
            h = 1;
            eff.cycles = 16;
            break;

        case 0x47: // BIT 0,A
            z = (regs[REG_A] & (1 << 0)) == 0;
            n = 0;
            h = 1;
            eff.cycles = 16;
            break;

        case 0x48: // BIT 1,B
            z = (regs[REG_B] & (1 << 1)) == 0;
            n = 0;
            h = 1;
            eff.cycles = 8;
            break;

        case 0x49: // BIT 1,C
            z = (regs[REG_C] & (1 << 1)) == 0;
            n = 0;
            h = 1;
            eff.cycles = 8;
            break;

        case 0x4a: // BIT 1,D
            z = (regs[REG_C] & (1 << 1)) == 0;
            n = 0;
            h = 1;
            eff.cycles = 8;
            break;

        case 0x4b: // BIT 1,E
            z = (regs[REG_E] & (1 << 1)) == 0;
            n = 0;
            h = 1;
            eff.cycles = 8;
            break;

        case 0x4c: // BIT 1,H
            z = (regs[REG_H] & (1 << 1)) == 0;
            n = 0;
            h = 1;
            eff.cycles = 8;
            break;

        case 0x4d: // BIT 1,L
            z = (regs[REG_L] & (1 << 1)) == 0;
            n = 0;
            h = 1;
            eff.cycles = 8;
            break;

        case 0x4e: // BIT 1,(HL)
            z = (mem(hl()) & (1 << 1)) == 0;
            n = 0;
            h = 1;
            eff.cycles = 16;
            break;

        case 0x4f: // BIT 1,A
            z = (regs[REG_A] & (1 << 1)) == 0;
            n = 0;
            h = 1;
            eff.cycles = 8;
            break;

        case 0x50: // BIT 2,B
            z = (regs[REG_B] & (1 << 2)) == 0;
            n = 0;
            h = 1;
            eff.cycles = 8;
            break;

        case 0x51: // BIT 2,C
            instr_bit(regs[REG_C],2);
            eff.cycles = 8;
            break;

        case 0x52: // BIT 2,D
            instr_bit(regs[REG_D],2);
            eff.cycles = 8;
            break;

        case 0x53: // BIT 2,E
            instr_bit(regs[REG_E],2);
            eff.cycles = 8;
            break;

        case 0x54: // BIT 2,H
            instr_bit(regs[REG_H],2);
            eff.cycles = 8;
            break;

        case 0x55: // BIT 2,L
            instr_bit(regs[REG_L],2);
            eff.cycles = 8;
            break;

        case 0x56: // BIT 2,(HL)
            instr_bit(mem(hl()),2);
            eff.cycles = 16;
            break;

        case 0x57: // BIT 2,A
            z = (regs[REG_A] & (1 << 2)) == 0;
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

        case 0x59: // BIT 3,C
            instr_bit(regs[REG_C], 3);
            eff.cycles = 8;
            break;

        case 0x5a: // BIT 3,D
            instr_bit(regs[REG_D], 3);
            eff.cycles = 8;
            break;

        case 0x5b: // BIT 3,E
            instr_bit(regs[REG_E], 3);
            eff.cycles = 8;
            break;

        case 0x5c: // BIT 3,H
            instr_bit(regs[REG_H], 3);
            eff.cycles = 8;
            break;

        case 0x5d: // BIT 3,L
            instr_bit(regs[REG_L], 3);
            eff.cycles = 8;
            break;

        case 0x5e: // BIT 3,(HL)
            instr_bit(mem(hl()), 3);
            eff.cycles = 16;
            break;

        case 0x5f: // BIT 3,A
            z = (regs[REG_A] & (1 << 3)) == 0;
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

        case 0x61: // BIT 4,C
            z = (regs[REG_C] & (1 << 4)) == 0;
            n = 0;
            h = 1;
            eff.cycles = 8;
            break;

        case 0x62: // BIT 4,D
            instr_bit(regs[REG_D], 4);
            eff.cycles = 8;
            break;

        case 0x63: // BIT 4,E
            instr_bit(regs[REG_E], 4);
            eff.cycles = 8;
            break;

        case 0x64: // BIT 4,H
            instr_bit(regs[REG_H], 4);
            eff.cycles = 8;
            break;

        case 0x65: // BIT 4,L
            instr_bit(regs[REG_L], 4);
            eff.cycles = 8;
            break;

        case 0x66: // BIT 4,(HL)
            instr_bit(mem(hl()), 4);
            eff.cycles = 16;
            break;

        case 0x67: // BIT 4, A
            instr_bit(regs[REG_A], 4);
            eff.cycles = 8;
            break;

        case 0x68: // BIT 5,B
            z = (regs[REG_B] & (1 << 5)) == 0;
            n = 0;
            h = 1;
            eff.cycles = 8;
            break;

        case 0x69: // BIT 5,C
            z = (regs[REG_C] & (1 << 5)) == 0;
            n = 0;
            h = 1;
            eff.cycles = 8;
            break;

        case 0x6c: // BIT 5,H
            z = (regs[REG_H] & (1 << 5)) == 0;
            n = 0;
            h = 1;
            eff.cycles = 8;
            break;

        case 0x6f: // BIT 5,A
            z = (regs[REG_A] & (1 << 5)) == 0;
            n = 0;
            h = 1;
            eff.cycles = 8;
            break;
        
        case 0x70: // BIT 6,B
            z = (regs[REG_B] & (1 << 6)) == 0;
            n = 0;
            h = 1;
            eff.cycles = 8;
            break;

        case 0x77: // BIT 6,A
            z = (regs[REG_A] & (1 << 6)) == 0;
            n = 0;
            h = 1;
            eff.cycles = 8;
            break;

        case 0x78: // BIT 7,B
            z = (regs[REG_B] & (1 << 7)) == 0;
            n = 0;
            h = 1;
            eff.cycles = 8;
            break;

        case 0x79: // BIG 7,C
            z = (regs[REG_C] & (1 << 7)) == 0;
            n = 0;
            h = 1;
            eff.cycles = 8;
            break;

        case 0x7d: // BIT 7,L
            z = (regs[REG_L] & (1 << 7)) == 0;
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

        case 0x82: // RES 0,D
            regs[REG_D] &= ~1;
            eff.cycles = 8;
            break;

        case 0x86: // RES 0,(HL)
            memw(hl(), mem(hl()) & ~1);
            eff.cycles = 16;
            break;

        case 0x87: // RES 0,A
            regs[REG_A] &= ~1;
            eff.cycles = 8;
            break;

        case 0x8e: // RES 1,(HL)
            memw(hl(), mem(hl()) & ~(1<<1));
            eff.cycles = 16;
            break;

        case 0xbe: // RES 7,(HL)
            memw(hl(), mem(hl()) & ~(1<<7));
            eff.cycles = 16;
            break;

        case 0xfe: // SET 7,(HL)
            memw(hl(), mem(hl()) | (1 << 7));
            eff.cycles = 16;
            break;


        default:
            fprintf(stderr, "Unknown prefix instruction CB %02x (pc = %02x)\n", instr, pc-2);
            exit(1);
    }
}


// Stolen from SameBoy
void Cpu::daa()
{

    int16_t result = regs[REG_A];

    if (n) {
        if (h) {
            result = (result - 0x06) & 0xFF;
        }

        if (c) {
            result -= 0x60;
        }
    }
    else {
        if (h || (result & 0x0F) > 0x09) {
            result += 0x06;
        }

        if (c || result > 0x9F) {
            result += 0x60;
        }
    }

    if ((result & 0x100) == 0x100) {
        c = 1;
    }
    h = 0;
    regs[REG_A] = result & 0xff;
    z = regs[REG_A] == 0;
}

SerialController::SerialController(Cpu* cpu)
{
    this->cpu = cpu;
    remaining = 0;
    remaining_bits = 0;
    sb = 0;
    sc = 0;
}

void SerialController::exec(uint8_t cycles)
{
    // TODO: external clock, clock speeds
    if ((sc & (1 << 7)) > 0) {
        if (remaining_bits == 0) { // begin transfer
            remaining_bits = 8;
        }
        remaining -= cycles;
        if (remaining <= 0) { // send a bit
            remaining = 512;
            remaining_bits--;

            sb <<= 1; // receive a 0

            if (remaining_bits == 0) { // end transfer
                sc &= ~(1 << 7);
                cpu->if_ |= (1 << 3);
            }
        }
    }
}


JoypadController::JoypadController()
{
    select_buttons = true;
    buttons_state = 0b11011111;
    directions_state = 0b11101111;
}

uint8_t JoypadController::joyp()
{
    return select_buttons ? buttons_state : directions_state;
}

void Cpu::instr_bit(uint8_t v, uint8_t bit)
{
    puts("instr bit unimplemented");
}
