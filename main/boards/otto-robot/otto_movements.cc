#include "otto_movements.h"

#include <algorithm>

#include "freertos/idf_additions.h"
#include "oscillator.h"

static const char* TAG = "OttoMovements";

#define HAND_HOME_POSITION 45

Otto::Otto() { /* proprietary implementation */ }

Otto::~Otto() { /* proprietary implementation */ }

unsigned long IRAM_ATTR millis() { /* proprietary implementation */ return {}; }

void Otto::Init(int left_leg, int right_leg, int left_foot, int right_foot, int left_hand,
                int right_hand) { /* proprietary implementation */ }

///////////////////////////////////////////////////////////////////
//-- ATTACH & DETACH FUNCTIONS ----------------------------------//
///////////////////////////////////////////////////////////////////
void Otto::AttachServos() { /* proprietary implementation */ }

void Otto::DetachServos() { /* proprietary implementation */ }

///////////////////////////////////////////////////////////////////
//-- OSCILLATORS TRIMS ------------------------------------------//
///////////////////////////////////////////////////////////////////
void Otto::SetTrims(int left_leg, int right_leg, int left_foot, int right_foot, int left_hand,
                    int right_hand) { /* proprietary implementation */ }

///////////////////////////////////////////////////////////////////
//-- BASIC MOTION FUNCTIONS -------------------------------------//
///////////////////////////////////////////////////////////////////
void Otto::MoveServos(int time, int servo_target[]) { /* proprietary implementation */ }

void Otto::MoveSingle(int position, int servo_number) { /* proprietary implementation */ }

void Otto::OscillateServos(int amplitude[SERVO_COUNT], int offset[SERVO_COUNT], int period,
                           double phase_diff[SERVO_COUNT], float cycle) { /* proprietary implementation */ }

void Otto::Execute(int amplitude[SERVO_COUNT], int offset[SERVO_COUNT], int period,
                   double phase_diff[SERVO_COUNT], float steps) { /* proprietary implementation */ }

void Otto::Execute2(int amplitude[SERVO_COUNT], int center_angle[SERVO_COUNT], int period,
                    double phase_diff[SERVO_COUNT], float steps) { /* proprietary implementation */ }

///////////////////////////////////////////////////////////////////
//-- HOME = Otto at rest position -------------------------------//
///////////////////////////////////////////////////////////////////
void Otto::Home(bool hands_down) { /* proprietary implementation */ }

bool Otto::GetRestState() { /* proprietary implementation */ return {}; }

void Otto::SetRestState(bool state) { /* proprietary implementation */ }

///////////////////////////////////////////////////////////////////
//-- PREDETERMINED MOTION SEQUENCES -----------------------------//
///////////////////////////////////////////////////////////////////
void Otto::Jump(float steps, int period) { /* proprietary implementation */ }

void Otto::Walk(float steps, int period, int dir, int amount) { /* proprietary implementation */ }

void Otto::Turn(float steps, int period, int dir, int amount) { /* proprietary implementation */ }

void Otto::Bend(int steps, int period, int dir) { /* proprietary implementation */ }

void Otto::ShakeLeg(int steps, int period, int dir) { /* proprietary implementation */ }

void Otto::Sit() { /* proprietary implementation */ }

void Otto::UpDown(float steps, int period, int height) { /* proprietary implementation */ }

void Otto::Swing(float steps, int period, int height) { /* proprietary implementation */ }

void Otto::TiptoeSwing(float steps, int period, int height) { /* proprietary implementation */ }

void Otto::Jitter(float steps, int period, int height) { /* proprietary implementation */ }

void Otto::AscendingTurn(float steps, int period, int height) { /* proprietary implementation */ }

void Otto::Moonwalker(float steps, int period, int height, int dir) { /* proprietary implementation */ }

void Otto::Crusaito(float steps, int period, int height, int dir) { /* proprietary implementation */ }

void Otto::Flapping(float steps, int period, int height, int dir) { /* proprietary implementation */ }

void Otto::WhirlwindLeg(float steps, int period, int amplitude) { /* proprietary implementation */ }

void Otto::HandsUp(int period, int dir) { /* proprietary implementation */ }

void Otto::HandsDown(int period, int dir) { /* proprietary implementation */ }

void Otto::HandWave(int dir) { /* proprietary implementation */ }

void Otto::Windmill(float steps, int period, int amplitude) { /* proprietary implementation */ }

void Otto::Takeoff(float steps, int period, int amplitude) { /* proprietary implementation */ }

void Otto::Fitness(float steps, int period, int amplitude) { /* proprietary implementation */ }

void Otto::Greeting(int dir, float steps) { /* proprietary implementation */ }

void Otto::Shy(int dir, float steps) { /* proprietary implementation */ }

void Otto::RadioCalisthenics() { /* proprietary implementation */ }

void Otto::MagicCircle() { /* proprietary implementation */ }

void Otto::Showcase() { /* proprietary implementation */ }

void Otto::EnableServoLimit(int diff_limit) { /* proprietary implementation */ }

void Otto::DisableServoLimit() { /* proprietary implementation */ }
