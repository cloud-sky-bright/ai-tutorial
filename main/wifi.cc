#include "wifi.h"

#include <cstdio>
#include <cstring>
#include "esp_log.h"
#include "esp_netif.h"

#include "esp_timer.h"
#include "esp_wifi.h"

static const char* TAG = "wifi";

void WifiStation::Start() {
    ESP_ERROR_CHECK(esp_netif_init());
    esp_err_t err = esp_event_loop_create_default();
    if(err != ESP_OK && err != ESP_ERR_INVALID_STATE){
        ESP_ERROR_CHECK(err);
    }
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init_config));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &WifiStation::WifiEventHandler, this));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &WifiStation::IpEventHandler, this));

    esp_timer_create_args_t timer_args = {};
    timer_args.callback = &WifiStation::ReconnectTimerCallback;
    timer_args.arg = this;

    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &reconnect_timer_));

    wifi_config_t config = {};
    strncpy(reinterpret_cast<char*>(config.sta.ssid), ssid_, sizeof(config.sta.ssid) - 1); // '\0'
    strncpy(reinterpret_cast<char*>(config.sta.password), password_, sizeof(config.sta.password) - 1);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &config));

    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "WifiStation started, 开始连接 \"%s\"", ssid_);
}

void WifiStation::WifiEventHandler(void* arg, esp_event_base_t base, int32_t event_id, void* data) {
    auto* self = static_cast<WifiStation*>(arg);
    switch(event_id){
        case WIFI_EVENT_STA_START:
            self->Notify(Event::Connecting);
            esp_wifi_connect();
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            self->connected_ = false;
            self->ip_[0] = '\0';
            self->retry_count_++;
            ESP_LOGW(TAG, "连接断开（第%d 次）， 5秒后重连", self->retry_count_);
            esp_timer_start_once(self->reconnect_timer_, 5000000);
            self->Notify(Event::Disconnected);
            break;
        default:
            break;
    }
}

void WifiStation::IpEventHandler(void* arg, esp_event_base_t base, int32_t event_id, void* data) {
    auto* self = static_cast<WifiStation*>(arg);
    auto* event = static_cast<ip_event_got_ip_t*>(data);
    snprintf(self->ip_, sizeof(self->ip_), IPSTR, IP2STR(&event->ip_info.ip));
    self->connected_ = true;
    self->retry_count_ = 0;
    ESP_LOGI(TAG, "连接成功， IP: %s", self->ip_);
    self->Notify(Event::Connected);
}

void WifiStation::ReconnectTimerCallback(void* arg) {
    (void)arg;
    ESP_LOGI(TAG, "ReconnectTimerCallback, 尝试重连");
    esp_wifi_connect();
}

int WifiStation::GetRssi() const {
    wifi_ap_record_t ap_record = {};
    if(!connected_ || esp_wifi_sta_get_ap_info(&ap_record) != ESP_OK){
        return 0;
    }
    return ap_record.rssi;
}
