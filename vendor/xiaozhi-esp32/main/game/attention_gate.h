#pragma once
#include <cstdint>
namespace desk_gap {
// Real uptime, independent of currency, game speed and wall-clock synchronization.
class AttentionGate {
public:
    bool TryShake(uint64_t now_ms, bool idle) {
        if (!idle || (used_ && (now_ms < last_ms_ || now_ms - last_ms_ < 30000)))
            return false;
        last_ms_ = now_ms;
        used_ = true;
        return true;
    }

private:
    uint64_t last_ms_ = 0;
    bool used_ = false;
};
}  // namespace desk_gap
