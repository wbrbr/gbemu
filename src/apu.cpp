#include "apu.hpp"
#include <cassert>

Apu::Apu(SDL_AudioDeviceID audio_dev, SDL_AudioSpec audio_spec)
{
    this->audio_dev = audio_dev;
    this->audio_spec = audio_spec;
    elapsed_cycles = 0;
    assert(audio_spec.format == AUDIO_S16);
    assert(audio_spec.channels == 1);
}

static float x = 0.f;

void Apu::exec(uint8_t cycles)
{
    elapsed_cycles += cycles;
    int CLOCK_FREQUENCY = 4194304;
    int cycles_per_sample = CLOCK_FREQUENCY / audio_spec.freq;

    if (elapsed_cycles >= cycles_per_sample) {
        elapsed_cycles -= cycles_per_sample;
        int16_t sample;
        sample = (int16_t)(sin(x*440*2*M_PI) * 5000);
        SDL_QueueAudio(audio_dev, &sample, sizeof(int16_t));

        x += 1.f / (float)audio_spec.freq;
    }
}