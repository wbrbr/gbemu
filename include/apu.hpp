#ifndef GBEMU_APU_HPP
#define GBEMU_APU_HPP
#include <cstdint>
#include <SDL2/SDL_audio.h>

struct Pulse {
    uint8_t sweep;
    uint8_t length;
    uint8_t volume;
    uint8_t frequency;
    uint8_t control;

    uint16_t wavelength() const {
        return (uint16_t)frequency | ((uint16_t)(control & 0b111) << 8);
    }

    float frequencyHz() const {
        return 131072.f / (float)(2048 - wavelength());
    }
};

struct Apu {
    Apu(SDL_AudioDeviceID audio_dev, SDL_AudioSpec audio_spec);
    void exec(uint8_t cycles);

    SDL_AudioDeviceID audio_dev;
    SDL_AudioSpec audio_spec;
    Pulse pulseA;
    Pulse pulseB;
    int elapsed_cycles;
};

#endif //GBEMU_APU_HPP
