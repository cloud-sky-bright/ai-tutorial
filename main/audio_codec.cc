#include "audio_codec.h"
#include <esp_log.h>

#define TAG "AudioCodec"

void AudioCodec::EnableInput(bool enable) {
    if(enable == input_enabled_){
        return;
    }
    input_enabled_ = enable;
    ESP_LOGI(TAG, "Set input enable to %s", enable ? "true" : "false");
}

bool AudioCodec::InputData(std::vector<int16_t>& data) {
    int samples = Read(data.data(), data.size());
    return samples > 0;
}
