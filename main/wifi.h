#pragma once

#include <functional>
#include "esp_event.h"
#include "esp_timer.h"

class WifiStation{

    public:
        enum class Event{
            Connected,
            Disconnected,
            Connecting,
        };
        using EventCallback = std::function<void(Event)>;
        WifiStation(const char* ssid, const char* password) : ssid_(ssid), password_(password) {}

        void SetEventCallback(EventCallback callback) { callback_ = std::move(callback); }
        void Start();

        bool IsConnected() const { return connected_; }
        const char* GetIp() const { return ip_; }
        int GetRssi() const;
    private:
        static void WifiEventHandler(void* arg, esp_event_base_t base, int32_t event_id, void* data);
        static void IpEventHandler(void* arg, esp_event_base_t base, int32_t event_id, void* data);
        static void ReconnectTimerCallback(void* arg);

        void Notify(Event event){
            if(callback_){
                callback_(event);
            }
        }

        const char* ssid_;
        const char* password_;
        EventCallback callback_;
        bool connected_ = false;
        char ip_[16] = {0};
        int retry_count_ = 0;
        esp_timer_handle_t reconnect_timer_ = nullptr;
};