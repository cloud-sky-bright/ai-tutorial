// ============================================================================
// 这是配置模板，不是实际使用的配置文件。
//
//   cp main/config.example.h main/config.h     （Windows: Copy-Item）
//
// 然后编辑 main/config.h 填入你自己的引脚 / 地址 / 凭据。
// main/config.h 已被 .gitignore 排除，不会进版本库；Wi-Fi 密码、大模型 API Key
// 等敏感信息只写在 config.h 里。代码中一律 #include "config.h"，不要改成 include
// 本文件。若本文件新增了宏，记得同步到 config.h，否则编译会报未定义。
// ============================================================================

#pragma once

#include "driver/gpio.h"

// 板载 WS2812 单灯的数据线
#define LED_GPIO GPIO_NUM_48

// OLED：SSD1306 128x32，软件无关，直接走硬件 I2C 主机驱动
#define DISPLAY_I2C_PORT 0
#define DISPLAY_I2C_ADDR 0x3C

#define DISPLAY_SDA_PIN GPIO_NUM_41
#define DISPLAY_SCL_PIN GPIO_NUM_42

#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 32


#define WIFI_SSID "your_wifi_ssid"
#define WIFI_PASSWORD "your_wifi_password"
