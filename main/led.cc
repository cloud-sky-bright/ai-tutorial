#include "led.h"

#include "driver/rmt_encoder.h"
#include "driver/rmt_tx.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_rom_sys.h"

static const char* TAG = "led";

static constexpr uint32_t kRmtResolutionHz = 10 * 1000 * 1000;
static constexpr uint32_t kBit1High = 8;
static constexpr uint32_t kBit1Low = 4;
static constexpr uint32_t kBit0High = 4;
static constexpr uint32_t kBit0Low = 8;

Led::Led(gpio_num_t gpio) {
    rmt_tx_channel_config_t tx_config = {};
    tx_config.gpio_num = gpio;
    tx_config.clk_src = RMT_CLK_SRC_DEFAULT;
    tx_config.mem_block_symbols = 64;
    tx_config.trans_queue_depth = 4;
    tx_config.resolution_hz = kRmtResolutionHz;
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_config, &channel_));
    rmt_copy_encoder_config_t copy_config = {};
    ESP_ERROR_CHECK(rmt_new_copy_encoder(&copy_config, &encoder_));
    ESP_ERROR_CHECK(rmt_enable(channel_));
    ESP_LOGI(TAG, "ws2812 initialized(GPIO%d, RMT @10MHz)", (int)gpio);

}

Led::~Led() {
    if(channel_ != nullptr){
        ESP_ERROR_CHECK(rmt_disable(channel_));
        ESP_ERROR_CHECK(rmt_del_channel(channel_));
    }
    if(encoder_ != nullptr){
        ESP_ERROR_CHECK(rmt_del_encoder(encoder_));
    }
}

void Led::SetColor(uint8_t r, uint8_t g, uint8_t b)
{
    if(channel_ == nullptr){
        return;
    }

    const uint8_t bytes[3] = {g, r, b}; 
    rmt_symbol_word_t symbols[24];
    int idx = 0;
    for(int byte = 0; byte < 3; byte++){
        for(int bit = 7; bit >= 0; bit--){
            const bool one = (bytes[byte] >> bit) & 0x01; // 10010100 >> 4 = 00001001 & 00000001 = 00000001
            rmt_symbol_word_t &sym = symbols[idx++];
            sym.level0 = 1;
            sym.duration0 = one ? kBit1High : kBit0High;
            sym.level1 = 0;
            sym.duration1 = one ? kBit1Low : kBit0Low;
        }
    }
    rmt_transmit_config_t transmit_config = {};
    transmit_config.loop_count = 0;
    ESP_ERROR_CHECK(rmt_transmit(channel_, encoder_, symbols, sizeof(symbols), &transmit_config));
    ESP_ERROR_CHECK(rmt_tx_wait_all_done(channel_, 100)); // 0.9+0.3us = 1.2us, 24 bits = 28.8us

    esp_rom_delay_us(60);


}
