#pragma once
#include <cmath>
namespace spring_ui {
// Board gravity projection: X changes with the screen's forward/back pitch;
// Y is the lateral component. Keep pitch out of the left/right control angle.
inline float LateralTilt(float x, float y, float z) {
    return std::atan2(y, std::sqrt(x * x + z * z)) * 57.29578f;
}
}  // namespace spring_ui
