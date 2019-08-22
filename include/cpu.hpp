#ifndef CPU_HPP
#define CPU_HPP
#include <stdint.h>

struct Ppu;

struct SideEffects
{
    uint8_t cycles;
};

enum Registers {
    REG_A,
    REG_B,
    REG_C,
    REG_D,
    REG_E,
    REG_L,
    REG_H
};

struct Cpu
{
    Cpu();
    void load(const char* path);
    SideEffects cycle();
    void disas(uint16_t addr, char* buf);

    uint16_t af();
    uint16_t bc();
    uint16_t de();
    uint16_t hl();

    uint8_t mem(uint16_t a);
    void memw(uint16_t a, uint8_t v);
    void push(uint16_t v);

    Ppu* ppu;

    uint8_t rom[0x8000];
    uint8_t wram[0x2000];
    uint8_t hram[63];
    // uint8_t memory[0xffff];
    uint8_t regs[7];
    uint16_t sp, pc;
    bool z, h, n, c;

    bool ime;
    uint8_t ie, if_;
};
#endif
