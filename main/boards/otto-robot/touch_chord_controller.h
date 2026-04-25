#pragma once
#include <functional>
#include <string>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

enum class Chord { C, Am, F, G };
using ChordTouchCallback = std::function<void(Chord chord)>;
using ChordLongPressCallback = std::function<void(bool up)>;  // true=上翻，false=下翻

class TouchChordController {
public:
    explicit TouchChordController(ChordTouchCallback callback,
                                   ChordLongPressCallback on_long_press = nullptr);
    ~TouchChordController();
    void Start();
    static const char* ChordName(Chord chord);

private:
    ChordTouchCallback callback_;
    ChordLongPressCallback long_press_cb_;  // 长按翻页回调
    TaskHandle_t task_handle_ = nullptr;
    uint32_t baseline_[4] = {0};
    void Calibrate();
    static void PollTask(void* arg);
};
