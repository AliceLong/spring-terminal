#pragma once
#include <cstdint>
namespace spring {
// Numeric evidence only. Stored in previously unused FullSave words 326..345;
// no change to the outer save size, resource rules or elapsed-time rules.
enum ChapterFlag : uint32_t {
    ProductionRecorded = 1,
    DroughtStarted = 2,
    DroughtEnded = 4,
    BeyondWorkObserved = 8,
    BenchPermission = 16,
    BenchRestCompleted = 32,
    LegacyChapterBaseline = 64
};
struct ChapterState {
    uint32_t version = 0, flags = 0;
    uint32_t start_second = 0, end_second = 0, start_water = 0, end_water = 0;
    uint32_t start_energy = 0, end_energy = 0;
    uint32_t manual_harvests = 0, requested_cares = 0, requested_harvests = 0,
             automatic_harvests = 0, last_requested_slot = 255;
    uint32_t drought_chain = 0, observation_sequence = 0, bench_sequence = 0,
             observation_evidence = 0, bench_evidence = 0, invitation_evidence = 0;
};
inline bool ChapterHas(const ChapterState& c, ChapterFlag f) { return (c.flags & f) != 0; }
inline bool ChapterBench(const ChapterState& c) { return ChapterHas(c, BenchPermission); }
}  // namespace spring
