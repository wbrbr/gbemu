#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "cpu.hpp"
#include "ppu.hpp"
#include "timer.hpp"
#include "opcodes.hpp"
#include "util.hpp"

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
    halted = false;
    reset();

    log_file = fopen("logfile.txt", "wb");
    if (!log_file) {
        perror("Failed to open logfile.txt:");
        exit(1);
    }
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
    regs[REG_A] = 0x01;
    regs[REG_C] = 0x13;
    regs[REG_E] = 0xd8;
    regs[REG_H] = 0x01;
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

uint8_t Cpu::mem(uint16_t a, bool bypass) const
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
            case 0xFF04: return timer->div;
            case 0xFF05: return timer->tima;
            case 0xFF06: return timer->tma;
            case 0xFF07: return timer->tac;
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
            case 0xFF01: serial.sb = v; printf("%c", v); break;
            case 0xFF02: serial.sc = v; break;
            case 0xFF04: timer->div = v; break;
            case 0xFF05: timer->tima = v; break;
            case 0xFF06: timer->tma = v; break;
            case 0xFF07: timer->tac = v; break;
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

void Cpu::instr_add(uint8_t v)
{
    h = (regs[REG_A] & 0xf) + (v & 0xf) > 0xf;
    c = regs[REG_A] + v > 0xff;
    regs[REG_A] += v;
    z = regs[REG_A] == 0;
    n = 0;
}

void Cpu::instr_adc(uint8_t v)
{
    uint8_t old_c = c;
    h = ((uint64_t)regs[REG_A] & 0xf) + ((uint64_t)v & 0xf) + ((uint64_t)c & 0xf) > 0xf;
    c = (uint64_t)regs[REG_A] + (uint64_t)v + (uint64_t)c > 0xff;
    regs[REG_A] += v + old_c;
    z = regs[REG_A] == 0;
    n = 0;
}

void Cpu::instr_sbc(uint8_t v)
{
    uint8_t old_c = c;
    h = (v & 0xf) + c > (regs[REG_A] & 0xf);
    c = v + c > regs[REG_A];
    n = 1;
    regs[REG_A] -= v + old_c;
    z = regs[REG_A] == 0;
}

void Cpu::instr_rst(uint16_t addr)
{
    push(pc);
    pc = addr;
}

void Cpu::instr_inc8(uint8_t &v)
{
    z = (v == 0xff);
    n = 0;
    h = ((v & 0x0f) == 0x0f);
    v++;
}

void Cpu::instr_dec8(uint8_t& v)
{
    z = (v == 1);
    n = 1;
    h = ((v & 0x0f) == 0);
    v--;
}

void Cpu::instr_sub(uint8_t v)
{
    c = v > regs[REG_A];
    h = (v & 0xf) > (regs[REG_A] & 0xf);
    n = 1;
    regs[REG_A] -= v;
    z = regs[REG_A] == 0;
}

void Cpu::instr_and(uint8_t v)
{
    regs[REG_A] &= v;
    z = regs[REG_A] == 0;
    h = 1;
    n = c = 0;
}

void Cpu::instr_xor(uint8_t v)
{
    regs[REG_A] ^= v;
    z = (regs[REG_A] == 0);
    n = h = c = 0;
}

void Cpu::instr_or(uint8_t v)
{
    regs[REG_A] |= v;
    n = c = h = 0;
    z = regs[REG_A] == 0;
}

void Cpu::instr_cp(uint8_t v)
{
    c = v > regs[REG_A];
    z = v == regs[REG_A];
    n = 1;
    h = (v & 0xF) > (regs[REG_A] & 0xF);
}

void Cpu::instr_rlc(uint8_t &v)
{
    c = v >> 7;
    v <<= 1;
    v |= c;
    z = v == 0;
    n = h = 0;
}

void Cpu::instr_rrc(uint8_t &v)
{
    c = v & 1;
    v >>= 1;
    v |= (c << 7);
    z = (v == 0);
    n = h = 0;
}

void Cpu::instr_rl(uint8_t &v)
{
    uint8_t old_c = c;
    c = v >> 7;
    v <<= 1;
    v |= old_c;
    z = v == 0;
    h = n = 0;
}

void Cpu::instr_rr(uint8_t &v)
{
    uint8_t old_c = c;
    c = v & 1;
    v >>= 1;
    v |= (old_c << 7);
    z = (v == 0);
    h = n = 0;
}

void Cpu::instr_sla(uint8_t &v)
{
    c = v >> 7;
    v <<= 1;
    z = (v == 0);
    h = n = 0;
}

void Cpu::instr_swap(uint8_t& v)
{
    v = (v << 4) | (v >> 4);
    z = (v == 0);
    c = h = n = 0;
}

void Cpu::instr_srl(uint8_t& v)
{
    c = v & 1;
    v >>= 1;
    z = (v == 0);
    h = n = 0;
}

void Cpu::instr_sra(uint8_t &v)
{
    c = v & 1;
    uint8_t b7 = v & (1 << 7);
    v >>= 1;
    v |= b7;
    z = (v == 0);
    h = n = 0;
}

void Cpu::executeInstruction(uint8_t instr, SideEffects& eff) {

    Opcode opcode = g_opcode_table[instr];
    eff.cycles = opcode.cycles;

    switch(instr) {
        case 0x00: // NOP
            break;

        case 0x01: // LD BC, d16
            regs[REG_C] = mem(pc++);
            regs[REG_B] = mem(pc++);
            break;

        case 0x02: // LD (BC), A
            memw(bc(), regs[REG_A]);
            break;

        case 0x03: // INC BC
        {
            uint16_t v = bc()+1;
            regs[REG_B] = v >> 8;
            regs[REG_C] = v & 0xff;
            break;
        }

        case 0x04: // INC B
            instr_inc8(regs[REG_B]);
            break;

        case 0x05: // DEC B
            instr_dec8(regs[REG_B]);
            break;

        case 0x06: // LD B,d8
            regs[REG_B] = mem(pc++);
            break;

        case 0x07: // RLCA
            c = (regs[REG_A] & (1 << 7)) > 0;
            regs[REG_A] <<= 1;
            regs[REG_A] |= c;
            z = n = h = 0;
            break;


        case 0x08: // LD (a16),SP
        {
            uint16_t a16 = mem(pc) | (mem(pc+1) << 8);
            memw(a16, sp & 0xff);
            memw(a16+1, sp >> 8);
            pc += 2;
            break;
        }

        case 0x09: // ADD HL,BC
        {
            c = (uint64_t)hl() + (uint64_t)bc() > 0xffff;
            h = (hl() & 0xfff) + (bc() & 0xfff) > 0xfff;
            uint16_t v = hl() + bc();
            regs[REG_L] = v & 0xff;
            regs[REG_H] = v >> 8;
            n = 0;
            break;
        }

        case 0x0a: // LD A,(BC)
            regs[REG_A] = mem(bc());
            break;

        case 0x0b: // DEC BC
        {
            uint16_t v = bc()-1;
            regs[REG_B] = v >> 8;
            regs[REG_C] = v & 0xFF;
            break;
        }


        case 0x0c: // INC C
            instr_inc8(regs[REG_C]);
            break;

        case 0x0d: // DEC C
            instr_dec8(regs[REG_C]);
            break;

        case 0x0e: // LD C,d8
            regs[REG_C] = mem(pc++);
            break;

        case 0x0f: // RRCA
            instr_rrc(regs[REG_A]);
            z = 0;
            break;

        case 0x10: // STOP
            pc++;
            break;
            // TODO: wait for interrupt

        case 0x11: // LD DE,d16
            regs[REG_E] = mem(pc++);
            regs[REG_D] = mem(pc++);
            break;

        case 0x12: // LD (DE),A
            memw(de(), regs[REG_A]);
            break;

        case 0x13: // INC DE
        {
            uint16_t v = de() + 1;
            regs[REG_E] = v & 0xff;
            regs[REG_D] = v >> 8;
            break;
        }

        case 0x14: // INC D
            instr_inc8(regs[REG_D]);
            break;

        case 0x15: // DEC D
            instr_dec8(regs[REG_D]);
            break;

        case 0x16: // LD D,d8
            regs[REG_D] = mem(pc++);
            break;

        case 0x17: // RLA
        {
            instr_rl(regs[REG_A]);
            z = 0;
            break;
        }

        case 0x1A: // LD A,(DE)
            regs[REG_A] = mem(de());
            break;

        case 0x18: // JR r8
            pc += unsigned_to_signed(mem(pc))+1;
            break;

        case 0x19: // ADD HL,DE
        {
            c = (uint64_t)hl() + (uint64_t)de() > 0xffff;
            h = (hl() & 0xfff) + (de() & 0xfff) > 0xfff;
            uint16_t v = hl() + de();
            regs[REG_L] = v & 0xff;
            regs[REG_H] = v >> 8;
            n = 0;
            break;
        }

        case 0x1b: // DEC DE
        {
            uint16_t v = de() - 1;
            regs[REG_E] = v & 0xff;
            regs[REG_D] = v >> 8;
            break;
        }

        case 0x1c: // INC E
            instr_inc8(regs[REG_E]);
            break;

        case 0x1d: // DEC E
            instr_dec8(regs[REG_E]);
            break;

        case 0x1e: // LD E,d8
            regs[REG_E] = mem(pc++);
            break;

        case 0x1f: // RRA
        {
            uint8_t tmp = c;
            c = regs[REG_A] & 1;
            regs[REG_A] >>= 1;
            regs[REG_A] |= (tmp << 7);
            z = n = h = 0;
            break;
        }

        case 0x20: // JR NZ,r8
            if (!z) {
                pc += unsigned_to_signed(mem(pc))+1;
                eff.cycles = 12;
            } else {
                pc++;
                eff.cycles = 8;
            }
            break;

        case 0x21: // LD HL,d16
            regs[REG_L] = mem(pc++);
            regs[REG_H] = mem(pc++);
            break;

        case 0x22: // LD (HL+),A
        {
            memw(hl(), regs[REG_A]);
            uint16_t v = hl()+1;
            regs[REG_L] = v & 0xff;
            regs[REG_H] = v >> 8;
            break;
        }

        case 0x23: // INC HL
        {
            uint16_t v = hl()+1;
            regs[REG_L] = v & 0xff;
            regs[REG_H] = v >> 8;
            break;
        }

        case 0x24: // INC H
            instr_inc8(regs[REG_H]);
            break;

        case 0x25: // DEC H
            instr_dec8(regs[REG_H]);
            break;


        case 0x26: // LD H,d8
            regs[REG_H] = mem(pc++);
            break;

        case 0x27: // DAA
            daa();
            break;

        case 0x28: // JR Z,r8
            if (z) {
                pc += unsigned_to_signed(mem(pc))+1;
                eff.cycles = 12;
            } else {
                pc++;
                eff.cycles = 8;
            }
            break;

        case 0x29: // ADD HL,HL
        {
            c = (uint64_t)hl() + (uint64_t)hl() > 0xffff;
            h = (hl() & 0xfff) + (hl() & 0xfff) > 0xfff;
            uint16_t v = hl() + hl();
            regs[REG_L] = v & 0xff;
            regs[REG_H] = v >> 8;
            n = 0;
            break;
        }

        case 0x2a: // LD A,(HL+)
        {
            regs[REG_A] = mem(hl());
            uint16_t v = hl()+1;
            regs[REG_L] = v & 0xFF;
            regs[REG_H] = v >> 8;
            break;
        }

        case 0x2b: // DEC HL
        {
            uint16_t v = hl()-1;
            regs[REG_L] = v & 0xff;
            regs[REG_H] = v >> 8;
            break;
        }

        case 0x2c: // INC L
            instr_inc8(regs[REG_L]);
            break;

        case 0x2d: // DEC L
            instr_dec8(regs[REG_L]);
            break;

        case 0x2e: // LD L,d8
            regs[REG_L] = mem(pc++);
            break;

        case 0x2f: // CPL
            regs[REG_A] ^= 0xff;
            n = 1;
            h = 1;
            break;

        case 0x30: // JR NC,r8
            if (!c) {
                pc += unsigned_to_signed(mem(pc))+1;
                eff.cycles = 12;
            } else {
                pc++;
                eff.cycles = 8;
            }
            break;

        case 0x31: // LD SP,d16
            sp = mem(pc) | (mem(pc+1) << 8);
            pc += 2;
            break;


        case 0x32: // LDD (HL),A
        {
            memw(hl(), regs[REG_A]);
            uint16_t v = hl()-1;
            regs[REG_L] = v & 0xFF;
            regs[REG_H] = v >> 8;
            break;
        }

        case 0x33: // INC SP
            sp++;
            break;

        case 0x34: // INC (HL)
        {
            uint8_t v = mem(hl());
            z = v == 0xff;
            h = ((v & 0xf) == 0xf);
            n = 0;
            memw(hl(), v+1);
            break;
        }

        case 0x35: // DEC (HL)
        {
            uint8_t v = mem(hl());
            z = v == 1;
            h = (v & 0xF) == 0;
            n = 1;
            memw(hl(), v-1);
            break;
        }

        case 0x36: // LD (HL),d8
            memw(hl(), mem(pc++));
            break;

        case 0x37: // SCF
            c = 1;
            n = 0;
            h = 0;
            break;


        case 0x38: // JR C,r8
            if (c) {
                pc += unsigned_to_signed(mem(pc))+1;
                eff.cycles = 12;
            } else {
                pc++;
                eff.cycles = 8;
            }
            break;

        case 0x39: // ADD HL,SP
        {
            c = (uint64_t)hl() + (uint64_t)sp > 0xffff;
            h = (hl() & 0xfff) + (sp & 0xfff) > 0xfff;
            uint16_t v = hl() + sp;
            regs[REG_L] = v & 0xff;
            regs[REG_H] = v >> 8;
            n = 0;
            break;
        }


        case 0x3a: // LD A,(HL-)
        {
            regs[REG_A] = mem(hl());
            uint16_t v = hl()-1;
            regs[REG_L] = v & 0xff;
            regs[REG_H] = v >> 8;
            break;
        }

        case 0x3b: // DEC SP
            sp--;
            break;

        case 0x3c: // INC A
            instr_inc8(regs[REG_A]);
            break;

        case 0x3d: // DEC A
            instr_dec8(regs[REG_A]);
            break;

        case 0x3e: // LD A,d8
            regs[REG_A] = mem(pc++);
            break;

        case 0x3f: // CCF
            c = !c;
            n = 0;
            h = 0;
            break;

        case 0x40: // LD B,B
            break;

        case 0x41: // LD B,C
            regs[REG_B] = regs[REG_C];
            break;

        case 0x42: // LD B,D
            regs[REG_B] = regs[REG_D];
            break;

        case 0x43: // LD B,E
            regs[REG_B] = regs[REG_E];
            break;

        case 0x44: // LD B,H
            regs[REG_B] = regs[REG_H];
            break;

        case 0x45: // LD B,L
            regs[REG_B] = regs[REG_L];
            break;

        case 0x46: // LD B,(HL)
            regs[REG_B] = mem(hl());
            break;

        case 0x47: // LD B,A
            regs[REG_B] = regs[REG_A];
            break;

        case 0x48: // LD C,B
            regs[REG_C] = regs[REG_B];
            break;

        case 0x49: // LD C,C
            break;

        case 0x4a: // LD C,D
            regs[REG_C] = regs[REG_D];
            break;

        case 0x4b: // LD C,E
            regs[REG_C] = regs[REG_E];
            break;

        case 0x4c: // LD C,H
            regs[REG_C] = regs[REG_H];
            break;

        case 0x4d: // LD C,L
            regs[REG_C] = regs[REG_L];
            break;

        case 0x4e: // LD C,(HL)
            regs[REG_C] = mem(hl());
            break;

        case 0x4f: // LD C,A
            regs[REG_C] = regs[REG_A];
            break;

        case 0x50: // LD D,B
            regs[REG_D] = regs[REG_B];
            break;

        case 0x51: // LD D,C
            regs[REG_D] = regs[REG_C];
            break;

        case 0x52: // LD D,D
            break;

        case 0x53: // LD D,E
            regs[REG_D] = regs[REG_E];
            break;

        case 0x54: // LD D,H
            regs[REG_D] = regs[REG_H];
            break;

        case 0x55: // LD D,L
            regs[REG_D] = regs[REG_L];
            break;

        case 0x56: // LD D,(HL)
            regs[REG_D] = mem(hl());
            break;

        case 0x57: // LD D,A
            regs[REG_D] = regs[REG_A];
            break;

        case 0x58: // LD E,B
            regs[REG_E] = regs[REG_B];
            break;

        case 0x59: // LD E,C
            regs[REG_E] = regs[REG_C];
            break;

        case 0x5a: // LD E,D
            regs[REG_E] = regs[REG_D];
            break;

        case 0x5b: // LD E,E
            break;

        case 0x5c: // LD E,H
            regs[REG_E] = regs[REG_H];
            break;

        case 0x5d: // LD E,L
            regs[REG_E] = regs[REG_L];
            break;

        case 0x5e: // LD E,(HL)
            regs[REG_E] = mem(hl());
            break;

        case 0x5f: // LD E,A
            regs[REG_E] = regs[REG_A];
            break;

        case 0x60: // LD H,B
            regs[REG_H] = regs[REG_B];
            break;

        case 0x61: // LD H,C
            regs[REG_H] = regs[REG_C];
            break;

        case 0x62: // LD H,D
            regs[REG_H] = regs[REG_D];
            break;

        case 0x63: // LD H,E
            regs[REG_H] = regs[REG_E];
            break;

        case 0x64: // LD H,H
            break;

        case 0x65: // LD H,L
            regs[REG_H] = regs[REG_L];
            break;

        case 0x66: // LD H,(HL)
            regs[REG_H] = mem(hl());
            break;

        case 0x67: // LD H,A
            regs[REG_H] = regs[REG_A];
            break;

        case 0x68: // LD L,B
            regs[REG_L] = regs[REG_B];
            break;

        case 0x69: // LD L,C
            regs[REG_L] = regs[REG_C];
            break;

        case 0x6a: // LD L,D
            regs[REG_L] = regs[REG_D];
            break;

        case 0x6b: // LD L,E
            regs[REG_L] = regs[REG_E];
            break;

        case 0x6c: // LD L,H
            regs[REG_L] = regs[REG_H];
            break;

        case 0x6d: // LD L,L
            break;

        case 0x6e: // LD L,(HL)
            regs[REG_L] = mem(hl());
            break;

        case 0x6f: // LD L,A
            regs[REG_L] = regs[REG_A];
            break;

        case 0x70: // LD (HL),B
            memw(hl(), regs[REG_B]);
            break;

        case 0x71: // LD (HL),C
            memw(hl(), regs[REG_C]);
            break;

        case 0x72: // LD (HL),D
            memw(hl(), regs[REG_D]);
            break;

        case 0x73: // LD (HL),E
            memw(hl(), regs[REG_E]);
            break;

        case 0x74: // LD (HL),H
            memw(hl(), regs[REG_H]);
            break;

        case 0x75: // LD (HL),L
            memw(hl(), regs[REG_L]);
            break;

        case 0x76: // HALT
            halted = true;
            break;

        case 0x77: // LD (HL),A
            memw(hl(), regs[REG_A]);
            break;

        case 0x78: // LD A,B
            regs[REG_A] = regs[REG_B];
            break;

        case 0x79: // LD A,C
            regs[REG_A] = regs[REG_C];
            break;

        case 0x7a: // LD A,D
            regs[REG_A] = regs[REG_D];
            break;

        case 0x7b: // LD A,E
            regs[REG_A] = regs[REG_E];
            break;

        case 0x7c: // LD A,H
            regs[REG_A] = regs[REG_H];
            break;

        case 0x7d: // LD A,L
            regs[REG_A] = regs[REG_L];
            break;

        case 0x7e: // LD A,(HL)
            regs[REG_A] = mem(hl());
            break;

        case 0x7f: // LD A,A
            break;

        case 0x80: // ADD A,B
            instr_add(regs[REG_B]);
            break;

        case 0x81: // ADD A,C
            instr_add(regs[REG_C]);
            break;

        case 0x82: // ADD A,D
            instr_add(regs[REG_D]);
            break;

        case 0x83: // ADD A,E
            instr_add(regs[REG_E]);
            break;

        case 0x84: // ADD A,H
            instr_add(regs[REG_H]);
            break;

        case 0x85: // ADD A,L
            instr_add(regs[REG_L]);
            break;

        case 0x86: // ADD A,(HL)
            instr_add(mem(hl()));
            break;

        case 0x87: // ADD A,A
            instr_add(regs[REG_A]);
            break;

        case 0x88: // ADC A,B
            instr_adc(regs[REG_B]);
            break;

        case 0x89: // ADC A,C
            instr_adc(regs[REG_C]);
            break;

        case 0x8a: // ADC A,D
            instr_adc(regs[REG_D]);
            break;

        case 0x8b: // ADC A,E
            instr_adc(regs[REG_E]);
            break;

        case 0x8c: // ADC A,H
            instr_adc(regs[REG_H]);
            break;

        case 0x8d: // ADC A,L
            instr_adc(regs[REG_L]);
            break;

        case 0x8e: // ADC A,(HL)
            instr_adc(mem(hl()));
            break;

        case 0x8f: // ADC A,A
            instr_adc(regs[REG_A]);
            break;

        case 0x90: // SUB B
            instr_sub(regs[REG_B]);
            break;

        case 0x91: // SUB C
            instr_sub(regs[REG_C]);
            break;

        case 0x92: // SUB D
            instr_sub(regs[REG_D]);
            break;

        case 0x93: // SUB E
            instr_sub(regs[REG_E]);
            break;

        case 0x94: // SUB H
            instr_sub(regs[REG_H]);
            break;

        case 0x95: // SUB L
            instr_sub(regs[REG_L]);
            break;

        case 0x96: // SUB (HL)
            instr_sub(mem(hl()));
            break;

        case 0x97: // SUB A
            instr_sub(regs[REG_A]);
            break;

        case 0x98: // SBC A,B
            instr_sbc(regs[REG_B]);
            break;

        case 0x99: // SBC A,C
            instr_sbc(regs[REG_C]);
            break;

        case 0x9a: // SBC A,D
            instr_sbc(regs[REG_D]);
            break;

        case 0x9b: // SBC A,E
            instr_sbc(regs[REG_E]);
            break;

        case 0x9c: // SBC A,H
            instr_sbc(regs[REG_H]);
            break;

        case 0x9d: // SBC A,L
            instr_sbc(regs[REG_L]);
            break;

        case 0x9e: // SBC A,(HL)
            instr_sbc(mem(hl()));
            break;

        case 0x9f: // SBC A,A
            instr_sbc(regs[REG_A]);
            break;

        case 0xa0: // AND B
            instr_and(regs[REG_B]);
            break;

        case 0xa1: // AND C
            instr_and(regs[REG_C]);
            break;

        case 0xa2: // AND D
            instr_and(regs[REG_D]);
            break;

        case 0xa3: // AND E
            instr_and(regs[REG_E]);
            break;

        case 0xa4: // AND H
            instr_and(regs[REG_H]);
            break;

        case 0xa5: // AND L
            instr_and(regs[REG_L]);
            break;

        case 0xa6: // AND (HL)
            instr_and(mem(hl()));
            break;

        case 0xa7: // AND A
            instr_and(regs[REG_A]);
            break;

        case 0xa8: // XOR B
            instr_xor(regs[REG_B]);
            break;

        case 0xa9: // XOR C
            instr_xor(regs[REG_C]);
            break;

        case 0xaa: // XOR D
            instr_xor(regs[REG_D]);
            break;

        case 0xab: // XOR E
            instr_xor(regs[REG_E]);
            break;

        case 0xac: // XOR H
            instr_xor(regs[REG_H]);
            break;

        case 0xad: // XOR L
            instr_xor(regs[REG_L]);
            break;

        case 0xae: // XOR (HL)
            instr_xor(mem(hl()));
            break;

        case 0xaf: // XOR A
            instr_xor(regs[REG_A]);
            break;

        case 0xb0: // OR B
            instr_or(regs[REG_B]);
            break;

        case 0xb1: // OR C
            instr_or(regs[REG_C]);
            break;

        case 0xb2: // OR D
            instr_or(regs[REG_D]);
            break;

        case 0xb3: // OR E
            instr_or(regs[REG_E]);
            break;

        case 0xb4: // OR H
            instr_or(regs[REG_H]);
            break;

        case 0xb5: // OR L
            instr_or(regs[REG_L]);
            break;

        case 0xb6: // OR (HL)
            instr_or(mem(hl()));
            break;

        case 0xb7: // OR A
            instr_or(regs[REG_A]);
            break;

        case 0xb8: // CP B
            instr_cp(regs[REG_B]);
            break;

        case 0xb9: // CP C
            instr_cp(regs[REG_C]);
            break;

        case 0xba: // CP D
            instr_cp(regs[REG_D]);
            break;

        case 0xbb: // CP E
            instr_cp(regs[REG_E]);
            break;

        case 0xbc: // CP H
            instr_cp(regs[REG_H]);
            break;

        case 0xbd: // CP L
            instr_cp(regs[REG_L]);
            break;

        case 0xbe: // CP (HL)
            instr_cp(mem(hl()));
            break;

        case 0xbf: // CP A
            instr_cp(regs[REG_A]);
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
            break;

        case 0xc2: // JP NZ,a16
            if (!z) {
                pc = mem(pc) | (mem(pc+1) << 8);
            } else {
                pc += 2;
            }
            break;

        case 0xc3: // JP a16
            pc = mem(pc) | (mem(pc+1) << 8);
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
            break;

        case 0xc6: // ADD A,d8
        {
            instr_add(mem(pc++));
            break;
        }

        case 0xc7: // RST 00H
            instr_rst(0);
            break;

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

        case 0xcc: // CALL Z,a16
            if (z) {
                push(pc+2);
                pc = mem(pc) | (mem(pc+1) << 8);
                eff.cycles = 24;
            } else {
                pc += 2;
                eff.cycles = 12;
            }
            break;

        case 0xcd: // CALL a16
            push(pc+2);
            pc = mem(pc) | (mem(pc+1) << 8);
            break;

        case 0xce: // ADC A,d8
        {
            instr_adc(mem(pc++));
            break;
        }

        case 0xcf: // RST 08H
            instr_rst(0x08);
            break;

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
            break;

        case 0xd2: // JP NC,a16
            if (!c) {
                pc = mem(pc) | (mem(pc+1) << 8);
                eff.cycles = 16;
            } else {
                pc += 2;
                eff.cycles = 12;
            }
            break;

        case 0xd4: // CALL NC,a16
            if (!c) {
                push(pc+2);
                pc = mem(pc) | (mem(pc+1) << 8);
                eff.cycles = 24;
            } else {
                pc += 2;
                eff.cycles = 12;
            }
            break;

        case 0xd5: // PUSH DE
            push(de());
            break;

        case 0xd6: // SUB d8
            instr_sub(mem(pc++));
            break;

        case 0xd7: // RST 10H
            instr_rst(0x10);
            break;

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
            break;

        case 0xda: // JP C,a16
            if (c) {
                pc = mem(pc) | (mem(pc+1) << 8);
                eff.cycles = 16;
            } else {
                pc += 2;
                eff.cycles = 12;
            }
            break;

        case 0xdc: // CALL C,a16
            if (c) {
                push(pc+2);
                pc = mem(pc) | (mem(pc+1) << 8);
                eff.cycles = 24;
            } else {
                pc += 2;
                eff.cycles = 12;
            }
            break;

        case 0xde: // SBC A,d8
            instr_sbc(mem(pc++));
            break;

        case 0xdf: // RST 18H
            instr_rst(0x18);
            break;

        case 0xe0: // LD ($ff00+a8),A
            memw(0xff00 + mem(pc++), regs[REG_A]);
            break;

        case 0xe1: // POP HL
            regs[REG_L] = pop8();
            regs[REG_H] = pop8();
            break;

        case 0xe2: // LD ($ff00+C),A
            memw(0xff00+regs[REG_C], regs[REG_A]);
            break;

        case 0xe5: // PUSH HL
            push(hl());
            break;

        case 0xe6: // AND d8
            instr_and(mem(pc++));
            break;

        case 0xe7: // RST 20H
            instr_rst(0x20);
            break;

        case 0xea: // LD (a16),A
            memw(mem(pc) | (mem(pc+1) << 8), regs[REG_A]);
            pc += 2;
            break;

        case 0xe8: // ADD SP,r8
        {
            uint16_t u8 = mem(pc++);
            int16_t r8 = unsigned_to_signed(u8);
            c = (sp & 0xff) + (u8 & 0xff) > 0xff;
            h = (sp & 0xf) + (u8 & 0xf) > 0xf;
            z = n = 0;
            sp += r8;
            break;
        }

        case 0xe9: // JP HL
            pc = hl();
            break;

        case 0xee: // XOR d8
            instr_xor(mem(pc++));
            break;

        case 0xef: // RST $28
            instr_rst(0x28);
            break;


        case 0xf0: // LD A,($ff00+a8)
            regs[REG_A] = mem(0xff00+mem(pc++));
            break;

        case 0xf1: // POP AF
        {
            uint8_t f = pop8();
            regs[REG_A] = pop8();
            z = f >> 7;
            n = (f >> 6) & 1;
            h = (f >> 5) & 1;
            c = (f >> 4) & 1;
            break;
        }

        case 0xf2: // LDH A,(C)
            regs[REG_A] = mem(0xff00+regs[REG_C]);
            break;

        case 0xf3: // DI
            ime = false;
            break;

        case 0xf5: // PUSH AF
            push(af());
            break;

        case 0xf6: // OR d8
            instr_or(mem(pc++));
            break;

        case 0xf7: // RST 30H
            instr_rst(0x30);
            break;

        case 0xf8: // LD HL,SP+r8
        {
            uint16_t u8 = mem(pc++);
            int16_t r8 = unsigned_to_signed(u8);
            c = (sp & 0xff) + (u8 & 0xff) > 0xff;
            h = (sp & 0xf) + (u8 & 0xf) > 0xf;
            z = n = 0;
            uint16_t v = sp+r8;
            regs[REG_L] = v & 0xff;
            regs[REG_H] = v >> 8;
            break;
        }

        case 0xf9: // LD SP,HL
            sp = hl();
            break;


        case 0xfb: // EI
            ime = true;
            break;
            // FIXME: bug here: the next instruction cannot be interrupted on the GB

        case 0xfa: // LD A,(a16)
            regs[REG_A] = mem(mem(pc) | (mem(pc+1) << 8));
            pc += 2;
            break;

        case 0xfe: // CP d8
        {
            instr_cp(mem(pc++));
            break;
        }

        case 0xff: // RST 38H
            instr_rst(0x38);
            break;

        default:
            fprintf(stderr, "Unknown instruction %02x at address %04x\n", instr, pc-1);
            exit(1);
    }

}

SideEffects Cpu::cycle()
{
    SideEffects eff{};
    eff.cycles = 0;
    eff.break_ = false;

    if ((ie & if_) != 0) {
        halted = false;
    }

    if (ime) {
        uint16_t int_handlers[] = {0x40, 0x48, 0x50, 0x58, 0x60};
        for (int i = 0; i <= 4; i++)
        {
            if (((ie & if_) >> i) & 1) {
                if_ &= ~(1 << i);
                ime = false;
                push(pc);
                pc = int_handlers[i];

                eff.cycles = 5*4;
                return eff;
            }
        }
    }

    if (halted) {
        eff.cycles += 4;
    } else {
        //fprintf(log_file, "A: %02x B: %02x C: %02x D: %02x E: %02x H: %02x L: %02x F: %02x PC: %04x (%02x %02x %02x) LY: %02x\n", regs[REG_A], regs[REG_B], regs[REG_C], regs[REG_D], regs[REG_E], regs[REG_H], regs[REG_L], af() & 0xff, pc, mem(pc), mem(pc+1), mem(pc+2), ppu->ly);
        uint8_t instr = mem(pc);
        pc++;
        executeInstruction(instr, eff);
    }

    assert(eff.cycles > 0);

    timer->update(eff.cycles, *this);

    // serial.exec(eff.cycles);

    if (pc == breakpoint) eff.break_ = true;
    return eff;
}

void Cpu::execPrefix(SideEffects& eff)
{
    uint8_t instr = mem(pc++);
    // TODO: algorithmic decoding

    Opcode opcode = g_prefix_opcode_table[instr];
    eff.cycles = opcode.cycles;
    

    if (instr >= 0x40 && instr <= 0x7f) { // BIT
        uint16_t off = instr - 0x40;
        uint8_t bit = off / 0x8;
        uint8_t reg = off % 0x8;
        uint8_t v;
        
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
        }
        return;
    }

    if (instr >= 0xc0) { // SET
        uint16_t off = instr - 0xc0;
        uint8_t bit = off / 0x8;
        uint8_t reg = off % 0x8;
        if (reg == 6) { // (HL)
            memw(hl(), mem(hl()) | (1 << bit));
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
        }
        return;
    }

    switch(instr) {

        case 0x00: // RLC B
            instr_rlc(regs[REG_B]);
            break;

        case 0x01: // RLC C
            instr_rlc(regs[REG_C]);
            break;

        case 0x02: // RLC D
            instr_rlc(regs[REG_D]);
            break;

        case 0x03: // RLC E
            instr_rlc(regs[REG_E]);
            break;

        case 0x04: // RLC H
            instr_rlc(regs[REG_H]);
            break;

        case 0x05: // RLC L
            instr_rlc(regs[REG_L]);
            break;

        case 0x06: // RLC (HL)
        {
            uint8_t v = mem(hl());
            c = v >> 7;
            v <<= 1;
            v |= c;
            z = v == 0;
            n = h = 0;
            memw(hl(), v);
            break;
        }

        case 0x07: // RLC A
            instr_rlc(regs[REG_A]);
            break;

        case 0x08: // RRC B
            instr_rrc(regs[REG_B]);
            break;

        case 0x09: // RRC C
            instr_rrc(regs[REG_C]);
            break;

        case 0x0a: // RRC D
            instr_rrc(regs[REG_D]);
            break;

        case 0x0b: // RRC E
            instr_rrc(regs[REG_E]);
            break;

        case 0x0c: // RRC H
            instr_rrc(regs[REG_H]);
            break;

        case 0x0d: // RRC L
            instr_rrc(regs[REG_L]);
            break;

        case 0x0e: // RRC (HL)
        {
            uint8_t v = mem(hl());
            instr_rrc(v);
            memw(hl(), v);
            break;
        }

        case 0x0f: // RRC A
            instr_rrc(regs[REG_A]);
            break;

        case 0x10: // RL B
            instr_rl(regs[REG_B]);
            break;

        case 0x11: // RL C
            instr_rl(regs[REG_C]);
            break;

        case 0x12: // RL D
            instr_rl(regs[REG_D]);
            break;

        case 0x13: // RL E
            instr_rl(regs[REG_E]);
            break;

        case 0x14: // RL H
            instr_rl(regs[REG_H]);
            break;

        case 0x15: // RL L
            instr_rl(regs[REG_L]);
            break;

        case 0x16: // RL (HL)
        {
            uint8_t v = mem(hl());
            instr_rl(v);
            memw(hl(), v);
            break;
        }

        case 0x17: // RL A
            instr_rl(regs[REG_A]);
            break;

        case 0x18: // RR B
            instr_rr(regs[REG_B]);
            break;

        case 0x19: // RR C
            instr_rr(regs[REG_C]);
            break;

        case 0x1a: // RR D
            instr_rr(regs[REG_D]);
            break;

        case 0x1b: // RR E
            instr_rr(regs[REG_E]);
            break;

        case 0x1c: // RR H
            instr_rr(regs[REG_H]);
            break;

        case 0x1d: // RR L
            instr_rr(regs[REG_L]);
            break;

        case 0x1e: // RR (HL)
        {
            uint8_t v = mem(hl());
            instr_rr(v);
            memw(hl(), v);
            break;
        }

        case 0x1f: // RR A
            instr_rr(regs[REG_A]);
            break;

        case 0x20: // SLA B
            instr_sla(regs[REG_B]);
            break;

        case 0x21: // SLA C
            instr_sla(regs[REG_C]);
            break;

        case 0x22: // SLA D
            instr_sla(regs[REG_D]);
            break;

        case 0x23: // SLA E
            instr_sla(regs[REG_E]);
            break;

        case 0x24: // SLA H
            instr_sla(regs[REG_H]);
            break;

        case 0x25: // SLA L
            instr_sla(regs[REG_L]);
            break;

        case 0x26: // SLA (HL)
        {
            uint8_t v = mem(hl());
            instr_sla(v);
            memw(hl(), v);
            break;
        }

        case 0x27: // SLA A
            instr_sla(regs[REG_A]);
            break;

        case 0x28: // SRA B
            instr_sra(regs[REG_B]);
            break;

        case 0x29: // SRA C
            instr_sra(regs[REG_C]);
            break;

        case 0x2a: // SRA D
            instr_sra(regs[REG_D]);
            break;

        case 0x2b: // SRA E
            instr_sra(regs[REG_E]);
            break;

        case 0x2c: // SRA H
            instr_sra(regs[REG_H]);
            break;

        case 0x2d: // SRA L
            instr_sra(regs[REG_L]);
            break;

        case 0x2e: // SRA (HL)
        {
            uint8_t v = mem(hl());
            instr_sra(v);
            memw(hl(), v);
            break;
        }

        case 0x2f: // SRA A
            instr_sra(regs[REG_A]);
            break;

        case 0x30: // SWAP B
            instr_swap(regs[REG_B]);
            break;

        case 0x31: // SWAP C
            instr_swap(regs[REG_C]);
            break;

        case 0x32: // SWAP D
            instr_swap(regs[REG_D]);
            break;

        case 0x33: // SWAP E
            instr_swap(regs[REG_E]);
            break;

        case 0x34: // SWAP H
            instr_swap(regs[REG_H]);
            break;

        case 0x35: // SWAP L
            instr_swap(regs[REG_L]);
            break;

        case 0x36: // SWAP (HL)
        {
            uint8_t v = mem(hl());
            instr_swap(v);
            memw(hl(), v);
            break;
        }

        case 0x37: // SWAP A
            regs[REG_A] = ((regs[REG_A] & 0x0f) << 4) | ((regs[REG_A] & 0xf0) >> 4);
            z = regs[REG_A] == 0;
            n = h = c = 0;
            break;

        case 0x38: // SRL B
            instr_srl(regs[REG_B]);
            break;

        case 0x39: // SRL C
            instr_srl(regs[REG_C]);
            break;

        case 0x3a: // SRL D
            instr_srl(regs[REG_D]);
            break;

        case 0x3b: // SRL E
            instr_srl(regs[REG_E]);
            break;

        case 0x3c: // SRL H
            instr_srl(regs[REG_H]);
            break;

        case 0x3d: // SRL L
            instr_srl(regs[REG_L]);
            break;

        case 0x3e: // SRL (HL)
        {
            uint8_t v = mem(hl());
            instr_srl(v);
            memw(hl(), v);
            break;
        }

        case 0x3f: // SRL A
            instr_srl(regs[REG_A]);
            break;

        case 0x40: // BIT 0,B
            instr_bit(regs[REG_B], 0);
            break;

        case 0x41: // BIT 0,C
            instr_bit(regs[REG_C], 0);
            break;

        case 0x42: // BIT 0,D
            instr_bit(regs[REG_D], 0);
            break;

        case 0x43: // BIT 0,E
            instr_bit(regs[REG_E], 0);
            break;

        case 0x44: // BIT 0,H
            z = (regs[REG_H] & (1 << 0)) == 0;
            n = 0;
            h = 1;
            break;

        case 0x45: // BIT 0,L
            z = (regs[REG_L] & (1 << 0)) == 0;
            n = 0;
            h = 1;
            break;

        case 0x46: // BIT 0,(HL)
            z = (mem(hl()) & (1 << 0)) == 0;
            n = 0;
            h = 1;
            break;

        case 0x47: // BIT 0,A
            z = (regs[REG_A] & (1 << 0)) == 0;
            n = 0;
            h = 1;
            break;

        case 0x48: // BIT 1,B
            z = (regs[REG_B] & (1 << 1)) == 0;
            n = 0;
            h = 1;
            break;

        case 0x49: // BIT 1,C
            z = (regs[REG_C] & (1 << 1)) == 0;
            n = 0;
            h = 1;
            break;

        case 0x4a: // BIT 1,D
            z = (regs[REG_C] & (1 << 1)) == 0;
            n = 0;
            h = 1;
            break;

        case 0x4b: // BIT 1,E
            z = (regs[REG_E] & (1 << 1)) == 0;
            n = 0;
            h = 1;
            break;

        case 0x4c: // BIT 1,H
            z = (regs[REG_H] & (1 << 1)) == 0;
            n = 0;
            h = 1;
            break;

        case 0x4d: // BIT 1,L
            z = (regs[REG_L] & (1 << 1)) == 0;
            n = 0;
            h = 1;
            break;

        case 0x4e: // BIT 1,(HL)
            z = (mem(hl()) & (1 << 1)) == 0;
            n = 0;
            h = 1;
            break;

        case 0x4f: // BIT 1,A
            z = (regs[REG_A] & (1 << 1)) == 0;
            n = 0;
            h = 1;
            break;

        case 0x50: // BIT 2,B
            z = (regs[REG_B] & (1 << 2)) == 0;
            n = 0;
            h = 1;
            break;

        case 0x51: // BIT 2,C
            instr_bit(regs[REG_C],2);
            break;

        case 0x52: // BIT 2,D
            instr_bit(regs[REG_D],2);
            break;

        case 0x53: // BIT 2,E
            instr_bit(regs[REG_E],2);
            break;

        case 0x54: // BIT 2,H
            instr_bit(regs[REG_H],2);
            break;

        case 0x55: // BIT 2,L
            instr_bit(regs[REG_L],2);
            break;

        case 0x56: // BIT 2,(HL)
            instr_bit(mem(hl()),2);
            break;

        case 0x57: // BIT 2,A
            z = (regs[REG_A] & (1 << 2)) == 0;
            n = 0;
            h = 1;
            break;

        case 0x58: // BIT 3,B
            z = (regs[REG_B] & (1 << 3)) == 0;
            n = 0;
            h = 1;
            break;

        case 0x59: // BIT 3,C
            instr_bit(regs[REG_C], 3);
            break;

        case 0x5a: // BIT 3,D
            instr_bit(regs[REG_D], 3);
            break;

        case 0x5b: // BIT 3,E
            instr_bit(regs[REG_E], 3);
            break;

        case 0x5c: // BIT 3,H
            instr_bit(regs[REG_H], 3);
            break;

        case 0x5d: // BIT 3,L
            instr_bit(regs[REG_L], 3);
            break;

        case 0x5e: // BIT 3,(HL)
            instr_bit(mem(hl()), 3);
            break;

        case 0x5f: // BIT 3,A
            z = (regs[REG_A] & (1 << 3)) == 0;
            n = 0;
            h = 1;
            break;

        case 0x60: // BIT 4,B
            z = (regs[REG_B] & (1 << 4)) == 0;
            n = 0;
            h = 1;
            break;

        case 0x61: // BIT 4,C
            z = (regs[REG_C] & (1 << 4)) == 0;
            n = 0;
            h = 1;
            break;

        case 0x62: // BIT 4,D
            instr_bit(regs[REG_D], 4);
            break;

        case 0x63: // BIT 4,E
            instr_bit(regs[REG_E], 4);
            break;

        case 0x64: // BIT 4,H
            instr_bit(regs[REG_H], 4);
            break;

        case 0x65: // BIT 4,L
            instr_bit(regs[REG_L], 4);
            break;

        case 0x66: // BIT 4,(HL)
            instr_bit(mem(hl()), 4);
            break;

        case 0x67: // BIT 4, A
            instr_bit(regs[REG_A], 4);
            break;

        case 0x68: // BIT 5,B
            z = (regs[REG_B] & (1 << 5)) == 0;
            n = 0;
            h = 1;
            break;

        case 0x69: // BIT 5,C
            z = (regs[REG_C] & (1 << 5)) == 0;
            n = 0;
            h = 1;
            break;

        case 0x6c: // BIT 5,H
            z = (regs[REG_H] & (1 << 5)) == 0;
            n = 0;
            h = 1;
            break;

        case 0x6f: // BIT 5,A
            z = (regs[REG_A] & (1 << 5)) == 0;
            n = 0;
            h = 1;
            break;
        
        case 0x70: // BIT 6,B
            z = (regs[REG_B] & (1 << 6)) == 0;
            n = 0;
            h = 1;
            break;

        case 0x77: // BIT 6,A
            z = (regs[REG_A] & (1 << 6)) == 0;
            n = 0;
            h = 1;
            break;

        case 0x78: // BIT 7,B
            z = (regs[REG_B] & (1 << 7)) == 0;
            n = 0;
            h = 1;
            break;

        case 0x79: // BIG 7,C
            z = (regs[REG_C] & (1 << 7)) == 0;
            n = 0;
            h = 1;
            break;

        case 0x7d: // BIT 7,L
            z = (regs[REG_L] & (1 << 7)) == 0;
            n = 0;
            h = 1;
            break;

        case 0x7e: // BIT 7,(HL)
            z = (mem(hl()) & (1 << 7)) == 0;
            n = 0;
            h = 1;
            break;

        case 0x7f: // BIT 7,A
            z = (regs[REG_A] & (1 << 7)) == 0;
            n = 0;
            h = 1;
            break;

        case 0x82: // RES 0,D
            regs[REG_D] &= ~1;
            break;

        case 0x86: // RES 0,(HL)
            memw(hl(), mem(hl()) & ~1);
            break;

        case 0x87: // RES 0,A
            regs[REG_A] &= ~1;
            break;

        case 0x8e: // RES 1,(HL)
            memw(hl(), mem(hl()) & ~(1<<1));
            break;

        case 0xbe: // RES 7,(HL)
            memw(hl(), mem(hl()) & ~(1<<7));
            break;

        case 0xfe: // SET 7,(HL)
            memw(hl(), mem(hl()) | (1 << 7));
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

uint8_t JoypadController::joyp() const
{
    return select_buttons ? buttons_state : directions_state;
}

void Cpu::instr_bit(uint8_t v, uint8_t bit)
{
    puts("instr bit unimplemented");
}
