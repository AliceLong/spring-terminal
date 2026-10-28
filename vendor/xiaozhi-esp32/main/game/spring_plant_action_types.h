#pragma once
#include <cstdint>
#include "spring_schedule_types.h"
namespace spring {
enum class PlantActionStatus : uint32_t {
    Unknown,
    Queued,
    Traveling,
    Running,
    Completed,
    Rejected,
    Failed,
    Cancelled
};
enum class PlantActionReason : uint32_t {
    None,
    Storage,
    Unavailable,
    InvalidTarget,
    StaleCycle,
    Busy,
    NotMature,
    AlreadyMature,
    UnpaidCycle,
    NoEnergy,
    Timeout,
    Interrupted,
    Restarted,
    QueueFull,
    SessionClosed,
    PlayerActivity
};
struct PlantAction {
    uint32_t id = 0, kind = 0, slot = 0, cycle = 0;
    PlantActionStatus status = PlantActionStatus::Unknown;
    PlantActionReason reason = PlantActionReason::None;
    uint32_t elapsed_ms = 0, work_ms = 0, chain = 0, amount = 0;
    bool arrived = false;
    ScheduleBlocker wait = ScheduleBlocker::None;
};
static_assert(sizeof(PlantAction) <= 48, "bounded runtime plant action");
inline bool PlantActionActive(const PlantAction& a) {
    return a.status == PlantActionStatus::Queued || a.status == PlantActionStatus::Traveling ||
           a.status == PlantActionStatus::Running;
}
inline const char* PlantActionStatusName(PlantActionStatus s) {
    static const char* n[] = {"UNKNOWN",   "QUEUED",   "TRAVELING", "RUNNING",
                              "COMPLETED", "REJECTED", "FAILED",    "CANCELLED"};
    return n[static_cast<unsigned>(s) < 8 ? static_cast<unsigned>(s) : 0];
}
inline const char* PlantActionReasonName(PlantActionReason r) {
    static const char* n[] = {"none",
                              "storage_unavailable",
                              "awakening_or_story_unavailable",
                              "invalid_target",
                              "stale_crop_cycle",
                              "important_work_busy",
                              "not_mature",
                              "already_mature_use_harvest",
                              "cycle_inputs_not_paid",
                              "no_energy",
                              "timeout",
                              "interrupted",
                              "restart_interrupted",
                              "queue_full",
                              "voice_session_closed",
                              "another_player_activity"};
    return n[static_cast<unsigned>(r) < 16 ? static_cast<unsigned>(r) : 0];
}
}  // namespace spring
