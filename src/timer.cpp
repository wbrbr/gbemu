#include "timer.hpp"
#include <cassert>

Timer::Timer()
{
    div = 0x18;
    tima = 0;
    tma = 0;
    tac = 0xf8;

    internal_timer = 0;
}

void Timer::update(uint8_t cycles, Cpu& cpu)
{
    unsigned int prev_internal_timer = internal_timer;

    internal_timer += cycles;
    div = internal_timer / 256;

    if (tac & (1 << 2)) {

        unsigned int cycles_num;
        switch(tac & 0b11) {
            case 0:
                cycles_num = 1024;
                break;

            case 1:
                cycles_num = 16;
                break;

            case 2:
                cycles_num = 64;
                break;

            case 3:
                cycles_num = 256;
                break;

            default:
                assert(0);
        }

        unsigned int num_increments = (internal_timer / cycles_num) - (prev_internal_timer / cycles_num);

        for (unsigned int i = 0; i < num_increments; i++) {
            if (tima == 0xff) {
                tima = tma;
                cpu.if_ |= (1 << 2);
            } else {
                tima++;
            }
        }
    }

    internal_timer %= 1024;
}

void Timer::reset_timer()
{
    internal_timer = 0;
}
