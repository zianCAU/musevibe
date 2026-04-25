#pragma once
#include "display/lcd_display.h"
#include "display/lvgl_display/gif/lvgl_gif.h"
#include "touch_chord_controller.h"
#include <string>
#include <vector>
#include <memory>

class MuseDisplay : public SpiLcdDisplay {
public:
    MuseDisplay(esp_lcd_panel_io_handle_t panel_io,
                esp_lcd_panel_handle_t panel,
                int width, int height,
                int offset_x, int offset_y,
                bool mirror_x, bool mirror_y, bool swap_xy);
    virtual ~MuseDisplay();

    // Override parent interface: chat bubbles
    virtual void SetChatMessage(const char* role, const char* content) override;

    // Override status display: Connecting / Ready / 👂
    virtual void SetStatus(const char* status) override;

    // Override camera preview: show fullscreen on top of all MuseDisplay screens
    virtual void SetPreviewImage(std::unique_ptr<LvglImage> image) override;

    // Chord button highlight (called on touch, auto-restores after 150ms)
    void HighlightChord(Chord chord);

    // Switch to lyrics view
    struct LyricLine { std::string chord; std::string lyric; };
    void ShowLyrics(const std::string& title, const std::vector<LyricLine>& lines);

    // Return to main screen
    void ShowMain();

    // Show companion mode screen
    void ShowCompanionMode();

    // Show "composing..." status
    void SetComposing(bool composing);

    // Scroll lyrics (dy>0 scroll down, <0 scroll up)
    void ScrollLyrics(int dy);

private:
    // Main screen LVGL objects
    lv_obj_t* main_screen_ = nullptr;
    // Avatar GIF animation
    lv_obj_t* avatar_img_ = nullptr;
    std::unique_ptr<LvglGif> avatar_gif_;
    // Status label in left column (Connecting / Ready / 👂)
    lv_obj_t* avatar_status_label_ = nullptr;
    lv_obj_t* chat_scroll_ = nullptr;
    lv_obj_t* chord_btns_[4] = {};
    lv_obj_t* composing_label_ = nullptr;

    // Lyrics screen LVGL objects
    lv_obj_t* lyrics_screen_ = nullptr;
    lv_obj_t* lyrics_title_ = nullptr;
    lv_obj_t* lyrics_scroll_ = nullptr;
    lv_obj_t* lyrics_chord_bar_[4] = {};

    // Companion mode: badge overlay on main_screen (hidden when in music mode)
    lv_obj_t* companion_screen_ = nullptr;    // small badge container
    lv_obj_t* companion_heart_label_ = nullptr;
    lv_obj_t* companion_name_label_ = nullptr;

    // Camera preview overlay (fullscreen, on top of all screens)
    lv_obj_t* camera_overlay_ = nullptr;   // dark container
    lv_obj_t* camera_img_obj_ = nullptr;   // lv_image for camera frame
    std::unique_ptr<LvglImage> camera_frame_cached_;  // keeps pixel data alive
    esp_timer_handle_t camera_hide_timer_ = nullptr;  // auto-hide after idle

    // Chat messages (keep at most 5)
    struct ChatMsg { std::string role; std::string content; };
    std::vector<ChatMsg> messages_;

    void BuildMainScreen();
    void BuildLyricsScreen();
    void BuildCompanionScreen();
    void BuildCameraOverlay();
    void RebuildChatBubbles();

    // Highlight timers (one per chord button)
    esp_timer_handle_t highlight_timer_[4] = {};

    // Timer callback context (value semantics, avoids heap allocation)
    struct HighlightTimerCtx {
        MuseDisplay* display;
        int index;
    };
    HighlightTimerCtx highlight_ctx_[4] = {};

    // Dark neon theme colors
    static constexpr lv_color_t kBgColor     = LV_COLOR_MAKE(0x06, 0x06, 0x10);
    static constexpr lv_color_t kPurple      = LV_COLOR_MAKE(0x9c, 0x5f, 0xff);
    static constexpr lv_color_t kPurpleLight = LV_COLOR_MAKE(0xd0, 0xad, 0xff);
    static constexpr lv_color_t kPurpleDark  = LV_COLOR_MAKE(0x3a, 0x1a, 0x6e);
    static constexpr lv_color_t kWhite       = LV_COLOR_MAKE(0xff, 0xff, 0xff);
    static constexpr lv_color_t kGray        = LV_COLOR_MAKE(0x44, 0x44, 0x44);

    // Camera preview auto-hide timeout (ms)
    static constexpr int kCameraHideTimeoutMs = 3000;
};
