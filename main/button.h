#pragma once
#include <functional>

#include <utility>
#include "driver/gpio.h"
#include "esp_timer.h"

class Button{
    public:
        Button(gpio_num_t gpio_num, bool active_high = false, uint16_t long_press_time = 1000);
        ~Button();

        void OnPressDown(std::function<void()> callback) {on_press_down_ = std::move(callback);}
        void OnPressUp(std::function<void()> callback) {on_press_up_ = std::move(callback);}
        void OnClick(std::function<void()> callback) {on_click_ = std::move(callback);}
        void OnLongPress(std::function<void()> callback) {on_long_press_ = std::move(callback);}
    private:
        
// ┌─── sys_evt 任务（优先级 1，栈 4096）───┐
// │  WifiEventHandler                       │  ← Wi-Fi 事件来了才调
// │  IpEventHandler                         │  ← IP 事件来了才调
// │  （以及它们内部调的 lambda/Notify）       │
// └─────────────────────────────────────────┘

// ┌─── esp_timer 任务（优先级 22，栈 3584）──┐
// │  PollCallback（每 20ms 自动调）           │  ← 按键轮询
// │  ReconnectTimerCallback（5 秒到点调）     │  ← Wi-Fi 重连
// │  （以及 PollCallback 内部调的 on_click_） │  ← 按键 lambda
// └──────────────────────────────────────────┘

        static void PollCallback(void* arg);
        void Poll();

        bool IsRawPressed() const{
            return gpio_get_level(gpio_num_) == (active_high_ ? 1 : 0);
        }
        gpio_num_t gpio_num_;
        bool active_high_;
        uint16_t long_press_time_;
        esp_timer_handle_t timer_ = nullptr;
        bool stable_pressed_ = false;
        bool candidate_pressed_ = false;
        int64_t candidate_since_us_ = 0;
        int64_t press_start_us_ = 0;
        bool long_press_fired_ = false;
        std::function<void()> on_press_down_;
        std::function<void()> on_press_up_;
        std::function<void()> on_click_;
        std::function<void()> on_long_press_;

};