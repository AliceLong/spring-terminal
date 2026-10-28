#pragma once
#include <algorithm>
#include <array>
#include <cstdint>

namespace spring_collection {
// Gate-1 presentation only. This type has no Core, storage, resource or AI access.
constexpr unsigned kCapacity = 6, kSpriteSide = 32, kHitRadius = 28;
constexpr int kDurationMs = 6000, kFlyMs = 300;
enum class Kind : uint8_t { Water, Energy, Fiber, Leaf };
enum class Phase : uint8_t { Floating, Caught, Gone, Missed };
struct Particle {
    int16_t x = 0, y = 0;
    Kind kind = Kind::Water;
    Phase phase = Phase::Gone;
    uint32_t caught_ms = 0;
    int16_t caught_x = 0, caught_y = 0;
};
inline bool SegmentHit(int ax, int ay, int bx, int by, int px, int py, int radius = kHitRadius) {
    const int64_t dx = bx - ax, dy = by - ay, qx = px - ax, qy = py - ay;
    const int64_t length2 = dx * dx + dy * dy, dot = qx * dx + qy * dy;
    if (!length2 || dot <= 0)
        return qx * qx + qy * qy <= int64_t(radius) * radius;
    if (dot >= length2)
        return int64_t(px - bx) * (px - bx) + int64_t(py - by) * (py - by) <=
               int64_t(radius) * radius;
    const int64_t cross = qx * dy - qy * dx;
    return cross * cross <= int64_t(radius) * radius * length2;
}
class Session {
public:
    std::array<Particle, kCapacity> particles{};
    uint32_t batches = 0, hits = 0, misses = 0, ignored_shakes = 0, cancels = 0;
    unsigned count = 0, collected = 0;
    bool active = false, growth = false, voice_profile = false;

    bool Begin(uint64_t now, bool growing, bool voice) {
        if (active || (next_allowed && now < next_allowed)) {
            ++ignored_shakes;
            return false;
        }
        active = true;
        growth = growing;
        voice_profile = voice;
        count = growth ? 4 : kCapacity;
        collected = 0;
        started = now;
        next_allowed = now + kDurationMs + 800;
        ++batches;
        touching = false;
        for (unsigned i = 0; i < kCapacity; ++i) {
            particles[i] = {};
            if (i < count) {
                particles[i].phase = Phase::Floating;
                particles[i].kind = growth ? Kind::Leaf : static_cast<Kind>(i / 2);
            }
        }
        Tick(now);
        return true;
    }
    bool Tick(uint64_t now) {
        if (!active)
            return false;
        const auto elapsed = static_cast<uint32_t>(std::min<uint64_t>(now - started, kDurationMs));
        bool any = false;
        for (unsigned i = 0; i < count; ++i) {
            auto& p = particles[i];
            if (p.phase == Phase::Caught) {
                const auto t = std::min<uint64_t>(now - started - p.caught_ms, kFlyMs);
                // During voice, keep all drawing below the compact call card.
                const int target_x = voice_profile            ? p.caught_x
                                     : p.kind == Kind::Water  ? 115
                                     : p.kind == Kind::Energy ? 185
                                                              : 45;
                const int target_y = voice_profile ? 286 : 34;
                p.x = p.caught_x + (target_x - p.caught_x) * int(t) / kFlyMs;
                p.y = p.caught_y + (target_y - p.caught_y) * int(t) / kFlyMs;
                if (t == kFlyMs)
                    p.phase = Phase::Gone;
            } else if (p.phase == Phase::Floating) {
                if (elapsed == kDurationMs) {
                    p.phase = Phase::Missed;
                    ++misses;
                } else {
                    constexpr int source_x[] = {234, 130, 338};
                    const int origin = growth ? 137 + int(i) * 66 : source_x[i / 2];
                    const int direction = i % 2 ? 1 : -1;
                    const int drift = direction * int(std::min<unsigned>(elapsed, 240)) * 18 / 240;
                    const int wave = int((elapsed + i * 170) % 1200);
                    const int sway = ((wave < 600 ? wave : 1200 - wave) - 300) / 55;
                    p.x = std::clamp(origin + drift + sway, 106, 370);
                    const int top = voice_profile ? 296 : 228;
                    const int bottom = voice_profile ? 338 : 334;
                    const int source_y = voice_profile ? 338 : 342;
                    const int falling = top + (bottom - top) * std::max(0, int(elapsed) - 240) /
                                                  (kDurationMs - 240);
                    p.y = elapsed < 240
                              ? source_y + (top - source_y) * int(elapsed) / 240
                              : std::clamp(falling + (p.kind == Kind::Energy ? -4 : int(i % 2) * 7),
                                           top, bottom);
                }
            }
            any |= p.phase == Phase::Floating || p.phase == Phase::Caught;
        }
        if (!any)
            active = touching = false;
        return true;
    }
    void Down(int x, int y) {
        touching = active;
        previous_x = x;
        previous_y = y;
    }
    unsigned Move(int x, int y, uint64_t now) {
        if (!touching || !active)
            return 0;
        if (now - started >= kDurationMs) {
            Tick(now);
            return 0;
        }
        const int dx = x - previous_x, dy = y - previous_y;
        if (dx * dx + dy * dy < 9)
            return 0;  // A stationary tap is not the swipe exercise.
        unsigned found = 0;
        for (unsigned i = 0; i < count; ++i) {
            auto& p = particles[i];
            if (p.phase != Phase::Floating || !SegmentHit(previous_x, previous_y, x, y, p.x, p.y))
                continue;
            p.phase = Phase::Caught;
            p.caught_ms = static_cast<uint32_t>(now - started);
            p.caught_x = p.x;
            p.caught_y = p.y;
            ++found;
        }
        previous_x = x;
        previous_y = y;
        hits += found;
        collected += found;
        return found;
    }
    void Up() { touching = false; }
    void Cancel() {
        if (active)
            ++cancels;
        active = touching = false;
        for (auto& p : particles)
            p.phase = Phase::Gone;
    }

private:
    uint64_t started = 0, next_allowed = 0;
    int previous_x = 0, previous_y = 0;
    bool touching = false;
};
static_assert(sizeof(Session) <= 256, "Collection state must remain a bounded small pool");
}  // namespace spring_collection
