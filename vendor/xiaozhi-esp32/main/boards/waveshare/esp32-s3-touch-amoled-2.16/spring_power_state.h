#pragma once
#include <cstdint>
namespace spring_power {
enum class Mode { Play, Quiet, Sleep };
inline const char* Name(Mode mode) {
    return mode == Mode::Play ? "游玩" : mode == Mode::Quiet ? "展示" : "休眠";
}
// Independent of the gardener/world state; timestamps are monotonic milliseconds.
class State {
public:
    Mode mode = Mode::Play;
    uint64_t last_activity = 0, motion_block_until = 0;
    bool consume_touch = false;
    void Activity(uint64_t now) {
        if (mode == Mode::Sleep)
            motion_block_until = now + 1500;
        last_activity = now;
        mode = Mode::Play;
    }
    bool Touch(uint64_t now, bool down) {
        if (down) {
            if (!held && mode == Mode::Sleep)
                consume_touch = true;
            Activity(now);
            held_at = now;
        } else if (held) {
            Activity(now);
        }
        held = down;
        const bool consumed = consume_touch;
        if (!down)
            consume_touch = false;
        return consumed;
    }
    void Tick(uint64_t now, bool busy) {
        if (held && now - held_at > 1500) {
            held = false;  // A dead touch controller cannot pin the device awake.
            consume_touch = false;
        }
        if (busy || held) {
            Activity(now);
            return;
        }
        const auto idle = now - last_activity;
        mode = idle >= 120000 ? Mode::Sleep : idle >= 30000 ? Mode::Quiet : Mode::Play;
    }

private:
    bool held = false;
    uint64_t held_at = 0;
};
}  // namespace spring_power
