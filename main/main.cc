#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led.h"
#include "config.h"
#include "ssd1306.h"
#include "esp_heap_caps.h"

static const char* TAG = "stage1";

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "=============================");
    ESP_LOGI(TAG, "小智简易版 · stage 1");
    ESP_LOGI(TAG, "=============================");
    Led led(LED_GPIO);
    led.SetColor(32, 0, 0);
    vTaskDelay(pdMS_TO_TICKS(1000));
    led.SetColor(0, 32, 0);
    vTaskDelay(pdMS_TO_TICKS(1000));
    led.SetColor(0, 0, 32);
    vTaskDelay(pdMS_TO_TICKS(1000));
    led.Off();

    SSD1306 display(DISPLAY_I2C_PORT, DISPLAY_SDA_PIN, DISPLAY_SCL_PIN, DISPLAY_I2C_ADDR);
    const bool display_ok = display.Init();
    if(display_ok){
        display.DrawBootScreen();
    }

    int heartbeat = 0;
    while(true){
        heartbeat++;
        led.SetColor(0, 0, 24);
        vTaskDelay(pdMS_TO_TICKS(60));
        led.Off();
        if(heartbeat % 5 == 0){
            ESP_LOGI(TAG, "heartbeat: %d | 屏幕： %s | PSRAM 空闲： %d KB", heartbeat, display_ok ? "OK" : "FAIL", (int)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));
        }
        
        vTaskDelay(pdMS_TO_TICKS(940));
    }
}

