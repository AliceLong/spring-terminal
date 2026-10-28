#pragma once
#include <cstdint>
#include <functional>
#include "spring_power_state.h"
namespace desk_gap {
void ConfigureGamePower(std::function<void(spring_power::Mode)> apply);
void PowerActivity(const char* reason);
bool PowerTouch(bool down);  // True: consume the complete wake-only touch gesture.
void PollGamePower();
spring_power::Mode GamePowerMode();
unsigned PowerIdleSeconds();
bool PowerMotionAllowed();
void PowerVoiceProgress();
}  // namespace desk_gap
