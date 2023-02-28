#ifndef MBC_HPP
#define MBC_HPP
#include <cstdint>

struct Mbc
{
    virtual ~Mbc();
    void load(uint8_t* cartridge, unsigned int size);
    virtual void reset() = 0;
    virtual uint8_t mem(uint16_t a) = 0;
    virtual void memw(uint16_t a, uint8_t v) = 0;

    uint8_t* rom;
};

struct Mbc0: public Mbc
{
    Mbc0();
    void reset() override;
    uint8_t mem(uint16_t a) override;
    void memw(uint16_t a, uint8_t v) override;

    uint8_t ram[0x2000];
};

struct Mbc1: public Mbc
{
    Mbc1();
    void reset() override;
    uint8_t mem(uint16_t a) override;
    void memw(uint16_t a, uint8_t v) override;

    bool ram_enabled;
    uint8_t rom_bank;
    uint8_t ram_bank;
    uint8_t bank_mode;

    uint8_t* ram;
};

struct Mbc2: public Mbc
{
    Mbc2();
    void reset() override;
    uint8_t mem(uint16_t a) override;
    void memw(uint16_t a, uint8_t v) override;

    bool ram_enabled;
    uint8_t rom_bank;

    uint8_t ram[512];
};

struct Mbc3: public Mbc
{
    Mbc3();
    void reset() override;
    uint8_t mem(uint16_t a) override;
    void memw(uint16_t a, uint8_t v) override;

    bool ram_enabled;
    uint8_t rom_bank;
    uint8_t ram_bank;

    uint8_t* ram;

    bool clock_latch;

    // TODO: real time clock
    struct {
        uint8_t seconds;
        uint8_t minutes;
        uint8_t hours;
        uint8_t days_lo;
        uint8_t days_hi;
    } clock;
};

struct Mbc5: public Mbc
{
    Mbc5();
    void reset() override;
    uint8_t mem(uint16_t a) override;
    void memw(uint16_t a, uint8_t v) override;

    bool ram_enabled;
    uint16_t rom_bank;
    uint8_t ram_bank;

    uint8_t* ram;
};

#endif // MBC_HPP
