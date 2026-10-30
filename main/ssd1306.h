#pragma once
#include "driver/gpio.h"
#include "driver/i2c_master.h"

#include <cstdint>
#include <cstddef>

class SSD1306 {
public:
    SSD1306(i2c_port_num_t port, gpio_num_t sda, gpio_num_t scl, uint8_t addr);
    ~SSD1306();

    bool Init();
    void Clear(uint8_t pattern = 0x00);
    void DrawPixel(int x, int y, bool on);
    void Flush();

    void DrawBootScreen();

private:
    void WriteCommands(const uint8_t* cmds, size_t n);
    void WriteData(const uint8_t* data, size_t n);

    i2c_master_bus_handle_t bus_ = nullptr;
    i2c_master_dev_handle_t dev_ = nullptr;

    i2c_port_num_t port_;
    gpio_num_t sda_;
    gpio_num_t scl_;
    uint8_t addr_;
    uint8_t buffer_[128*32/8];
};
