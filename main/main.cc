#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led.h"
#include "config.h"
#include "ssd1306.h"
#include "esp_heap_caps.h"
#include "font.h"
#include "display.h"
#include <cstdio>
#include "wifi.h"
#include "nvs_flash.h"

static const char* TAG = "stage2";

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "=============================");
    ESP_LOGI(TAG, "小智简易版 · stage 3 (WIFI 系统)");
    ESP_LOGI(TAG, "=============================");
    
    esp_err_t nvs_err = nvs_flash_init();
    if(nvs_err == ESP_ERR_NVS_NO_FREE_PAGES || nvs_err == ESP_ERR_NVS_NEW_VERSION_FOUND){
        ESP_LOGW(TAG, "NVS initialization failed, 尝试重新格式化 NVS");
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_err);
    // const uint8_t* glyph = FontGetGlyph('A');

    // ESP_LOGI(TAG, "Glyph for 'A':");

    // for(int row = 0; row < FONT_HEIGHT; row++){
    //     char bits[FONT_WIDTH + 1]; // \0
    //     for(int col = 0; col < FONT_WIDTH; col++){
    //         bits[col] = (glyph[row] & (0x80 >> col)) ? '#' : '.';
    //     }
    //     bits[FONT_WIDTH] = '\0';
    //     ESP_LOGI(TAG, "%s", bits);
    // }

    Led led(LED_GPIO);
    led.SetColor(32, 0, 0);
    vTaskDelay(pdMS_TO_TICKS(150));
    led.SetColor(0, 32, 0);
    vTaskDelay(pdMS_TO_TICKS(150));
    led.SetColor(0, 0, 32);
    vTaskDelay(pdMS_TO_TICKS(150));
    led.Off();

    SSD1306 oled(DISPLAY_I2C_PORT, DISPLAY_SDA_PIN, DISPLAY_SCL_PIN, DISPLAY_I2C_ADDR);
    const bool display_ok = oled.Init();
    Display display(oled);
    if(display_ok){
        oled.DrawBootScreen();
        vTaskDelay(pdMS_TO_TICKS(1000));
        display.SetStatus("Stage 3 | WIFI");
        display.ShowNotification("XiaoZhi Tutorial");
        vTaskDelay(pdMS_TO_TICKS(2000));
        display.SetStatus("WIFI: connecting...");
        display.SetChatMessage("sys", "wifi start");

    }
    else{
        ESP_LOGW(TAG, "Display initialization failed, 跳过显示功能");
    }

    WifiStation wifi(WIFI_SSID, WIFI_PASSWORD);
    wifi.SetEventCallback([](WifiStation::Event event){
        switch(event){
            case WifiStation::Event::Connected:
                ESP_LOGI(TAG, "Connected to WIFI");
                break;
            case WifiStation::Event::Disconnected:
                ESP_LOGI(TAG, "Disconnected from WIFI");
                break;
            case WifiStation::Event::Connecting:
                ESP_LOGI(TAG, "Connecting to WIFI");
                break;
        }
    });
    wifi.Start();
    bool last_connected = false;
    int heartbeat = 0;
    while(true){
        heartbeat++;
        led.SetColor(0, 0, 24);
        vTaskDelay(pdMS_TO_TICKS(60));
        led.Off();
        const bool connected = wifi.IsConnected();
        if(display_ok){
            char status[32];
            if(connected){
                snprintf(status, sizeof(status), "IP %s", wifi.GetIp());
            }
            else{
                snprintf(status, sizeof(status), "WIFI: disconnected");
            }
            display.SetStatus(status);
            if(connected != last_connected){
                if(connected){
                    display.SetChatMessage("wifi", "wifi connected");
                }
                else{
                    display.SetChatMessage("wifi", "retrying");
                }
                last_connected = connected;
            }
        }
        if(heartbeat % 5 == 0){
            if(wifi.IsConnected()){
                ESP_LOGI(TAG, "heartbeat: %d | IP: %s | RSSI %d dBm | PSRAM 空闲： %d KB", heartbeat, wifi.GetIp(), wifi.GetRssi(), (int)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));
            }
            else{
                ESP_LOGI(TAG, "heartbeat: %d |  WIFI 未连接 | PSRAM 空闲： %d KB", heartbeat, (int)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(940));
    }
}

