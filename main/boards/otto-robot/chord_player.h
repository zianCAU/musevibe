// main/boards/otto-robot/chord_player.h
#pragma once
#include "touch_chord_controller.h"
#include "audio/audio_codec.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>
#include <lwip/sockets.h>
#include <atomic>
#include <array>
#include <functional>

enum class PlayStyle { Arpeggio, Strum };

struct PlayRequest {
    Chord     chord;
    PlayStyle style;
};

enum class ArpPattern {
    P532123 = 0,  // 5→3→2→1→2→3  民谣基础
    P512321,      // 5→1→2→3→2→1  流行常用
    P521323,      // 5→2→1→3→2→3  指弹风
    P123456,      // 上行
    P654321,      // 下行连扫
    P531531,      // 三连分解
    COUNT
};

// 单个 Karplus-Strong 拨弦声部
struct KSVoice {
    static constexpr int MAX_PERIOD = 512;
    float buf[MAX_PERIOD] = {};
    int   period    = 0;
    int   pos       = 0;
    bool  active    = false;
    float damping   = 0.498f;

    void Trigger(float freq, float sample_rate, float damp);
    float Next();
};

class ChordPlayer {
public:
    explicit ChordPlayer(AudioCodec* codec);
    ~ChordPlayer();

    static int BpmToNoteMs(int bpm) { return 60000 / bpm / 2; }

    void Play(Chord chord, PlayStyle style = PlayStyle::Arpeggio);
    void SetStyle(PlayStyle style)       { style_   = style; }
    void SetCapo(int semitones)          { capo_    = semitones; }
    void SetBpm(int bpm)                 { bpm_     = bpm; }
    void SetArpPattern(ArpPattern pat)   { pattern_ = pat; }

    // 演奏手势回调：is_strum=true 扫弦，false 琶音；bpm 当前节拍
    void SetGestureCallback(std::function<void(bool is_strum, int bpm)> cb) {
        gesture_cb_ = std::move(cb);
    }

    // 远程音频模式：设备只发 UDP 意图包，PC 端负责播放
    void EnableRemoteAudio(const char* host, uint16_t port = 7777);
    void DisableRemoteAudio();

    PlayStyle  style()   const { return style_; }
    int        capo()    const { return capo_; }
    int        bpm()     const { return bpm_; }
    ArpPattern pattern() const { return pattern_; }
    bool       remote_audio_enabled() const { return remote_audio_enabled_; }

private:
    static constexpr int kMaxVoices = 8;
    static constexpr int kChunk     = 240;

    AudioCodec* codec_;
    float       sample_rate_;
    PlayStyle   style_   = PlayStyle::Arpeggio;
    int         capo_    = 0;
    int         bpm_     = 80;
    ArpPattern  pattern_ = ArpPattern::P532123;

    SemaphoreHandle_t voices_mutex_;
    std::array<KSVoice, kMaxVoices> voices_;

    QueueHandle_t play_queue_  = nullptr;
    TaskHandle_t  play_task_   = nullptr;
    TaskHandle_t  mix_task_    = nullptr;

    // 远程音频
    bool               remote_audio_enabled_ = false;
    int                udp_sock_             = -1;
    struct sockaddr_in remote_addr_          = {};

    // 演奏手势回调
    std::function<void(bool, int)> gesture_cb_;

    void TriggerVoice(float freq, float damping);
    void SendChordUDP(Chord chord, PlayStyle style);
    static void PlayTask(void* arg);
    static void MixTask(void* arg);
};
