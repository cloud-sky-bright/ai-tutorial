#include "vad_processor.h"

#include <esp_log.h>

static const char* TAG = "Vad";

void VadProcessor::Reset() {
    speaking_ = false;
    speech_frames_ = 0;
    silence_frames_ = 0;
}


int VadProcessor::Feed(const std::vector<int16_t>& frame) {
    int64_t sum = 0;
    for (int16_t s : frame){
        sum += (s >= 0) ? s : -s;
    }
    const int level = frame.empty() ? 0 : static_cast<int>(sum / frame.size());
    if(level >= VAD_ENERGY_THRESHOLD){
        silence_frames_ = 0;
        if(!speaking_ && ++speech_frames_ >= VAD_START_FRAMES){
            speaking_ = true;
            ESP_LOGI(TAG, "开始说话 （连续 %d 帧 >= %d）, level = %d", VAD_START_FRAMES, VAD_ENERGY_THRESHOLD, level);
// VAD 的回调（住在 VadProcessor）
//    ├─ 在 服务层 注册（Initialize）
//    └─ 改 服务层 的变量 voice_detected_
//         └─ 并在内部再调 服务的回调

// 服务的回调（住在 AudioService）
//    ├─ 在 应用层 注册（main）
//    └─ 改 应用层 的变量 speech_finished

// 两层回调的最终目的就是为了将底层的状态变化透传到顶层，顺便完成自己这一层的记账

            if(callback_) callback_(true);
        }
    } else {
        speech_frames_ = 0;
        if(speaking_ && ++silence_frames_ >= VAD_STOP_FRAMES){
            speaking_ = false;
            ESP_LOGI(TAG, "结束说话 （连续 %d 帧 < %d）, level = %d", VAD_STOP_FRAMES, VAD_ENERGY_THRESHOLD, level);
            if(callback_) callback_(false);
        }
    }
    return level;
}
