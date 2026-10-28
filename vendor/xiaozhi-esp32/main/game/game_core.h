#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <mutex>

namespace desk_gap {
enum class CreatureState : uint32_t {
    kIdle,
    kWander,
    kScavenge,
    kInspect,
    kCarry,
    kBuild,
    kSleep,
    kReact
};
enum class ConstructionState : uint32_t {
    kNotStarted,
    kDelivered,
    kDiscovered,
    kWaiting,
    kBuilding,
    kCompleted,
    kCarried
};
enum class EventType : uint32_t {
    kDelivered,
    kDiscovered,
    kCarried,
    kMaterial,
    kBuildStarted,
    kBuildCompleted,
    kFurnitureUsed,
    kResonance,
    kOffline,
    kMigrated,
    kVoice,
    kShake,
    kKnock
};
enum class Item : uint32_t {
    kBottleCap,
    kStick,
    kPaper,
    kTable,
    kNone,
    kCotton,
    kPaperclip,
    kBed,
    kStorage
};
inline const char* WorldStateName(CreatureState s) {
    constexpr const char* names[] = {"IDLE",  "WANDER", "SCAVENGE", "INSPECT",
                                     "CARRY", "BUILD",  "SLEEP",    "REACT"};
    return names[static_cast<uint32_t>(s) < 8 ? static_cast<uint32_t>(s) : 0];
}
inline const char* ConstructionName(ConstructionState s) {
    constexpr const char* names[] = {"NOT_STARTED",       "ITEM_DELIVERED", "ITEM_DISCOVERED",
                                     "WAITING_MATERIALS", "BUILDING",       "COMPLETED",
                                     "ITEM_CARRIED"};
    return names[static_cast<uint32_t>(s) < 7 ? static_cast<uint32_t>(s) : 0];
}
inline const char* ItemName(Item s) {
    constexpr const char* names[] = {"bottle_cap",       "stick",    "paper_scrap",
                                     "bottle_cap_table", "none",     "cotton",
                                     "paperclip",        "soft_bed", "storage_rack"};
    return names[static_cast<uint32_t>(s) < 9 ? static_cast<uint32_t>(s) : 4];
}
inline const char* EventName(EventType s) {
    constexpr const char* names[] = {"ITEM_DELIVERED",
                                     "ITEM_DISCOVERED",
                                     "ITEM_CARRIED",
                                     "MATERIAL_FOUND",
                                     "CONSTRUCTION_STARTED",
                                     "CONSTRUCTION_COMPLETED",
                                     "FURNITURE_USED",
                                     "RESONANCE_GAINED",
                                     "OFFLINE_SETTLED",
                                     "SAVE_MIGRATED",
                                     "VOICE_INTERACTION_COMPLETED",
                                     "SHAKE_DETECTED",
                                     "KNOCK_DETECTED"};
    return names[static_cast<uint32_t>(s) < 13 ? static_cast<uint32_t>(s) : 9];
}
struct GameEvent {
    uint64_t id = 0;
    uint64_t game_ms = 0;
    int64_t timestamp = 0;  // UTC seconds; 0 = unknown, not fabricated.
    EventType type = EventType::kMaterial;
    Item target = Item::kNone;
    uint32_t quantity = 0;
    uint32_t offline = 0;
};
struct LegacyProgress {
    uint32_t version = 1, paper = 0, collection_id = 0;
};
// Stored atomically under a new key. The old progress key is retained for migration.
struct ProgressV2 {
    uint32_t version = 2;
    uint32_t paper = 0, sticks = 0, resonance = 0, claimed_rewards = 0;
    ConstructionState construction = ConstructionState::kNotStarted;
    CreatureState phase = CreatureState::kScavenge;
    uint32_t rng = 0x6d2b79f5;
    uint64_t remaining_ms = 300000;
    uint64_t game_ms = 0;
    uint64_t resonance_fraction_ms = 0;
    uint64_t unsynced_online_ms = 0;
    uint64_t revision = 1, next_event_id = 1;
    int64_t last_wall_epoch = 0;
    uint32_t event_count = 0;
    std::array<GameEvent, 20> events{};  // Oldest first, bounded and persisted.
};
// V2 layout stays intact for safe migration; all new state is committed in world_v3.
struct PersistentProgress : ProgressV2 {
    ConstructionState bed = ConstructionState::kNotStarted;
    ConstructionState storage = ConstructionState::kNotStarted;
    Item active_item = Item::kNone;
    Item carried_material = Item::kNone;
    uint64_t suspended_search_ms = 0;
    uint64_t last_voice_epoch = 0, last_shake_epoch = 0;
    uint32_t reward_day = 0, voice_today = 0, shake_today = 0;
    uint32_t routine = 0;
    PersistentProgress() { version = 3; }
};
inline bool IsDeliveryItem(Item item) {
    return item == Item::kBottleCap || item == Item::kCotton || item == Item::kPaperclip;
}
inline uint32_t ItemCost(Item item) {
    return item == Item::kCotton ? 15 : item == Item::kPaperclip ? 25 : 0;
}
inline ConstructionState ItemStage(const PersistentProgress& s, Item item) {
    return item == Item::kBottleCap ? s.construction : item == Item::kCotton ? s.bed : s.storage;
}
inline Item FurnitureFor(Item item) {
    return item == Item::kBottleCap ? Item::kTable
           : item == Item::kCotton  ? Item::kBed
                                    : Item::kStorage;
}
inline uint64_t BuildDuration(Item item) {
    return item == Item::kBottleCap ? 120000 : item == Item::kCotton ? 45000 : 60000;
}
inline uint32_t Comfort(const PersistentProgress& s) {
    return 10 + (s.construction == ConstructionState::kCompleted ? 20 : 0) +
           (s.bed == ConstructionState::kCompleted ? 15 : 0);
}
inline uint32_t GrowthStage(const PersistentProgress& s) {
    return s.bed == ConstructionState::kCompleted            ? 2
           : s.construction == ConstructionState::kCompleted ? 1
                                                             : 0;
}
inline const char* DeliveryBlock(const PersistentProgress& s, Item item, bool storage_available) {
    if (!IsDeliveryItem(item))
        return "unknown_item";
    if (!storage_available)
        return "save_unavailable";
    if (ItemStage(s, item) != ConstructionState::kNotStarted)
        return "already_delivered";
    if (s.resonance < ItemCost(item))
        return "insufficient_resonance";
    return "available";
}
struct GameState : PersistentProgress {
    uint64_t saved_revision = 0, elapsed_ms = 0;
    uint32_t simulation_ticks = 0, touches = 0, time_scale = 1;
    uint32_t offline_seconds = 0;
    bool storage_available = false, clock_synced = false, clock_rollback = false;
    bool motion_sensor_available = false;
    bool reacting = false, carrying_paper = false, carrying_stick = false;
    bool visual_walking = false, facing_left = false;  // Runtime only; never persisted.
    Item using_furniture = Item::kNone;
    CreatureState creature_state = CreatureState::kScavenge;
    int x = 68, y = 150, sprite_frame = 0;
};
enum class RewardStatus {
    kGranted,
    kFull,
    kCooldown,
    kDailyLimit,
    kClockUnavailable,
    kStorageUnavailable
};
struct RewardOutcome {
    RewardStatus status;
    uint32_t amount = 0, cooldown_seconds = 0;
};
class GameCore {
public:
    static constexpr uint64_t kSearchMs = 300000, kBuildMs = 120000;
    static bool Valid(const PersistentProgress& p) {
        if (p.version != 3 || p.paper > 1000000 || p.sticks > 1000000 || p.resonance > 100 ||
            p.claimed_rewards > 63 || static_cast<uint32_t>(p.phase) > 6 || p.remaining_ms == 0 ||
            p.remaining_ms > kSearchMs || p.event_count > 20 || p.resonance_fraction_ms >= 360000 ||
            !p.rng || !p.revision || !p.next_event_id || p.last_wall_epoch < 0 ||
            p.last_wall_epoch > 4102444800LL || p.game_ms > 3153600000000ULL ||
            p.unsynced_online_ms > 3153600000000ULL || p.suspended_search_ms > kSearchMs ||
            p.voice_today > 15 || p.shake_today > 10 || p.last_voice_epoch > 4102444800ULL ||
            p.last_shake_epoch > 4102444800ULL || p.reward_day > 47483 || p.routine > 3)
            return false;
        if (p.active_item != Item::kNone && !IsDeliveryItem(p.active_item))
            return false;
        if (p.carried_material != Item::kNone && p.carried_material != Item::kStick &&
            p.carried_material != Item::kPaper)
            return false;
        unsigned busy = 0;
        for (auto item : {Item::kBottleCap, Item::kCotton, Item::kPaperclip}) {
            auto c = ItemStage(p, item);
            if (static_cast<uint32_t>(c) > 6)
                return false;
            if (c == ConstructionState::kDiscovered || c == ConstructionState::kBuilding ||
                c == ConstructionState::kCarried) {
                ++busy;
                if (p.active_item != item)
                    return false;
            }
            if (c == ConstructionState::kBuilding && p.phase != CreatureState::kBuild)
                return false;
            if (c == ConstructionState::kDiscovered && p.phase != CreatureState::kCarry)
                return false;
            if (c == ConstructionState::kCarried && p.phase != CreatureState::kIdle)
                return false;
        }
        if (busy > 1)
            return false;
        if (p.active_item != Item::kNone && p.phase != CreatureState::kBuild &&
            p.phase != CreatureState::kCarry && p.phase != CreatureState::kInspect &&
            !(p.phase == CreatureState::kIdle &&
              (ItemStage(p, p.active_item) == ConstructionState::kDelivered ||
               ItemStage(p, p.active_item) == ConstructionState::kCarried)))
            return false;
        if (p.phase == CreatureState::kInspect &&
            (p.active_item == Item::kNone ||
             ItemStage(p, p.active_item) != ConstructionState::kDelivered))
            return false;
        if (p.phase == CreatureState::kCarry &&
            !((p.active_item != Item::kNone &&
               ItemStage(p, p.active_item) == ConstructionState::kDiscovered) ||
              (p.active_item == Item::kNone && p.carried_material != Item::kNone)))
            return false;
        if (p.carried_material != Item::kNone &&
            (p.phase != CreatureState::kCarry || p.active_item != Item::kNone))
            return false;
        if (p.phase == CreatureState::kBuild &&
            (p.active_item == Item::kNone ||
             ItemStage(p, p.active_item) != ConstructionState::kBuilding))
            return false;
        if (p.phase == CreatureState::kBuild && p.active_item == Item::kBottleCap && p.sticks < 2)
            return false;
        if ((p.construction >= ConstructionState::kDiscovered && !(p.claimed_rewards & 1)) ||
            (p.construction == ConstructionState::kCompleted && (p.claimed_rewards & 3) != 3) ||
            (p.construction != ConstructionState::kCompleted && (p.claimed_rewards & 6)) ||
            (p.construction == ConstructionState::kNotStarted && (p.claimed_rewards & 7)) ||
            ((p.claimed_rewards & 8) != 0) != (p.bed == ConstructionState::kCompleted) ||
            ((p.claimed_rewards & 16) && p.bed != ConstructionState::kCompleted) ||
            ((p.claimed_rewards & 32) && p.storage != ConstructionState::kCompleted))
            return false;
        uint64_t maximum = p.phase == CreatureState::kScavenge  ? kSearchMs
                           : p.phase == CreatureState::kBuild   ? BuildDuration(p.active_item)
                           : p.phase == CreatureState::kInspect ? 4000
                           : p.phase == CreatureState::kCarry   ? 5000
                           : p.phase == CreatureState::kSleep   ? 20000
                                                                : 10000;
        if (p.remaining_ms > maximum)
            return false;
        for (uint32_t i = 0; i < p.event_count; ++i) {
            const auto& e = p.events[i];
            if (!e.id || e.id >= p.next_event_id || (i && e.id <= p.events[i - 1].id) ||
                static_cast<uint32_t>(e.type) > 12 || static_cast<uint32_t>(e.target) > 8 ||
                e.offline > 1 || e.game_ms > p.game_ms || e.timestamp < 0 ||
                e.timestamp > 4102444800LL)
                return false;
        }
        return true;
    }
    bool MigrateV2(const ProgressV2& old, bool available) {
        if (old.version != 2 || static_cast<uint32_t>(old.construction) > 5 ||
            static_cast<uint32_t>(old.phase) > 6 || old.claimed_rewards > 7)
            return false;
        // Preserve the complete old progress and event sequence. Infer the old single task.
        PersistentProgress next;
        static_cast<ProgressV2&>(next) = old;
        next.version = 3;
        if (old.construction == ConstructionState::kDiscovered ||
            old.construction == ConstructionState::kBuilding ||
            old.phase == CreatureState::kInspect ||
            old.construction == ConstructionState::kDelivered)
            next.active_item = Item::kBottleCap;
        for (uint32_t i = 0; i < std::min<uint32_t>(old.event_count, 20); ++i)
            if (static_cast<uint32_t>(old.events[i].type) > 9 ||
                static_cast<uint32_t>(old.events[i].target) > 4)
                return false;
        if (!Restore(next, available))
            return false;
        std::lock_guard<std::mutex> lock(mutex_);
        AddEvent(EventType::kMigrated, Item::kNone, 2);
        return true;
    }
    bool Restore(const PersistentProgress& p, bool available) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (state_.simulation_ticks || !Valid(p))
            return false;
        static_cast<PersistentProgress&>(state_) = p;
        state_.storage_available = available;
        state_.saved_revision = p.revision;
        UpdateVisual();
        return true;
    }
    bool Migrate(const LegacyProgress& p, bool available) {
        if (p.version != 1 || p.paper > 1000000 || p.paper != p.collection_id)
            return false;
        PersistentProgress fresh;
        fresh.paper = p.paper;
        if (!Restore(fresh, available))
            return false;
        std::lock_guard<std::mutex> lock(mutex_);
        AddEvent(EventType::kMigrated, Item::kPaper, p.paper);
        return true;
    }
    void Tick(uint64_t elapsed_ms) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (elapsed_ms < state_.elapsed_ms)
            return;
        // Bound catch-up work even after a stalled task, same limit as offline.
        uint64_t delta =
            std::min<uint64_t>(elapsed_ms - state_.elapsed_ms, 43200000 / state_.time_scale);
        if (!state_.clock_synced)
            state_.unsynced_online_ms += delta;
        state_.elapsed_ms = elapsed_ms;
        ++state_.simulation_ticks;
        Advance(delta * state_.time_scale, false);
        UpdateVisual();
    }
    bool DeliverBottleCap() { return Deliver(Item::kBottleCap); }
    bool Deliver(Item item) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!IsDeliveryItem(item) || !state_.storage_available ||
            ItemStage(state_, item) != ConstructionState::kNotStarted ||
            state_.resonance < ItemCost(item))
            return false;
        state_.resonance -= ItemCost(item);
        Stage(item) = ConstructionState::kDelivered;
        AddEvent(EventType::kDelivered, item, 1);
        // Do not discard an in-flight search or interrupt building/another delivery.
        if (state_.phase != CreatureState::kBuild && state_.phase != CreatureState::kInspect &&
            state_.phase != CreatureState::kCarry && state_.active_item == Item::kNone) {
            if (state_.phase == CreatureState::kScavenge)
                state_.suspended_search_ms = state_.remaining_ms;
            state_.active_item = item;
            SetPhase(CreatureState::kIdle, 1000);
        }
        UpdateVisual();
        return true;
    }
    bool OnVoiceInteraction() { return CompleteInteraction(false).amount > 0; }
    bool OnShake() { return CompleteInteraction(true).amount > 0; }
    RewardOutcome CompleteInteraction(bool shake) { return InteractionReward(shake); }
    void SetTimeScale(uint32_t scale) {
        std::lock_guard<std::mutex> lock(mutex_);
        state_.time_scale = scale == 10 ? 10 : 1;
    }
    bool OnTouch(bool direct_attention = true) {
        std::lock_guard<std::mutex> lock(mutex_);
        ++state_.touches;
        bool knocked = false;
        if (last_touch_ms_ && state_.elapsed_ms - last_touch_ms_ >= 80 &&
            state_.elapsed_ms - last_touch_ms_ <= 500) {
            AddEvent(EventType::kKnock, Item::kNone, 1);
            knocked = true;
            last_touch_ms_ = 0;
        } else
            last_touch_ms_ = state_.elapsed_ms;
        if (direct_attention || knocked)
            react_until_ms_ = state_.elapsed_ms + 1500;
        UpdateVisual();
        return knocked;
    }
    // Invoke after a real network UTC sync. Apply offline progress once, then only
    // maintain the wall checkpoint. No valid clock => no invented offline income.
    void SyncClock(int64_t now) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (now < 1767225600 || now > 4102444800LL)
            return;
        if (!clock_resolved_) {
            clock_resolved_ = true;
            const auto prior = state_.last_wall_epoch;
            if (prior && now < prior) {
                state_.clock_rollback = true;
                return;
            }
            if (prior) {
                // Exclude online time already simulated while awaiting sync.
                const int64_t seconds = std::max<int64_t>(
                    0, now - prior - static_cast<int64_t>(state_.unsynced_online_ms / 1000));
                state_.offline_seconds = static_cast<uint32_t>(std::min<int64_t>(seconds, 43200));
                Advance(static_cast<uint64_t>(state_.offline_seconds) * 1000, true);
                if (state_.offline_seconds)
                    AddEvent(EventType::kOffline, Item::kNone, state_.offline_seconds, true);
            }
        }
        if (now < state_.last_wall_epoch) {
            state_.clock_rollback = true;
            return;
        }
        state_.unsynced_online_ms = 0;
        state_.clock_rollback = false;
        state_.clock_synced = true;
        state_.last_wall_epoch = now;
        UpdateVisual();
    }
    void MarkSaved(uint64_t revision) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (revision <= state_.revision)
            state_.saved_revision = std::max(state_.saved_revision, revision);
    }
    GameState Snapshot() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return state_;
    }

private:
    void AddEvent(EventType type, Item target, uint32_t quantity, bool offline = false) {
        if (state_.event_count == 20) {
            for (size_t i = 1; i < 20; ++i)
                state_.events[i - 1] = state_.events[i];
            --state_.event_count;
        }
        state_.events[state_.event_count++] = {
            state_.next_event_id++,
            state_.game_ms,
            offline ? 0 : (state_.clock_synced ? state_.last_wall_epoch : 0),
            type,
            target,
            quantity,
            offline ? 1u : 0u};
        ++state_.revision;
    }
    void RewardOnce(uint32_t flag, bool offline) {
        if (state_.claimed_rewards & flag)
            return;
        state_.claimed_rewards |= flag;
        const auto amount = std::min<uint32_t>(10, 100 - state_.resonance);
        state_.resonance += amount;
        AddEvent(EventType::kResonance, flag == 2 ? Item::kTable : Item::kBottleCap, amount,
                 offline);
    }
    void SetPhase(CreatureState s, uint64_t duration) {
        state_.phase = s;
        state_.remaining_ms = duration;
        ++state_.revision;
    }
    ConstructionState& Stage(Item item) {
        return item == Item::kBottleCap ? state_.construction
               : item == Item::kCotton  ? state_.bed
                                        : state_.storage;
    }
    bool StartPriority(bool offline) {
        for (auto item : {Item::kBottleCap, Item::kCotton, Item::kPaperclip}) {
            if (Stage(item) == ConstructionState::kWaiting &&
                (item != Item::kBottleCap || state_.sticks >= 2)) {
                state_.active_item = item;
                Stage(item) = ConstructionState::kBuilding;
                SetPhase(CreatureState::kBuild, BuildDuration(item));
                AddEvent(EventType::kBuildStarted, FurnitureFor(item), 1, offline);
                return true;
            }
        }
        for (auto item : {Item::kBottleCap, Item::kCotton, Item::kPaperclip}) {
            if (Stage(item) == ConstructionState::kDelivered) {
                state_.active_item = item;
                SetPhase(CreatureState::kInspect, 4000);
                return true;
            }
        }
        return false;
    }
    void ChooseNext(bool offline) {
        state_.active_item = Item::kNone;
        if (StartPriority(offline))
            return;
        if (state_.construction == ConstructionState::kCompleted && !(state_.claimed_rewards & 4)) {
            state_.claimed_rewards |= 4;
            SetPhase(CreatureState::kIdle, 8000);
            AddEvent(EventType::kFurnitureUsed, Item::kTable, 1, offline);
        } else if (state_.bed == ConstructionState::kCompleted && !(state_.claimed_rewards & 16)) {
            state_.claimed_rewards |= 16;
            SetPhase(CreatureState::kSleep, 20000);
            AddEvent(EventType::kFurnitureUsed, Item::kBed, 1, offline);
        } else if (state_.storage == ConstructionState::kCompleted &&
                   !(state_.claimed_rewards & 32)) {
            state_.claimed_rewards |= 32;
            state_.routine = 3;
            SetPhase(CreatureState::kIdle, 8000);
            AddEvent(EventType::kFurnitureUsed, Item::kStorage, 1, offline);
        } else {
            const auto remaining = state_.suspended_search_ms;
            state_.suspended_search_ms = 0;
            SetPhase(CreatureState::kScavenge, remaining ? remaining : kSearchMs);
        }
    }
    void EndPhase(bool offline) {
        const auto item = state_.active_item;
        switch (state_.phase) {
            case CreatureState::kInspect:
                Stage(item) = ConstructionState::kDiscovered;
                AddEvent(EventType::kDiscovered, item, 1, offline);
                if (item == Item::kBottleCap)
                    RewardOnce(1, offline);
                SetPhase(CreatureState::kCarry, 5000);
                break;
            case CreatureState::kCarry:
                if (state_.carried_material != Item::kNone) {
                    auto material = state_.carried_material;
                    auto& stock = material == Item::kStick ? state_.sticks : state_.paper;
                    if (stock < 1000000) {
                        ++stock;
                        AddEvent(EventType::kMaterial, material, 1, offline);
                    }
                    AddEvent(EventType::kCarried, material, 1, offline);
                    state_.carried_material = Item::kNone;
                    state_.active_item = Item::kNone;
                    if (!StartPriority(offline))
                        SetPhase(CreatureState::kIdle, 8000);
                } else {
                    Stage(item) = ConstructionState::kCarried;
                    AddEvent(EventType::kCarried, item, 1, offline);
                    SetPhase(CreatureState::kIdle, 1000);
                }
                break;
            case CreatureState::kBuild: {
                Stage(item) = ConstructionState::kCompleted;
                auto target = FurnitureFor(item);
                if (item == Item::kBottleCap)
                    state_.sticks -= 2;
                AddEvent(EventType::kBuildCompleted, target, 1, offline);
                if (item == Item::kBottleCap)
                    RewardOnce(2, offline);
                if (item == Item::kCotton && !(state_.claimed_rewards & 8)) {
                    state_.claimed_rewards |= 8;
                    const auto amount = std::min<uint32_t>(15, 100 - state_.resonance);
                    state_.resonance += amount;
                    AddEvent(EventType::kResonance, Item::kBed, amount, offline);
                }
                ChooseNext(offline);
                break;
            }
            case CreatureState::kScavenge:
                state_.rng = state_.rng * 1664525u + 1013904223u;
                state_.carried_material =
                    ((state_.construction == ConstructionState::kWaiting && state_.sticks < 2) ||
                     state_.rng % 100 < 70)
                        ? Item::kStick
                        : Item::kPaper;
                state_.active_item = Item::kNone;
                SetPhase(CreatureState::kCarry, 5000);
                break;
            case CreatureState::kIdle:
                if (item != Item::kNone && Stage(item) == ConstructionState::kCarried) {
                    Stage(item) = ConstructionState::kWaiting;
                    ChooseNext(offline);
                } else if (item != Item::kNone && Stage(item) == ConstructionState::kDelivered) {
                    // A ready recipe has priority even when another object just arrived.
                    if (!StartPriority(offline))
                        ChooseNext(offline);
                } else if (!StartPriority(offline)) {
                    state_.active_item = Item::kNone;
                    SetPhase(CreatureState::kWander, 10000);
                }
                break;
            case CreatureState::kWander:
                if (!StartPriority(offline))
                    SetPhase(CreatureState::kSleep,
                             state_.bed == ConstructionState::kCompleted ? 20000 : 10000);
                break;
            case CreatureState::kSleep:
                state_.routine = (state_.routine + 1) % 4;
                ChooseNext(offline);
                break;
            case CreatureState::kReact:
                break;  // Visual overlay only, never a persisted phase.
        }
    }
    RewardOutcome InteractionReward(bool shake) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!state_.storage_available)
            return {RewardStatus::kStorageUnavailable};
        AddEvent(shake ? EventType::kShake : EventType::kVoice, Item::kNone, 1);
        if (!state_.clock_synced || state_.clock_rollback)
            return {RewardStatus::kClockUnavailable};
        const uint64_t now = state_.last_wall_epoch;
        const uint32_t day = (now + 8 * 3600) / 86400;  // Project timezone: Asia/Shanghai.
        if (day < state_.reward_day)
            return {RewardStatus::kClockUnavailable};
        if (day > state_.reward_day) {
            state_.reward_day = day;
            state_.voice_today = state_.shake_today = 0;
        }
        auto& last = shake ? state_.last_shake_epoch : state_.last_voice_epoch;
        auto& today = shake ? state_.shake_today : state_.voice_today;
        const uint32_t amount = shake ? 2 : 5, cap = shake ? 10 : 15, cooldown = shake ? 30 : 300;
        if (state_.resonance == 100)
            return {RewardStatus::kFull};
        if (today + amount > cap)
            return {RewardStatus::kDailyLimit};
        if (last && now < last + cooldown)
            return {RewardStatus::kCooldown, 0, static_cast<uint32_t>(last + cooldown - now)};
        last = now;
        today += amount;
        const auto actual = std::min<uint32_t>(amount, 100 - state_.resonance);
        state_.resonance += actual;
        AddEvent(EventType::kResonance, Item::kNone, actual);
        return {RewardStatus::kGranted, actual};
    }
    void Advance(uint64_t delta, bool offline) {
        while (delta) {
            const auto step = std::min(delta, state_.remaining_ms);
            const uint64_t total = state_.resonance_fraction_ms + step;
            const uint32_t gain = std::min<uint64_t>(total / 360000, 100 - state_.resonance);
            state_.resonance += gain;
            state_.resonance_fraction_ms = total % 360000;

            state_.remaining_ms -= step;
            state_.game_ms += step;
            if (gain)
                AddEvent(EventType::kResonance, Item::kNone, gain, offline);
            delta -= step;
            if (!state_.remaining_ms)
                EndPhase(offline);
        }
    }
    void UpdateVisual() {
        state_.creature_state = state_.phase;
        state_.x = 68;
        state_.y = 150;
        state_.using_furniture = Item::kNone;
        state_.carrying_paper = state_.carrying_stick = false;
        state_.sprite_frame = state_.elapsed_ms % 3200 > 2980 ? 1 : 0;
        if (state_.phase == CreatureState::kScavenge) {
            // Long search remains visible; last half of the trip returns home.
            const int travel = state_.remaining_ms > kSearchMs / 2
                                   ? (kSearchMs - state_.remaining_ms) * 97 / (kSearchMs / 2)
                                   : 45 + state_.remaining_ms * 52 / (kSearchMs / 2);
            state_.x += travel;
            state_.y += travel * 28 / 97;
            const bool returning = state_.remaining_ms <= kSearchMs / 2;
            const bool stick =
                (state_.construction == ConstructionState::kWaiting && state_.sticks < 2) ||
                (state_.rng * 1664525u + 1013904223u) % 100 < 70;
            state_.carrying_stick = returning && stick;
            state_.carrying_paper = returning && !stick;
            state_.sprite_frame = returning ? 4 : 2 + (state_.elapsed_ms / 180) % 2;
        } else if (state_.phase == CreatureState::kInspect) {
            const int progress = (4000 - state_.remaining_ms) * 100 / 4000;
            state_.x += progress;
            state_.y += progress * 18 / 100;
            state_.sprite_frame = 2 + (state_.elapsed_ms / 180) % 2;
        } else if (state_.phase == CreatureState::kCarry) {
            state_.x = 150 + state_.remaining_ms * 18 / 5000;
            state_.y = 154 + state_.remaining_ms * 14 / 5000;
            state_.sprite_frame = 4;
            if (state_.active_item == Item::kCotton) {
                state_.x = 68 + state_.remaining_ms * 100 / 5000;
                state_.y = 150 + state_.remaining_ms * 18 / 5000;
            }
            if (state_.active_item == Item::kPaperclip) {
                state_.x = 196 - state_.remaining_ms * 28 / 5000;
                state_.y = 154 + state_.remaining_ms * 14 / 5000;
            }
            if (state_.carried_material != Item::kNone) {
                state_.x = 68 + state_.remaining_ms * 45 / 5000;
                state_.y = 150 + state_.remaining_ms * 13 / 5000;
            }
            state_.carrying_stick = state_.carried_material == Item::kStick;
            state_.carrying_paper = state_.carried_material == Item::kPaper;
        } else if (state_.phase == CreatureState::kBuild) {
            state_.x = state_.active_item == Item::kCotton      ? 68
                       : state_.active_item == Item::kPaperclip ? 196
                                                                : 150;
            state_.y = 154;
            state_.sprite_frame = (state_.elapsed_ms / 350) % 2;
        } else if (state_.phase == CreatureState::kSleep) {
            state_.sprite_frame = 6 + (state_.elapsed_ms /
                                       (state_.bed == ConstructionState::kCompleted ? 1100 : 700)) %
                                          2;
            if (state_.bed == ConstructionState::kCompleted) {
                state_.y = 146;
                state_.using_furniture = Item::kBed;
            }
        } else if (state_.phase == CreatureState::kWander) {
            state_.x += (10000 - state_.remaining_ms) * 50 / 10000;
            state_.sprite_frame = 2 + (state_.elapsed_ms / 180) % 2;
        } else if (state_.active_item != Item::kNone &&
                   ItemStage(state_, state_.active_item) == ConstructionState::kCarried) {
            state_.x = state_.active_item == Item::kCotton      ? 68
                       : state_.active_item == Item::kPaperclip ? 196
                                                                : 150;
            state_.y = 154;
        } else if (state_.storage == ConstructionState::kCompleted && state_.routine == 3) {
            state_.x = 196;
            state_.y = 154;
            state_.using_furniture = Item::kStorage;
        } else if (state_.construction == ConstructionState::kCompleted) {
            state_.x = 150;
            state_.y = 154;
            state_.using_furniture = Item::kTable;
        }
        // Visual travel is independent of long search timers. The old five-minute
        // interpolation looked stationary, and every phase snapped to a new anchor.
        // These are continuous, collision-free floor paths; furniture only affects
        // draw order. Resource timing, construction and persistent state stay intact.
        int target_x = state_.x, target_y = state_.y;
        auto work_position = [&]() {
            target_x = state_.active_item == Item::kCotton      ? 90
                       : state_.active_item == Item::kPaperclip ? 176
                                                                : 136;
            target_y = state_.active_item == Item::kCotton ? 156 : 166;
        };
        if (state_.phase == CreatureState::kScavenge || state_.phase == CreatureState::kWander) {
            constexpr int path_x[] = {70, 114, 194, 170};
            constexpr int path_y[] = {164, 180, 170, 146};
            if (std::abs(visual_x_q8_ / 256 - path_x[visual_waypoint_]) <= 1 &&
                std::abs(visual_y_q8_ / 256 - path_y[visual_waypoint_]) <= 1)
                visual_waypoint_ = (visual_waypoint_ + 1) % 4;
            target_x = path_x[visual_waypoint_];
            target_y = path_y[visual_waypoint_];
            state_.carrying_paper = state_.carrying_stick = false;
        } else if (state_.phase == CreatureState::kInspect) {
            target_x = 148;
            target_y = 170;
        } else if (state_.phase == CreatureState::kCarry) {
            if (state_.carried_material != Item::kNone) {
                target_x = 110;
                target_y = 164;
            } else
                work_position();
        } else if (state_.phase == CreatureState::kBuild ||
                   (state_.active_item != Item::kNone &&
                    ItemStage(state_, state_.active_item) == ConstructionState::kCarried)) {
            work_position();
        } else if (state_.phase == CreatureState::kIdle && state_.using_furniture == Item::kTable) {
            target_x = 136;
            target_y = 166;
        } else if (state_.phase == CreatureState::kIdle &&
                   state_.using_furniture == Item::kStorage) {
            target_x = 176;
            target_y = 166;
        }
        const uint64_t dt = std::min<uint64_t>(200, state_.elapsed_ms - visual_clock_ms_);
        visual_clock_ms_ = state_.elapsed_ms;
        const int dx = target_x * 256 - visual_x_q8_, dy = target_y * 256 - visual_y_q8_;
        const int distance = std::max(std::abs(dx), std::abs(dy));
        const int step = std::min<int>(distance, dt * 22 * 256 / 1000);
        state_.visual_walking = distance > 256;
        if (distance && step) {
            visual_x_q8_ += dx * step / distance;
            visual_y_q8_ += dy * step / distance;
            if (std::abs(dx) > 128)
                state_.facing_left = dx < 0;
        }
        state_.x = visual_x_q8_ / 256;
        state_.y = visual_y_q8_ / 256;
        if (state_.visual_walking)
            state_.using_furniture = Item::kNone;
        state_.reacting = state_.elapsed_ms < react_until_ms_;
        if (state_.reacting) {
            state_.creature_state = CreatureState::kReact;
            state_.sprite_frame = 1;
        }
    }
    mutable std::mutex mutex_;
    GameState state_;
    uint64_t react_until_ms_ = 0, last_touch_ms_ = 0;
    uint64_t visual_clock_ms_ = 0;
    int visual_x_q8_ = 68 * 256, visual_y_q8_ = 150 * 256;
    unsigned visual_waypoint_ = 0;
    bool clock_resolved_ = false;
};
}  // namespace desk_gap
