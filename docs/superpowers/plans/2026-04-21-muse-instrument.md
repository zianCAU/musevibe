# 随身乐器「缪斯 MUSE」Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 Otto-Robot ESP32-S3 摄像头版上实现随身乐器功能：4个电容触摸和弦按钮、吉他音效播放、AI缪斯角色对话、摄像头景色填词+屏幕歌词弹唱。

**Architecture:** 在现有 `OttoRobot` 板级代码基础上新增4个独立模块（`TouchChordController`、`ChordPlayer`、`MuseDisplay`、`MuseMcpTools`），替换 `OttoController`（舵机）并换掉 `OttoEmojiDisplay`（表情显示）。所有模块通过全局单例互相调用，不引入新的通信机制。

**Tech Stack:** ESP-IDF 5.x, FreeRTOS, LVGL 9.x, ESP32-S3 touch_pad driver, I2S audio, cJSON, MCP tool framework (已有), EspVideo (已有)

---

## 文件变更总览

| 文件 | 操作 | 职责 |
|---|---|---|
| `main/boards/otto-robot/touch_chord_controller.h` | 新增 | 触摸按钮驱动接口 |
| `main/boards/otto-robot/touch_chord_controller.cc` | 新增 | touch_pad 初始化 + FreeRTOS 轮询任务 |
| `main/boards/otto-robot/chord_player.h` | 新增 | 和弦播放器接口 |
| `main/boards/otto-robot/chord_player.cc` | 新增 | PCM 数据混音输出到 AudioCodec |
| `main/boards/otto-robot/assets/chords/chord_C_arp.raw` | 新增 | C和弦分解弦 16kHz/16bit/mono PCM |
| `main/boards/otto-robot/assets/chords/chord_Am_arp.raw` | 新增 | Am和弦分解弦 PCM |
| `main/boards/otto-robot/assets/chords/chord_F_arp.raw` | 新增 | F和弦分解弦 PCM |
| `main/boards/otto-robot/assets/chords/chord_G_arp.raw` | 新增 | G和弦分解弦 PCM |
| `main/boards/otto-robot/assets/chords/chord_C_strum.raw` | 新增 | C和弦扫弦 PCM |
| `main/boards/otto-robot/assets/chords/chord_Am_strum.raw` | 新增 | Am和弦扫弦 PCM |
| `main/boards/otto-robot/assets/chords/chord_F_strum.raw` | 新增 | F和弦扫弦 PCM |
| `main/boards/otto-robot/assets/chords/chord_G_strum.raw` | 新增 | G和弦扫弦 PCM |
| `main/boards/otto-robot/muse_display.h` | 新增 | MuseDisplay 接口（两个界面） |
| `main/boards/otto-robot/muse_display.cc` | 新增 | LVGL 主界面①+歌词界面② |
| `main/boards/otto-robot/muse_mcp_tools.cc` | 新增 | 6个 MCP 工具注册 |
| `main/boards/otto-robot/otto_robot.cc` | 修改 | 替换 OttoEmojiDisplay→MuseDisplay，替换 OttoController→TouchChordController |
| `main/boards/otto-robot/config.h` | 修改 | 添加触摸针脚宏，移除舵机针脚 |
| `main/CMakeLists.txt` | 修改 | 添加新 .cc 文件，添加 EMBED_FILES 音频 |

---

## Task 1: 触摸按钮识别（TouchChordController）

**Files:**
- Create: `main/boards/otto-robot/touch_chord_controller.h`
- Create: `main/boards/otto-robot/touch_chord_controller.cc`
- Modify: `main/boards/otto-robot/config.h`
- Modify: `main/boards/otto-robot/otto_robot.cc`
- Modify: `main/CMakeLists.txt`

- [ ] **Step 1: 在 config.h 添加触摸针脚宏**

在 `main/boards/otto-robot/config.h` 末尾（`#endif` 之前）添加：

```cpp
// 触摸按钮针脚（复用舵机插针 Signal 脚）
// CAMERA_VERSION_CONFIG 下：left_hand=4, left_leg=5, left_foot=6, right_hand=7
#define TOUCH_BTN_C    TOUCH_PAD_NUM4   // GPIO 4
#define TOUCH_BTN_AM   TOUCH_PAD_NUM5   // GPIO 5
#define TOUCH_BTN_F    TOUCH_PAD_NUM6   // GPIO 6
#define TOUCH_BTN_G    TOUCH_PAD_NUM7   // GPIO 7

// 触摸灵敏度阈值（触摸值下降超过此比例时触发，0.2 = 20%）
#define TOUCH_THRESHOLD_RATIO 0.2f
```

- [ ] **Step 2: 创建 touch_chord_controller.h**

```cpp
// main/boards/otto-robot/touch_chord_controller.h
#pragma once
#include <functional>
#include <string>

// 和弦名称枚举
enum class Chord { C, Am, F, G };

// 触摸事件回调：按下时触发，传入和弦名
using ChordTouchCallback = std::function<void(Chord chord)>;

class TouchChordController {
public:
    // callback: 每次触摸确认时调用
    explicit TouchChordController(ChordTouchCallback callback);
    ~TouchChordController();

    // 启动后台轮询任务（FreeRTOS task）
    void Start();

    // 将 Chord 枚举转为字符串，供日志/MCP 使用
    static const char* ChordName(Chord chord);

private:
    ChordTouchCallback callback_;
    TaskHandle_t task_handle_ = nullptr;

    // 校准基线（开机时采集10次均值）
    uint32_t baseline_[4] = {0};

    void Calibrate();
    static void PollTask(void* arg);
};
```

- [ ] **Step 3: 创建 touch_chord_controller.cc**

```cpp
// main/boards/otto-robot/touch_chord_controller.cc
#include "touch_chord_controller.h"
#include "config.h"

#include <driver/touch_pad.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define TAG "TouchChord"

// GPIO→Chord 的映射表（顺序与 Chord 枚举一致）
static const touch_pad_t kPads[4] = {
    TOUCH_BTN_C,   // Chord::C
    TOUCH_BTN_AM,  // Chord::Am
    TOUCH_BTN_F,   // Chord::F
    TOUCH_BTN_G,   // Chord::G
};

const char* TouchChordController::ChordName(Chord chord) {
    switch (chord) {
        case Chord::C:  return "C";
        case Chord::Am: return "Am";
        case Chord::F:  return "F";
        case Chord::G:  return "G";
    }
    return "?";
}

TouchChordController::TouchChordController(ChordTouchCallback callback)
    : callback_(callback) {
    touch_pad_init();
    touch_pad_set_fsm_mode(TOUCH_FSM_MODE_TIMER);
    touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_1V);
    for (int i = 0; i < 4; i++) {
        touch_pad_config(kPads[i]);
    }
    touch_pad_filter_start(10);  // 10ms 滤波
    Calibrate();
}

TouchChordController::~TouchChordController() {
    if (task_handle_) {
        vTaskDelete(task_handle_);
        task_handle_ = nullptr;
    }
    touch_pad_deinit();
}

void TouchChordController::Calibrate() {
    // 采集 10 次均值作为基线（静止状态）
    vTaskDelay(pdMS_TO_TICKS(100));
    for (int i = 0; i < 4; i++) {
        uint32_t sum = 0;
        for (int n = 0; n < 10; n++) {
            uint32_t val = 0;
            touch_pad_read_filtered(kPads[i], &val);
            sum += val;
            vTaskDelay(pdMS_TO_TICKS(5));
        }
        baseline_[i] = sum / 10;
        ESP_LOGI(TAG, "Pad[%d] baseline=%lu", i, baseline_[i]);
    }
}

void TouchChordController::Start() {
    xTaskCreate(PollTask, "touch_poll", 2048, this,
                configMAX_PRIORITIES - 2, &task_handle_);
}

void TouchChordController::PollTask(void* arg) {
    auto* self = static_cast<TouchChordController*>(arg);
    // 防抖：需要连续 2 次触发才算有效
    int debounce[4] = {0};
    bool pressed[4] = {false};

    while (true) {
        for (int i = 0; i < 4; i++) {
            uint32_t val = 0;
            touch_pad_read_filtered(kPads[i], &val);
            // 触摸时电容值下降
            float drop_ratio = (float)(self->baseline_[i] - val) / (float)self->baseline_[i];
            bool touching = (drop_ratio > TOUCH_THRESHOLD_RATIO);

            if (touching) {
                debounce[i]++;
                if (debounce[i] == 2 && !pressed[i]) {
                    pressed[i] = true;
                    Chord chord = static_cast<Chord>(i);
                    ESP_LOGI(TAG, "Touch: %s (drop=%.1f%%)",
                             TouchChordController::ChordName(chord), drop_ratio * 100);
                    self->callback_(chord);
                }
            } else {
                debounce[i] = 0;
                pressed[i] = false;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(20));  // 20ms 轮询，50Hz
    }
}
```

- [ ] **Step 4: 在 CMakeLists.txt 的 otto-robot 段添加新文件**

找到 `main/CMakeLists.txt` 第 585 行 `elseif(CONFIG_BOARD_TYPE_OTTO_ROBOT)` 块，添加：

```cmake
elseif(CONFIG_BOARD_TYPE_OTTO_ROBOT)
    set(BOARD_TYPE "otto-robot")
    set(BUILTIN_TEXT_FONT font_puhui_16_4)
    set(BUILTIN_ICON_FONT font_awesome_16_4)
    set(DEFAULT_EMOJI_COLLECTION otto-gif)
    list(APPEND SOURCES
        "boards/otto-robot/otto_robot.cc"
        "boards/otto-robot/otto_emoji_display.cc"
        "boards/otto-robot/otto_icon_font.c"
        "boards/otto-robot/otto_movements.cc"
        "boards/otto-robot/touch_chord_controller.cc"
    )
```

> 注意：原来 otto-robot 的 SOURCES 列表在哪里需要先确认（`grep -n "otto_robot.cc" main/CMakeLists.txt`），把 touch_chord_controller.cc 加入同一个 list 即可。

- [ ] **Step 5: 在 otto_robot.cc 中集成 TouchChordController（仅日志输出，未接音频）**

在 `main/boards/otto-robot/otto_robot.cc` 顶部 include 区域添加：

```cpp
#include "touch_chord_controller.h"
```

在 `OttoRobot` 类 private 成员区添加：

```cpp
TouchChordController* touch_controller_ = nullptr;
```

在 `InitializeOttoController()` 函数之后添加新方法：

```cpp
void InitializeTouchController() {
    touch_controller_ = new TouchChordController([](Chord chord) {
        ESP_LOGI("OttoRobot", "和弦触摸: %s", TouchChordController::ChordName(chord));
        // M2 完成后在此处调用 chord_player_->Play(chord)
    });
    touch_controller_->Start();
}
```

在构造函数 `OttoRobot()` 的末尾（`GetBacklight()->RestoreBrightness()` 之前）调用：

```cpp
InitializeTouchController();
```

同时将构造函数中原有的 `InitializeOttoController();` 一行**注释掉**（不再需要舵机）：

```cpp
// InitializeOttoController();  // 已替换为触摸和弦控制
```

- [ ] **Step 6: 编译验证**

```bash
cd /path/to/heikesongmusic
idf.py build 2>&1 | tail -20
```

期望输出：`Project build complete.`（无 error，warning 可忽略）

- [ ] **Step 7: 烧录并验证触摸**

```bash
idf.py flash monitor
```

用手指触碰接在 GPIO 4/5/6/7 的铜箔，串口应看到：

```
I (xxxx) TouchChord: Pad[0] baseline=3200
I (xxxx) TouchChord: Pad[1] baseline=3150
I (xxxx) TouchChord: Pad[2] baseline=3210
I (xxxx) TouchChord: Pad[3] baseline=3180
I (xxxx) TouchChord: Touch: C (drop=35.2%)
I (xxxx) OttoRobot: 和弦触摸: C
```

若 drop 百分比太小（<5%），说明导线太长或铜箔太小；若频繁误触，将 `TOUCH_THRESHOLD_RATIO` 从 0.2 调高到 0.3。

- [ ] **Step 8: Commit**

```bash
git add main/boards/otto-robot/touch_chord_controller.h \
        main/boards/otto-robot/touch_chord_controller.cc \
        main/boards/otto-robot/config.h \
        main/boards/otto-robot/otto_robot.cc \
        main/CMakeLists.txt
git commit -m "feat(M1): add TouchChordController for 4-pad capacitive touch chords"
```

---

## Task 2: 和弦音频播放（ChordPlayer）

**Files:**
- Create: `main/boards/otto-robot/chord_player.h`
- Create: `main/boards/otto-robot/chord_player.cc`
- Create: `main/boards/otto-robot/assets/chords/gen_chords.py`（生成占位音频的脚本）
- Create: `main/boards/otto-robot/assets/chords/chord_C_arp.raw` 等 8 个 .raw 文件
- Modify: `main/CMakeLists.txt`（添加 chord_player.cc + EMBED_FILES）
- Modify: `main/boards/otto-robot/otto_robot.cc`（接入 ChordPlayer）

- [ ] **Step 1: 生成占位 PCM 音频文件**

创建 `main/boards/otto-robot/assets/chords/gen_chords.py`：

```python
#!/usr/bin/env python3
"""
生成占位和弦 PCM 文件（16kHz / 16bit / mono）
后续替换为真实吉他采样。
和弦频率（以 A=440Hz 为基准）：
  C  : 262, 330, 392 Hz (C4-E4-G4)
  Am : 220, 262, 330 Hz (A3-C4-E4)
  F  : 175, 220, 262 Hz (F3-A3-C4)
  G  : 196, 247, 294 Hz (G3-B3-D4)
"""
import struct, math, os

SAMPLE_RATE = 16000
DURATION_ARR = 1.2   # 分解弦总时长（秒）
DURATION_STR = 0.6   # 扫弦总时长（秒）
NOTE_DURATION = 0.4  # 分解弦每音时长（秒）
AMPLITUDE = 16000

CHORDS = {
    'C':  [262, 330, 392],
    'Am': [220, 262, 330],
    'F':  [175, 220, 262],
    'G':  [196, 247, 294],
}

def sine_wave(freq, duration, sample_rate, amplitude, decay=0.8):
    n = int(sample_rate * duration)
    samples = []
    for i in range(n):
        t = i / sample_rate
        env = math.exp(-decay * t)  # 指数衰减包络
        s = int(amplitude * env * math.sin(2 * math.pi * freq * t))
        s = max(-32768, min(32767, s))
        samples.append(s)
    return samples

def mix(a, b):
    return [max(-32768, min(32767, x + y)) for x, y in zip(a, b)]

os.makedirs(os.path.dirname(__file__), exist_ok=True)

for name, freqs in CHORDS.items():
    # 分解弦：三个音依次播放，各 NOTE_DURATION 秒
    arp_samples = []
    for freq in freqs:
        arp_samples += sine_wave(freq, NOTE_DURATION, SAMPLE_RATE, AMPLITUDE)
    fn = os.path.join(os.path.dirname(__file__), f'chord_{name}_arp.raw')
    with open(fn, 'wb') as f:
        for s in arp_samples:
            f.write(struct.pack('<h', s))
    print(f'Written {fn} ({len(arp_samples)} samples)')

    # 扫弦：三个音同时叠加，DURATION_STR 秒
    total = int(SAMPLE_RATE * DURATION_STR)
    strum = [0] * total
    for freq in freqs:
        wave = sine_wave(freq, DURATION_STR, SAMPLE_RATE, AMPLITUDE // len(freqs))
        strum = mix(strum, wave)
    fn = os.path.join(os.path.dirname(__file__), f'chord_{name}_strum.raw')
    with open(fn, 'wb') as f:
        for s in strum:
            f.write(struct.pack('<h', s))
    print(f'Written {fn} ({len(strum)} samples)')
```

运行脚本生成 8 个 .raw 文件：

```bash
python3 main/boards/otto-robot/assets/chords/gen_chords.py
ls -lh main/boards/otto-robot/assets/chords/*.raw
```

期望输出：8 个文件，每个 10–40KB。

- [ ] **Step 2: 创建 chord_player.h**

```cpp
// main/boards/otto-robot/chord_player.h
#pragma once
#include "touch_chord_controller.h"  // for Chord enum
#include "audio/audio_codec.h"

enum class PlayStyle { Arpeggio, Strum };

class ChordPlayer {
public:
    explicit ChordPlayer(AudioCodec* codec);

    // 播放指定和弦（在独立任务中异步执行，不阻塞调用方）
    void Play(Chord chord, PlayStyle style = PlayStyle::Arpeggio);

    void SetStyle(PlayStyle style);
    void SetCapo(int semitones);  // -3 ~ +3，用于移调（M6 实现）

    PlayStyle style() const { return style_; }
    int capo() const { return capo_; }

private:
    AudioCodec* codec_;
    PlayStyle style_ = PlayStyle::Arpeggio;
    int capo_ = 0;

    struct PlayRequest {
        Chord chord;
        PlayStyle style;
    };

    QueueHandle_t queue_ = nullptr;
    TaskHandle_t task_ = nullptr;

    static void PlayTask(void* arg);
    void OutputPcm(const uint8_t* data, size_t size_bytes);
};
```

- [ ] **Step 3: 创建 chord_player.cc**

```cpp
// main/boards/otto-robot/chord_player.cc
#include "chord_player.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <vector>
#include <cstring>

#define TAG "ChordPlayer"

// EMBED_FILES 自动生成的符号（CMakeLists 中声明）
extern const uint8_t chord_C_arp_raw_start[]   asm("_binary_chord_C_arp_raw_start");
extern const uint8_t chord_C_arp_raw_end[]     asm("_binary_chord_C_arp_raw_end");
extern const uint8_t chord_Am_arp_raw_start[]  asm("_binary_chord_Am_arp_raw_start");
extern const uint8_t chord_Am_arp_raw_end[]    asm("_binary_chord_Am_arp_raw_end");
extern const uint8_t chord_F_arp_raw_start[]   asm("_binary_chord_F_arp_raw_start");
extern const uint8_t chord_F_arp_raw_end[]     asm("_binary_chord_F_arp_raw_end");
extern const uint8_t chord_G_arp_raw_start[]   asm("_binary_chord_G_arp_raw_start");
extern const uint8_t chord_G_arp_raw_end[]     asm("_binary_chord_G_arp_raw_end");
extern const uint8_t chord_C_strum_raw_start[]  asm("_binary_chord_C_strum_raw_start");
extern const uint8_t chord_C_strum_raw_end[]    asm("_binary_chord_C_strum_raw_end");
extern const uint8_t chord_Am_strum_raw_start[] asm("_binary_chord_Am_strum_raw_start");
extern const uint8_t chord_Am_strum_raw_end[]   asm("_binary_chord_Am_strum_raw_end");
extern const uint8_t chord_F_strum_raw_start[]  asm("_binary_chord_F_strum_raw_start");
extern const uint8_t chord_F_strum_raw_end[]    asm("_binary_chord_F_strum_raw_end");
extern const uint8_t chord_G_strum_raw_start[]  asm("_binary_chord_G_strum_raw_start");
extern const uint8_t chord_G_strum_raw_end[]    asm("_binary_chord_G_strum_raw_end");

struct ChordPcm { const uint8_t* start; const uint8_t* end; };

// [Chord][PlayStyle]: 0=Arpeggio, 1=Strum
static const ChordPcm kPcmTable[4][2] = {
    { {chord_C_arp_raw_start,  chord_C_arp_raw_end},  {chord_C_strum_raw_start,  chord_C_strum_raw_end}  },  // C
    { {chord_Am_arp_raw_start, chord_Am_arp_raw_end}, {chord_Am_strum_raw_start, chord_Am_strum_raw_end} },  // Am
    { {chord_F_arp_raw_start,  chord_F_arp_raw_end},  {chord_F_strum_raw_start,  chord_F_strum_raw_end}  },  // F
    { {chord_G_arp_raw_start,  chord_G_arp_raw_end},  {chord_G_strum_raw_start,  chord_G_strum_raw_end}  },  // G
};

ChordPlayer::ChordPlayer(AudioCodec* codec) : codec_(codec) {
    queue_ = xQueueCreate(4, sizeof(PlayRequest));
    xTaskCreate(PlayTask, "chord_play", 4096, this,
                configMAX_PRIORITIES - 3, &task_);
}

void ChordPlayer::Play(Chord chord, PlayStyle style) {
    PlayRequest req = { chord, style };
    xQueueSend(queue_, &req, 0);  // 非阻塞，丢弃旧请求
}

void ChordPlayer::SetStyle(PlayStyle style) { style_ = style; }
void ChordPlayer::SetCapo(int semitones) { capo_ = semitones; }

void ChordPlayer::OutputPcm(const uint8_t* data, size_t size_bytes) {
    // 每次写入 DMA_FRAME_NUM 个采样（240 samples = 480 bytes）
    const size_t kChunk = 240;
    const int16_t* samples = reinterpret_cast<const int16_t*>(data);
    size_t total = size_bytes / 2;
    std::vector<int16_t> buf(kChunk, 0);

    for (size_t offset = 0; offset < total; offset += kChunk) {
        size_t n = std::min(kChunk, total - offset);
        memcpy(buf.data(), samples + offset, n * 2);
        if (n < kChunk) memset(buf.data() + n, 0, (kChunk - n) * 2);
        codec_->OutputData(buf);
    }
}

void ChordPlayer::PlayTask(void* arg) {
    auto* self = static_cast<ChordPlayer*>(arg);
    PlayRequest req;
    while (true) {
        if (xQueueReceive(self->queue_, &req, portMAX_DELAY) == pdTRUE) {
            int idx = static_cast<int>(req.chord);
            int style_idx = (req.style == PlayStyle::Strum) ? 1 : 0;
            const ChordPcm& pcm = kPcmTable[idx][style_idx];
            size_t size = pcm.end - pcm.start;
            ESP_LOGI(TAG, "Playing %s %s (%zu bytes)",
                     TouchChordController::ChordName(req.chord),
                     style_idx ? "strum" : "arp", size);
            self->OutputPcm(pcm.start, size);
        }
    }
}
```

- [ ] **Step 4: 在 CMakeLists.txt 中添加 chord_player.cc 和 EMBED_FILES**

找到 `main/CMakeLists.txt` 中 `elseif(CONFIG_BOARD_TYPE_OTTO_ROBOT)` 的 SOURCES list，添加 `chord_player.cc`：

```cmake
list(APPEND SOURCES
    ...
    "boards/otto-robot/touch_chord_controller.cc"
    "boards/otto-robot/chord_player.cc"
)
```

在 `idf_component_register(...)` 的 `EMBED_FILES` 行中追加音频文件：

```cmake
idf_component_register(SRCS ${SOURCES}
    EMBED_FILES ${LANG_SOUNDS} ${COMMON_SOUNDS}
        boards/otto-robot/assets/chords/chord_C_arp.raw
        boards/otto-robot/assets/chords/chord_Am_arp.raw
        boards/otto-robot/assets/chords/chord_F_arp.raw
        boards/otto-robot/assets/chords/chord_G_arp.raw
        boards/otto-robot/assets/chords/chord_C_strum.raw
        boards/otto-robot/assets/chords/chord_Am_strum.raw
        boards/otto-robot/assets/chords/chord_F_strum.raw
        boards/otto-robot/assets/chords/chord_G_strum.raw
    INCLUDE_DIRS ${INCLUDE_DIRS}
    ...
)
```

- [ ] **Step 5: 在 otto_robot.cc 中接入 ChordPlayer**

添加 include：

```cpp
#include "chord_player.h"
```

在 `OttoRobot` 类 private 成员添加：

```cpp
ChordPlayer* chord_player_ = nullptr;
```

添加初始化方法：

```cpp
void InitializeChordPlayer() {
    chord_player_ = new ChordPlayer(audio_codec_);
}
```

在构造函数中 `InitializeTouchController()` 之前调用：

```cpp
InitializeChordPlayer();
```

将 `InitializeTouchController()` 中的 lambda 改为实际播放：

```cpp
void InitializeTouchController() {
    touch_controller_ = new TouchChordController([this](Chord chord) {
        ESP_LOGI("OttoRobot", "和弦触摸: %s", TouchChordController::ChordName(chord));
        chord_player_->Play(chord, chord_player_->style());
    });
    touch_controller_->Start();
}
```

- [ ] **Step 6: 编译并烧录测试**

```bash
idf.py build && idf.py flash monitor
```

触摸 GPIO 4 对应的铜箔，喇叭应发出 C 和弦音效。串口输出：

```
I (xxxx) TouchChord: Touch: C (drop=32.1%)
I (xxxx) ChordPlayer: Playing C arp (38400 bytes)
```

若无声音，检查：`idf.py menuconfig` → Audio → output volume 是否 > 0。

- [ ] **Step 7: Commit**

```bash
git add main/boards/otto-robot/chord_player.h \
        main/boards/otto-robot/chord_player.cc \
        main/boards/otto-robot/assets/chords/ \
        main/CMakeLists.txt \
        main/boards/otto-robot/otto_robot.cc
git commit -m "feat(M2): add ChordPlayer with embedded PCM audio for 4 chords"
```

---

## Task 3: MuseDisplay 主界面UI（界面①）

**Files:**
- Create: `main/boards/otto-robot/muse_display.h`
- Create: `main/boards/otto-robot/muse_display.cc`
- Modify: `main/boards/otto-robot/otto_robot.cc`（替换 OttoEmojiDisplay → MuseDisplay）
- Modify: `main/CMakeLists.txt`

- [ ] **Step 1: 创建 muse_display.h**

```cpp
// main/boards/otto-robot/muse_display.h
#pragma once
#include "display/lcd_display.h"
#include "touch_chord_controller.h"  // for Chord enum
#include <string>
#include <vector>

// 主界面：左角色 + 右对话 + 右下和弦按钮
// 歌词界面：全屏逐行滚动
class MuseDisplay : public SpiLcdDisplay {
public:
    MuseDisplay(esp_lcd_panel_io_handle_t panel_io,
                esp_lcd_panel_handle_t panel,
                int width, int height,
                int offset_x, int offset_y,
                bool mirror_x, bool mirror_y, bool swap_xy);
    virtual ~MuseDisplay() = default;

    // 覆盖父类接口：对话气泡
    virtual void SetChatMessage(const char* role, const char* content) override;
    virtual void ClearChatMessages() override;

    // 和弦按钮高亮（触摸时调用，150ms 后自动恢复）
    void HighlightChord(Chord chord);

    // 切换到歌词弹唱界面
    struct LyricLine { std::string chord; std::string lyric; };
    void ShowLyrics(const std::string& title, const std::vector<LyricLine>& lines);

    // 返回主界面
    void ShowMain();

    // 显示"正在创作..."进度状态
    void SetComposing(bool composing);

private:
    // 主界面 LVGL 对象
    lv_obj_t* main_screen_ = nullptr;
    lv_obj_t* avatar_circle_ = nullptr;
    lv_obj_t* avatar_label_ = nullptr;
    lv_obj_t* name_label_ = nullptr;
    lv_obj_t* chat_scroll_ = nullptr;   // 消息滚动容器
    lv_obj_t* chord_btns_[4] = {};      // C / Am / F / G 按钮
    lv_obj_t* composing_label_ = nullptr;

    // 歌词界面 LVGL 对象
    lv_obj_t* lyrics_screen_ = nullptr;
    lv_obj_t* lyrics_title_ = nullptr;
    lv_obj_t* lyrics_scroll_ = nullptr;
    lv_obj_t* lyrics_chord_bar_[4] = {};

    // 对话消息队列（只保留最新 5 条）
    struct ChatMsg { std::string role; std::string content; };
    std::vector<ChatMsg> messages_;

    // 主界面构建
    void BuildMainScreen();
    void BuildLyricsScreen();
    void RebuildChatBubbles();

    // 高亮计时器
    esp_timer_handle_t highlight_timer_[4] = {};
    lv_obj_t* GetChordBtn(Chord chord);

    // 暗夜霓虹主题色
    static constexpr lv_color_t kBgColor      = LV_COLOR_MAKE(0x06, 0x06, 0x10);
    static constexpr lv_color_t kPurple       = LV_COLOR_MAKE(0x9c, 0x5f, 0xff);
    static constexpr lv_color_t kPurpleLight  = LV_COLOR_MAKE(0xd0, 0xad, 0xff);
    static constexpr lv_color_t kPurpleDark   = LV_COLOR_MAKE(0x3a, 0x1a, 0x6e);
    static constexpr lv_color_t kWhite        = LV_COLOR_MAKE(0xff, 0xff, 0xff);
    static constexpr lv_color_t kGray         = LV_COLOR_MAKE(0x66, 0x66, 0x66);
};
```

- [ ] **Step 2: 创建 muse_display.cc（主界面构建）**

```cpp
// main/boards/otto-robot/muse_display.cc
#include "muse_display.h"
#include <esp_log.h>
#include <esp_timer.h>

#define TAG "MuseDisplay"

// chord 索引 → 名称
static const char* kChordNames[4] = {"C", "Am", "F", "G"};

MuseDisplay::MuseDisplay(esp_lcd_panel_io_handle_t panel_io,
                         esp_lcd_panel_handle_t panel,
                         int width, int height,
                         int offset_x, int offset_y,
                         bool mirror_x, bool mirror_y, bool swap_xy)
    : SpiLcdDisplay(panel_io, panel, width, height,
                    offset_x, offset_y, mirror_x, mirror_y, swap_xy) {
    DisplayLockGuard lock(this);
    // 隐藏父类默认 UI（emoji_box、chat_message_label 等）
    if (emoji_box_) lv_obj_add_flag(emoji_box_, LV_OBJ_FLAG_HIDDEN);
    if (content_)   lv_obj_add_flag(content_,   LV_OBJ_FLAG_HIDDEN);
    // 设置屏幕背景色
    lv_obj_set_style_bg_color(lv_screen_active(), kBgColor, 0);
    lv_obj_set_style_bg_opa(lv_screen_active(), LV_OPA_COVER, 0);

    BuildMainScreen();
    BuildLyricsScreen();

    // 默认显示主界面
    lv_obj_remove_flag(main_screen_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(lyrics_screen_, LV_OBJ_FLAG_HIDDEN);
}

void MuseDisplay::BuildMainScreen() {
    lv_obj_t* scr = lv_screen_active();

    // 主界面容器（240×240，透明背景）
    main_screen_ = lv_obj_create(scr);
    lv_obj_set_size(main_screen_, 240, 240);
    lv_obj_align(main_screen_, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_opa(main_screen_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(main_screen_, 0, 0);
    lv_obj_set_style_pad_all(main_screen_, 0, 0);

    // ── 左栏（88px 宽）：角色 + 名字 ──
    lv_obj_t* left = lv_obj_create(main_screen_);
    lv_obj_set_size(left, 88, 240);
    lv_obj_align(left, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(left, LV_COLOR_MAKE(0x14, 0x08, 0x28), 0);
    lv_obj_set_style_bg_opa(left, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(left, 0, 0);
    lv_obj_set_style_pad_all(left, 0, 0);
    lv_obj_set_flex_flow(left, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(left, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // 头像圆形
    avatar_circle_ = lv_obj_create(left);
    lv_obj_set_size(avatar_circle_, 56, 56);
    lv_obj_set_style_radius(avatar_circle_, 28, 0);
    lv_obj_set_style_bg_color(avatar_circle_, kPurple, 0);
    lv_obj_set_style_bg_opa(avatar_circle_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(avatar_circle_, kPurple, 0);
    lv_obj_set_style_border_width(avatar_circle_, 2, 0);
    lv_obj_set_style_shadow_color(avatar_circle_, kPurple, 0);
    lv_obj_set_style_shadow_width(avatar_circle_, 16, 0);
    lv_obj_set_style_shadow_spread(avatar_circle_, 2, 0);
    lv_obj_set_style_pad_all(avatar_circle_, 0, 0);

    // 头像内音符文字
    avatar_label_ = lv_label_create(avatar_circle_);
    lv_label_set_text(avatar_label_, "♪");
    lv_obj_set_style_text_color(avatar_label_, kWhite, 0);
    lv_obj_set_style_text_font(avatar_label_, &lv_font_montserrat_20, 0);
    lv_obj_center(avatar_label_);

    // 名字标签
    name_label_ = lv_label_create(left);
    lv_label_set_text(name_label_, "MUSE");
    lv_obj_set_style_text_color(name_label_, kPurpleLight, 0);
    lv_obj_set_style_text_font(name_label_, &lv_font_montserrat_10, 0);
    lv_obj_set_style_pad_top(name_label_, 6, 0);

    // ── 右栏分隔线 ──
    lv_obj_t* sep = lv_obj_create(main_screen_);
    lv_obj_set_size(sep, 1, 240);
    lv_obj_align(sep, LV_ALIGN_TOP_LEFT, 88, 0);
    lv_obj_set_style_bg_color(sep, kPurpleDark, 0);
    lv_obj_set_style_bg_opa(sep, LV_OPA_50, 0);
    lv_obj_set_style_border_width(sep, 0, 0);

    // ── 右栏（152px）：上部对话 + 下部和弦按钮 ──
    lv_obj_t* right = lv_obj_create(main_screen_);
    lv_obj_set_size(right, 151, 240);
    lv_obj_align(right, LV_ALIGN_TOP_LEFT, 89, 0);
    lv_obj_set_style_bg_opa(right, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(right, 0, 0);
    lv_obj_set_style_pad_all(right, 0, 0);

    // 对话滚动区（上 180px）
    chat_scroll_ = lv_obj_create(right);
    lv_obj_set_size(chat_scroll_, 151, 176);
    lv_obj_align(chat_scroll_, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_flex_flow(chat_scroll_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(chat_scroll_, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_bg_opa(chat_scroll_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(chat_scroll_, 0, 0);
    lv_obj_set_style_pad_all(chat_scroll_, 4, 0);
    lv_obj_set_style_pad_row(chat_scroll_, 4, 0);

    // 和弦按钮区（下 64px，2×2 网格）
    lv_obj_t* btn_area = lv_obj_create(right);
    lv_obj_set_size(btn_area, 151, 64);
    lv_obj_align(btn_area, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_bg_color(btn_area, LV_COLOR_MAKE(0x00, 0x00, 0x00), 0);
    lv_obj_set_style_bg_opa(btn_area, LV_OPA_50, 0);
    lv_obj_set_style_border_color(btn_area, kPurpleDark, 0);
    lv_obj_set_style_border_width(btn_area, 0, 0);
    lv_obj_set_style_border_side(btn_area, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_pad_all(btn_area, 6, 0);
    lv_obj_set_style_pad_gap(btn_area, 5, 0);
    lv_obj_set_flex_flow(btn_area, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(btn_area, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    for (int i = 0; i < 4; i++) {
        chord_btns_[i] = lv_obj_create(btn_area);
        lv_obj_set_size(chord_btns_[i], 64, 24);
        lv_obj_set_style_bg_color(chord_btns_[i], kPurpleDark, 0);
        lv_obj_set_style_bg_opa(chord_btns_[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(chord_btns_[i], kPurple, 0);
        lv_obj_set_style_border_width(chord_btns_[i], 1, 0);
        lv_obj_set_style_radius(chord_btns_[i], 5, 0);
        lv_obj_set_style_pad_all(chord_btns_[i], 0, 0);

        lv_obj_t* lbl = lv_label_create(chord_btns_[i]);
        lv_label_set_text(lbl, kChordNames[i]);
        lv_obj_set_style_text_color(lbl, kPurpleLight, 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        lv_obj_center(lbl);

        // 高亮计时器（150ms 后恢复）
        esp_timer_create_args_t args = {
            .callback = [](void* arg) {
                auto* self = static_cast<MuseDisplay*>(arg);
                // 恢复所有按钮样式（简单起见全部重置）
                for (int j = 0; j < 4; j++) {
                    lv_obj_set_style_bg_color(self->chord_btns_[j], kPurpleDark, 0);
                    lv_obj_set_style_shadow_width(self->chord_btns_[j], 0, 0);
                }
            },
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "chord_hl",
            .skip_unhandled_events = true,
        };
        esp_timer_create(&args, &highlight_timer_[i]);
    }

    // 初始化"正在创作..."标签（默认隐藏）
    composing_label_ = lv_label_create(chat_scroll_);
    lv_label_set_text(composing_label_, "正在创作...");
    lv_obj_set_style_text_color(composing_label_, kPurpleLight, 0);
    lv_obj_set_style_text_font(composing_label_, &lv_font_montserrat_12, 0);
    lv_obj_add_flag(composing_label_, LV_OBJ_FLAG_HIDDEN);
}

void MuseDisplay::BuildLyricsScreen() {
    lv_obj_t* scr = lv_screen_active();
    lyrics_screen_ = lv_obj_create(scr);
    lv_obj_set_size(lyrics_screen_, 240, 240);
    lv_obj_align(lyrics_screen_, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(lyrics_screen_, kBgColor, 0);
    lv_obj_set_style_bg_opa(lyrics_screen_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(lyrics_screen_, 0, 0);
    lv_obj_set_style_pad_all(lyrics_screen_, 0, 0);

    // 标题栏（28px）
    lv_obj_t* title_bar = lv_obj_create(lyrics_screen_);
    lv_obj_set_size(title_bar, 240, 28);
    lv_obj_align(title_bar, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(title_bar, LV_COLOR_MAKE(0x14, 0x08, 0x28), 0);
    lv_obj_set_style_bg_opa(title_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(title_bar, 0, 0);
    lv_obj_set_style_pad_all(title_bar, 4, 0);

    lyrics_title_ = lv_label_create(title_bar);
    lv_label_set_text(lyrics_title_, "♪ --");
    lv_obj_set_style_text_color(lyrics_title_, kPurpleLight, 0);
    lv_obj_set_style_text_font(lyrics_title_, &lv_font_montserrat_10, 0);
    lv_obj_align(lyrics_title_, LV_ALIGN_LEFT_MID, 4, 0);

    // 歌词滚动区（240-28-36 = 176px）
    lyrics_scroll_ = lv_obj_create(lyrics_screen_);
    lv_obj_set_size(lyrics_scroll_, 240, 176);
    lv_obj_align(lyrics_scroll_, LV_ALIGN_TOP_LEFT, 0, 28);
    lv_obj_set_flex_flow(lyrics_scroll_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_opa(lyrics_scroll_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(lyrics_scroll_, 0, 0);
    lv_obj_set_style_pad_all(lyrics_scroll_, 2, 0);
    lv_obj_set_style_pad_row(lyrics_scroll_, 0, 0);
    lv_obj_set_scroll_dir(lyrics_scroll_, LV_DIR_VER);

    // 底部和弦条（36px）
    lv_obj_t* chord_bar = lv_obj_create(lyrics_screen_);
    lv_obj_set_size(chord_bar, 240, 36);
    lv_obj_align(chord_bar, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_set_style_bg_color(chord_bar, LV_COLOR_MAKE(0x00, 0x00, 0x00), 0);
    lv_obj_set_style_bg_opa(chord_bar, LV_OPA_80, 0);
    lv_obj_set_style_border_width(chord_bar, 0, 0);
    lv_obj_set_flex_flow(chord_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(chord_bar, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    for (int i = 0; i < 4; i++) {
        lyrics_chord_bar_[i] = lv_obj_create(chord_bar);
        lv_obj_set_size(lyrics_chord_bar_[i], 48, 24);
        lv_obj_set_style_bg_color(lyrics_chord_bar_[i], kPurpleDark, 0);
        lv_obj_set_style_bg_opa(lyrics_chord_bar_[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(lyrics_chord_bar_[i], kPurple, 0);
        lv_obj_set_style_border_width(lyrics_chord_bar_[i], 1, 0);
        lv_obj_set_style_radius(lyrics_chord_bar_[i], 4, 0);
        lv_obj_t* lbl = lv_label_create(lyrics_chord_bar_[i]);
        lv_label_set_text(lbl, kChordNames[i]);
        lv_obj_set_style_text_color(lbl, kPurpleLight, 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
        lv_obj_center(lbl);
    }
}

void MuseDisplay::SetChatMessage(const char* role, const char* content) {
    DisplayLockGuard lock(this);
    messages_.push_back({role, content});
    if (messages_.size() > 5) messages_.erase(messages_.begin());
    RebuildChatBubbles();
}

void MuseDisplay::ClearChatMessages() {
    DisplayLockGuard lock(this);
    messages_.clear();
    RebuildChatBubbles();
}

void MuseDisplay::RebuildChatBubbles() {
    // 清除旧气泡（保留 composing_label_）
    lv_obj_t* child = lv_obj_get_child(chat_scroll_, 0);
    while (child) {
        lv_obj_t* next = lv_obj_get_child_by_id(chat_scroll_,
                            lv_obj_get_child_id(child) + 1);
        if (child != composing_label_) lv_obj_delete(child);
        child = next;
    }

    for (auto& msg : messages_) {
        bool is_ai = (msg.role == "assistant");
        lv_obj_t* bubble = lv_obj_create(chat_scroll_);
        lv_obj_set_width(bubble, LV_PCT(90));
        lv_obj_set_height(bubble, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_color(bubble, is_ai ?
            LV_COLOR_MAKE(0x2a, 0x12, 0x50) : LV_COLOR_MAKE(0x1a, 0x1a, 0x2a), 0);
        lv_obj_set_style_bg_opa(bubble, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(bubble, is_ai ? kPurple : kGray, 0);
        lv_obj_set_style_border_width(bubble, 1, 0);
        lv_obj_set_style_radius(bubble, 8, 0);
        lv_obj_set_style_pad_all(bubble, 4, 0);
        lv_obj_set_style_align(bubble, is_ai ? LV_ALIGN_LEFT_MID : LV_ALIGN_RIGHT_MID, 0);

        lv_obj_t* lbl = lv_label_create(bubble);
        lv_label_set_text(lbl, msg.content.c_str());
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(lbl, LV_PCT(100));
        lv_obj_set_style_text_color(lbl, kWhite, 0);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_10, 0);
    }

    // 滚动到底部
    lv_obj_scroll_to_y(chat_scroll_, LV_COORD_MAX, LV_ANIM_ON);
}

void MuseDisplay::HighlightChord(Chord chord) {
    DisplayLockGuard lock(this);
    int i = static_cast<int>(chord);
    lv_obj_set_style_bg_color(chord_btns_[i], kPurple, 0);
    lv_obj_set_style_shadow_color(chord_btns_[i], kPurple, 0);
    lv_obj_set_style_shadow_width(chord_btns_[i], 12, 0);
    esp_timer_stop(highlight_timer_[i]);
    esp_timer_start_once(highlight_timer_[i], 150 * 1000);  // 150ms
}

void MuseDisplay::ShowLyrics(const std::string& title,
                              const std::vector<LyricLine>& lines) {
    DisplayLockGuard lock(this);
    lv_label_set_text(lyrics_title_, ("♪ " + title).c_str());

    // 清空旧歌词
    lv_obj_clean(lyrics_scroll_);

    for (size_t i = 0; i < lines.size(); i++) {
        const auto& line = lines[i];
        lv_obj_t* row = lv_obj_create(lyrics_scroll_);
        lv_obj_set_size(row, 234, 26);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_pad_all(row, 2, 0);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START,
                              LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        // 和弦胶囊
        lv_obj_t* chord_tag = lv_obj_create(row);
        lv_obj_set_size(chord_tag, 28, 18);
        lv_obj_set_style_bg_color(chord_tag, kPurpleDark, 0);
        lv_obj_set_style_bg_opa(chord_tag, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(chord_tag, kPurple, 0);
        lv_obj_set_style_border_width(chord_tag, 1, 0);
        lv_obj_set_style_radius(chord_tag, 4, 0);
        lv_obj_t* ct = lv_label_create(chord_tag);
        lv_label_set_text(ct, line.chord.c_str());
        lv_obj_set_style_text_color(ct, kPurpleLight, 0);
        lv_obj_set_style_text_font(ct, &lv_font_montserrat_10, 0);
        lv_obj_center(ct);

        // 歌词文字
        lv_obj_t* lyric_lbl = lv_label_create(row);
        lv_label_set_text(lyric_lbl, line.lyric.c_str());
        lv_obj_set_style_text_color(lyric_lbl, kWhite, 0);
        lv_obj_set_style_text_font(lyric_lbl, &lv_font_montserrat_12, 0);
        lv_obj_set_style_pad_left(lyric_lbl, 4, 0);
    }

    // 切换到歌词界面
    lv_obj_remove_flag(lyrics_screen_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(main_screen_, LV_OBJ_FLAG_HIDDEN);
}

void MuseDisplay::ShowMain() {
    DisplayLockGuard lock(this);
    lv_obj_add_flag(lyrics_screen_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(main_screen_, LV_OBJ_FLAG_HIDDEN);
}

void MuseDisplay::SetComposing(bool composing) {
    DisplayLockGuard lock(this);
    if (composing) {
        lv_obj_remove_flag(composing_label_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(composing_label_, LV_OBJ_FLAG_HIDDEN);
    }
}
```

- [ ] **Step 3: 在 CMakeLists.txt 的 otto-robot SOURCES 添加 muse_display.cc**

```cmake
list(APPEND SOURCES
    ...
    "boards/otto-robot/touch_chord_controller.cc"
    "boards/otto-robot/chord_player.cc"
    "boards/otto-robot/muse_display.cc"
)
```

- [ ] **Step 4: 在 otto_robot.cc 替换 OttoEmojiDisplay → MuseDisplay**

将：

```cpp
#include "otto_emoji_display.h"
```

替换为：

```cpp
#include "muse_display.h"
```

将 `display_` 的类型从 `LcdDisplay*` 改为 `MuseDisplay*`：

```cpp
MuseDisplay* display_ = nullptr;
```

在 `InitializeLcdDisplay()` 中将：

```cpp
display_ = new OttoEmojiDisplay(
    panel_io, panel, DISPLAY_WIDTH, DISPLAY_HEIGHT, ...);
```

替换为：

```cpp
display_ = new MuseDisplay(
    panel_io, panel, DISPLAY_WIDTH, DISPLAY_HEIGHT,
    DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y,
    DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
```

在 `InitializeTouchController()` 的 lambda 中添加高亮调用：

```cpp
void InitializeTouchController() {
    touch_controller_ = new TouchChordController([this](Chord chord) {
        chord_player_->Play(chord, chord_player_->style());
        display_->HighlightChord(chord);
    });
    touch_controller_->Start();
}
```

- [ ] **Step 5: 编译并烧录测试**

```bash
idf.py build && idf.py flash monitor
```

期望：
- 屏幕显示左侧紫色圆形头像 + "MUSE" + 右侧暗色聊天区 + 底部 4 个和弦按钮
- 触摸 GPIO 4 铜箔：C 按钮变紫色爆光，150ms 后恢复，同时播放音效

- [ ] **Step 6: Commit**

```bash
git add main/boards/otto-robot/muse_display.h \
        main/boards/otto-robot/muse_display.cc \
        main/boards/otto-robot/otto_robot.cc \
        main/CMakeLists.txt
git commit -m "feat(M3): add MuseDisplay with neon-purple main UI and chord highlights"
```

---

## Task 4: AI 缪斯角色 + MCP 工具（music.play_chord / music.back_to_main）

**Files:**
- Create: `main/boards/otto-robot/muse_mcp_tools.cc`
- Modify: `main/boards/otto-robot/otto_robot.cc`
- Modify: `main/CMakeLists.txt`

- [ ] **Step 1: 创建 muse_mcp_tools.cc（M4 注册 2 个工具）**

```cpp
// main/boards/otto-robot/muse_mcp_tools.cc
#include "muse_display.h"
#include "chord_player.h"
#include "touch_chord_controller.h"
#include "mcp_server.h"
#include "board.h"
#include <esp_log.h>
#include <string>
#include <map>

#define TAG "MuseMcp"

// 全局指针（由 otto_robot.cc 的 InitializeMuseMcpTools() 注入）
static ChordPlayer* g_chord_player = nullptr;
static MuseDisplay* g_muse_display = nullptr;

static const std::map<std::string, Chord> kChordMap = {
    {"C", Chord::C}, {"Am", Chord::Am}, {"F", Chord::F}, {"G", Chord::G}
};

void InitializeMuseMcpTools(ChordPlayer* player, MuseDisplay* display) {
    g_chord_player = player;
    g_muse_display = display;

    auto& mcp = McpServer::GetInstance();

    // music.play_chord
    mcp.AddTool("music.play_chord",
        "播放指定吉他和弦音效。chord: C/Am/F/G；style: arpeggio（分解弦）或 strum（扫弦）",
        PropertyList({
            Property("chord", kPropertyTypeString, std::string("C")),
            Property("style", kPropertyTypeString, std::string("arpeggio")),
        }),
        [](const PropertyList& props) -> ReturnValue {
            std::string chord_name = props["chord"].value<std::string>();
            std::string style_str  = props["style"].value<std::string>();
            auto it = kChordMap.find(chord_name);
            if (it == kChordMap.end()) return "错误：和弦必须是 C/Am/F/G 之一";
            PlayStyle style = (style_str == "strum") ? PlayStyle::Strum : PlayStyle::Arpeggio;
            g_chord_player->Play(it->second, style);
            g_muse_display->HighlightChord(it->second);
            return true;
        });

    // music.back_to_main
    mcp.AddTool("music.back_to_main",
        "关闭歌词界面，返回主界面（AI角色对话界面）",
        PropertyList(),
        [](const PropertyList&) -> ReturnValue {
            g_muse_display->ShowMain();
            return true;
        });

    // music.set_style
    mcp.AddTool("music.set_style",
        "设置演奏风格。style: arpeggio（分解弦，默认）或 strum（扫弦）",
        PropertyList({
            Property("style", kPropertyTypeString, std::string("arpeggio")),
        }),
        [](const PropertyList& props) -> ReturnValue {
            std::string s = props["style"].value<std::string>();
            g_chord_player->SetStyle(s == "strum" ? PlayStyle::Strum : PlayStyle::Arpeggio);
            return "演奏风格已切换为 " + s;
        });

    ESP_LOGI(TAG, "音乐 MCP 工具注册完成（music.play_chord, music.back_to_main, music.set_style）");
}
```

- [ ] **Step 2: 在 otto_robot.cc 中声明并调用**

在 `otto_robot.cc` 顶部添加：

```cpp
extern void InitializeMuseMcpTools(ChordPlayer* player, MuseDisplay* display);
```

在 `StartNetwork()` 的 `InitializeWebSocketControlServer()` 之后调用：

```cpp
void StartNetwork() override {
    WifiBoard::StartNetwork();
    vTaskDelay(pdMS_TO_TICKS(1000));
    InitializeWebSocketControlServer();
    InitializeMuseMcpTools(chord_player_, display_);
}
```

- [ ] **Step 3: 在 CMakeLists.txt 添加 muse_mcp_tools.cc**

```cmake
list(APPEND SOURCES
    ...
    "boards/otto-robot/muse_display.cc"
    "boards/otto-robot/muse_mcp_tools.cc"
)
```

- [ ] **Step 4: 编译烧录，验证 AI 对话**

```bash
idf.py build && idf.py flash monitor
```

连接网络后，对小智说：「帮我弹一下 Am 和弦」  
期望：AI 调用 `music.play_chord(chord="Am", style="arpeggio")`，喇叭发出 Am 音效，屏幕 Am 按钮高亮。

- [ ] **Step 5: Commit**

```bash
git add main/boards/otto-robot/muse_mcp_tools.cc \
        main/boards/otto-robot/otto_robot.cc \
        main/CMakeLists.txt
git commit -m "feat(M4): register music MCP tools, AI Muse character can play chords"
```

---

## Task 5: 摄像头拍照 + AI 填词 + 歌词界面渲染

**Files:**
- Modify: `main/boards/otto-robot/muse_mcp_tools.cc`（添加 3 个工具）

- [ ] **Step 1: 在 muse_mcp_tools.cc 添加 music.capture_photo、music.show_lyrics、music.set_composing**

在 `InitializeMuseMcpTools()` 中，在现有工具之后追加：

```cpp
// music.capture_photo：拍照并返回 base64 JPEG
mcp.AddTool("music.capture_photo",
    "用摄像头拍摄当前画面，返回 JPEG 图片（base64编码）。在创作歌词前调用以获取景色灵感。",
    PropertyList(),
    [](const PropertyList&) -> ReturnValue {
        auto& board = Board::GetInstance();
        auto* camera = board.GetCamera();
        if (!camera) return "错误：摄像头不可用";

        auto image = camera->Capture();
        if (!image) return "错误：拍照失败";

        std::string jpeg_data(reinterpret_cast<const char*>(image->data),
                              image->size);
        auto* img_content = new ImageContent("image/jpeg", jpeg_data);
        return img_content;
    });

// music.set_composing：显示/隐藏"正在创作..."状态
mcp.AddTool("music.set_composing",
    "显示或隐藏"正在创作…"状态提示。在开始填词前设为 true，完成后设为 false。",
    PropertyList({
        Property("composing", kPropertyTypeBoolean, true),
    }),
    [](const PropertyList& props) -> ReturnValue {
        bool c = props["composing"].value<bool>();
        g_muse_display->SetComposing(c);
        return true;
    });

// music.show_lyrics：渲染歌词界面
mcp.AddTool("music.show_lyrics",
    "在屏幕上显示歌词+和弦，供用户边看边弹唱。"
    "lyrics_json 格式：{\"title\":\"歌名\",\"lines\":["
    "{\"chord\":\"C\",\"lyric\":\"歌词第一行\"},...]}。"
    "chord 只能使用 C/Am/F/G。",
    PropertyList({
        Property("lyrics_json", kPropertyTypeString, std::string("{}")),
    }),
    [](const PropertyList& props) -> ReturnValue {
        std::string json_str = props["lyrics_json"].value<std::string>();
        cJSON* root = cJSON_Parse(json_str.c_str());
        if (!root) return "错误：lyrics_json 格式无效";

        std::string title;
        cJSON* j_title = cJSON_GetObjectItem(root, "title");
        if (cJSON_IsString(j_title)) title = j_title->valuestring;

        std::vector<MuseDisplay::LyricLine> lines;
        cJSON* j_lines = cJSON_GetObjectItem(root, "lines");
        if (cJSON_IsArray(j_lines)) {
            cJSON* item;
            cJSON_ArrayForEach(item, j_lines) {
                cJSON* jc = cJSON_GetObjectItem(item, "chord");
                cJSON* jl = cJSON_GetObjectItem(item, "lyric");
                if (cJSON_IsString(jc) && cJSON_IsString(jl)) {
                    lines.push_back({jc->valuestring, jl->valuestring});
                }
            }
        }
        cJSON_Delete(root);

        if (lines.empty()) return "错误：歌词为空";

        g_muse_display->SetComposing(false);
        g_muse_display->ShowLyrics(title, lines);
        return true;
    });

ESP_LOGI(TAG, "全部音乐 MCP 工具注册完成（共 6 个）");
```

- [ ] **Step 2: 编译烧录**

```bash
idf.py build && idf.py flash monitor
```

- [ ] **Step 3: 验证完整填词流程**

对小智说：「帮我基于《同桌的你》的旋律，结合你现在看到的景色，给我写几句歌词」

期望流程（串口可见）：
1. AI 调用 `music.set_composing(composing=true)` → 屏幕显示"正在创作..."
2. AI 调用 `music.capture_photo` → 返回 base64 JPEG
3. AI 生成歌词，调用 `music.show_lyrics(lyrics_json="{...}")`
4. 屏幕切换到歌词界面，显示带和弦标注的逐行歌词
5. 按下铜箔按钮弹奏对应和弦，对应和弦条高亮

- [ ] **Step 4: Commit**

```bash
git add main/boards/otto-robot/muse_mcp_tools.cc
git commit -m "feat(M5): add music.capture_photo, music.show_lyrics, full AI lyric creation flow"
```

---

## Task 6: 移调（Capo）+ 翻页手势 + 整体联调

**Files:**
- Modify: `main/boards/otto-robot/chord_player.cc`（移调重采样）
- Modify: `main/boards/otto-robot/muse_mcp_tools.cc`（music.set_capo + music.set_tempo）
- Modify: `main/boards/otto-robot/touch_chord_controller.cc`（长按翻页）

- [ ] **Step 1: ChordPlayer 实现简单移调（线性重采样）**

在 `chord_player.cc` 的 `OutputPcm()` 替换为支持 capo 的版本：

```cpp
void ChordPlayer::OutputPcm(const uint8_t* data, size_t size_bytes) {
    const int16_t* src = reinterpret_cast<const int16_t*>(data);
    size_t total_src = size_bytes / 2;

    // capo_=0 → ratio=1.0; capo_=+1 → ratio=2^(1/12)≈1.0595（升半音加速播放）
    float ratio = 1.0f;
    if (capo_ != 0) {
        ratio = powf(2.0f, capo_ / 12.0f);
    }

    const size_t kChunk = 240;
    std::vector<int16_t> buf(kChunk, 0);
    size_t dst_idx = 0;

    for (size_t i = 0; ; i++) {
        float src_pos = i * ratio;
        size_t si = (size_t)src_pos;
        if (si >= total_src) break;
        // 线性插值
        float frac = src_pos - si;
        int16_t s0 = src[si];
        int16_t s1 = (si + 1 < total_src) ? src[si + 1] : 0;
        buf[dst_idx++] = (int16_t)(s0 + frac * (s1 - s0));
        if (dst_idx == kChunk) {
            codec_->OutputData(buf);
            dst_idx = 0;
        }
    }
    if (dst_idx > 0) {
        memset(buf.data() + dst_idx, 0, (kChunk - dst_idx) * 2);
        codec_->OutputData(buf);
    }
}
```

- [ ] **Step 2: 在 muse_mcp_tools.cc 添加 music.set_capo**

在 `InitializeMuseMcpTools()` 末尾追加：

```cpp
mcp.AddTool("music.set_capo",
    "设置移调半音数（变调夹）。semitones: -3~+3，0为原调，+1升半音，-1降半音。",
    PropertyList({
        Property("semitones", kPropertyTypeInteger, 0, -3, 3),
    }),
    [](const PropertyList& props) -> ReturnValue {
        int n = props["semitones"].value<int>();
        g_chord_player->SetCapo(n);
        return "移调设置为 " + std::to_string(n) + " 半音";
    });
```

- [ ] **Step 3: 在 touch_chord_controller.cc 实现长按翻页**

在 `PollTask` 中，增加长按计数（连续触摸 > 40 次 × 20ms = 800ms）时触发翻页回调。

在 `TouchChordController` 构造函数中新增第二个回调参数：

```cpp
// touch_chord_controller.h 中，构造函数改为：
using ChordLongPressCallback = std::function<void(bool up)>;  // true=上翻，false=下翻

explicit TouchChordController(ChordTouchCallback on_touch,
                               ChordLongPressCallback on_long_press = nullptr);
```

在 `.cc` 的 `PollTask` 中：

```cpp
// 长按计数
int long_count[4] = {0};
bool long_fired[4] = {false};
// ...在 touching 判断内：
if (touching) {
    debounce[i]++;
    long_count[i]++;
    // 短按（首次触发）
    if (debounce[i] == 2 && !pressed[i]) {
        pressed[i] = true;
        if (!long_fired[i]) {
            self->callback_(static_cast<Chord>(i));
        }
    }
    // 长按（800ms，触发翻页）
    if (long_count[i] == 40 && !long_fired[i] && self->long_press_cb_) {
        long_fired[i] = true;
        bool scroll_up = (i == 0);   // BTN1(C) 长按=上翻
        bool scroll_dn = (i == 3);   // BTN4(G) 长按=下翻
        if (scroll_up || scroll_dn) {
            self->long_press_cb_(scroll_up);
        }
    }
} else {
    debounce[i] = 0;
    pressed[i] = false;
    long_count[i] = 0;
    long_fired[i] = false;
}
```

在 `otto_robot.cc` 的 `InitializeTouchController()` 中传入长按回调：

```cpp
void InitializeTouchController() {
    touch_controller_ = new TouchChordController(
        [this](Chord chord) {
            chord_player_->Play(chord, chord_player_->style());
            display_->HighlightChord(chord);
        },
        [this](bool up) {
            // 长按 C 或 G 时滚动歌词
            DisplayLockGuard lock(display_);
            lv_obj_t* scroll = display_->lyrics_scroll_;
            if (scroll) {
                int delta = up ? -40 : 40;
                lv_obj_scroll_by(scroll, 0, delta, LV_ANIM_ON);
            }
        }
    );
    touch_controller_->Start();
}
```

> 注意：`lyrics_scroll_` 需要在 `muse_display.h` 中改为 `public` 或提供 `ScrollLyrics(int dy)` 方法。推荐后者：

在 `muse_display.h` 添加：

```cpp
void ScrollLyrics(int dy);  // dy>0 下翻，<0 上翻
```

在 `muse_display.cc` 实现：

```cpp
void MuseDisplay::ScrollLyrics(int dy) {
    DisplayLockGuard lock(this);
    lv_obj_scroll_by(lyrics_scroll_, 0, dy, LV_ANIM_ON);
}
```

长按回调改为：

```cpp
[this](bool up) {
    display_->ScrollLyrics(up ? -40 : 40);
}
```

- [ ] **Step 4: 编译烧录全功能联调**

```bash
idf.py build && idf.py flash monitor
```

完整验证清单：
- [ ] 触摸 4 个按钮各发出对应和弦音效，对应按钮高亮 150ms
- [ ] 对 AI 说「换成扫弦」→ AI 调用 `music.set_style(style="strum")` → 后续弹奏变为扫弦
- [ ] 对 AI 说「升一个调」→ AI 调用 `music.set_capo(semitones=1)` → 音调升高
- [ ] 对 AI 说「帮我基于xxx歌曲和眼前景色写歌词」→ 屏幕切换歌词界面
- [ ] 长按 BTN1（C，GPIO 4）→ 歌词上翻；长按 BTN4（G，GPIO 7）→ 歌词下翻
- [ ] 对 AI 说「返回主界面」→ AI 调用 `music.back_to_main` → 恢复主界面

- [ ] **Step 5: 最终 Commit**

```bash
git add main/boards/otto-robot/chord_player.cc \
        main/boards/otto-robot/touch_chord_controller.h \
        main/boards/otto-robot/touch_chord_controller.cc \
        main/boards/otto-robot/muse_display.h \
        main/boards/otto-robot/muse_display.cc \
        main/boards/otto-robot/muse_mcp_tools.cc
git commit -m "feat(M6): add capo transpose, long-press lyric scroll, full integration"
```

---

## 常见问题排查

| 现象 | 原因 | 解决 |
|---|---|---|
| 触摸毫无反应 | 基线校准失败 / 导线太长 | 检查串口基线值；导线 <20cm；增大铜箔面积 |
| 频繁误触 | 阈值太低 | 将 `TOUCH_THRESHOLD_RATIO` 从 0.2 调高到 0.3 |
| 无声音 | I2S 未启动 / volume=0 | `idf.py menuconfig` 检查音量；检查 audio_codec_ 是否 nullptr |
| 编译报 `undefined reference chord_C_arp` | CMakeLists EMBED_FILES 路径错误 | 路径必须相对于 `main/` 目录 |
| 歌词界面空白 | JSON 格式错误 | 检查 AI 返回的 lyrics_json 是否有效；必须含 title + lines 数组 |
| 摄像头返回错误 | OTTO_HARDWARE_VERSION 未配置 | 确认 `config.h` 中 `#define OTTO_HARDWARE_VERSION OTTO_VERSION_CAMERA` |

---

## 替换真实吉他采样（M2完成后进行）

1. 准备 16kHz / 16bit / mono 的吉他 WAV 文件
2. 用 ffmpeg 转换为 raw PCM：
   ```bash
   ffmpeg -i guitar_C.wav -ar 16000 -ac 1 -f s16le chord_C_arp.raw
   ffmpeg -i guitar_C_strum.wav -ar 16000 -ac 1 -f s16le chord_C_strum.raw
   # 对 Am/F/G 重复
   ```
3. 替换 `main/boards/otto-robot/assets/chords/` 下对应文件
4. 重新 `idf.py build && idf.py flash`
