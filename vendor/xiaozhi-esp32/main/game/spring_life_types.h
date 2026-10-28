#pragma once
#include <array>
#include <cstdint>

namespace spring {
enum class LifeIntent : uint32_t { None, Window, Inspect, Rest };
enum class LifePhase : uint32_t { None, Requested, Traveling, Observing, Paused };
enum class LifeReason : uint32_t {
    None,
    WorkFinished,
    CheckPlant,
    RestAfterWork,
    PlayerInvitation,
    ArchiveReflection
};
enum class LifeResult : uint32_t { Completed, WorkInterrupted, Rebooted, Timeout, TargetChanged };
struct LifeRecord {
    uint32_t id = 0, intent = 0, target = 0, plant = 0, reason = 0, result = 0, second = 0,
             under_tree = 0, reached = 0;
};
struct LifeState {
    uint32_t intent = 0, phase = 0, reason = 0, target = 0, plant = 0, under_tree = 0, reached = 0;
    uint32_t elapsed = 0, travel_ms = 0, pause_ms = 0, cooldown = 120000, sequence = 0;
    std::array<uint32_t, 3> repeat_cooldown{};
    uint32_t count = 0;
    std::array<LifeRecord, 6> records{};
};
inline const char* LifeIntentName(uint32_t i) {
    static const char* names[] = {"none", "watch_window", "inspect_plant", "rest"};
    return names[i < 4 ? i : 0];
}
inline const char* LifePhaseName(uint32_t i) {
    static const char* names[] = {"none", "requested", "traveling", "observing",
                                  "paused_for_voice"};
    return names[i < 5 ? i : 0];
}
inline const char* LifeReasonName(uint32_t i) {
    static const char* names[] = {"none",
                                  "work_finished_observe_window",
                                  "routine_plant_check",
                                  "short_rest_after_work",
                                  "player_invitation",
                                  "post_archive_head_observation"};
    return names[i < 6 ? i : 0];
}
inline const char* LifeResultName(uint32_t i) {
    static const char* names[] = {"completed", "interrupted_by_work", "interrupted_by_restart",
                                  "expired", "target_changed"};
    return names[i < 5 ? i : 3];
}
}  // namespace spring
