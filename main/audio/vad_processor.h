#pragma once

#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

#define VAD_ENERGY_THRESHOLD 1200
#define VAD_START_FRAMES 3
#define VAD_STOP_FRAMES 50

class VadProcessor {
public:
    using VadCallback = std::function<void(bool speaking)>;
    void OnVadStateChange(VadCallback callback){
        callback_ = std::move(callback);
    }

    void Reset();

    int Feed(const std::vector<int16_t>& frame);

    bool IsSpeaking() const {
        return speaking_;
    }
private:
    bool speaking_ = false;
    int speech_frames_ = 0;
    int silence_frames_ = 0;
    VadCallback callback_;
};

