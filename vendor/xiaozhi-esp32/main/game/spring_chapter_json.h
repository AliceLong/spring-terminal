#pragma once
#include <sstream>
#include "spring_core.h"
namespace spring {
inline std::string ChapterJson(const Snapshot& s, bool compact = false) {
    const auto& c = s.chapters;
    std::ostringstream o;
    o << "{\"scope\":\"" << (s.roadshow ? "roadshow_fixture_and_live_events" : "device_observation")
      << "\",\"version\":" << c.version
      << ",\"drought_recorded\":" << (ChapterHas(c, DroughtStarted) ? "true" : "false")
      << ",\"drought_ended\":" << (ChapterHas(c, DroughtEnded) ? "true" : "false")
      << ",\"beyond_work_observed\":" << (ChapterHas(c, BeyondWorkObserved) ? "true" : "false")
      << ",\"bench_permission\":" << (ChapterBench(c) ? "true" : "false")
      << ",\"bench_rest_completed\":" << (ChapterHas(c, BenchRestCompleted) ? "true" : "false")
      << ",\"legacy_baseline\":" << (ChapterHas(c, LegacyChapterBaseline) ? "true" : "false");
    o << ",\"observation_verified\":" << (c.observation_evidence ? "true" : "false")
      << ",\"seat_rest_verified\":" << (c.bench_evidence ? "true" : "false");
    if (!compact) {
        o << ",\"drought\":{";
        if (ChapterHas(c, DroughtStarted))
            o << "\"start_second\":" << c.start_second << ",\"start_water\":" << c.start_water
              << ",\"start_energy\":" << c.start_energy << ',';
        if (ChapterHas(c, DroughtEnded))
            o << "\"end_second\":" << c.end_second << ",\"end_water\":" << c.end_water
              << ",\"end_energy\":" << c.end_energy << ',';
        o << "\"manual_harvests\":" << c.manual_harvests
          << ",\"completed_requested_cares\":" << c.requested_cares
          << ",\"completed_requested_harvests\":" << c.requested_harvests
          << ",\"automatic_harvests\":" << c.automatic_harvests << ",\"last_requested_slot\":"
          << (c.last_requested_slot == 255 ? 0 : c.last_requested_slot + 1)
          << ",\"chain\":" << c.drought_chain
          << "},\"evidence\":{\"observation\":" << c.observation_evidence
          << ",\"seat_invitation\":" << c.invitation_evidence
          << ",\"seat_rest\":" << c.bench_evidence << '}';
    }
    o << ",\"rule\":\"Care has no new yield or recovery bonus. Invitation is not completed rest. "
         "Legacy baseline is not a newly witnessed event. No inferred shared viewing.\"}";
    return o.str();
}
}  // namespace spring
