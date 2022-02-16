#include "ppu.hpp"
#include "cpu.hpp"
#include "util.hpp"
#include <stdio.h>
#include <assert.h>

constexpr uint8_t OBJ_ENABLE_BIT = 1 << 1;
constexpr uint8_t OBJ_SIZE_BIT = 1 << 2;
constexpr uint8_t BG_MAP_ADDRESSING_BIT = 1 << 3;
constexpr uint8_t TILE_DATA_ADDRESSING_BIT = 1 << 4;
constexpr uint8_t WINDOW_ENABLE_BIT = 1 << 5;
constexpr uint8_t WINDOW_MAP_ADDRESSING_BIT = 1 << 6;
constexpr uint8_t LCD_ENABLE_BIT = 1 << 7;

Ppu::Ppu()
{
    cpu = nullptr;
    reset();
}

void Ppu::reset()
{
    lcdc = 0x91;
    ly = 0x90;
    stat = 0x85;
    scx = scy = lyc = dma = bgp = obp0 = obp1 = wx = wy = 0;
    cycle_count = 0;
    memset(vram, 0, sizeof(vram));
    memset(oam, 0, sizeof(oam));
    memset(framebuf, 0, sizeof(framebuf));
}

void Ppu::exec(uint8_t cycles)
{
    if ((lcdc & LCD_ENABLE_BIT) == 0) return;
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
                uint32_t values[] = { 0xffd0f8e0, 0xff70c088, 0xff566834, 0xff201808 };

                for (uint8_t x = 0; x < 160; x++)
                {
                    uint8_t tile_num;
                    uint8_t x_off, y_off;
                    if ((lcdc & WINDOW_ENABLE_BIT) &&  x >= wx && ly >= wy) {
                        uint8_t win_x = x-wx;
                        uint8_t win_y = ly-wy;
                        uint8_t tile_x = win_x / 8;
                        uint8_t tile_y = win_y / 8;
                        x_off = win_x % 8;
                        y_off = win_y % 8;
                        if (lcdc & WINDOW_MAP_ADDRESSING_BIT) {
                            tile_num = vram[0x1c00 + tile_y * 32 + tile_x];
                        } else {
                            tile_num = vram[0x1800 + tile_y * 32 + tile_x];
                        }
                    } else {
                        uint8_t bg_x = scx+x;
                        uint8_t bg_y = scy+ly;
                        uint8_t tile_x = bg_x / 8;
                        uint8_t tile_y = bg_y / 8;
                        x_off = bg_x % 8;
                        y_off = bg_y % 8;
                        if (lcdc & BG_MAP_ADDRESSING_BIT) {
                            tile_num = vram[0x1c00 + tile_y * 32 + tile_x];
                        } else {
                            tile_num = vram[0x1800 + tile_y * 32 + tile_x];
                        }
                    }

                    uint8_t b1, b2;
                    if (lcdc & TILE_DATA_ADDRESSING_BIT) {
                        b1 = vram[tile_num * 16 + 2*y_off];
                        b2 = vram[tile_num * 16 + 2*y_off+1];
                    } else {
                        b1 = vram[0x1000 + unsigned_to_signed(tile_num) * 16 + 2 * y_off];
                        b2 = vram[0x1000 + unsigned_to_signed(tile_num) * 16 + 2 * y_off+1];
                    }
                    uint8_t lsb = (b1 >> (7 - x_off)) & 1;
                    uint8_t msb = (b2 >> (7 - x_off)) & 1;
                    uint8_t bg_col_id = lsb | (msb << 1);
                    uint8_t col = palette(bgp, bg_col_id);
                    
                    // TODO: priority, obj-to-bg priority...
                    for (int i = 0; i < 40; i++)
                    {
                        int sprite_y = (int)oam[4*i] - 16;
                        int sp_yoff = ly - sprite_y;
                        int sp_xoff =  x - (int)oam[4*i+1] + 8;
                        uint8_t sp_flags = oam[4*i+3];
                        int sprite_height = lcdc & OBJ_SIZE_BIT ? 16 : 8;

                        if (sp_yoff >= 0 && sp_yoff < sprite_height && sp_xoff >= 0 && sp_xoff < 8) {
                            tile_num = oam[4*i+2];

                            if (sp_flags & (1 << 5)) sp_xoff = 7 - sp_xoff;
                            if (sp_flags & (1 << 6)) sp_yoff = sprite_height - 1 - sp_yoff;
                            b1 = vram[tile_num * 16 + 2*sp_yoff];
                            b2 = vram[tile_num * 16 + 2*sp_yoff+1];
                            lsb = (b1 >> (7 - sp_xoff)) & 1;
                            msb = (b2 >> (7 - sp_xoff)) & 1;
                            uint8_t col_id = lsb | (msb << 1);
                            uint8_t obj_col = palette((sp_flags >> 4) & 1 ? obp1 : obp0, lsb | (msb << 1));

                            bool hidden = false;
                            if (sp_flags & (1 << 7)) { // BG/window over OBJ
                                if (bg_col_id != 0) hidden = true;
                            } else {
                                if (col_id == 0) hidden = true;
                            }
                            if (!hidden) {
                                col = obj_col;
                            }
                            break;
                        }
                    }

                    if (ly < 144) {
                        framebuf[ly*160+x] = values[col];
                    }
                }
                ly++;
                if (ly >= 144) {
                    stat &= ~(0b11);
                    stat |= MODE_VBLANK;
                    cpu->if_ |= 1;
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

uint8_t Ppu::palette(uint8_t p, uint8_t i)
{
    assert(i < 4);
    return (p >> (2*i)) & 3;
}
