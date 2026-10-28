#pragma once
#include <sstream>
#include "spring_chapter_json.h"
#include "spring_core.h"
namespace spring {
inline std::string StoryQuote(const char* text) {
    std::ostringstream o;
    o << '"';
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(text); *p; ++p) {
        if (*p == '"' || *p == '\\')
            o << '\\' << char(*p);
        else if (*p == '\n')
            o << "\\n";
        else if (*p >= 32)
            o << char(*p);
    }
    o << '"';
    return o.str();
}
// Overview has live identity/status only; gated openings and history are queried separately.
inline std::string StoryOverviewJson(const Snapshot& s) {
    const auto& n = s.story;
    std::ostringstream o;
    o << "{\"enabled\":" << (n.enabled ? "true" : "false")
      << ",\"version\":46,\"language_stage\":" << n.language
      << ",\"robot_name\":" << StoryQuote(n.robot_named ? n.robot_name.data() : "01-99")
      << ",\"active_node\":" << n.active << ",\"pending_node\":" << n.pending
      << ",\"flower_stage\":" << n.flower << ",\"new_leaf\":" << (n.new_leaf ? "true" : "false")
      << ",\"details_tool\":\"get_story_state\"}";
    return o.str();
}
inline std::string StoryJson(const Snapshot& s, unsigned node = kNoStory,
                             unsigned record_offset = 0, unsigned record_limit = 16,
                             bool newest_first = false, const std::string& next_cursor = "") {
    const auto& n = s.story;
    record_offset = std::min<unsigned>(record_offset, n.count);
    std::ostringstream o;
    if (node == kNoStory)
        node = StoryCurrentNode(n);
    o << "{\"enabled\":" << (n.enabled ? "true" : "false")
      << ",\"version\":46,\"revision\":" << n.revision << ",\"language_stage\":" << n.language
      << ",\"robot_number\":\"GDN-01-99\",\"robot_name\":"
      << StoryQuote(n.robot_named ? n.robot_name.data() : "01-99")
      << ",\"player_name\":" << StoryQuote(n.player_name.data());
    o << ",\"self\":{\"head_plant\":{\"location\":\"头顶透明种植舱\",\"living\":true,"
         "\"maintained_by\":\"self\",\"species\":\"unknown\",\"new_leaf\":"
      << (n.new_leaf ? "true" : "false")
      << ",\"origin\":" << StoryQuote((n.archives_read & (1u << 6)) ? "G-17活枝" : "unknown")
      << ",\"initial_motive\":\"unknown\"}}";
    o << ",\"active_node\":" << n.active << ",\"pending_node\":" << n.pending
      << ",\"archive_selected\":" << n.archive_selected << ",\"paused_node\":" << n.paused
      << ",\"available_mask\":" << n.available << ",\"presented_mask\":" << n.presented
      << ",\"discussed_mask\":" << n.discussed << ",\"completed_mask\":" << n.completed
      << ",\"flower_stage\":" << n.flower << ",\"flower_seconds\":" << n.flower_ms / 1000
      << ",\"new_leaf\":" << (n.new_leaf ? "true" : "false") << ",\"seed_choice\":" << n.seed_choice
      << ",\"animal_discovery\":" << (n.animal_discovery ? "true" : "false");
    o << ",\"plant_proposal\":{\"pending\":" << (n.offered ? "true" : "false")
      << ",\"token\":" << n.offer_token
      << ",\"water_cost\":2,\"can_afford\":" << (s.water >= 2 ? "true" : "false")
      << ",\"resource_yield\":0,\"uses_crop_slot\":false},\"name_proposal\":{\"who\":"
      << n.candidate_who << ",\"token\":" << n.candidate_token
      << ",\"candidate\":" << StoryQuote(n.candidate.data()) << "}";
    if (node < kStoryCount && (n.available & (1u << node))) {
        o << ",\"node\":{\"id\":" << node << ",\"title\":" << StoryQuote(kStorySpecs[node].title)
          << ",\"opening\":" << StoryQuote(StoryNodeOpening(s, node))
          << ",\"detail\":" << StoryQuote(StoryNodeDetail(s, node))
          << ",\"archive\":" << kStorySpecs[node].archive
          << ",\"presented\":" << ((n.presented & (1u << node)) ? "true" : "false")
          << ",\"completed\":" << ((n.completed & (1u << node)) ? "true" : "false") << "}";
    }
    o << ",\"chapter_experience\":" << ChapterJson(s);
    o << ",\"choices\":[";
    for (unsigned i = 0; i < kStoryCount; ++i) {
        if (i)
            o << ',';
        o << unsigned(n.choices[i]);
    }
    o << "],\"archives\":[";
    bool comma = false;
    for (unsigned i = 0; i < 7; ++i)
        if (n.archives & (1u << i)) {
            if (comma)
                o << ',';
            comma = true;
            o << "{\"id\":" << i << ",\"title\":" << StoryQuote(kStoryArchiveTitles[i])
              << ",\"read\":" << ((n.archives_read & (1u << i)) ? "true" : "false") << "}";
        }
    o << "],\"life\":{\"shown\":" << n.life_shown << ",\"noticed\":" << n.life_attention
      << ",\"discussed\":" << n.life_talked << "},\"records\":[";
    const unsigned record_end =
        std::min<unsigned>(n.count, record_offset + std::min(record_limit, 16u));
    for (unsigned i = std::min<unsigned>(record_offset, n.count); i < record_end; ++i) {
        if (i != record_offset)
            o << ',';
        const auto& e = n.records[newest_first ? n.count - 1 - i : i];
        o << "{\"node\":" << e.node << ",\"action\":" << e.action << ",\"value\":" << e.value
          << ",\"game_second\":" << e.second << ",\"participation\":"
          << StoryQuote(e.action == 2 || e.action == 6 || e.action == 10 || e.action == 12 ||
                                e.action == 13
                            ? "explicit_player_action"
                            : "device_or_world_record")
          << "}";
    }
    o << "],\"record_count\":" << n.count;
    if (newest_first)
        o << ",\"records_order\":\"newest_first\",\"records_revision\":" << n.revision
          << ",\"returned_record_count\":" << record_end - record_offset
          << ",\"has_more_records\":" << (record_end < n.count ? "true" : "false")
          << ",\"next_record_cursor\":" << StoryQuote(next_cursor.c_str());
    else
        o << ",\"next_record_offset\":" << (record_end < n.count ? record_end : 0);
    o << ",\"rules\":\"Fixed history is gated by discovered archives. Thoughts are not execution. "
         "Never claim player watched an autonomous activity. No name, flower or animal required to "
         "finish. Explicit new user turn required for spending/name confirmation.\"}";
    return o.str();
}
inline std::string StoryArchiveJson(const Snapshot& s, unsigned id) {
    if (id >= 7 || !(s.story.archives & (1u << id)))
        return "{\"available\":false}";
    return "{\"available\":true,\"id\":" + std::to_string(id) +
           ",\"title\":" + StoryQuote(kStoryArchiveTitles[id]) +
           ",\"excerpt\":" + StoryQuote(kStoryArchiveTexts[id]) +
           ",\"player_read\":" + ((s.story.archives_read & (1u << id)) ? "true" : "false") + "}";
}
}  // namespace spring
