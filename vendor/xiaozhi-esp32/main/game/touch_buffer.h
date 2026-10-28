#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
namespace spring {
struct TouchSample {
    int16_t x = 0, y = 0;
    bool pressed = false, edge = false, reset = false;
    uint64_t sampled_us = 0;
};
// Closing an overlay consumes its whole gesture, including brief contact bounce.
// Caller uses the same lock as the edge queue, so old events cannot race a close.
class TouchTransitionGuard {
public:
    void Begin(uint64_t now) {
        blocked_ = true;
        until_ = now + 300000;
        released_at_ = 0;
    }
    bool Blocked(bool valid, bool down, uint64_t now) {
        if (!blocked_)
            return false;
        if (!valid || down)
            released_at_ = 0;
        else if (!released_at_)
            released_at_ = now;
        else if (now >= until_ && now - released_at_ >= 120000)
            blocked_ = false;
        return true;  // The release that rearms input is consumed as well.
    }

private:
    bool blocked_ = false;
    uint64_t until_ = 0, released_at_ = 0;
};
// A missing controller frame must never become a successful finger release.
// After a timeout, cancel once and require a real release before accepting down.
class TouchFrameFilter {
public:
    bool Update(bool valid, bool down, int16_t x, int16_t y, uint64_t now, TouchSample& out) {
        out = {};
        out.sampled_us = now;
        if (!valid) {
            if (!pressed_ || now - last_valid_ < 250000)
                return false;
            pressed_ = false;
            quarantined_ = true;
            out.reset = true;
            return true;
        }
        last_valid_ = now;
        if (quarantined_) {
            if (!down)
                quarantined_ = false;
            return false;
        }
        if (down) {
            release_at_ = 0;
            last_x_ = x;
            last_y_ = y;
        } else if (pressed_) {
            if (!release_at_)
                release_at_ = now;
            if (now - release_at_ < 50000)
                return false;
            release_at_ = 0;
        }
        pressed_ = down;
        // Controllers commonly report(0,0) on release. Keep the actual target.
        out.x = last_x_;
        out.y = last_y_;
        out.pressed = down;
        return true;
    }

private:
    bool pressed_ = false, quarantined_ = false;
    uint64_t last_valid_ = 0, release_at_ = 0;
    int16_t last_x_ = 0, last_y_ = 0;
};
// Merge contact bounce while still allowing deliberate taps about4 times/second.
class TapGuard {
public:
    bool Accept(uint64_t now) {
        if (seen_ && now >= last_ && now - last_ < 240000)
            return false;
        seen_ = true;
        last_ = now;
        return true;
    }

private:
    bool seen_ = false;
    uint64_t last_ = 0;
};
// Caller holds a short lock. Preserve down/up edges while the display is busy;
// coalesce move reports only, and cancel safely on bounded queue overflow.
class TouchBuffer {
public:
    void Cancel(uint64_t now) {
        head_ = count_ = 0;
        current_ = latest_ = {};
        TouchSample cancel{};
        cancel.reset = cancel.edge = true;
        cancel.sampled_us = now;
        Append(cancel);
    }
    bool Push(TouchSample sample) {
        sample.edge = sample.pressed != latest_.pressed;
        if (!sample.reset && !sample.edge &&
            (!sample.pressed || (sample.x == latest_.x && sample.y == latest_.y)))
            return false;
        latest_ = sample;
        if (!sample.reset && !sample.edge && count_) {
            auto& last = items_[(head_ + count_ - 1) % items_.size()];
            if (!last.edge && last.pressed) {
                last = sample;
                return true;
            }
        }
        if (count_ == items_.size()) {
            head_ = count_ = 0;
            ++overflows_;
            TouchSample cancel = sample;
            cancel.pressed = false;
            cancel.edge = true;
            cancel.reset = true;
            Append(cancel);
            if (!sample.pressed)
                return true;
            sample.edge = true;
        }
        Append(sample);
        return true;
    }
    bool Pop(TouchSample& sample) {
        if (!count_) {
            sample = current_;
            return false;
        }
        sample = items_[head_];
        head_ = (head_ + 1) % items_.size();
        --count_;
        current_ = sample;
        current_.reset = false;
        return true;
    }
    size_t Pending() const { return count_; }
    unsigned Overflows() const { return overflows_; }

private:
    void Append(TouchSample s) {
        items_[(head_ + count_) % items_.size()] = s;
        ++count_;
    }
    std::array<TouchSample, 16> items_{};
    size_t head_ = 0, count_ = 0;
    unsigned overflows_ = 0;
    TouchSample current_{}, latest_{};
};
}  // namespace spring
