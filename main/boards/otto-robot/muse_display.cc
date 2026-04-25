#include "muse_display.h"
#include "muse_avatar_gif.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <cstring>
#include <cstdio>

#define TAG "MuseDisplay"

// Chord names in display order: C, Am, F, G
static const char* kChordNames[4] = {"C", "Am", "F", "G"};

MuseDisplay::MuseDisplay(esp_lcd_panel_io_handle_t panel_io,
                         esp_lcd_panel_handle_t panel,
                         int width, int height,
                         int offset_x, int offset_y,
                         bool mirror_x, bool mirror_y, bool swap_xy)
    : SpiLcdDisplay(panel_io, panel, width, height, offset_x, offset_y,
                    mirror_x, mirror_y, swap_xy) { /* proprietary implementation */ }

MuseDisplay::~MuseDisplay() { /* proprietary implementation */ }

void MuseDisplay::BuildMainScreen() { /* proprietary implementation */ }

void MuseDisplay::BuildLyricsScreen() { /* proprietary implementation */ }

void MuseDisplay::BuildCameraOverlay() { /* proprietary implementation */ }

void MuseDisplay::SetPreviewImage(std::unique_ptr<LvglImage> image) { /* proprietary implementation */ }

void MuseDisplay::SetChatMessage(const char* role, const char* content) { /* proprietary implementation */ }

void MuseDisplay::RebuildChatBubbles() { /* proprietary implementation */ }

void MuseDisplay::HighlightChord(Chord chord) { /* proprietary implementation */ }

void MuseDisplay::ShowLyrics(const std::string& title, const std::vector<LyricLine>& lines) { /* proprietary implementation */ }

void MuseDisplay::ShowMain() { /* proprietary implementation */ }

void MuseDisplay::BuildCompanionScreen() { /* proprietary implementation */ }

void MuseDisplay::ShowCompanionMode() { /* proprietary implementation */ }

void MuseDisplay::SetComposing(bool composing) { /* proprietary implementation */ }

void MuseDisplay::ScrollLyrics(int dy) { /* proprietary implementation */ }

void MuseDisplay::SetStatus(const char* status) { /* proprietary implementation */ }
