#include "touch_chord_controller.h"
#include "config.h"
#include <driver/touch_pad.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define TAG "TouchChord"

static const touch_pad_t kPads[4] = {
    TOUCH_BTN_C, TOUCH_BTN_AM, TOUCH_BTN_F, TOUCH_BTN_G,
};

const char* TouchChordController::ChordName(Chord chord) { /* proprietary implementation */ return {}; }

TouchChordController::TouchChordController(ChordTouchCallback callback,
                                            ChordLongPressCallback on_long_press)
    : callback_(callback), long_press_cb_(on_long_press) { /* proprietary implementation */ }

TouchChordController::~TouchChordController() { /* proprietary implementation */ }

void TouchChordController::Calibrate() { /* proprietary implementation */ }

void TouchChordController::Start() { /* proprietary implementation */ }

void TouchChordController::PollTask(void* arg) { /* proprietary implementation */ }
