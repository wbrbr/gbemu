#ifndef GBEMU_APU_HPP
#define GBEMU_APU_HPP
#include <cstdint>
#include <SDL2/SDL_audio.h>

struct CycleTimer {
    CycleTimer(int num_cycles, int start = 0) {
        this->num_cycles = num_cycles;
        this->current = start;
    }

    bool tick(int cycles) {
        current += cycles;
        if (current >= num_cycles) {
            current -= num_cycles;
            return true;
        }
        return false;
    }

    int current;
    int num_cycles;
};

constexpr int CLOCK_FREQUENCY = 4194304;
constexpr uint8_t FF26_CHANNEL_1_ON_BIT = 1 << 0;
constexpr uint8_t FF26_CHANNEL_2_ON_BIT = 1 << 1;
constexpr uint8_t FF26_CHANNEL_3_ON_BIT = 1 << 2;
constexpr uint8_t FF26_CHANNEL_4_ON_BIT = 1 << 3;
constexpr uint8_t FF26_SOUND_ON_BIT = 1 << 7;

struct Pulse {
    uint8_t sweep;
    uint8_t length;
    uint8_t volume;
    uint8_t frequency;
    uint8_t control;

    uint8_t duty_step;

    Pulse(uint8_t channel_num): length_timer(CLOCK_FREQUENCY / 256, 0),
             enveloppe_timer(CLOCK_FREQUENCY / 64, 0),
             elapsed_cycles_duty_step(0),
             enveloppe_pace(0),
             enveloppe_inc(0),
             channel_num(channel_num) {}
    uint16_t wavelength() const;
    int value() const;
    void tick(int cycles, uint8_t& sound_on);
    int gain() const;
    void trigger();

private:
    int elapsed_cycles_duty_step;
    CycleTimer length_timer;
    CycleTimer enveloppe_timer;

    uint8_t enveloppe_pace;
    uint8_t current_volume;
    uint8_t period_timer;
    uint8_t enveloppe_inc;

    uint8_t channel_num;
};

struct Apu {
    Apu(SDL_AudioDeviceID audio_dev, SDL_AudioSpec audio_spec);
    void exec(uint8_t cycles);

    SDL_AudioDeviceID audio_dev;
    SDL_AudioSpec audio_spec;
    Pulse pulseA;
    Pulse pulseB;

    uint8_t sound_on;

private:
    int elapsed_cycles;
};

#endif //GBEMU_APU_HPP
