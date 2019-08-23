#include "ppu.hpp"
#include "cpu.hpp"
#include <stdio.h>
#include <assert.h>

Ppu::Ppu()
{
    lcdc = 0x91;
    stat = 0x85;
    scx = scy = ly = lyc = dma = bgp = obp0 = obp1 = wx = wy = 0;
    cpu = nullptr;
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
                    
                    // TODO: 8x16, priority, obj-to-bg priority, flips,...
                    for (int i = 0; i < 40; i++)
                    {
                        int sprite_y = (int)oam[4*i] - 16;
                        int sp_yoff = ly - sprite_y;
                        int sp_xoff =  x - (int)oam[4*i+1] + 8;
                        if (sp_yoff >= 0 && sp_yoff < 8 && sp_xoff >= 0 && sp_xoff < 8) {
                            tile_num = oam[4*i+2];
                            b1 = vram[tile_num * 16 + 2*sp_yoff];
                            b2 = vram[tile_num * 16 + 2*sp_yoff+1];
                            lsb = (b1 >> (7 - sp_xoff)) & 1;
                            msb = (b2 >> (7 - sp_xoff)) & 1;
                            pal = lsb | (msb << 1);
                            break;
                        }
                    }

                    framebuf[ly*160+x] = values[palette(pal)];
                }
                ly++;
                if (ly == 144) {
                    stat &= ~(0b11);
                    stat |= MODE_VBLANK;
                    cpu->if_ |= 1;
                    puts("vblank");
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
