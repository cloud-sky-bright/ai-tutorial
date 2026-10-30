#include "ssd1306.h"
#include <cstring>

#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "ssd1306";
SSD1306::SSD1306(i2c_port_num_t port, gpio_num_t sda, gpio_num_t scl, uint8_t addr) : port_(port), sda_(sda), scl_(scl), addr_(addr){}

SSD1306::~SSD1306() {
    if(dev_ != nullptr){
        i2c_master_bus_rm_device(dev_);
    }
    if(bus_ != nullptr){
        i2c_del_master_bus(bus_);
    }

}

bool SSD1306::Init() {
    i2c_master_bus_config_t bus_config = {};
    bus_config.i2c_port = port_;
    bus_config.sda_io_num = sda_;
    bus_config.scl_io_num = scl_;
    bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_config.glitch_ignore_cnt = 7;
    bus_config.flags.enable_internal_pullup = true;
    esp_err_t err = i2c_new_master_bus(&bus_config, &bus_);
    if(err != ESP_OK){
        ESP_LOGE(TAG, "Failed to create I2C bus: %s", esp_err_to_name(err));
        bus_ = nullptr;
        return false;
    }

    ESP_LOGI(TAG, "I2C 总线扫描：");
    bool found = false;
    for(uint8_t i = 1; i < 127; i++){    
        if(i2c_master_probe(bus_, i, 50) == ESP_OK){
            ESP_LOGI(TAG, "Found I2C device at address: 0x%02X%s", i, (i == addr_) ? " <---OLED" : "");
            if(i == addr_){
                found = true;
            }
        }
    }
    if(!found){
        ESP_LOGW(TAG, "OLED not found at address: 0x%02X, 请检查 SDA=GPIO%d / SCL=GPIO%d", addr_, (int)sda_, (int)scl_);
        return false;
    }

    i2c_device_config_t device_config = {};
    device_config.device_address = addr_;
    device_config.scl_speed_hz = 10 * 1000;
    device_config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    err = i2c_master_bus_add_device(bus_, &device_config, &dev_);
    if(err != ESP_OK){
        ESP_LOGE(TAG, "Failed to add device to I2C bus: %s", esp_err_to_name(err));
        dev_ = nullptr;
        return false;
    }
    static const uint8_t kInitCommands[] = {
        0xAE,        // 关闭显示（初始化期间先关，避免花屏）
        0x20, 0x00,  // 显存寻址模式：水平模式
        0xB0,        // 页地址 = 0
        0xC8,        // COM 扫描方向：从下到上
        0x00,        // 列地址低位 = 0
        0x10,        // 列地址高位 = 0
        0x40,        // 显示起始行 = 0
        0x81, 0xCF,  // 对比度
        0xA1,        // 段重映射（左右镜像方向）
        0xA6,        // 正常显示（0xA7 为反色）
        0xA8, 0x1F,  // 多路复用比 = 32（128x32 屏；128x64 屏是 0x3F）
        0xA4,        // 显示内容跟随显存
        0xD3, 0x00,  // 显示偏移 = 0
        0xD5, 0x80,  // 时钟分频
        0xD9, 0xF1,  // 预充电周期
        0xDA, 0x02,  // COM 引脚配置（128x32 顺序模式；128x64 屏是 0x12）
        0xDB, 0x40,  // VCOMH 电压
        0x8D, 0x14,  // 使能内部电荷泵（很多模块缺这条就点不亮）
        0xAF,        // 打开显示
    };
    WriteCommands(kInitCommands, sizeof(kInitCommands));
    Clear();
    Flush();
    ESP_LOGI(TAG, "OLED initialized successfully(128x32 @ 0x%02X, 100kHz)", addr_);
    return true;
}


void SSD1306::Clear(uint8_t pattern) {
    memset(buffer_, pattern, sizeof(buffer_));
}

void SSD1306::Flush() {
    static const uint8_t kSetWindow[] = {
        0x21, 0x00, 0x7F,
        0x22, 0x00, 0x03,
    };
    WriteCommands(kSetWindow, sizeof(kSetWindow));
    WriteData(buffer_, sizeof(buffer_));
}

void SSD1306::WriteCommands(const uint8_t* cmd, size_t n) {
    uint8_t buffer[64];
    buffer[0] = 0x00;

    memcpy(buffer + 1, cmd, n);
    for(int attempt = 0; attempt < 3; attempt++){
        esp_err_t err = i2c_master_transmit(dev_, buffer, n + 1, 200);
        if(err == ESP_OK){
            return;
        }
        ESP_LOGW(TAG, "I2C command failed(第 %d 次) ：  %s", attempt + 1, esp_err_to_name(err));
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    ESP_LOGE(TAG, "I2C command failed after 3 attempts");
}

void SSD1306::WriteData(const uint8_t* data, size_t n) {
    uint8_t buffer[129];
    buffer[0] = 0x40;

    while(n > 0){
        const size_t chunk = (n > 128) ? 128 : n;
        memcpy(buffer + 1, data, chunk);
        bool ok = false;
        for(int attempt = 0; attempt < 3; attempt++){
            esp_err_t err = i2c_master_transmit(dev_, buffer, chunk + 1, 200);
            if(err == ESP_OK){
                ok = true;
                break;
            }
            ESP_LOGW(TAG, "I2C data failed(第 %d 次, %d 字节) ：  %s", attempt + 1, (int)chunk, esp_err_to_name(err));
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        if(!ok){
            ESP_LOGE(TAG, "I2C data failed after 3 attempts(共 %d 字节)", (int)chunk);
            return;
        } 
        data += chunk;
        n -= chunk;
    }

}

void SSD1306::DrawPixel(int x, int y, bool on) {
    if(x < 0 || x >= 128 || y < 0 || y >= 32){
        return;
    }
    uint8_t& pixel = buffer_[y / 8 * 128 + x];
    const uint8_t mask = 1 << (y % 8); //00000001
    if(on){
        pixel |= mask; //00000001
    } else {
        pixel &= ~mask; //11111110
    }
}

void SSD1306::DrawBootScreen(){
    Clear();

    for(int x = 1; x < 127; x++){
        DrawPixel(x, 1, true);
        DrawPixel(x, 30, true);
    }

    for(int y = 1; y < 31; y++){
        DrawPixel(1, y, true);
        DrawPixel(126, y, true);
    }

    for(int dy = 0; dy < 10; dy++){
        for(int dx = 0; dx < 8; dx++){
            DrawPixel(40 + dx, 6 + dy, true);
            DrawPixel(80 + dx, 6 + dy, true);
        }
    }

    for(int x = 32; x <= 96; x++){
        const int t = x - 64;
        const int y = 24 - (t * t) / 160;
        DrawPixel(x, y, true);
    }
    Flush();
}