#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include "game/spring_core.h"

// Presentation controller only. Never changes the world, resources or task timers.
namespace spring_gardener {
enum class Voice { None, Listen, Wait, Speak };
enum class Mode {
    Hover,
    Move,
    Takeoff,
    Land,
    Tend,
    Water,
    Harvest,
    Build,
    Call,
    Listen,
    Speak,
    Recover,
    Squint,
    Wave,
    Rest,
    Celebrate,
    WatchWindow,
    InspectPlant,
    Explore
};
enum class Animation {
    Hover,
    Takeoff,
    Rest,
    Tend,
    Water,
    Build,
    Listen,
    Speak,
    Recover,
    Squint,
    Wave,
    Care,
    Harvest,
    InspectLeft,
    InspectRight,
    RestTree,
    WatchWindow
};
inline const char* Name(Mode mode) {
    static const char* names[] = {"hover",     "move",         "takeoff",       "land",   "tend",
                                  "water",     "harvest",      "build",         "call",   "listen",
                                  "speak",     "recover",      "squint",        "wave",   "rest",
                                  "celebrate", "watch_window", "inspect_plant", "explore"};
    static_assert(sizeof(names) / sizeof(names[0]) == unsigned(Mode::Explore) + 1);
    return names[static_cast<unsigned>(mode)];
}
inline bool CanHarvestOnShake(const spring::Snapshot& s, Voice voice) {
    return voice == Voice::None && s.work == spring::Work::Rest && s.economy.construction == 255 &&
           !s.economy.recycle_ms && !s.economy.journey_work && !s.tree_work_ms &&
           !s.economy.emergency_work;
}
inline unsigned WaterFrame(uint64_t elapsed, uint32_t remaining) {
    // A single task has a beginning and end; never loop the put-away gesture.
    if (remaining <= 1000)
        return 3;
    if (elapsed < 800)
        return 0;
    if (elapsed < 1500)
        return 1;
    return 2;
}
struct Pose {
    Mode mode = Mode::Hover;
    Animation animation = Animation::Hover;
    Voice voice = Voice::None;
    unsigned frame = 0;
    float x = 246, y = 292;
};
inline float LifeX(const spring::Snapshot& s) {
    if (s.life.intent == 3 && s.life.under_tree == 2)
        return 240;
    if (s.life.intent == 2 && s.life.target == 6)
        return 282;
    if (s.life.intent == 1)
        return 282;
    if (s.life.intent == 2 && s.life.target == 4)
        return 236;
    if (s.life.intent == 2 && s.life.target == 5)
        return 386;
    if (s.life.intent == 2)
        return s.life.target == 3 ? 416 : s.life.target == 0 ? 150 : 106 + s.life.target * 104;
    return s.life.under_tree ? 320 : 282;
}
inline float LifeY(const spring::Snapshot& s) {
    if (s.life.intent == 3 && s.life.under_tree == 2)
        return 285;
    if (s.life.intent == 2 && s.life.target == 6)
        return 268;
    if (s.life.intent == 2 && s.life.target == 4)
        return 270;
    if (s.life.intent == 2 && s.life.target == 5)
        return 248;
    return s.life.intent == 1 ? 254 : s.life.intent == 3 && s.life.under_tree ? 414 : 319;
}
inline bool LifeArrived(const spring::Snapshot& s, const Pose& p) {
    const bool mode = s.life.intent == 1   ? p.mode == Mode::WatchWindow
                      : s.life.intent == 2 ? p.mode == Mode::InspectPlant
                                           : s.life.intent == 3 && p.mode == Mode::Rest;
    return mode && std::abs(p.x - LifeX(s)) <= 3 && std::abs(p.y - LifeY(s)) <= 3;
}
inline float PlantWorkX(unsigned slot) {
    return slot == 0 ? 204 : slot == 1 ? 238 : slot == 2 ? 294 : 416;
}
inline bool PlantWorkArrived(const spring::Snapshot& s, const Pose& p) {
    const auto& a = s.plant_action;
    return spring::PlantActionActive(a) &&
           (p.mode == Mode::Water || p.mode == Mode::Tend || p.mode == Mode::Harvest) &&
           std::abs(p.x - PlantWorkX(a.slot)) <= 3 && std::abs(p.y - 292) <= 3;
}
class Controller {
public:
    void Activity(uint64_t now) { idle_at_ = now; }
    bool Wave(uint64_t now) {
        if (wave_until_ > now)
            return false;
        wave_at_ = now;
        wave_until_ = now + 1100;
        return true;
    }
    bool Shake(uint64_t now, uint32_t random, Voice voice, bool harvested = false) {
        if (have_shake_ && now - shake_at_ < 1000)
            return false;
        have_shake_ = true;
        shake_at_ = now;
        nudge_until_ = now + 650;
        pose_.x += voice == Voice::None ? 1.2f : .5f;
        pose_.y -= .5f;  // Immediate acknowledgement; worker supplies the eased continuation.
        quiet_ack_ = voice != Voice::None;
        const bool resting = mode_ == Mode::Rest || mode_ == Mode::Land;
        if (voice != Voice::None || working_ || resting || mode_ == Mode::Takeoff ||
            mode_ == Mode::Celebrate) {
            if (resting)
                idle_at_ = now;  // Open eyes first; takeoff follows after350ms.
            return true;
        }
        idle_at_ = now;
        if (have_big_ && now - big_at_ < 6500)
            return true;  // Small acknowledgement still works during the big-action cooldown.
        have_big_ = true;
        big_at_ = now;
        unsigned next = harvested ? 2 : random % 3;
        if (!harvested && next == reaction_)
            next = (next + 1 + ((random >> 8) & 1)) % 3;
        reaction_ = next;
        reaction_until_ = now + 1100;
        reaction_at_ = now;
        return true;
    }
    Pose Step(const spring::Snapshot& s, uint64_t now, Voice voice) {
        if (!started_ || s.demo != demo_ || now < last_at_) {
            started_ = true;
            demo_ = s.demo;
            last_at_ = state_at_ = idle_at_ = now;
            built_ = s.economy.built;
            tree_ = s.tree_stage;
            reaction_until_ = celebration_until_ = nudge_until_ = wave_until_ = 0;
            have_big_ = have_shake_ = working_ = had_life_ = false;
            mode_ = s.story_stage ? Mode::Hover : Mode::Rest;
            x_ = 246;
            y_ = s.story_stage ? 292 : 319;
            velocity_ = vertical_velocity_ = 0;
        }
        const float dt = std::min<uint64_t>(now - last_at_, 100) / 1000.f;
        last_at_ = now;
        if (had_life_ && !s.life.intent)
            idle_at_ = now;  // Leave a landed life activity through the normal takeoff.
        had_life_ = s.life.intent != 0;
        if (s.economy.built != built_ || s.tree_stage > tree_) {
            if ((s.economy.built & ~built_) || s.tree_stage > tree_)
                celebration_until_ = now + 2400;
            built_ = s.economy.built;
            tree_ = s.tree_stage;
        }
        if (voice != last_voice_) {
            voice_at_ = now;
            last_voice_ = voice;
        }
        if (voice == Voice::Wait && now - voice_at_ >= 30000)
            voice = Voice::None;
        const bool task = s.work != spring::Work::Rest || s.economy.construction < 8 ||
                          s.economy.recycle_ms || s.economy.journey_work || s.tree_work_ms ||
                          s.economy.emergency_work;
        working_ = task;
        const bool invited_life =
            s.life.intent &&
            s.life.reason == static_cast<unsigned>(spring::LifeReason::PlayerInvitation);
        if (task || voice != Voice::None)
            idle_at_ = now;
        float destination = 246, height = 292;
        Mode desired = Mode::Hover;
        if (!s.story_stage)
            desired = Mode::Rest;
        else if (now < wave_until_) {
            desired = Mode::Wave;
            destination = x_;
            height = y_;
        } else if (s.plant_action.status == spring::PlantActionStatus::Traveling ||
                   s.plant_action.status == spring::PlantActionStatus::Running) {
            destination = PlantWorkX(s.plant_action.slot);
            height = 292;
            desired = s.plant_action.kind == 2 ? Mode::Harvest : Mode::Water;
            if (mode_ == Mode::Rest || mode_ == Mode::Land ||
                (mode_ == Mode::Takeoff && now - state_at_ < 900))
                desired = Mode::Takeoff;
            else if (std::abs(destination - x_) > 3 || std::abs(height - y_) > 3)
                desired = Mode::Move;
        } else if (voice != Voice::None && !invited_life && !task) {
            desired = voice == Voice::Speak ? Mode::Speak : Mode::Listen;
            reaction_until_ = celebration_until_ = 0;
            destination = x_;
            height = y_;
        } else if (now < celebration_until_) {
            desired = Mode::Celebrate;
            destination = x_;
            height = y_;
        } else if (now < reaction_until_) {
            desired = reaction_ == 0 ? Mode::Recover : reaction_ == 1 ? Mode::Squint : Mode::Wave;
            destination = x_;
            height = y_;
        } else if (mode_ == Mode::Rest && now < nudge_until_ && now - shake_at_ < 350) {
            desired = Mode::Rest;
            destination = x_;
        } else if ((!s.life.intent || task) && ((mode_ == Mode::Takeoff && now - state_at_ < 900) ||
                                                (mode_ == Mode::Rest && now - idle_at_ < 20000)))
            desired = Mode::Takeoff;
        else if (task && !s.energy && !s.economy.emergency_work &&
                 s.work != spring::Work::Harvest) {
            desired = Mode::Hover;
            destination = x_;
            height = y_;
        } else if (task) {
            if (s.work != spring::Work::Harvest && s.economy.journey_work) {
                // An accepted journey is exploration, not facility repair.
                desired = Mode::Explore;
                destination = s.economy.journey_task == 1 ? 204 : 294;
                height = s.economy.journey_task == 1 ? 292 : 254;
            } else if (s.work != spring::Work::Harvest &&
                       (s.work == spring::Work::Build || s.economy.construction < 8 ||
                        (s.economy.recycle_ms || s.economy.journey_work || s.tree_work_ms ||
                         s.economy.emergency_work))) {
                desired = Mode::Build;
                const auto facility = s.economy.construction;
                destination =
                    facility == 2 || facility == 3 || facility == 5 || s.economy.recycle_ms ? 204
                                                                                            : 294;
                height = facility == 2                     ? 244
                         : facility == 3                   ? 266
                         : facility == 7 || s.tree_work_ms ? 260
                                                           : 292;
            } else {
                destination = s.target == 0 ? 204 : s.target == 1 ? 238 : 294;
                desired = s.work == spring::Work::Water     ? Mode::Water
                          : s.work == spring::Work::Harvest ? Mode::Harvest
                                                            : Mode::Tend;
            }
            if (mode_ == Mode::Rest || mode_ == Mode::Land ||
                (mode_ == Mode::Takeoff && now - state_at_ < 900))
                desired = Mode::Takeoff;
            else if ((std::abs(destination - x_) > 3 || std::abs(height - y_) > 3) &&
                     desired != Mode::Harvest)
                desired = Mode::Move;
        } else if (s.life.intent && (s.life.phase == 2 || s.life.phase == 3)) {
            destination = LifeX(s);
            height = LifeY(s);
            if (s.life.intent == 3) {
                if (mode_ == Mode::Rest)
                    desired = Mode::Rest;
                else if (mode_ == Mode::Land)
                    desired = now - state_at_ >= 900 ? Mode::Rest : Mode::Land;
                else if (std::abs(destination - x_) > 3 || std::abs(height - 27 - y_) > 3) {
                    desired = Mode::Move;
                    height -= 27;  // Approach above the actual floor, then land gently.
                } else
                    desired = Mode::Land;
            } else if (mode_ == Mode::Rest || mode_ == Mode::Land ||
                       (mode_ == Mode::Takeoff && now - state_at_ < 900))
                desired = Mode::Takeoff;
            else if (std::abs(destination - x_) > 3 || std::abs(height - y_) > 3)
                desired = Mode::Move;
            else
                desired = s.life.intent == 1 ? Mode::WatchWindow : Mode::InspectPlant;
        } else if (!s.economy.enabled && now - idle_at_ >= 20000) {
            destination = 282;
            if (mode_ == Mode::Rest)
                desired = Mode::Rest;
            else if (mode_ == Mode::Land && now - state_at_ >= 900)
                desired = Mode::Rest;
            else if (mode_ == Mode::Land)
                desired = Mode::Land;
            else if (std::abs(destination - x_) > 3 || std::abs(height - y_) > 3)
                desired = Mode::Move;
            else
                desired = Mode::Land;
        } else if (s.notices && now - idle_at_ < 2800)
            desired = Mode::Call;
        else {
            destination = 246 + std::sin((now - idle_at_) / 2400.f) * 34;
            // Different periods create soft rising/falling arcs rather than a
            // horizontal track. Stay in the open window space above the plots.
            height = 274 + std::cos((now - idle_at_) / 3000.f) * 24;
            desired = std::abs(destination - x_) > 3 || std::abs(height - y_) > 3 ? Mode::Move
                                                                                  : Mode::Hover;
        }
        if (desired != mode_) {
            if (desired == Mode::Takeoff)
                takeoff_y_ = y_;
            if (desired == Mode::Land)
                landing_y_ = y_;
            mode_ = desired;
            state_at_ = now;
        }
        const bool freeze = (voice != Voice::None && !invited_life && !task) ||
                            mode_ == Mode::Recover || mode_ == Mode::Squint || mode_ == Mode::Wave;
        if (freeze)
            velocity_ = vertical_velocity_ = 0;
        else {
            const float gap = destination - x_;
            const float speed = std::clamp(gap * 1.8f, -20.f, 20.f);
            velocity_ += (speed - velocity_) * std::min(1.f, dt * 4.f);
            const float step = velocity_ * dt;
            if (std::abs(step) > std::abs(gap)) {
                x_ = destination;
                velocity_ = 0;
            } else
                x_ = std::clamp(x_ + step, 140.f, 420.f);
        }
        const float rest_y = s.life.intent == 3 && s.life.under_tree ? LifeY(s) : 319.f;
        if (mode_ == Mode::Rest)
            height = rest_y;
        if (mode_ == Mode::Land)
            height = landing_y_ + (rest_y - landing_y_) * std::min(1.f, (now - state_at_) / 900.f);
        if (mode_ == Mode::Takeoff)
            height = takeoff_y_ - 27 * std::min(1.f, (now - state_at_) / 900.f);
        if (mode_ == Mode::Rest || mode_ == Mode::Land || mode_ == Mode::Takeoff) {
            vertical_velocity_ = 0;
            y_ += (height - y_) * std::min(1.f, dt * 6.f);
        } else if (!freeze) {
            const float gap = height - y_;
            const float speed = std::clamp(gap * 1.8f, -18.f, 18.f);
            vertical_velocity_ += (speed - vertical_velocity_) * std::min(1.f, dt * 4.f);
            const float step = vertical_velocity_ * dt;
            if (std::abs(step) > std::abs(gap)) {
                y_ = height;
                vertical_velocity_ = 0;
            } else
                y_ = std::clamp(y_ + step, 240.f, 414.f);
        }
        Pose out;
        out.mode = mode_;
        out.voice = voice;
        out.x = x_;
        out.y = y_;
        const uint64_t elapsed = now - state_at_;
        out.frame = (elapsed / 260) % 4;
        switch (mode_) {
            case Mode::Takeoff:
                out.animation = Animation::Takeoff;
                out.frame = std::min<uint64_t>(3, elapsed / 225);
                break;
            case Mode::Land:
                out.animation = Animation::Takeoff;
                out.frame = 3 - std::min<uint64_t>(3, elapsed / 225);
                break;
            case Mode::Rest:
                out.animation =
                    s.life.intent == 3 && s.life.under_tree ? Animation::RestTree : Animation::Rest;
                out.frame = (elapsed / (out.animation == Animation::RestTree ? 900 : 650)) % 4;
                break;
            case Mode::InspectPlant: {
                const float plant_x = s.life.target == 6   ? x_ - 1
                                      : s.life.target == 4 ? 194
                                      : s.life.target == 5 ? 430
                                      : s.life.target == 3 ? 446
                                                           : 130 + 104 * s.life.target;
                out.animation = plant_x < x_ ? Animation::InspectLeft : Animation::InspectRight;
                out.frame = (elapsed / 500) % 4;
                // A routine right-facing inspection includes empty-handed leaf care.
                // Left-facing checks keep their separately drawn readable-number sprites.
                if (out.animation == Animation::InspectRight && elapsed >= 4000) {
                    out.animation = Animation::Care;
                    out.frame = ((elapsed - 4000) / 550) % 4;
                }
                break;
            }
            case Mode::Tend:
                out.animation = Animation::Care;
                out.frame = (elapsed / 550) % 4;
                break;
            case Mode::Harvest:
                out.animation = Animation::Harvest;
                out.frame = std::min<uint64_t>(3, elapsed / 400);
                break;
            case Mode::WatchWindow:
                out.animation = Animation::WatchWindow;
                out.frame = (elapsed / 650) % 4;
                break;
            case Mode::Explore:
                out.animation =
                    s.economy.journey_task == 1 ? Animation::InspectLeft : Animation::WatchWindow;
                out.frame = (elapsed / 650) % 4;
                break;
            case Mode::Water:
                out.animation = Animation::Water;
                out.frame = WaterFrame(elapsed, s.work_ms);
                if (spring::PlantActionActive(s.plant_action)) {
                    const auto& a = s.plant_action;
                    const auto& plant = a.slot < 3 ? s.plots[a.slot] : s.economy.fourth;
                    if (plant.plant == spring::Plant::Moss) {
                        out.animation = Animation::Care;
                        out.frame = (elapsed / 550) % 4;
                    } else
                        out.frame =
                            WaterFrame(elapsed, 5000 - std::min<uint32_t>(5000u, a.work_ms));
                }
                break;
            case Mode::Build:
                out.animation = Animation::Build;
                break;
            case Mode::Listen:
                out.animation = Animation::Listen;
                break;
            case Mode::Speak:
                out.animation = Animation::Speak;
                break;
            case Mode::Recover:
                out.animation = Animation::Recover;
                out.frame = std::min<uint64_t>(3, (now - reaction_at_) / 220);
                break;
            case Mode::Squint:
                out.animation = Animation::Squint;
                out.frame = std::min<uint64_t>(3, (now - reaction_at_) / 220);
                break;
            case Mode::Wave:
            case Mode::Call:
            case Mode::Celebrate:
                out.animation = Animation::Wave;
                if (mode_ == Mode::Wave)
                    out.frame = std::min<uint64_t>(
                        3, (now - (wave_until_ > now ? wave_at_ : reaction_at_)) / 220);
                break;
            default:
                out.animation = Animation::Hover;
                out.frame = (elapsed / 420) % 4;
                break;
        }
        if (mode_ != Mode::Rest && mode_ != Mode::Land && mode_ != Mode::Takeoff)
            out.y += std::sin(now / 650.f) * 2;
        if (mode_ == Mode::Recover) {
            const float u = std::min(1.f, (now - reaction_at_) / 1100.f);
            out.x += std::sin(u * 6.283185f) * 4 * (1 - u);
        }
        if (mode_ == Mode::Wave) {
            const float u = std::min(1.f, (now - reaction_at_) / 1100.f);
            out.y -= std::sin(u * 3.14159f) * 4;
        } else if (mode_ == Mode::Celebrate)
            out.y -= std::sin(std::min(1.f, elapsed / 2400.f) * 3.14159f) * 9;
        if (now < nudge_until_) {
            const float u = (now - shake_at_) / 650.f;
            const float strength = quiet_ack_ ? .65f : 2.5f;
            out.x += std::sin(u * 6.283185f) * strength * (1 - u);
            out.y -= std::sin(u * 3.14159f) * (quiet_ack_ ? .4f : 1.4f);
            if (mode_ == Mode::Rest && now - shake_at_ < 350) {
                out.animation = Animation::Takeoff;
                out.frame = 0;  // Open eyes, feet stay extended.
            }
        }
        return pose_ = out;
    }
    const Pose& Get() const { return pose_; }

private:
    Pose pose_;
    Mode mode_ = Mode::Hover;
    Voice last_voice_ = Voice::None;
    float x_ = 246, y_ = 292, takeoff_y_ = 319, landing_y_ = 292, velocity_ = 0,
          vertical_velocity_ = 0;
    uint64_t last_at_ = 0, state_at_ = 0, idle_at_ = 0, voice_at_ = 0, shake_at_ = 0,
             reaction_until_ = 0, celebration_until_ = 0, nudge_until_ = 0, reaction_at_ = 0,
             big_at_ = 0, wave_at_ = 0, wave_until_ = 0;
    uint32_t built_ = 0, tree_ = 0, reaction_ = 99;
    bool started_ = false, demo_ = false, have_shake_ = false, have_big_ = false, working_ = false,
         quiet_ack_ = false, had_life_ = false;
};
}  // namespace spring_gardener
