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
| `stage4` | 状态机与按键：`DeviceStateMachine` 合法迁移表 + 监听器；`Button` 20ms 轮询消抖、短按/长按/按下/松开四种事件；BOOT 键点按切换说话状态，TALK 键按住说话 | `main/device_state_machine.*` `main/device_state.h` `main/button.*` |
| `stage5` | 音频采集：`AudioCodec` 抽象基类（模板方法：公开 `InputData` 统一入口、派生类只实现 `Read`）+ `NoAudioCodecSimplex` 直驱 I2S0 读 INMP441；处于 listening 时屏上显示实时 MIC 电平条 | `main/audio_codec.*` `main/no_audio_codec.*` |
| `stage6` | 音频服务：`AudioService` 三任务流水线（采集 / 处理 / 播放）、`std::deque` 帧队列 + 条件变量背压；响度用 attack/release 包络平滑，主循环只读 `GetLevel()`；音频代码迁入 `main/audio/` | `main/audio/*` |
| `stage7` | 语音活动检测：`VadProcessor` 按每帧平均幅度做能量判决，3 帧起说 / 50 帧停说（双门限滞回）；回调经 `AudioService` 再透传到应用层；说话时 LED 绿灯变亮、屏幕标题出现 `<<TALK`，说完一句自动 `listening → idle` | `main/audio/vad_processor.*` |
| `stage8` | *规划中* | — |

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

7. **按键"按下了但屏幕没反应"，多数不是按键坏了**
   `stage4` 碰到两个观测陷阱：① `kDebounceUs = 40ms` 但轮询周期是 20ms，判定用的是严格大于 `>`，`0→20→40→60` 里 `40 > 40` 不成立，所以**实际消抖窗口是 60ms**；也就是说窗口被采样周期量化了，把常量改成比 20ms 小的值（如 10）**不会生效**，想改就得让常量是轮询周期的整数倍。② 主循环一轮是 `60ms + 940ms = 1s`，`SetStatus` 只在那里调，所以**状态机是毫秒级、屏幕是 1Hz**；快速短按（<1s）的那次 `listening` 会被显示循环整个跳过，看起来就像"按了没反应"。真相在 `state: Transitioned from X to Y` 日志里（毫秒级打印），不在屏上。想要屏也跟得上，就把刷新放进 `AddStateChangeListener` 回调（一次 I2C 刷新约 12ms，但不能在里面阻塞）。

8. **单击（click）必然在"松开"时触发，不是代码写反了**
   按下那一刻无法区分"单击"和"长按"，只能先记下 `press_start_us_`；按住满 `long_press_time_`（默认 1000ms）就发 `on_long_press_` 并置 `long_press_fired_`，否则要等到松开、确认 `!long_press_fired_` 才发 `on_click_`。所以 BOOT 键（注册的是 `OnClick`）表现为"松开才变化"，TALK 键（注册的是 `OnPressDown/Up`）才是"按下就变"——两颗键手感不同是设计使然。

9. **I2S 麦克风读回来全是 0，先分清是"没时钟"还是"没数据"**
   `stage5` 调试实录：先打印 32 位原始值，得到 `bytes=960 raw: 0 0 0 0 0 0`——`bytes=960` 恰好是 240 帧 × 4 字节，说明**通道使能、DMA 搬运、时钟输出全部正常**，问题只可能在那根数据线上。验证法：把 DIN 引脚临时当普通 GPIO，分别开内部上拉/下拉各读一次——`上拉读=1 下拉读=0` 就是**悬空**（没人驱动）。INMP441 上最常见三处：① 构造参数顺序写错（`(sample_rate, sck, ws, din)` 被传成 `(…, ws, sck, din)`，WS/SCK 互换后时序全废）；② **L/R 脚悬空** → 数据脚 SD 进高阻 → 读全 0（接 GND 才是左声道）；③ L/R 接 3.3V 时数据只在右声道，而代码读的是 `I2S_STD_SLOT_LEFT` → 也是全 0。另一个判断技巧：因为 `InputData()` 读失败会直接 `return false` 而屏幕不刷新，所以**只要屏上能显示出 "Mic: 0"，就证明读取是成功的**，不用再怀疑通道使能。最后：INMP441 是 24 位数据，`raw[i] >> 12` 只砍 12 位会留下 16 倍余量导致饱和，确认对齐方式后应为 `>> 16`（左对齐）或 `>> 8`（右对齐）。

10. **把采集搬进独立任务后，要想清楚谁阻塞、谁丢帧**
    `stage6` 的 `AudioService` 是三级流水线：input（优先级 8，阻塞式读 I2S）→ 帧队列（最多 4 帧 ≈ 60ms）→ process（优先级 2，算 attack/release 包络）→ output（优先级 4，目前空转，留给后续接扬声器）。三个后果值得记住：① 队列满时 input 任务**阻塞等待**（反压），而 I2S 硬件仍在往 DMA 里写，6×15ms=90ms 之后就开始丢东西，所以采集任务优先级必须高于处理任务，宁可卡处理也不能让 DMA 溢出；② `EnableVoiceProcessing(false)` 直接调 `codec_->EnableInput(false)` → `i2s_channel_disable()`，而此时 input 任务可能正阻塞在 `i2s_channel_read()` 里（超时 200ms），二者存在竞争，目前靠 read 超时兜底——更稳的做法是先置标志、等任务回到等待点再动硬件；③ 启用后先丢 `AUDIO_INPUT_WARMUP_FRAMES` 帧（预热）再入队，用于滤掉通道刚启动那一瞬的无效数据。另外：把 `std::condition_variable` 用在 ESP-IDF 是可行的（底层是 pthread），但所有等待都必须带 `service_stopped_` 断言，否则析构时任务永远醒不来。

11. **语音检测（VAD）的门限是「帧数」，不是「时间」，而且门限值不能照抄**
    `stage7` 的 `VadProcessor` 用 `VAD_START_FRAMES = 3` / `VAD_STOP_FRAMES = 50`，一帧是 `AUDIO_CODEC_DMA_FRAME_NUM = 240` 点 ÷ 16 kHz ≈ 15 ms，所以**起说延迟约 45 ms、停说判定约 750 ms**；改帧长就等于悄悄改了这两段时间。两个方向都要留滞回：只设一个门限会让电平在门限附近抖动时疯狂重复 `说话/结束说话`。另外 `VAD_ENERGY_THRESHOLD = 1200` 是**绝对值**（平均幅度，满量程 32767 的 3.7%），换麦克风、改增益、甚至换个房间背景噪声变了都得重新标定——先看日志里静止时的 `level`，把门限定在噪声的 2~3 倍。还有一点容易踩：`Feed()` 顺便把算出来的电平返回给响度条，判决和取电平耦合在同一个函数里，改判决逻辑时别把返回值改坏。最后，VAD 回调跑在 process 任务里，**里面不能做阻塞操作**（刷 I2C 屏 ≈ 12 ms、`vTaskDelay`、等队列）；`main` 现在的做法是用 `std::atomic<bool>` 置标志、回到主循环再 `exchange(false)` 处理，这是对的模式。

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
│   ├── device_state_machine.*  状态机（合法迁移表 + 状态变化监听器）
│   ├── device_state.h          状态枚举
│   ├── button.cc / button.h    按键（20ms 轮询消抖 + 按下/松开/单击/长按）
│   ├── audio/
│   │   ├── audio_codec.cc / .h  音频输入抽象基类（InputData → Read 模板方法）
│   │   ├── no_audio_codec.*     不用编解码芯片、直驱 I2S 的派生实现（INMP441）
│   │   ├── audio_service.*      三级流水线：采集 / 处理 / 播放 + 响度包络
│   │   └── vad_processor.*      语音活动检测（能量门限 + 起停帧计数滞回）
│   └── main.cc                 app_main 入口
└── tools
    └── gen_font8x16.ps1        字模生成脚本（改字体后重新生成 font.cc）
```

> **注意 `main/CMakeLists.txt` 的 `SRCS`**：新加 `.cc` 文件必须手动列进去，否则不会参与编译（`main/CMakeLists.txt` 里的 `SRCS` 不会自动扫描目录）。
