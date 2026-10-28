#pragma once
#include "spring_json.h"
namespace spring {
struct MonitorRuntime {
    uint64_t uptime_ms = 0;
    const char* fsm = "unknown";
    const char* voice = "unknown";
    const char* power = "unknown";
    float x = 0, y = 0;
    unsigned internal_free = 0, psram_free = 0, monitor_stack_free = 0;
    std::array<uint32_t, 18> queries{};
    std::string audio_json = "{}";
    std::string communication_json = "{}";
    std::string observation_json = "{}";
    std::string collection_json = "{}";
    std::string roadshow_json = "{\"available\":false}";
    const char* background_storage = "unknown";
    unsigned psram_min_free = 0, psram_largest_free = 0, internal_min_free = 0;
    unsigned background_bytes = 0, background_allocations = 0;
    bool background_ready = false;
    uint32_t mind_boot = 0, mind_offer = 0, mind_mask = 0, mind_status = 0, mind_sequence = 0;
    uint64_t mind_deadline = 0, mind_reply_deadline = 0;
    uint32_t mind_received = 0, mind_parsed = 0;
    uint32_t mind_social_status = 0, mind_focus_status = 0, mind_trigger_kind = 0,
             mind_trigger_source = 0, mind_trigger_target = 0;
};
// No mutation or model-generated interpretation. Labels come from fixed enum tables.
inline std::string MonitorJson(const Snapshot& s, const MonitorRuntime& r, unsigned call) {
    std::ostringstream o;
    o << "{\"schema\":1,\"uptime_ms\":" << r.uptime_ms << ",\"world\":" << WorldJson(s, call)
      << ",\"memory\":" << MemoryJson(s, 0, 0, 0, 128, true) << ",\"events\":" << EventsJson(s)
      << ",\"runtime\":{\"fsm\":\"" << r.fsm << "\",\"voice\":\"" << r.voice << "\",\"power\":\""
      << r.power << "\",\"x\":" << r.x << ",\"y\":" << r.y
      << ",\"internal_free\":" << r.internal_free << ",\"psram_free\":" << r.psram_free
      << ",\"monitor_stack_free\":" << r.monitor_stack_free << ",\"background_storage\":\""
      << r.background_storage << "\",\"audio\":" << r.audio_json
      << ",\"collection\":" << r.collection_json << ",\"roadshow\":" << r.roadshow_json
      << ",\"psram_min_free\":" << r.psram_min_free
      << ",\"psram_largest_free\":" << r.psram_largest_free
      << ",\"internal_min_free\":" << r.internal_min_free
      << ",\"background_bytes\":" << r.background_bytes
      << ",\"background_allocations\":" << r.background_allocations
      << ",\"background_ready\":" << (r.background_ready ? "true" : "false")
      << ",\"mind\":{\"transport\":\"usb\",\"boot\":" << r.mind_boot
      << ",\"offer\":" << r.mind_offer << ",\"mask\":" << r.mind_mask
      << ",\"status\":" << r.mind_status << ",\"sequence\":" << r.mind_sequence
      << ",\"current_life_sequence\":" << s.life.sequence
      << ",\"received_bytes\":" << r.mind_received << ",\"parsed_packets\":" << r.mind_parsed
      << ",\"deadline_ms\":" << r.mind_deadline
      << ",\"reply_deadline_ms\":" << r.mind_reply_deadline
      << ",\"social_status\":" << r.mind_social_status
      << ",\"focus_status\":" << r.mind_focus_status << ",\"trigger_kind\":" << r.mind_trigger_kind
      << ",\"trigger_source\":" << r.mind_trigger_source
      << ",\"trigger_target\":" << r.mind_trigger_target << "}"
      << ",\"communication\":" << r.communication_json << ",\"observation\":" << r.observation_json
      << ",\"tool_queries\":{\"get_world_state\":" << r.queries[0]
      << ",\"get_life_state\":" << r.queries[1] << ",\"get_recent_events\":" << r.queries[2]
      << ",\"propose_garden_action\":" << r.queries[3] << ",\"get_memories\":" << r.queries[4]
      << ",\"invite_gardener_life\":" << r.queries[5] << ",\"get_story_state\":" << r.queries[6]
      << ",\"get_story_archive\":" << r.queries[7] << ",\"story_action\":" << r.queries[8]
      << ",\"approve_garden_action\":" << r.queries[9]
      << ",\"get_character_state\":" << r.queries[10]
      << ",\"request_gardener_work\":" << r.queries[11]
      << ",\"respond_life_wish\":" << r.queries[12] << ",\"request_plant_care\":" << r.queries[13]
      << ",\"request_plant_harvest\":" << r.queries[14]
      << ",\"get_plant_action_status\":" << r.queries[15]
      << ",\"get_gardener_schedule\":" << r.queries[16]
      << ",\"cancel_gardener_activity\":" << r.queries[17] << "}}}";
    return o.str();
}
}  // namespace spring
