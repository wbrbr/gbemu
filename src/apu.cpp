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
    assert(audio_spec.format == AUDIO_U16);
    assert(audio_spec.channels == 1);
}

static float x = 0.f;

void Apu::exec(uint8_t cycles)
{
    if ((sound_on & FF26_SOUND_ON_BIT) == 0) {
        return;
    }
    elapsed_cycles += cycles;

    pulseB.tick(cycles, sound_on);

    int cycles_per_sample = CLOCK_FREQUENCY / audio_spec.freq;

    if (elapsed_cycles >= cycles_per_sample) {
        elapsed_cycles -= cycles_per_sample;
        uint16_t sample = 0;

        if (sound_on & FF26_CHANNEL_1_ON_BIT) {
            //sample += (int16_t)(pulseA.value() * 5000);
        }
        if (sound_on & FF26_CHANNEL_2_ON_BIT) {
            sample += (uint16_t)(pulseB.value());
        }

        SDL_QueueAudio(audio_dev, &sample, sizeof(uint16_t));

        x += 1.f / (float)audio_spec.freq;
    }
}

int Pulse::value() const {
    int duty_id = length >> 6;
    int vol = volume >> 4;
    return (duty_cycles[duty_id][duty_step] * 5000 * vol) / 0xF;
}

uint16_t Pulse::wavelength() const {
    return (uint16_t)frequency | ((uint16_t)(control & 0b111) << 8);
}

void Pulse::tick(int cycles, uint8_t &sound_on) {
    if (control & (1 << 6)) {
        if (length_timer.tick(cycles)) {
            uint8_t sound_length = length & 0b00111111;
            sound_length = (sound_length + 1) % 64;
            length = (length & 0b11000000) | sound_length;
            printf("%d\n", length);
            if (sound_length == 0) {
                sound_on &= (uint8_t)(~(FF26_SOUND_ON_BIT));
            }
        }
    }

    int duty_step_frequency = 1048576 / (2048 - wavelength());
    //assert(duty_step_frequency > 0);
    int cycles_per_duty_step = CLOCK_FREQUENCY / duty_step_frequency;

    elapsed_cycles_duty_step += cycles;
    if (elapsed_cycles_duty_step >= cycles_per_duty_step) {
        elapsed_cycles_duty_step -= cycles_per_duty_step;
        duty_step = (duty_step + 1) % 16;
    }
}