//
// Created by wilhem on 10/16/21.
//

#ifndef GBEMU_TIMER_HPP
#define GBEMU_TIMER_HPP
#include <cstdint>
#include "cpu.hpp"

struct Timer {

    uint8_t div, tima, tma, tac;

    Timer();
    void update(uint8_t cycles, Cpu& cpu);
    void reset_timer();

private:

    unsigned int internal_timer;
};

#endif //GBEMU_TIMER_HPP
