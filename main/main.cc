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

static const char* TAG = "stage2";

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "=============================");
    ESP_LOGI(TAG, "小智简易版 · stage 2 (OLED 显示系统)");
    ESP_LOGI(TAG, "=============================");

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
    vTaskDelay(pdMS_TO_TICKS(200));
    led.SetColor(0, 32, 0);
    vTaskDelay(pdMS_TO_TICKS(200));
    led.SetColor(0, 0, 32);
    vTaskDelay(pdMS_TO_TICKS(200));
    led.Off();

    SSD1306 oled(DISPLAY_I2C_PORT, DISPLAY_SDA_PIN, DISPLAY_SCL_PIN, DISPLAY_I2C_ADDR);
    const bool display_ok = oled.Init();
    Display display(oled);
    if(display_ok){
        oled.DrawBootScreen();
        vTaskDelay(pdMS_TO_TICKS(1200));
        display.SetStatus("Stage2 | Display OK");
        display.ShowNotification("XiaoZhi Tutorial");
        vTaskDelay(pdMS_TO_TICKS(2000));
        display.SetStatus("Stage2 | Ready");
        display.SetChatMessage("boot", "led | oled | font ready");

    }
    else{
        ESP_LOGW(TAG, "Display initialization failed, 跳过显示功能");
    }
    static const char* kDemoMessages[] = {
        "font 8x16",
        "display ok",
        "last msg",
        "next: wifi"
    };
    int heartbeat = 0;
    int demo_count = 0;
    while(true){
        heartbeat++;
        led.SetColor(0, 0, 24);
        vTaskDelay(pdMS_TO_TICKS(60));
        led.Off();
        if(display_ok){
            char status[32];
            snprintf(status, sizeof(status), "stage 2 | up: %ds", heartbeat);
            display.SetStatus(status);
        }
        if(display_ok && heartbeat % 5 == 0 && demo_count < 4){
            display.SetChatMessage("demo", kDemoMessages[demo_count]);
            demo_count++;
        }
        if(heartbeat % 5 == 0){
            ESP_LOGI(TAG, "heartbeat: %d | 屏幕： %s | PSRAM 空闲： %d KB", heartbeat, display_ok ? "OK" : "FAIL", (int)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));
        }
        
        vTaskDelay(pdMS_TO_TICKS(940));
    }
}

