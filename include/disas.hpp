#ifndef DISAS_HPP
#define DISAS_HPP
#include <stdint.h>
#include "cpu.hpp"

void disassemble(const Cpu& cpu, uint16_t& pc, char* buf, unsigned int buf_size);
#endif // DISAS_HPP
