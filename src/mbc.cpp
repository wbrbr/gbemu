#include "mbc.hpp"
#include <cstring>
#include <cstdio>
#include <cstdlib>

Mbc::~Mbc() {
    if (rom) free(rom);
};

void Mbc::load(uint8_t* cartridge, unsigned int size)
{
    memcpy(rom, cartridge, size);
}

Mbc0::Mbc0()
{
    rom = (uint8_t*)calloc(1, 0x8000);
    memset(rom, 0, 0x8000);
    memset(ram, 0, sizeof(ram));
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
    ram = (uint8_t*)calloc(1, 0x8000);
    ram_enabled = false;
    rom_bank = 1;
    ram_bank = 1;
    bank_mode = 0;
}

void Mbc1::reset()
{
    memset(ram, 0, 0x8000);
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
        rom_bank &= 0b11100000;
        uint8_t x = v & 0b00011111;
        if (x == 0) x++;
        rom_bank |= x;
        return;
    }
    if (a >= 0x4000 && a <= 0x5fff) {
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
        ram[0x2000*ram_bank + a - 0xa000] = v;
        return;
    }
}

Mbc2::Mbc2()
{
    rom = (uint8_t*)calloc(1, 256 * (1 << 10));
}

void Mbc2::reset()
{
    memset(ram, 0, sizeof(ram));
    ram_enabled = false;
    rom_bank = 1;
}

uint8_t Mbc2::mem(uint16_t a)
{
    if (a <= 0x3fff) return rom[a];
    if (a <= 0x7fff) {
        return rom[0x4000*rom_bank + a-0x4000];
    }
    if (ram_enabled && a >= 0xa000 && a <= 0xbfff) {
        return ram[(a - 0xa000) & (0b111111111)] & 0xf;
    }

    return 0xff;
}

void Mbc2::memw(uint16_t a, uint8_t v)
{
    if (a <= 0x3fff) {
        if (a & (1 << 8)) {
            rom_bank = v & 0xf;
            if (rom_bank == 0) rom_bank++;
        } else {
            ram_enabled = (v == 0x0a);
        }
        return;
    }
    if (ram_enabled && a >= 0xa000 && a <= 0xbfff) {
        ram[(a - 0xa000) & 0b111111111] = v & 0xf;
        return;
    }
}

Mbc3::Mbc3()
{
    static_assert(2*(1<<20) == 0x200000);
    static_assert(64 * (1<<10) == 0x10000);
    rom = (uint8_t*)calloc(1, 2 * (1 << 20));
    ram = (uint8_t*)calloc(1, 64 * (1 << 10));
    Mbc3::reset();
}

void Mbc3::reset()
{
    memset(ram, 0, 64 * (1 << 10));
    ram_enabled = false;
    rom_bank = 1;
    ram_bank = 0;
    clock = { 0 };
    clock_latch = false;
}

uint8_t Mbc3::mem(uint16_t a)
{
    if (a <= 0x3fff) return rom[a];
    if (a <= 0x7fff) {
        return rom[0x4000*rom_bank + a-0x4000];
    }
    if (ram_enabled && a >= 0xa000 && a <= 0xbfff) {
        if (ram_bank <= 7) {
            return ram[0x2000*ram_bank + a - 0xa000];
        } else {
            switch(ram_bank) {
            case 0x8:
                return clock.seconds;

            case 0x9:
                return clock.minutes;

            case 0xa:
                return clock.hours;

            case 0xb:
                return clock.days_lo;

            case 0xc:
                return clock.days_hi;

            default:
                abort();
            }
        }
    }

    return 0xff;
}

void Mbc3::memw(uint16_t a, uint8_t v)
{
    if (a <= 0x1fff) {
        ram_enabled = (v & 0xf) == 0xA;
        return;
    }
    if (a >= 0x2000 && a <= 0x3fff) {
        rom_bank &= 0b10000000;
        rom_bank |= v & 0b01111111;
        if (rom_bank == 0) rom_bank++;
        return;
    }
    if (a >= 0x4000 && a <= 0x5fff) {
        if (v <= 0xc) {
            ram_bank = v;
        }
        return;
    }
    if (a >= 0x6000 && a <= 0x7fff) {
        clock_latch = !clock_latch;
        return;
    }
    if (ram_enabled && a >= 0xa000 && a <= 0xbfff) {
        if (ram_bank <= 7) {
            ram[ram_bank*0x2000 + a - 0xa000] = v;
        } else {
            switch(ram_bank) {
            case 0x8:
                clock.seconds = v;
                break;

            case 0x9:
                clock.minutes = v;
                break;

            case 0xa:
                clock.hours = v;
                break;

            case 0xb:
                clock.days_lo = v;
                break;

            case 0xc:
                clock.days_hi = v;
                break;

            default:
                abort();
            }
        }
        return;
    }
}

Mbc5::Mbc5()
{
    static_assert(2*(1<<20) == 0x200000);
    static_assert(64 * (1<<10) == 0x10000);
    rom = (uint8_t*)calloc(1, 8 * (1 << 20));
    ram = (uint8_t*)calloc(1, 64 * (1 << 10));
    Mbc5::reset();
}

void Mbc5::reset()
{
    memset(ram, 0, 64 * (1 << 10));
    ram_enabled = false;
    rom_bank = 1;
    ram_bank = 0;
}

uint8_t Mbc5::mem(uint16_t a)
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

void Mbc5::memw(uint16_t a, uint8_t v)
{
    if (a <= 0x1fff) {
        ram_enabled = (v & 0xf) == 0xA;
        return;
    }
    if (a >= 0x2000 && a <= 0x2fff) {
        rom_bank &= 0xff00;
        rom_bank |= v;
        return;
    }
    if (a >= 0x3000 && a <= 0x3fff) {
        rom_bank &= ~(1 << 8);
        rom_bank |= ((v&1) << 8);
        return;
    }
    if (a >= 0x4000 && a <= 0x5fff) {
        if (v <= 0xf) {
            ram_bank = v;
        }
        return;
    }
    if (ram_enabled && a >= 0xa000 && a <= 0xbfff) {
        ram[ram_bank*0x2000 + a - 0xa000] = v;
        return;
    }
}
