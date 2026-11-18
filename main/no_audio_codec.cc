#include "no_audio_codec.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <vector>
#define TAG "NoAudioCodec"


NoAudioCodecSimplex::NoAudioCodecSimplex(int input_sample_rate, gpio_num_t mic_sck, gpio_num_t mic_ws, gpio_num_t mic_din)
{
    input_sample_rate_ = input_sample_rate;
    i2s_chan_config_t chan_cfg = {
        .id = (i2s_port_t)1,  // 官方约定：扬声器占 0，麦克风占 1
        .role = I2S_ROLE_MASTER,                    // ESP32 出时钟（主模式），INMP441 跟随
        .dma_desc_num = AUDIO_CODEC_DMA_DESC_NUM,   // DMA 缓冲块数：6 块（环形）
        .dma_frame_num = AUDIO_CODEC_DMA_FRAME_NUM, // 每块 240 帧 = 240 点 ≈ 15ms（单声道）
        .auto_clear_after_cb = true,                // TX 发完清零：欠载发静音（RX 不生效）
        .auto_clear_before_cb = false,              // 与上项二选一：TX 回调前清，这里不用
        .allow_pd = false,                          // 睡眠时不掉电，保证连续采集
        .intr_priority = 0,                         // 0 = 驱动自动分配（较低）优先级
    };

    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, nullptr, &rx_handle_));
    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = (uint32_t)input_sample_rate_,
            .clk_src = I2S_CLK_SRC_DEFAULT,
            // 该字段在 IDF 5.5 头文件里按 SOC_I2S_HW_VERSION_2 存在（S3 是 v2）；用默认时钟源时它被忽略
            .ext_clk_freq_hz = 0,
            .mclk_multiple = I2S_MCLK_MULTIPLE_256,
            .bclk_div = 8,  // 仅从模式生效；主模式填 IDF 默认宏同值
        },
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_32BIT,  // INMP441 输出 24 位，读 32 位再移位
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,   // AUTO = 跟随数据位宽（32 位）
            .slot_mode = I2S_SLOT_MODE_MONO,             // 单声道：只取一个槽
            .slot_mask = I2S_STD_SLOT_LEFT,              // L/R 接地 → 数据在左槽
            .ws_width = I2S_DATA_BIT_WIDTH_32BIT,        // WS 高电平宽 = 32 个 BCLK
            .ws_pol = false,                             // 低电平在前（低 = 左）
            .bit_shift = true,                           // Philips：数据晚 1 个 BCLK
            .left_align = true,                          // 左对齐（有效位 < 槽宽时生效）
            .big_endian = false,                         // 小端（ESP32 原生）
            .bit_order_lsb = false,                      // MSB 先发（I2S 标准）
        },
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = mic_sck,
            .ws = mic_ws,
            .dout = I2S_GPIO_UNUSED,  // 只收不发
            .din = mic_din,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(rx_handle_, &std_cfg));
    ESP_LOGI(TAG, "麦克风 I2S 通道已创建（%d Hz）", input_sample_rate_);
}

int NoAudioCodecSimplex::Read(int16_t* dest, int samples)
{
    std::vector<int32_t> raw(samples);
    size_t bytes_read = 0;
    constexpr TickType_t kReadTimeout = pdMS_TO_TICKS(200);
    if(i2s_channel_read(rx_handle_, raw.data(), samples * sizeof(int32_t), &bytes_read, kReadTimeout) != ESP_OK){
        return 0;
    }

    samples = bytes_read / sizeof(int32_t);
    for(int i = 0; i < samples; i++){
        int32_t v = raw[i] >> 12;
        dest[i] = (v > INT16_MAX) ? INT16_MAX : (v < -INT16_MAX) ? -INT16_MAX : (int16_t)v;
    }
    return samples;
}

void NoAudioCodecSimplex::EnableInput(bool enable)
{
    if(enable == input_enabled_){
        return;
    }
    if(enable){
        ESP_ERROR_CHECK(i2s_channel_enable(rx_handle_));
    } else {
        ESP_ERROR_CHECK(i2s_channel_disable(rx_handle_));
    }
    AudioCodec::EnableInput(enable);
    ESP_LOGI(TAG, "麦克风采集输入%s", enable ? "启用" : "禁用");
}
