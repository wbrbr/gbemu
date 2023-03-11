#include "apu.hpp"
#include <cassert>

// four arrays of 16 elements
int duty_cycles[4][16] = {
        { 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 0},
        { 0, 1, 1, 1, 1, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 0},
        { 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0},
        { 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1}
};

Apu::Apu(SDL_AudioDeviceID audio_dev, SDL_AudioSpec audio_spec)
{
    this->audio_dev = audio_dev;
    this->audio_spec = audio_spec;
    elapsed_cycles = 0;
    elapsed_cycles_duty_step = 0;
    assert(audio_spec.format == AUDIO_S16);
    assert(audio_spec.channels == 1);
}

static float x = 0.f;

void Apu::exec(uint8_t cycles)
{
    elapsed_cycles += cycles;
    int CLOCK_FREQUENCY = 4194304;
    int cycles_per_sample = CLOCK_FREQUENCY / audio_spec.freq;

    int duty_step_frequency = 1048576 / (2048 - pulseB.wavelength());
    assert(duty_step_frequency > 0);
    int cycles_per_duty_step = CLOCK_FREQUENCY / duty_step_frequency;
    elapsed_cycles_duty_step += cycles;
    if (elapsed_cycles_duty_step >= cycles_per_duty_step) {
        elapsed_cycles_duty_step -= cycles_per_duty_step;
        pulseB.duty_step = (pulseB.duty_step + 1) % 16;
    }

    if (elapsed_cycles >= cycles_per_sample) {
        elapsed_cycles -= cycles_per_sample;
        int16_t sample;
        int square_wave = duty_cycles[2][pulseB.duty_step] * 2 - 1;
        sample = (int16_t)(square_wave * 5000);
        SDL_QueueAudio(audio_dev, &sample, sizeof(int16_t));

        x += 1.f / (float)audio_spec.freq;
    }
}