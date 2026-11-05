#include "display.h"

#include <cstring>
#include "font.h"
#include <vector>
#include <string>


void Display::SetStatus(const char* status) {
  std::strncpy(status_, status ? status : "", kCols);
  status_[kCols] = '\0';
  Render();
}

void Display::Render() {
  panel_.Clear();
  panel_.DrawString(0, 0, status_);

  // 状态栏
  // ┌─────┐
  // │ 空  │  ← 上面空 1 槽
  // │内容1│
  // │空   │  ← 下面空 0 槽
  // └─────┘

  int start_row = 0;
  if(centered_){
    start_row = (kRows - line_count_ + 1) / 2;
  }
  else {
    start_row = kRows - line_count_;
  }

  if(start_row < 0){
    start_row = 0;
  }

  for(int i = 0; i < line_count_; i++){
    int x = 0;
    if(centered_){
      x = ((kCols - static_cast<int>(strlen(lines_[i]))) * FONT_WIDTH) / 2;
    }
    panel_.DrawString(x, (start_row + i+1) * FONT_HEIGHT, lines_[i]);
  }
  panel_.Flush();
}

void Display::ShowNotification(const char* text) {
  ClearLines();
  centered_ = true;

  const char* p = text ? text : "";
  while(*p != '\0' && line_count_ < kRows){
    const char* nl = strchr(p, '\n');
    const size_t seg = nl ? static_cast<size_t>(nl - p) : strlen(p);
    const size_t n = (seg > kCols) ? kCols : seg;
    memcpy(lines_[line_count_], p, n);
    lines_[line_count_][n] = '\0';
    line_count_++;
    if(nl == nullptr){
      break;
    }
    p = nl + 1;
  }

  Render();
}

void Display::ClearLines() {
  memset(lines_, 0, sizeof(lines_));
  line_count_ = 0;
}

void Display::SetChatMessage(const char* role, const char* content) {
  centered_ = false;

  std::string text = std::string(role ? role : "") + ": " + std::string(content ? content : "");
  std::vector<std::string> wrapped;
  std::string line;
  for(char ch : text){
    if(ch == '\n'){
      wrapped.push_back(line);
      line.clear();
      continue;
    }
    line.push_back(ch);
    if(line.size() == kCols){
      wrapped.push_back(line);
      line.clear();
    }

  }
  if(!line.empty()){
    wrapped.push_back(line);
  }
  if(static_cast<int>(wrapped.size()) > kRows){
    wrapped.resize(kRows);
  }
  ClearLines();
  for(auto& line : wrapped){
    strncpy(lines_[line_count_], line.c_str(), kCols);
    lines_[line_count_][kCols] = '\0';
    line_count_++;
  }

  Render();

}
