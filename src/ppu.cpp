#include "ppu.hpp"
#include <stdio.h>

Ppu::Ppu()
{
    lcdc = 0x91;
    stat = 0x85;
    scx = scy = ly = lyc = dma = bgp = obp0 = obp1 = wx = wy = 0;
    memset(vram, 0, sizeof(vram));
    memset(oam, 0, sizeof(oam));
}

void Ppu::exec(uint8_t cycles)
{
    cycle_count += cycles;
    printf("%d\n", cycle_count);
    switch(stat & 0x3) {
        case MODE_OAM_SEARCH:
            if (cycle_count >= 80) {
                stat &= ~(0b11);
                stat |= MODE_PIXEL_TRANSFER;
                cycle_count -= 80;
            }
            break;

        case MODE_PIXEL_TRANSFER:
            if (cycle_count >= 172) {
                stat &= ~(0b11);
                stat |= MODE_HBLANK;
                cycle_count -= 172;
            }
            break;

        case MODE_HBLANK:
            if (cycle_count >= 204) {
                // TODO: renderLine()
                ly++;
                if (ly == 144) {
                    stat &= ~(0b11);
                    stat |= MODE_VBLANK;
                    // TODO: trigger interrupt
                } else {
                    stat &= ~(0b11);
                    stat |= MODE_OAM_SEARCH;
                }
                cycle_count -= 204;
            }
            break;

        case MODE_VBLANK:
            if (cycle_count >= 456) {
                ly++;
                if (ly == 154) {
                    ly = 0;
                    stat &= ~(0b11);
                    stat |= MODE_OAM_SEARCH;
                }
                cycle_count -= 456;
            }
            break;
    }
}

bool vramaccess()
{
    return ((lcdc & (1 << 7)) == 0) || (stat & 3 < 3);
}

bool oamaccess()
{
    return ((lcdc & (1 << 7)) == 0) || (stat & 3 < 2);
}

uint8_t palette(uint8_t i)
{
    assert(i < 4);
    return (bgp >> (2*i)) & 3;
}
