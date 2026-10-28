#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>
namespace spring {
enum class MemoryActor : uint32_t { Unknown, Robot, Player, System, VoiceRequest };
enum class MemoryAction : uint32_t {
    WorldEvent,
    Requested,
    Traveling,
    Arrived,
    Paused,
    Resumed,
    Finished,
    Proposed,
    Approved,
    Rejected,
    Superseded,
    Started,
    Completed,
    Scheduled,
    Applied,
    TrackingStarted
};
struct Experience {
    uint32_t id = 0, chain = 0, second = 0, category = 0, action = 0, actor = 0, kind = 0,
             target = 0, value = 0, result = 0, cost = 0, food_cost = 0, source_event = 0,
             flags = 0;
};
struct MemoryFact {
    uint32_t type = 0, event = 0, chain = 0, second = 0, actor = 0, kind = 0, target = 0, value = 0,
             cost = 0, food_cost = 0, flags = 0;
};
struct MemoryJob {
    uint32_t chain = 0, kind = 0, target = 0, value = 0, actor = 0, cost = 0, food_cost = 0,
             pause = 0, token = 0;
};
struct MemoryData {
    uint32_t version = 1, world_hi = 0, world_lo = 0, start_second = 0, generation = 1, last_id = 0,
             count = 0, dropped = 0, fact_count = 0, fact_overflow = 0, life_chain = 0,
             live_life_sequence = 0, imported = 0, fresh_start = 0;
    std::array<uint32_t, 8> totals{};  // life completed/interrupted, approvals/rejections,
                                       // manual/auto harvest, decisions completed, world events
    MemoryJob pending{};
    std::array<MemoryJob, 8> jobs{};  // facility/tree/recycle/journey/next-cycle plant0..3
    std::array<Experience, 128> records{};
    std::array<MemoryFact, 32> facts{};
};
static_assert(std::is_trivially_copyable<MemoryData>::value, "memory format must be numeric POD");
static_assert(sizeof(MemoryData) % 4 == 0, "memory word format");
constexpr size_t kMemoryWords = sizeof(MemoryData) / 4;
using ArchiveSave = std::array<uint32_t, 4 + 384 + kMemoryWords + 1>;
using CheckpointSave = std::array<uint32_t, 8 + 384 + 1>;
inline const char* MemoryActorName(uint32_t v) {
    static const char* n[] = {"unknown", "robot", "player", "system", "voice_request"};
    return n[v < 5 ? v : 0];
}
inline const char* MemoryActionName(uint32_t v) {
    static const char* n[] = {"world_event", "requested", "traveling",  "arrived",
                              "paused",      "resumed",   "finished",   "proposed",
                              "approved",    "rejected",  "superseded", "started",
                              "completed",   "scheduled", "applied",    "tracking_started"};
    return n[v < 16 ? v : 0];
}
}  // namespace spring
