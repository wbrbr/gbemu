#ifndef PPU_HPP
#define PPU_HPP
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#define MODE_HBLANK 0
#define MODE_VBLANK 1
#define MODE_OAM_SEARCH 2
#define MODE_PIXEL_TRANSFER 3

struct Ppu
{
    Ppu();
    void exec(uint8_t cycles);

    bool vramaccess();
    bool oamaccess();
    
    // palette index -> color code
    uint8_t palette(uint8_t i);

    uint8_t lcdc, stat, scy, scx, ly, lyc, dma, bgp, obp0, obp1, wy, wx;
    uint8_t vram[0x2000];
    uint8_t oam[160];

    int cycle_count;
};
#endif
