#ifndef AUDIO_SERVICE_H
#define AUDIO_SERVICE_H

#include "audio_codec.h"
#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <vector>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define AUDIO_QUEUE_MAX_FRAMES 4
#define AUDIO_INPUT_WARMUP_FRAMES 3

class AudioService {
public:
    
    AudioService() = default;
    ~AudioService();

    void Initialize(AudioCodec* codec);
    void Start();
    void Stop();

    void EnableVoiceProcessing(bool enable);
    bool IsVoiceProcessingRunning() const;

    int GetLevel() const { return level_.load(); }
private:
    void AudioInputTask();
    void AudioProcessTask();
    void AudioOutputTask();

    AudioCodec* codec_ = nullptr;

    TaskHandle_t audio_input_task_ = nullptr;
    TaskHandle_t audio_process_task_ = nullptr;
    TaskHandle_t audio_output_task_ = nullptr;

    mutable std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::deque<std::vector<int16_t>> audio_encode_queue_;
    std::deque<std::vector<int16_t>> audio_playback_queue_;

//     麦克风（INMP441）
//    │ 采样 → ADC → 24 位数字，从 SD 线一位一位送出
//    │ I2S 三线（SCK / WS / SD）
//    ▼
// ESP32 内部 I2S 外设（主模式出时钟；串行位流拼成 32 位采样）
//    │ DMA（不经过 CPU）
//    ▼
// RAM 里 6 块环形缓冲（每块 240 点 = 960 B = 15 ms，共 90 ms 余量）
//    │ 填满一块 → RX EOF 中断 → 把"块地址"push 进就绪队列
//    ▼
// 就绪队列 msg_queue（深度 5；排的是块地址，不是音频；满了丢最旧）
//    │ audio_input 任务：read 弹出地址 → memcpy 960 B
//    ▼
// 程序数组 raw（32 位）
//    │ 转换：raw[i] >> 12 + 削顶
//    ▼
// 240 点 int16（一帧）
//    │ push（队列满就 cv.wait 等空位）
//    ▼
// 应用队列 audio_encode_queue_（std::deque，最多 4 帧 = 60 ms）
//    │ pop
//    ▼
// audio_process 任务：算平均绝对值 → attack/release → level_
//    │
//    ▼
// 主循环 GetLevel() → 响度条上屏

    bool voice_processing_enabled_ = false;
    bool service_stopped_ = false;
    int warmup_frames_left_ = 0;
    std::atomic<int> level_{0};

};

#endif