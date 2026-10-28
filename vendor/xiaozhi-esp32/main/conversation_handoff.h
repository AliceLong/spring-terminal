#pragma once
#include <cstdint>
// Main-task only. A server TTS stop alone does not mean the speaker is drained.
class ConversationHandoff {
public:
    void Arm(int64_t now) {
        armed_ = true;
        stopped_ = false;
        at_ = now;
    }
    void Cancel() { armed_ = stopped_ = false; }
    bool TtsStopped(int64_t now, bool valid_reply) {
        if (!Active(now) || !valid_reply) {
            Cancel();
            return false;
        }
        stopped_ = true;
        return true;
    }
    bool Ready(int64_t now, bool playback_idle) { return Active(now) && stopped_ && playback_idle; }
    bool Active(int64_t now) {
        if (armed_ && (now < at_ || now - at_ > 30000000))
            Cancel();
        return armed_;
    }

private:
    bool armed_ = false, stopped_ = false;
    int64_t at_ = 0;
};
