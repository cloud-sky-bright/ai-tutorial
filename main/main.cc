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
#include "device_state_machine.h"
#include "button.h"
#include "audio/audio_codec.h"
#include "audio/no_audio_codec.h"
#include "audio/audio_service.h"
#include <string>
#include <atomic>
#include <vector>

#include "device_state.h"


static const char* TAG = "stage7";

static void LedColorForState(Led& led, DeviceState state) {
    switch(state){
        case kDeviceStateSpeaking:
            led.SetColor(28, 0, 28);
            break;
        case kDeviceStateIdle:
            led.SetColor(0, 0, 12);
            break;
        default:
            led.SetColor(16, 16, 0);
            break;
    }
}

// static void ShowMicLevel(Display& display, AudioCodec& codec){
//     static std::vector<int16_t> pcm(AUDIO_CODEC_DMA_FRAME_NUM);
//     if(!codec.InputData(pcm)){
//         return;
//     }
//     int64_t sum = 0;
//     for(int16_t s : pcm){
//         sum += (s >= 0) ? s : -s;
//     }
//     const int mean = static_cast<int>(sum / pcm.size());
//     int level = mean / 300;
//     if(level > 12){
//         level = 12;
//     }
//     std::string bar = "[";

//     for(int i = 0; i < 12; i++){
//         bar += (i < level) ? "#" : ".";
//     }

//     bar += "]";
//     char head[16];
//     snprintf(head, sizeof(head), "Mic: %d", mean);
//     display.ShowNotification((std::string(head) + "\n" + bar).c_str());
// }

static void ShowLevelBar(Display& display, int level, bool speaking){
    int cells = level / 250;
    if(cells > 14){
        cells = 14;
    }
    std::string bar = "[";
    for(int i = 0; i < 14; i++){
        bar += (i < cells) ? "#" : ".";
    }
    bar += "]";
    char head[24];
    snprintf(head, sizeof(head), speaking ? "mic %d <<TALK" : "mic %d", level);
    display.ShowNotification((std::string(head) + "\n" + bar).c_str());
}

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "=============================");
    ESP_LOGI(TAG, "小智简易版 · stage 7 说话检测");
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
    DeviceStateMachine machine;
    machine.AddStateChangeListener([](DeviceState from, DeviceState to){
        ESP_LOGI(TAG, "State changed from %s to %s", DeviceStateMachine::GetStateName(from), DeviceStateMachine::GetStateName(to));
    });
    machine.TransitionTo(kDeviceStateStarting);

    if(display_ok){
        oled.DrawBootScreen();
        vTaskDelay(pdMS_TO_TICKS(1000));
        display.SetStatus(DeviceStateMachine::GetStateName(machine.GetState()));
        display.SetChatMessage("sys", "stage7 boot");
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
    Button boot_button(BUTTON_BOOT_GPIO);
    Button talk_button(BUTTON_TALK_GPIO);
    boot_button.OnClick([&machine](){
        const DeviceState state = machine.GetState();
        if(state == kDeviceStateIdle){
            machine.TransitionTo(kDeviceStateListening);
        }
        else if(state == kDeviceStateListening){
            machine.TransitionTo(kDeviceStateIdle);
        }
        else{
            ESP_LOGW(TAG, "当前状态 %s 不允许切换说话", DeviceStateMachine::GetStateName(state));
        }
    });
    boot_button.OnLongPress([&machine](){
        ESP_LOGI(TAG, "Boot button long press");
        machine.TransitionTo(kDeviceStateIdle);
    });
    talk_button.OnPressDown([&machine](){
        if(machine.GetState() == kDeviceStateIdle){
            machine.TransitionTo(kDeviceStateListening);
        }
    });
    talk_button.OnPressUp([&machine](){
        if(machine.GetState() == kDeviceStateListening){
            machine.TransitionTo(kDeviceStateIdle);
        }
    });

    NoAudioCodecSimplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN);

    AudioService audio_service;
    audio_service.Initialize(&audio_codec);
    audio_service.Start();

    std::atomic<bool> speech_finished{false};
    audio_service.OnVadStateChange([&speech_finished](bool speaking){
        if(!speaking){
            ESP_LOGI(TAG, "Speech finished");
            speech_finished.store(true);
        }
    });

    DeviceState last_state = machine.GetState();

    bool last_connected = false;
    int heartbeat = 0;
    while(true){
        heartbeat++;

        if(speech_finished.exchange(false) && machine.GetState() == kDeviceStateListening){
            ESP_LOGI(TAG, "Speech finished, transitioning to kDeviceStateIdle");
            if(display_ok){
                display.SetChatMessage("sys", "speech finished");
            }
            machine.TransitionTo(kDeviceStateIdle);
        }

        if(machine.GetState() == kDeviceStateStarting && wifi.IsConnected()){
            machine.TransitionTo(kDeviceStateIdle);
        }
        const DeviceState state = machine.GetState();

        if(state != last_state){
            if(state == kDeviceStateListening){
                audio_service.EnableVoiceProcessing(true);
            }
            else if(last_state == kDeviceStateListening){
                audio_service.EnableVoiceProcessing(false);
            }
        }
        LedColorForState(led, state);
        if(state == kDeviceStateListening){
            led.SetColor(0, audio_service.IsVoiceDetected() ? 32 : 10, 0);
        }
        else {
            LedColorForState(led, state);
            vTaskDelay(pdMS_TO_TICKS(60));
            led.Off();
        }
        
        if(display_ok){
            display.SetStatus(DeviceStateMachine::GetStateName(state));
            const bool connected = wifi.IsConnected();
            if(connected != last_connected){
                if(connected){
                    display.SetChatMessage("ip", wifi.GetIp());
                }
                else{
                    display.SetChatMessage("wifi", "retrying");
                }
                last_connected = connected;
            }
            if(state != last_state){
                display.SetChatMessage("state", DeviceStateMachine::GetStateName(state));
            }

            if(state == kDeviceStateListening){
                ShowLevelBar(display, audio_service.GetLevel(), audio_service.IsVoiceDetected());
            }
        }
        last_state = state;
        if(heartbeat % 5 == 0){
            if(wifi.IsConnected()){
                ESP_LOGI(TAG, "heartbeat: %d | State: %s | IP: %s | RSSI %d dBm | PSRAM 空闲： %d KB", heartbeat, DeviceStateMachine::GetStateName(state), wifi.GetIp(), wifi.GetRssi(), (int)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));
            }
            else{
                ESP_LOGI(TAG, "heartbeat: %d | State: %s | WIFI 未连接 | PSRAM 空闲： %d KB", heartbeat, DeviceStateMachine::GetStateName(state), (int)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(state == kDeviceStateListening ? 40 : 940));
    }
}

