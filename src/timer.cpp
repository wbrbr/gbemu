#include "timer.hpp"
#include <cassert>

Timer::Timer()
{
    div = 0;
    tima = 0;
    tma = 0;
    tac = 0;
    cycles_since_div_increment = 0;
    cycles_since_tima_increment = 0;
}

void Timer::update(uint8_t cycles, Cpu& cpu)
{
    cycles_since_div_increment += cycles;
    if (cycles_since_div_increment >= 256) {
        cycles_since_div_increment %= 256;
        div++;
    }

    if (tac & (1 << 2)) {
        cycles_since_tima_increment += cycles;

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

        if (cycles_since_tima_increment >= cycles_num) {
            cycles_since_tima_increment %= cycles_num;
            if (tima == 0xff) {
                tima = tma;
                cpu.if_ |= (1 << 2);
            } else {
                tima++;
            }
        }
    }
}
