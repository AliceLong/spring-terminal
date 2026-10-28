#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
namespace desk_gap {
// Four alternating impulses on one axis, not a single acceleration spike.
// g units; preliminary thresholds must be calibrated on the actual enclosure.
class ShakeDetector {
public:
    bool Sample(uint64_t ms, float x, float y, float z) {
        const std::array<float, 3> value{x, y, z};
        if (!initialized_ || ms < last_sample_ || ms - last_sample_ > 200) {
            gravity_ = value;
            initialized_ = true;
            warm_until_ = ms + 1500;
            count_ = 0;
        }
        last_sample_ = ms;
        std::array<float, 3> dynamic{};
        for (int i = 0; i < 3; ++i) {
            dynamic[i] = value[i] - gravity_[i];
            gravity_[i] += 0.08f * dynamic[i];
        }
        if (ms < warm_until_ || ms < quiet_until_)
            return false;
        int axis = 0;
        for (int i = 1; i < 3; ++i)
            if (std::fabs(dynamic[i]) > std::fabs(dynamic[axis]))
                axis = i;
        if (std::fabs(dynamic[axis]) < 0.85f)
            return false;
        const int sign = dynamic[axis] > 0 ? 1 : -1;
        if (count_ && (ms - first_ > 1200 || ms - last_impulse_ > 450 || axis != axis_))
            count_ = 0;
        if (!count_) {
            axis_ = axis;
            first_ = ms;
            sign_ = sign;
            last_impulse_ = ms;
            count_ = 1;
            return false;
        }
        if (sign == sign_ || ms - last_impulse_ < 80)
            return false;
        sign_ = sign;
        last_impulse_ = ms;
        if (++count_ < 4)
            return false;
        count_ = 0;
        quiet_until_ = ms + 1500;
        return true;
    }

private:
    std::array<float, 3> gravity_{};
    bool initialized_ = false;
    uint64_t last_sample_ = 0, warm_until_ = 0, quiet_until_ = 0, first_ = 0, last_impulse_ = 0;
    int axis_ = 0, sign_ = 0, count_ = 0;
};
}  // namespace desk_gap
