#include "button.h"
#include "esp_log.h"

static const char* TAG = "Button";

static constexpr int64_t kPollIntervalUs = 20 * 1000;
static constexpr int64_t kDebounceUs = 40 * 1000;

Button::Button(gpio_num_t gpio_num, bool active_high, uint16_t long_press_time) : gpio_num_(gpio_num), active_high_(active_high), long_press_time_(long_press_time) {
    gpio_config_t config = {};
    config.pin_bit_mask = (1ULL << gpio_num_);
    config.mode = GPIO_MODE_INPUT;
    if(active_high_){
        config.pull_down_en = GPIO_PULLDOWN_ENABLE;
    } else {
        config.pull_up_en = GPIO_PULLUP_ENABLE;
    }
    ESP_ERROR_CHECK(gpio_config(&config));
    esp_timer_create_args_t timer_args = {};
    timer_args.callback = &Button::PollCallback;
    timer_args.arg = this;
    timer_args.name = "button";
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer_));
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer_, kPollIntervalUs));

    ESP_LOGI(TAG, "GPIO%d 按钮就绪（%s有效,  长按 %d ms)", gpio_num_, active_high_ ? "高电平" : "低电平", long_press_time_);

}

Button::~Button(){
    if(timer_ != nullptr){
        esp_timer_stop(timer_);
        esp_timer_delete(timer_);
    }
}

void Button::PollCallback(void* arg){
    static_cast<Button*>(arg)->Poll();
}

void Button::Poll(){
    const bool raw = IsRawPressed();
    const int64_t now = esp_timer_get_time();
    // 0 0 1 0 1 0 0 0 0 0 0 0 0....
    // 0 1 0 1 0 0 0 0 0 0 0 0 0....
    if(raw != candidate_pressed_){
        candidate_pressed_ = raw;
        candidate_since_us_ = now;
    }
    if(candidate_pressed_ != stable_pressed_ && now - candidate_since_us_ > kDebounceUs){
        stable_pressed_ = candidate_pressed_;
        if(stable_pressed_){
            press_start_us_ = now;
            long_press_fired_ = false;
            if(on_press_down_){
                on_press_down_();
            }
        } else {
            if(on_press_up_){
                on_press_up_();
            }
            if(!long_press_fired_ && on_click_){
                on_click_();
            }
        }
    }
    if(stable_pressed_ && !long_press_fired_ && now - press_start_us_ >= static_cast<int64_t>(long_press_time_ * 1000)){
        long_press_fired_ = true;
        if(on_long_press_){
            on_long_press_();
        }
    }
}

