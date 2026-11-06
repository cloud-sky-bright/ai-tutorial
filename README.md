# 小智 ESP32-S3 教学项目（ai-tutorial）

从零把一块面包板一步步养成"能听、能想、能说、能显示"的 AI 语音助手。**20 期演进，每期一个 git tag**，任意一期都能单独 `checkout` 出来编译烧录。

参考架构：[xiaozhi-esp32](https://github.com/78/xiaozhi-esp32)。本工程不复刻它，而是把它拆开、按顺序逐块长出来。

---

## 硬件（telesky ESP32-S3 面包板套件）

| 部件 | 规格 |
|---|---|
| 主控 | ESP32-S3 QFN56（rev v0.2），16 MB Flash，8 MB Octal PSRAM @80 MHz |
| 板载 RGB 灯 | WS2812 × 1，接 **GPIO48**，用 RMT 外设驱动 |
| 显示屏 | SSD1306 OLED **128×32**，I2C 地址 `0x3C`，SDA=**GPIO41** / SCL=**GPIO42** |
| 控制台串口 | UART0 TX=GPIO43 / RX=GPIO44（板载 USB 转串口） |

引脚以 `main/config.example.h` 为准，换板子改这份文件即可，不用动业务代码。

## 开发环境

- **ESP-IDF v5.5.2**（其它版本没测；`sdkconfig.defaults` 里的 Kconfig 符号在不同 IDF 版本间会改名/增删）
- C++ 标准：`gnu++2b`（即 C++23，IDF 默认，无需设置）
- 工具链：`xtensa-esp32s3-elf`，构建系统 CMake + Ninja

## 快速开始

```powershell
# 1. 克隆
git clone https://github.com/cloud-sky-bright/ai-tutorial.git
cd ai-tutorial

# 2. 生成自己的配置文件（这一步不做会 fatal error: config.h: No such file or directory）
Copy-Item main\config.example.h main\config.h
# 然后编辑 main\config.h，改成你自己的引脚

# 3. 指定芯片（必须，见下方"已知坑"第 1 条）
idf.py set-target esp32s3

# 4. 编译 + 烧录 + 看日志（端口按实际改）
idf.py -p COM12 flash monitor
```

Linux / macOS 把 `Copy-Item` 换成 `cp`、`COM12` 换成 `/dev/ttyACM0` 之类的设备路径。

## 分期目录

| Tag | 这一期做了什么 | 新增文件 |
|---|---|---|
| `stage1` | 工程骨架：分区表 / PSRAM / 日志配置；RMT 驱动板载 WS2812 三色灯 | `main/led.cc` `main/led.h` |
| `stage2` | 显示系统：SSD1306 I2C 驱动、8×16 字模、`Display` 上层（状态栏 / 通知 / 聊天行） | `main/ssd1306.*` `main/font.*` `main/display.*` `tools/gen_font8x16.ps1` |
| `stage3` | Wi-Fi 接入：`WifiStation` 封装（扫描/关联、事件回调、`esp_timer` 5 秒自动重连）、心跳栏显示 IP / RSSI / PSRAM 空闲 | `main/wifi.cc` `main/wifi.h` |
| `stage4` | *规划中* | — |

查看仓库里已有哪些期：

```bash
git tag -l
```

## 切换到某一期 / 对比两期差异

```bash
git checkout stage2                 # 进入该期（detached HEAD 属正常）
git switch -c my-stage2             # 想在这期上继续改，就开个分支

git log --oneline --decorate        # 看整条演进线
git diff stage1 stage2              # 看两期之间代码怎么长的（教学仓库的核心用法）
git switch main                     # 回到最新

# 每次换期之后，务必清一次构建产物：
idf.py fullclean
Remove-Item .\sdkconfig
idf.py set-target esp32s3
```

**为什么切期必须 fullclean**：`build/CMakeCache.txt` 里缓存了芯片型号（`IDF_TARGET`），`sdkconfig` 里缓存了所有 Kconfig 选项，二者都不会随 `git checkout` 自动失效。留着会出现"编译时用的还是上一期的配置"这种诡异现象。

## 关于 `config.h` 和 `config.example.h`

- `main/config.h` —— **你自己的真实配置，不入库**（`.gitignore` 已排除）
- `main/config.example.h` —— **公开的模板**，里面引脚、地址都是示例值

做法：**clone 后复制 `config.example.h` 为 `config.h`，再改里面的值。** 涉及 Wi-Fi 密码、大模型 API Key 之类的凭据只写进 `config.h`，永远不会被 commit。后面几期引入网络后，这条尤其重要——一旦真实凭据被 commit，它会**永久留在 git 历史里**（`git log -p`、`git checkout stageN` 都能翻出来），届时只能靠 `git filter-repo` 重写历史 + 立即轮换密钥来补救。

每次提交前扫一眼暂存区是个好习惯：

```bash
git diff --cached | Select-String "sk-|password|ssid|api_key"
```

## 已知坑（都是本项目实际踩过的）

1. **改了 `sdkconfig.defaults` 里的 `CONFIG_IDF_TARGET` 却还是不生效**
   IDF 只在 `sdkconfig` 不存在时才读 `sdkconfig.defaults`；而且 target 的判定优先级是 `$ENV{IDF_TARGET}` → `build/CMakeCache.txt` → `sdkconfig` → `sdkconfig.defaults`。**结论：换芯片用 `idf.py set-target`，别指望 defaults。**

2. **`partitions.csv` 报 `UnicodeDecodeError` 或分区表不生效**
   ① 文件必须 **UTF-8（建议纯 ASCII）**，用 GBK 存过不了 `gen_esp32part.py` 的解码；② 必须是标准 CSV（逗号分隔、不加引号）；③ Kconfig 符号是 `CONFIG_PARTITION_TABLE_CUSTOM_FILENAME`，**不是 `CUSTON`**。

3. **`rmt_transmit()` 第 4 个参数是「字节数」不是「元素个数」**
   IDF 5.5 签名：`rmt_transmit(channel, encoder, payload, size_t payload_bytes, config)`。想发 24 个 `rmt_symbol_word_t`（每个 4 字节）就传 `sizeof(symbols)`，传 `24` 只会发出 18 bit，WS2812 收到半截帧直接丢弃 → 灯死活不亮。

4. **上电后 RGB 灯自己乱亮 1~2 秒**
   不是 bug。WS2812 上电态未定义，且 RMT `enable` 之前 GPIO48 悬空，灯珠会自行锁存噪声；带 Octal PSRAM 的板子从复位到 `app_main` 恰好要 1~2 秒。想根治：驱动初始化后立刻发一帧全 0，并给 DIN 加 10 kΩ 下拉。

5. **数组用变量当下标之前，必须先做边界检查**
   `stage2` 曾经因为 `SetChatMessage` 折出的行数超过屏幕行数 `kRows`，`lines_[line_count_]` 越界写把 `line_count_` 自己覆盖成了字符串内容，随后拿文本字节当地址用 → `Guru Meditation Error: StoreProhibited` → **无限重启**。`0x952f80d5` 这种非对齐地址 + 寄存器里出现 ASCII，就是典型的"索引变量被踩"信号。

6. **Wi-Fi 第一次连接失败，通常不是密码错**
   `stage3` 实测：上电第一次 `esp_wifi_connect()` 花了 2.4 秒后报断开，期间日志里**完全没有** `state: init -> auth -> assoc -> run` 这三行 → 说明卡在**扫描阶段就没找到 AP**（reason 201 `NO_AP_FOUND`）；而 5 秒后的重试只花 110 毫秒，因为 `new:<1,0>` 表示它直接锁定信道 1、跳过了全信道扫描。手机热点尤其容易这样（锁屏省电会降低 Beacon 广播）。**排查第一步是把 `wifi_event_sta_disconnected_t::reason` 打进日志**，光看"连接断开"四个字定不了位。另外默认开的 modem sleep（日志里 `pm start, type: 1`）会让连接期间 80%+ 时间在休眠，表现为"跑着跑着莫名掉线"，教学/调试阶段建议 `esp_wifi_set_ps(WIFI_PS_NONE)`。

## 目录结构

```
.
├── CMakeLists.txt              工程级：定义项目名、包含 IDF 构建
├── partitions.csv              自定义分区表（factory 6MB）
├── sdkconfig.defaults          首次构建的种子配置（target / PSRAM / Flash / 日志）
├── sdkconfig                   生成物，不入库
├── main
│   ├── CMakeLists.txt          组件注册：SRCS 列出所有参与编译的 .cc
│   ├── config.h                你的真实配置，不入库
│   ├── config.example.h        公开模板
│   ├── led.cc / led.h          WS2812（RMT）
│   ├── ssd1306.cc / ssd1306.h  OLED 寄存器级驱动 + 显存
│   ├── font.cc / font.h        8×16 字模
│   ├── display.cc / display.h  显示上层（状态栏 / 通知 / 聊天）
│   ├── wifi.cc / wifi.h        Wi-Fi 站点模式封装（事件回调 + 自动重连）
│   └── main.cc                 app_main 入口
└── tools
    └── gen_font8x16.ps1        字模生成脚本（改字体后重新生成 font.cc）
```

> **注意 `main/CMakeLists.txt` 的 `SRCS`**：新加 `.cc` 文件必须手动列进去，否则不会参与编译（`main/CMakeLists.txt` 里的 `SRCS` 不会自动扫描目录）。
