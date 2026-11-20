#include "audio_service.h"
#include <esp_log.h>

static const char* TAG = "audio_service";

AudioService::~AudioService() {
    Stop();
}

void AudioService::Initialize(AudioCodec* codec) {
    codec_ = codec;
    vad_processor_.OnVadStateChange([this](bool speaking) {
        voice_detected_.store(speaking);
        if (vad_callback_) {
            vad_callback_(speaking);
        }
    });
    
    ESP_LOGI(TAG, "Audio service initialized");
}

void AudioService::Start(){
    xTaskCreate([](void* arg){
        static_cast<AudioService*>(arg)->AudioInputTask();
        vTaskDelete(nullptr);
    }, "audio_input", 4096, this, 8, &audio_input_task_);

    xTaskCreate([](void* arg){
        static_cast<AudioService*>(arg)->AudioProcessTask();
        vTaskDelete(nullptr);
    }, "audio_process", 4096, this, 2, &audio_process_task_);

    xTaskCreate([](void* arg){
        static_cast<AudioService*>(arg)->AudioOutputTask();
        vTaskDelete(nullptr);
    }, "audio_output", 3072, this, 4, &audio_output_task_);

    ESP_LOGI(TAG, "Audio service started");
}

void AudioService::Stop() {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        service_stopped_ = true;
        voice_processing_enabled_ = false;
        audio_encode_queue_.clear();
        audio_playback_queue_.clear();
    }
    queue_cv_.notify_all();
}

void AudioService::EnableVoiceProcessing(bool enable) {
    ESP_LOGI(TAG, "Enabling voice processing: %s", enable ? "true" : "false");
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        voice_processing_enabled_ = enable;
        if(enable){
            warmup_frames_left_ = AUDIO_INPUT_WARMUP_FRAMES;
            vad_processor_.Reset();
        }
        else{
            level_.store(0);
            voice_detected_.store(false);
            audio_encode_queue_.clear();
        }
    }
    queue_cv_.notify_all();
    if(codec_ != nullptr){
        codec_->EnableInput(enable);
    }
}

bool AudioService::IsVoiceProcessingRunning() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return voice_processing_enabled_;
}

void AudioService::OnVadStateChange(VadCallback callback) {
    vad_callback_ = std::move(callback);
}

void AudioService::AudioInputTask() {
    std::vector<int16_t> frame(AUDIO_CODEC_DMA_FRAME_NUM);
    while (true) {
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_cv_.wait(lock, [this] { return service_stopped_ || voice_processing_enabled_; });
            if(service_stopped_){
                break;
            }
        }

        if(codec_ == nullptr || !codec_->InputData(frame)){
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        std::unique_lock<std::mutex> lock(queue_mutex_);
        if(warmup_frames_left_ > 0){
            warmup_frames_left_--;
            continue;
        }
        if(!voice_processing_enabled_){
            continue;
        }

        queue_cv_.wait(lock, [this] { return audio_encode_queue_.size() < AUDIO_QUEUE_MAX_FRAMES || service_stopped_; });
        audio_encode_queue_.push_back(std::move(frame));
        frame.resize(AUDIO_CODEC_DMA_FRAME_NUM);
        queue_cv_.notify_all();

    }
    ESP_LOGI(TAG, "Audio input task finished");
}

void AudioService::AudioProcessTask() {
    while (true) {
        std::vector<int16_t> frame;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_cv_.wait(lock, [this] { return service_stopped_ || !audio_encode_queue_.empty(); });
            if(service_stopped_){
                break;
            }
            frame = std::move(audio_encode_queue_.front());
            audio_encode_queue_.pop_front();
            queue_cv_.notify_all();
        }
        const int mean = vad_processor_.Feed(frame);

        int level = level_.load();
        if(mean > level){
            level = (level + mean) / 2;
        }
        else{
            level = level - (level - mean) / 8;
        }
        level_.store(level);
    }
    ESP_LOGI(TAG, "Audio process task finished");
}

void AudioService::AudioOutputTask() {
    while (true) {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        queue_cv_.wait(lock, [this] { return service_stopped_ || !audio_playback_queue_.empty(); });
        if(service_stopped_){
            break;
        }

        auto frame = std::move(audio_playback_queue_.front());
        audio_playback_queue_.pop_front();
        queue_cv_.notify_all();
        lock.unlock();

        (void)frame;
    }
    ESP_LOGI(TAG, "Audio output task finished");

}
