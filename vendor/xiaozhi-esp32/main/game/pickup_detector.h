#pragma once
#include <array>
#include <cmath>
#include <cstdint>
namespace desk_gap {
// Wake once after a settled device starts moving. Does not award currency.
class PickupDetector {
public:
    bool Sample(uint64_t ms, float x, float y, float z) {
        const std::array<float, 3> value{x, y, z};
        if (!initialized_ || ms < last_ms_ || ms - last_ms_ > 250) {
            initialized_ = true;
            baseline_ = previous_ = value;
            last_ms_ = quiet_since_ = ms;
            armed_ = false;
            moving_ = 0;
            return false;
        }
        float change = 0, offset = 0;
        for (int i = 0; i < 3; ++i) {
            change += (value[i] - previous_[i]) * (value[i] - previous_[i]);
            offset += (value[i] - baseline_[i]) * (value[i] - baseline_[i]);
        }
        previous_ = value;
        last_ms_ = ms;
        if (!armed_) {
            if (change > 0.035f * 0.035f)
                quiet_since_ = ms;
            for (int i = 0; i < 3; ++i)
                baseline_[i] += 0.08f * (value[i] - baseline_[i]);
            if (ms - quiet_since_ >= 1000 && ms >= rearm_after_) {
                armed_ = true;
                moving_ = 0;
            }
            return false;
        }
        if (offset > 0.16f * 0.16f) {
            if (++moving_ >= 3) {
                armed_ = false;
                moving_ = 0;
                quiet_since_ = ms;
                baseline_ = value;
                rearm_after_ = ms + 5000;
                return true;
            }
        } else
            moving_ = 0;
        return false;
    }

private:
    std::array<float, 3> baseline_{}, previous_{};
    uint64_t last_ms_ = 0, quiet_since_ = 0, rearm_after_ = 0;
    unsigned moving_ = 0;
    bool initialized_ = false, armed_ = false;
};
}  // namespace desk_gap
