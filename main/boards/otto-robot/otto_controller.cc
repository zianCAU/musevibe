/*
    Otto机器人控制器 - MCP协议版本
*/

#include <cJSON.h>
#include <esp_log.h>
#include <esp_random.h>

#include <cstdlib>
#include <cstring>

#include "application.h"
#include "board.h"
#include "config.h"
#include "mcp_server.h"
#include "otto_movements.h"
#include "power_manager.h"
#include "sdkconfig.h"
#include "settings.h"
#include <wifi_manager.h>

#define TAG "OttoController"

class OttoController {
private:
    Otto otto_;
    TaskHandle_t action_task_handle_ = nullptr;
    TaskHandle_t talk_gesture_task_handle_ = nullptr;  // 对话手势后台任务
    QueueHandle_t action_queue_;
    bool has_hands_ = false;
    bool is_action_in_progress_ = false;

    struct OttoActionParams {
        int action_type;
        int steps;
        int speed;
        int direction;
        int amount;
        char servo_sequence_json[512];  // 用于存储舵机序列的JSON字符串
    };

    enum ActionType {
        ACTION_WALK = 1,
        ACTION_TURN = 2,
        ACTION_JUMP = 3,
        ACTION_SWING = 4,
        ACTION_MOONWALK = 5,
        ACTION_BEND = 6,
        ACTION_SHAKE_LEG = 7,
        ACTION_SIT = 25,  // 坐下
        ACTION_RADIO_CALISTHENICS = 26,  // 广播体操
        ACTION_MAGIC_CIRCLE = 27,  // 爱的魔力转圈圈
        ACTION_UPDOWN = 8,
        ACTION_TIPTOE_SWING = 9,
        ACTION_JITTER = 10,
        ACTION_ASCENDING_TURN = 11,
        ACTION_CRUSAITO = 12,
        ACTION_FLAPPING = 13,
        ACTION_HANDS_UP = 14,
        ACTION_HANDS_DOWN = 15,
        ACTION_HAND_WAVE = 16,
        ACTION_WINDMILL = 20,  // 大风车
        ACTION_TAKEOFF = 21,   // 起飞
        ACTION_FITNESS = 22,   // 健身
        ACTION_GREETING = 23,  // 打招呼
        ACTION_SHY = 24,        // 害羞
        ACTION_SHOWCASE = 28,   // 展示动作
        ACTION_HOME = 17,
        ACTION_SERVO_SEQUENCE = 18,  // 舵机序列（自编程）
        ACTION_WHIRLWIND_LEG = 19,   // 旋风腿
        // ── 演奏同步手势 ──────────────────────────────────────────
        ACTION_MUSIC_STRUM = 30,  // 扫弦手势（右手快速下扫）
        ACTION_MUSIC_ARP   = 31,  // 琶音手势（双手律动）
        ACTION_EMOTION     = 32,  // 情绪表达（direction 编码情绪类型）
        ACTION_COMPANION_REACT = 33,  // 陪伴模式触摸反馈（direction 编码触摸区域）
        ACTION_TALK_GESTURE = 34,  // 对话中随机手势（direction 编码手势类型 0-4）
    };

    static void ActionTask(void* arg) { /* proprietary implementation */ }

    void StartActionTaskIfNeeded() { /* proprietary implementation */ }

    void QueueAction(int action_type, int steps, int speed, int direction, int amount) { /* proprietary implementation */ }

    // 演奏同步手势：非阻塞，队列忙时丢弃，不干扰长动作
public:
    void QueueMusicGesture(bool is_strum, int bpm) { /* proprietary implementation */ }

    // 情绪表达手势：正常排队
    void QueueEmotion(const char* emotion) { /* proprietary implementation */ }

    // 陪伴模式触摸反馈：非阻塞，队列忙时丢弃
    void QueueCompanionReaction(int pad_index) { /* proprietary implementation */ }

    // 对话手势：非阻塞，队列忙时丢弃
    void QueueTalkGesture() { /* proprietary implementation */ }

    // 对话手势后台任务：每 3-5 秒触发一次随机手势
    static void TalkGestureTask(void* arg) { /* proprietary implementation */ }

public:
    // 开始对话手势循环（AI说话时调用）
    void StartTalkGestures() { /* proprietary implementation */ }

    // 停止对话手势循环
    void StopTalkGestures() { /* proprietary implementation */ }

    void QueueServoSequence(const char* servo_sequence_json) { /* proprietary implementation */ }

    void LoadTrimsFromNVS() { /* proprietary implementation */ }

public:
    OttoController(const HardwareConfig& hw_config) { /* proprietary implementation */ }

    void RegisterMcpTools() { /* proprietary implementation */ }

    ~OttoController() { /* proprietary implementation */ }
};

static OttoController* g_otto_controller = nullptr;

void InitializeOttoController(const HardwareConfig& hw_config) { /* proprietary implementation */ }

// ── 供外部模块调用的自由函数 ──────────────────────────────────────────────────
void OttoPlayMusicGesture(bool is_strum, int bpm) { /* proprietary implementation */ }

void OttoExpressEmotion(const char* emotion) { /* proprietary implementation */ }

void OttoCompanionReaction(int pad_index) { /* proprietary implementation */ }

void OttoStartTalkGestures() { /* proprietary implementation */ }

void OttoStopTalkGestures() { /* proprietary implementation */ }
