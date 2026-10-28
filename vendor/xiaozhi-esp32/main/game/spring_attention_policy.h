#pragma once
#include <cstdint>
namespace spring {
// Real-time pacing only; production time, permissions and save layout are unchanged.
struct AttentionPolicy {
    uint32_t model_ms, life_ms, repeat_ms, wish_ms;
};
constexpr AttentionPolicy AttentionPacing(bool roadshow) {
    return roadshow ? AttentionPolicy{30000, 45000, 90000, 90000}
                    : AttentionPolicy{120000, 120000, 300000, 600000};
}
}  // namespace spring
