#ifndef CPU_HPP
#define CPU_HPP
#include <stdint.h>

struct Ppu;

struct SideEffects
{
    uint8_t cycles;
    bool break_;
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

struct Mbc
{
    virtual ~Mbc();
    virtual void load(uint8_t* cartridge, unsigned int size) = 0;
    virtual void reset() = 0;
    virtual uint8_t mem(uint16_t a) = 0;
    virtual void memw(uint16_t a, uint8_t v) = 0;
};

struct Mbc0: public Mbc
{
    Mbc0();
    void load(uint8_t* cartridge, unsigned int size);
    void reset();
    uint8_t mem(uint16_t a);
    void memw(uint16_t a, uint8_t v);

    uint8_t rom[0x8000];
    uint8_t ram[0x2000];
};

struct Mbc1: public Mbc
{
    Mbc1();
    void load(uint8_t* cartridge, unsigned int size);
    void reset();
    uint8_t mem(uint16_t a);
    void memw(uint16_t a, uint8_t v);

    bool ram_enabled;
    uint8_t rom_bank;
    uint8_t ram_bank;
    uint8_t bank_mode;

    uint8_t* rom;
    uint8_t ram[0x8000];
};

struct Cpu;

struct SerialController
{
    SerialController(Cpu* cpu);
    void exec(uint8_t cycles);

    Cpu* cpu;
    int remaining;
    int remaining_bits;

    uint8_t sb, sc;
};

struct JoypadController
{
    JoypadController();
    uint8_t joyp();
    bool select_buttons;
    uint8_t buttons_state;
    uint8_t directions_state;
};

struct Cpu
{
    Cpu();
    ~Cpu();
    void load(const char* path);
    void reset();
    SideEffects cycle();
    void disas(uint16_t addr, char* buf);

    uint16_t af();
    uint16_t bc();
    uint16_t de();
    uint16_t hl();

    uint8_t mem(uint16_t a, bool bypass = false);
    bool memw(uint16_t a, uint8_t v);
    void push(uint16_t v);
    uint8_t pop8();
    uint16_t pop16();

    SerialController serial;
    JoypadController joypad;
    Ppu* ppu;
    Mbc* mbc;

    // uint8_t rom[0x8000];
    uint8_t wram[0x2000];
    uint8_t hram[128];
    // uint8_t memory[0xffff];
    uint8_t regs[7];
    uint16_t sp, pc;
    bool z, h, n, c;

    bool ime;
    uint8_t ie, if_;

    uint16_t breakpoint;

private:
    void instr_add(uint8_t v);
    void instr_adc(uint8_t v);
    void instr_sbc(uint8_t v);
    void daa();
    void execPrefix(SideEffects& eff);
    
    void instr_bit(uint8_t v, uint8_t bit);
};
#endif
