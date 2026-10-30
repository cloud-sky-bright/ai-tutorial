#pragma once

#include <cstdint>

#include "driver/gpio.h"
#include "driver/rmt_types.h"

class Led {
public:
    explicit Led(gpio_num_t gpio);
    ~Led();

    void SetColor(uint8_t red, uint8_t green, uint8_t blue);
    void Off() { SetColor(0, 0, 0); }
private:
    rmt_channel_handle_t channel_ = nullptr;
    rmt_encoder_handle_t encoder_ = nullptr;
};
