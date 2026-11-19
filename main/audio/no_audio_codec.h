#pragma once
#include <driver/gpio.h>
#include "audio_codec.h"

class NoAudioCodecSimplex : public AudioCodec {
public:
    NoAudioCodecSimplex(int input_sample_rate, gpio_num_t mic_sck, gpio_num_t mic_ws, gpio_num_t mic_din);
    void EnableInput(bool enable) override;
protected:
    int Read(int16_t* dest, int samples) override;
};
