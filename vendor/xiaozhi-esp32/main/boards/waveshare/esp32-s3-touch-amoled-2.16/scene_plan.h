#pragma once
#include <array>
#include <cstdint>
#include <limits>
#include "game/game_core.h"
#include "scene_art_pack.h"

namespace desk_gap {
struct SceneSprite {
    SceneArtKind art = SCENE_ART_IDLE;
    unsigned frame = 0;
    int x = 0, y = 0, depth = 0;
    bool visible = false, mirror = false;
};
struct ScenePlan {
    // actor, material, table, bed, rack, three delivery items, fx, sleep, blanket
    std::array<SceneSprite, 11> sprites{};
};
class ScenePlanner {
public:
    ScenePlan Update(const GameState& s) {
        ScenePlan plan;
        const auto now = s.elapsed_ms;
        if (!initialized_) {
            initialized_ = true;
            last_event_ = s.next_event_id - 1;  // Do not replay saved/offline deliveries/rewards.
            phase_ = s.phase;
            phase_at_ = now;
        }
        if (phase_ != s.phase || active_ != s.active_item) {
            phase_ = s.phase;
            active_ = s.active_item;
            phase_at_ = now;
        }
        for (unsigned i = 0; i < s.event_count; ++i) {
            const auto& event = s.events[i];
            if (event.id <= last_event_ || event.offline)
                continue;
            if (event.type == EventType::kDelivered && IsDeliveryItem(event.target))
                dropped_at_[DeliveryIndex(event.target)] = now;
            if (event.type == EventType::kBuildCompleted) {
                completed_at_ = now;
                completed_item_ = event.target;
            }
        }
        last_event_ = s.next_event_id - 1;
        const bool walking = s.visual_walking;
        auto pose = s.reacting ? SCENE_ART_REACT
                    : s.phase == CreatureState::kCarry
                        ? (s.carried_material == Item::kNone ? SCENE_ART_CARRY_LARGE
                                                             : SCENE_ART_CARRY_SMALL)
                    : walking                            ? SCENE_ART_WALK
                    : s.phase == CreatureState::kInspect ? SCENE_ART_INSPECT
                    : s.phase == CreatureState::kBuild
                        ? (s.active_item == Item::kCotton ? SCENE_ART_LAY_COTTON : SCENE_ART_BUILD)
                    : s.phase == CreatureState::kSleep    ? SCENE_ART_SLEEP
                    : s.using_furniture == Item::kTable   ? SCENE_ART_REST_TABLE
                    : s.using_furniture == Item::kStorage ? SCENE_ART_ORGANIZE
                                                          : SCENE_ART_IDLE;
        if (pose != pose_) {
            pose_ = pose;
            pose_at_ = now;
        }
        const auto pose_ms = now - pose_at_;
        auto& actor = plan.sprites[0];
        actor = {pose,         static_cast<unsigned>(pose_ms / 180), s.x, s.y, s.y * 10 + 2, true,
                 s.facing_left};
        if (pose == SCENE_ART_IDLE) {
            actor.frame = pose_ms % 3200 > 2950 ? 2 : static_cast<unsigned>(pose_ms / 1200 % 2);
        } else if (pose == SCENE_ART_SLEEP) {
            actor.frame = pose_ms < 1000 ? static_cast<unsigned>(pose_ms / 500)
                                         : 2 + static_cast<unsigned>((pose_ms - 1000) / 650 % 4);
            actor.mirror = false;
        } else if (pose == SCENE_ART_BUILD || pose == SCENE_ART_LAY_COTTON ||
                   pose == SCENE_ART_INSPECT || pose == SCENE_ART_REST_TABLE ||
                   pose == SCENE_ART_ORGANIZE) {
            actor.mirror = s.phase == CreatureState::kInspect
                               ? false
                               : s.active_item == Item::kCotton || s.using_furniture == Item::kBed;
        }
        if (s.phase == CreatureState::kCarry && s.carried_material != Item::kNone) {
            plan.sprites[1] = {
                s.carried_material == Item::kStick ? SCENE_ART_STICK : SCENE_ART_PAPER_SCRAP,
                1,
                s.x + (actor.mirror ? -13 : 13),
                s.y - 5,
                actor.depth + 1,
                true,
                false};
        }
        for (unsigned i = 0; i < 3; ++i) {
            constexpr Item items[] = {Item::kBottleCap, Item::kCotton, Item::kPaperclip};
            constexpr SceneArtKind furniture[] = {SCENE_ART_BOTTLE_CAP_TABLE, SCENE_ART_SOFT_BED,
                                                  SCENE_ART_STORAGE_RACK};
            constexpr SceneArtKind props[] = {SCENE_ART_BOTTLE_CAP, SCENE_ART_COTTON,
                                              SCENE_ART_PAPERCLIP};
            const auto item = items[i];
            const auto stage = ItemStage(s, item);
            const int fx = i == 0 ? 158 : i == 1 ? 68 : 198;
            const int fy = i == 1 ? 151 : 166;
            if (stage == ConstructionState::kBuilding || stage == ConstructionState::kCompleted) {
                unsigned frame = stage == ConstructionState::kCompleted ? (i == 1 ? 2 : 3) : 0;
                if (stage == ConstructionState::kBuilding) {
                    const auto progress =
                        BuildDuration(item) - std::min(BuildDuration(item), s.remaining_ms);
                    frame = i == 1 ? (progress >= BuildDuration(item) / 2 ? 1 : 0)
                                   : std::min<unsigned>(2, progress * 3 / BuildDuration(item));
                }
                auto& furniture_sprite = plan.sprites[2 + i];
                furniture_sprite = {furniture[i], frame, fx, fy, fy * 10, true, false};
                if (i == 1 && pose == SCENE_ART_SLEEP) {
                    furniture_sprite.frame = pose_ms < 1000 ? 2 : 3 + (pose_ms / 650 % 2);
                    furniture_sprite.depth = actor.depth - 1;
                    plan.sprites[10] = {SCENE_ART_SOFT_BED_FRONT,
                                        furniture_sprite.frame,
                                        fx,
                                        fy,
                                        actor.depth + 2,
                                        true,
                                        false};
                }
            } else if (stage != ConstructionState::kNotStarted) {
                auto& prop = plan.sprites[5 + i];
                prop = {props[i], 0, 170, 176, 1760, true, false};
                if (stage == ConstructionState::kDiscovered && s.active_item == item &&
                    s.phase == CreatureState::kCarry) {
                    prop.x = s.x + (actor.mirror ? -13 : 13);
                    prop.y = s.y - 5;
                    prop.depth = actor.depth + 1;
                } else if (stage == ConstructionState::kWaiting ||
                           stage == ConstructionState::kCarried) {
                    prop.x = fx;
                    prop.y = fy;
                    prop.depth = fy * 10;
                }
                if (stage == ConstructionState::kDelivered && dropped_at_[i] != kNever &&
                    now - dropped_at_[i] < 1200) {
                    const auto age = now - dropped_at_[i];
                    if (age < 700)
                        prop.y = 90 + age * age * 86 / (700 * 700);
                    else {
                        const auto bounce = (age - 700) % 250;
                        prop.y = 176 -
                                 (bounce < 125 ? bounce : 250 - bounce) * (age < 950 ? 8 : 3) / 125;
                        prop.frame = age < 820 ? 1 : 2;
                    }
                    prop.depth = 2200;
                    plan.sprites[8] = {SCENE_ART_DELIVERY_PORTAL,
                                       static_cast<unsigned>(age / 180),
                                       170,
                                       102,
                                       2199,
                                       true,
                                       false};
                }
            }
        }
        if (s.phase == CreatureState::kInspect && now - phase_at_ < 1600) {
            plan.sprites[8] = {SCENE_ART_DISCOVERY,
                               static_cast<unsigned>((now - phase_at_) / 400),
                               s.x,
                               s.y - 55,
                               2300,
                               true,
                               false};
        }
        if (s.phase == CreatureState::kBuild && !walking && s.active_item != Item::kCotton)
            plan.sprites[8] = {SCENE_ART_WOOD_CHIPS,
                               static_cast<unsigned>(now / 180),
                               s.x + (actor.mirror ? -15 : 15),
                               s.y - 12,
                               2300,
                               true,
                               false};
        if (pose == SCENE_ART_SLEEP)
            plan.sprites[9] = {SCENE_ART_SLEEP_MOTES,
                               static_cast<unsigned>(pose_ms / 450),
                               s.x + 23,
                               s.y - 30,
                               2301,
                               true,
                               false};
        if (completed_at_ != kNever && now - completed_at_ < 1600) {
            const int x = completed_item_ == Item::kBed       ? 68
                          : completed_item_ == Item::kStorage ? 198
                                                              : 158;
            plan.sprites[8] = {SCENE_ART_COMPLETION,
                               static_cast<unsigned>((now - completed_at_) / 400),
                               x,
                               170,
                               2400,
                               true,
                               false};
        }
        return plan;
    }

private:
    static unsigned DeliveryIndex(Item item) {
        return item == Item::kBottleCap ? 0 : item == Item::kCotton ? 1 : 2;
    }
    static constexpr uint64_t kNever = std::numeric_limits<uint64_t>::max();
    std::array<uint64_t, 3> dropped_at_ = {kNever, kNever, kNever};
    uint64_t completed_at_ = kNever, last_event_ = 0, phase_at_ = 0, pose_at_ = 0;
    Item completed_item_ = Item::kNone, active_ = Item::kNone;
    CreatureState phase_ = CreatureState::kIdle;
    SceneArtKind pose_ = SCENE_ART_IDLE;
    bool initialized_ = false;
};
}  // namespace desk_gap
