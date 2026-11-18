#pragma once

#include <driver/i2s_std.h>
#include <vector>

#define AUDIO_CODEC_DMA_DESC_NUM  6
#define AUDIO_CODEC_DMA_FRAME_NUM 240

class AudioCodec
{
public:
    virtual ~AudioCodec() = default;
    virtual void EnableInput(bool enable);

    bool InputData(std::vector<int16_t>& data);

    int input_sample_rate() const {
        return input_sample_rate_;
    }

    int input_channels() const {
        return input_channels_;
    }

    bool input_enabled() const {
        return input_enabled_;
    }

protected:
    i2s_chan_handle_t rx_handle_ = nullptr;
    int input_sample_rate_ = 0;
    int input_channels_ = 1;
    bool input_enabled_ = false;

    virtual int Read(int16_t* dest, int samples) = 0;
};
