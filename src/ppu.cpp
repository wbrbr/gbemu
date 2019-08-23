#include "ppu.hpp"
#include <stdio.h>
#include <assert.h>

Ppu::Ppu()
{
    lcdc = 0x91;
    stat = 0x85;
    scx = scy = ly = lyc = dma = bgp = obp0 = obp1 = wx = wy = 0;
    memset(vram, 0, sizeof(vram));
    memset(oam, 0, sizeof(oam));
    memset(framebuf, 0, sizeof(framebuf));
}

void Ppu::exec(uint8_t cycles)
{
    if ((lcdc & (1 << 7)) == 0) return;
    cycle_count += cycles;
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
                uint32_t values[] = { 0xff201808, 0xff566834, 0xff70c088, 0xffd0f8e0 };

                for (int x = 0; x < 160; x++)
                {
                    int tile_x = x / 8;
                    int tile_y = ly / 8;
                    // TODO: palette, addressing modes, scrolling
                    int tile_num = vram[0x1800 + tile_y * 32 + tile_x];

                    int x_off = x % 8;
                    int y_off = ly % 8;
                    uint8_t b1 = vram[tile_num * 16 + 2*y_off];
                    uint8_t b2 = vram[tile_num * 16 + 2*y_off+1];
                    uint8_t lsb = (b1 >> (7 - x_off)) & 1;
                    uint8_t msb = (b2 >> (7 - x_off)) & 1;
                    uint8_t pal = lsb | (msb << 1);
                    framebuf[ly*160+x] = values[pal];
                }
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

bool Ppu::vramaccess()
{
    return ((lcdc & (1 << 7)) == 0) || ((stat & 3) < 3);
}

bool Ppu::oamaccess()
{
    return ((lcdc & (1 << 7)) == 0) || ((stat & 3) < 2);
}

uint8_t Ppu::palette(uint8_t i)
{
    assert(i < 4);
    return (bgp >> (2*i)) & 3;
}
