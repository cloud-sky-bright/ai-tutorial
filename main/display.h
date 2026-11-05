//   ┌──────────────────┐
//   │ 状态栏（1 行）     │  y = 0
//   ├──────────────────┤
//   │ 内容区（1 行）     │  y = 16
//   └──────────────────┘

#pragma once

#include "ssd1306.h"


class Display{
public:
    explicit Display(SSD1306& panel) : panel_(panel){};
    void SetStatus(const char* status);

    void ShowNotification(const char* text);

    void SetChatMessage(const char* role, const char* content);

private:
    SSD1306& panel_;

    static constexpr int kCols = 16;
    static constexpr int kRows = 1;

    void Render();
    void ClearLines();

    char status_[kCols + 1] = {0};
    char lines_[kRows][kCols + 1] = {{0}};

    int line_count_ = 0;
    bool centered_ = false;
};