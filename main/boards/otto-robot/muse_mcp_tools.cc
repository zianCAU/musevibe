// main/boards/otto-robot/muse_mcp_tools.cc
#include "muse_display.h"
#include "chord_player.h"
#include "touch_chord_controller.h"
#include "mcp_server.h"
#include "board.h"
#include <esp_log.h>
#include <string>
#include <map>
#include <vector>

// 来自 otto_controller.cc 的情绪动作接口
extern void OttoExpressEmotion(const char* emotion);
// 来自 otto_robot.cc 的摄像头预览接口
extern void OttoStartCameraPreview();
extern void OttoStopCameraPreview();

#define TAG "MuseMcp"

static ChordPlayer* g_chord_player = nullptr;
static MuseDisplay* g_muse_display = nullptr;

static const std::map<std::string, Chord> kChordMap = {
    {"C", Chord::C}, {"Am", Chord::Am}, {"F", Chord::F}, {"G", Chord::G}
};

static const std::map<std::string, PlayStyle> kStyleMap = {
    {"arpeggio", PlayStyle::Arpeggio}, {"strum", PlayStyle::Strum}
};

void InitializeMuseMcpTools(ChordPlayer* player, MuseDisplay* display) { /* proprietary implementation */ }
