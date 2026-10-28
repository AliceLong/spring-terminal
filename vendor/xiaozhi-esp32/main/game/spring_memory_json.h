#pragma once
#include <iomanip>
#include <sstream>
#include "spring_core.h"
namespace spring {
inline void MemoryCosts(std::ostream& o, uint32_t cost, uint32_t food) {
    o << "{\"water\":" << (cost & 255) << ",\"energy\":" << ((cost >> 8) & 255)
      << ",\"fiber\":" << ((cost >> 16) & 255) << ",\"parts\":" << (cost >> 24)
      << ",\"food\":" << food << '}';
}
inline std::string MemoryWorldId(const MemoryData& m, bool demo) {
    std::ostringstream o;
    o << (demo ? "demo-" : "normal-") << std::hex << std::setfill('0') << std::setw(8) << m.world_hi
      << std::setw(8) << m.world_lo;
    return o.str();
}
inline std::string MemoryJson(const Snapshot& s, unsigned category = 0, uint32_t chain = 0,
                              uint32_t after = 0, unsigned limit = 16, bool all = false,
                              uint32_t before = 0, unsigned fact_offset = 0,
                              unsigned fact_limit = 32) {
    std::ostringstream o;
    o << "{\"enabled\":" << (s.memory ? "true" : "false");
    if (!s.memory) {
        o << ",\"records\":[],\"long_term\":[]}";
        return o.str();
    }
    const auto& m = *s.memory;
    o << ",\"world_id\":\"" << MemoryWorldId(m, s.demo) << "\",\"generation\":" << m.generation
      << ",\"language_stage\":" << StoryLanguage(s.story, s.story_stage >= 2 ? 1 : 0)
      << ",\"tracking_start_second\":" << m.start_second << ",\"scope\":\""
      << (s.roadshow      ? "roadshow_fixture_and_live_events"
          : m.fresh_start ? "since_new_game"
                          : "since_tracking_started")
      << "\",\"retained\":" << m.count << ",\"capacity\":" << m.records.size()
      << ",\"earliest_id\":" << (m.count ? m.records[0].id : 0) << ",\"latest_id\":" << m.last_id
      << ",\"overwritten\":" << m.dropped << ",\"long_term_count\":" << m.fact_count
      << ",\"long_term_capacity\":" << m.facts.size()
      << ",\"long_term_overflow\":" << m.fact_overflow << ",\"totals\":[";
    for (unsigned i = 0; i < m.totals.size(); ++i) {
        if (i)
            o << ',';
        o << m.totals[i];
    }
    o << "],\"records\":[";
    unsigned emitted = 0;
    uint32_t last_emitted = 0;
    limit = std::min<unsigned>(limit, all ? 128 : 24);
    for (unsigned n = m.count; n > 0 && emitted < limit; --n) {
        const auto& e = m.records[n - 1];
        if ((category && e.category != category) || (chain && e.chain != chain) || e.id <= after ||
            (before && e.id >= before))
            continue;
        if (emitted++)
            o << ',';
        last_emitted = e.id;
        o << "{\"id\":" << e.id << ",\"chain\":" << e.chain << ",\"game_second\":" << e.second
          << ",\"category\":" << e.category << ",\"action\":\"" << MemoryActionName(e.action)
          << "\",\"actor\":\"" << MemoryActorName(e.actor) << "\",\"kind\":" << e.kind
          << ",\"target\":" << e.target << ",\"value\":" << e.value << ",\"result\":" << e.result
          << ",\"source_event\":" << e.source_event << ",\"flags\":" << e.flags
          << ",\"player_participated\":" << ((e.flags & 1) ? "true" : "false") << ",\"cost_known\":"
          << ((e.category == 2 && (e.flags & 1) && e.action != 7 && e.action != 9 && e.action != 10)
                  ? "true"
                  : "false")
          << ",\"cost\":";
        MemoryCosts(o, e.cost, e.food_cost);
        if (e.category == 4 && (e.kind == 80 || e.kind == 81))
            o << ",\"plant_action\":\"" << (e.kind == 80 ? "CARE" : "HARVEST")
              << "\",\"crop_cycle\":" << e.value << ",\"action_reason\":\""
              << PlantActionReasonName(static_cast<PlantActionReason>(e.result)) << '\"';
        if (e.category == 1 && (e.action == unsigned(MemoryAction::Paused) ||
                                e.action == unsigned(MemoryAction::Resumed)))
            o << ",\"pause_cause\":\""
              << (e.result ? ScheduleBlockerName(static_cast<ScheduleBlocker>(e.result)) : "voice")
              << '\"';
        if (e.category == 1)
            o << ",\"plant_id\":" << (e.value & 255) << ",\"reason\":\""
              << LifeReasonName((e.value >> 8) & 255) << '"';
        if (e.category == 1 && e.action == unsigned(MemoryAction::Finished) && (e.value >> 16))
            o << ",\"interruption_cause\":\""
              << ScheduleBlockerName(static_cast<ScheduleBlocker>((e.value >> 16) & 255)) << '\"';
        if (e.category == 4 && e.kind >= 90 && e.kind <= 93) {
            static const char* chapters[] = {"DROUGHT_EXPERIENCE", "CARE_BEYOND_WORK",
                                             "FIRST_PASSENGER_SEAT_REST",
                                             "PASSENGER_SEAT_PERMISSION"};
            o << ",\"chapter_event\":\"" << chapters[e.kind - 90] << '"';
        }
        if (e.category == 3 || (e.category == 5 && e.action == 0))
            o << ",\"world_event\":\""
              << (e.kind < 23 ? EventName(static_cast<Event>(e.kind)) : "unknown") << '"';
        o << '}';
    }
    o << "],\"next_before\":"
      << (emitted >= limit && m.count && last_emitted > m.records[0].id ? last_emitted : 0)
      << ",\"long_term\":[";
    const unsigned fact_end =
        std::min<unsigned>(m.fact_count, fact_offset + std::min(fact_limit, 32u));
    for (unsigned i = std::min<unsigned>(fact_offset, m.fact_count); i < fact_end; ++i) {
        if (i != fact_offset)
            o << ',';
        const auto& f = m.facts[i];
        o << "{\"type\":" << f.type << ",\"evidence_id\":" << f.event << ",\"chain\":" << f.chain
          << ",\"game_second\":" << f.second << ",\"actor\":\"" << MemoryActorName(f.actor)
          << "\",\"kind\":" << f.kind << ",\"target\":" << f.target << ",\"value\":" << f.value
          << ",\"flags\":" << f.flags
          << ",\"player_participated\":" << ((f.flags & 1) ? "true" : "false") << ",\"scope\":\""
          << ((f.flags & 8)   ? "baseline_already_present"
              : (f.flags & 4) ? "first_recorded_since_tracking"
                              : "since_new_game")
          << "\",\"cost_known\":"
          << ((f.flags & 1) && !(f.flags & 8) &&
                      (f.kind == 1 || f.kind == 5 || f.kind == 6 || f.kind == 7)
                  ? "true"
                  : "false")
          << ",\"cost\":";
        MemoryCosts(o, f.cost, f.food_cost);
        o << '}';
    }
    o << "],\"next_fact_offset\":" << (fact_end < m.fact_count ? fact_end : 0) << "}";
    return o.str();
}
}  // namespace spring
