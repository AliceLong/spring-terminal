#pragma once
#include <sstream>
#include <string>
#include "game_core.h"
namespace desk_gap {
inline std::string EventJson(const GameEvent& e) {
    std::ostringstream o;
    o << "{\"id\":" << e.id << ",\"type\":\"" << EventName(e.type)
      << "\",\"timestamp\":" << e.timestamp << ",\"game_ms\":" << e.game_ms
      << ",\"offline\":" << (e.offline ? "true" : "false") << ",\"actor\":\""
      << (e.type == EventType::kDelivered ? "player"
          : e.type == EventType::kResonance || e.type == EventType::kOffline ||
                  e.type == EventType::kMigrated || e.type == EventType::kVoice ||
                  e.type == EventType::kShake || e.type == EventType::kKnock
              ? "system"
              : "resident_01")
      << "\",\"target\":\"" << ItemName(e.target) << "\",\"data\":{\"quantity\":" << e.quantity
      << "}}";
    return o.str();
}
inline const char* BottleLocation(const GameState& s) {
    switch (s.construction) {
        case ConstructionState::kNotStarted:
            return "not_delivered";
        case ConstructionState::kDelivered:
            return "drop_point";
        case ConstructionState::kDiscovered:
            return "carried_by_resident";
        case ConstructionState::kCarried:
        case ConstructionState::kWaiting:
            return "workshop";
        case ConstructionState::kBuilding:
            return "workshop_under_construction";
        case ConstructionState::kCompleted:
            return "part_of_table";
    }
    return "unknown";
}
inline const char* DeliveryLocation(const GameState& s, Item item) {
    switch (ItemStage(s, item)) {
        case ConstructionState::kNotStarted:
            return "not_delivered";
        case ConstructionState::kDelivered:
            return "drop_point";
        case ConstructionState::kDiscovered:
            return "carried_by_resident";
        case ConstructionState::kCarried:
        case ConstructionState::kWaiting:
            return item == Item::kCotton ? "beside_bed" : "workshop";
        case ConstructionState::kBuilding:
            return "under_construction";
        case ConstructionState::kCompleted:
            return item == Item::kCotton ? "part_of_soft_bed" : "part_of_storage_rack";
    }
    return "unknown";
}
inline std::string WorldStateJson(const GameState& s, uint32_t query) {
    std::ostringstream o;
    const char* carrying = s.phase == CreatureState::kCarry && s.active_item != Item::kNone
                               ? ItemName(s.active_item)
                           : s.carrying_stick ? "stick"
                           : s.carrying_paper ? "paper_scrap"
                                              : "none";
    o << "{\"schema_version\":4,\"query_id\":" << query << ",\"snapshot_id\":" << s.simulation_ticks
      << ",\"revision\":" << s.revision << ",\"sampled_at_uptime_ms\":" << s.elapsed_ms
      << ",\"game_ms\":" << s.game_ms << ",\"time_scale\":" << s.time_scale
      << ",\"creature\":{\"state\":\"" << WorldStateName(s.creature_state) << "\",\"x\":" << s.x
      << ",\"y\":" << s.y << ",\"carrying\":\"" << carrying << "\",\"using_furniture\":\""
      << ItemName(s.using_furniture) << "\",\"walking\":" << (s.visual_walking ? "true" : "false")
      << ",\"action_remaining_game_ms\":" << s.remaining_ms << "}"
      << ",\"inventory\":{\"paper_scrap\":" << s.paper << ",\"stick\":" << s.sticks << "}"
      << ",\"resonance\":" << s.resonance << ",\"comfort\":" << Comfort(s)
      << ",\"growth_stage\":" << GrowthStage(s) << ",\"active_item\":\"" << ItemName(s.active_item)
      << "\",\"underlying_phase\":\"" << WorldStateName(s.phase) << "\""
      << ",\"rewards_claimed\":{\"bottle_discovered\":"
      << ((s.claimed_rewards & 1) ? "true" : "false")
      << ",\"table_completed\":" << ((s.claimed_rewards & 2) ? "true" : "false")
      << ",\"bed_completed\":" << ((s.claimed_rewards & 8) ? "true" : "false") << "}"
      << ",\"free_bottle_cap_available\":"
      << (s.construction == ConstructionState::kNotStarted ? "true" : "false")
      << ",\"bottle_cap\":{\"location\":\"" << BottleLocation(s) << "\"}"
      << ",\"table\":{\"stage\":\"" << ConstructionName(s.construction)
      << "\",\"required_sticks\":2,\"build_duration_game_ms\":120000,\"remaining_game_ms\":"
      << (s.phase == CreatureState::kBuild && s.active_item == Item::kBottleCap ? s.remaining_ms
                                                                                : 0)
      << "}"
      << ",\"cotton\":{\"location\":\"" << DeliveryLocation(s, Item::kCotton)
      << "\"},\"paperclip\":{\"location\":\"" << DeliveryLocation(s, Item::kPaperclip) << "\"}"
      << ",\"soft_bed\":{\"stage\":\"" << ConstructionName(s.bed)
      << "\",\"build_duration_game_ms\":" << BuildDuration(Item::kCotton)
      << ",\"remaining_game_ms\":"
      << (s.phase == CreatureState::kBuild && s.active_item == Item::kCotton ? s.remaining_ms : 0)
      << "}"
      << ",\"storage_rack\":{\"stage\":\"" << ConstructionName(s.storage)
      << "\",\"build_duration_game_ms\":" << BuildDuration(Item::kPaperclip)
      << ",\"remaining_game_ms\":"
      << (s.phase == CreatureState::kBuild && s.active_item == Item::kPaperclip ? s.remaining_ms
                                                                                : 0)
      << "}"
      << ",\"motion_sensor_available\":" << (s.motion_sensor_available ? "true" : "false")
      << ",\"interaction_rewards\":{\"reward_day_since_epoch_Asia_Shanghai\":" << s.reward_day
      << ",\"voice_today\":" << s.voice_today
      << ",\"voice_daily_limit\":15,\"shake_today\":" << s.shake_today
      << ",\"shake_daily_limit\":10,\"requires_verified_clock\":true}"
      << ",\"save\":{\"available\":" << (s.storage_available ? "true" : "false")
      << ",\"saved_revision\":" << s.saved_revision
      << ",\"pending\":" << (s.saved_revision < s.revision ? "true" : "false") << "}"
      << ",\"offline\":{\"clock_synced\":" << (s.clock_synced ? "true" : "false")
      << ",\"clock_rollback\":" << (s.clock_rollback ? "true" : "false")
      << ",\"settled_seconds\":" << s.offline_seconds << ",\"cap_seconds\":43200}"
      << ",\"latest_event\":" << (s.event_count ? EventJson(s.events[s.event_count - 1]) : "null")
      << ",\"capabilities\":{\"delivery_bottle_cap\":true,\"construction_table\":true,\"recent_"
         "events\":true,\"cotton\":true,\"paperclip\":true,\"voice_delivery\":\"touch_confirmation_"
         "required\"}"
      << ",\"observation_note\":\"Query fresh for live claims. Recent history requires "
         "get_recent_events. Timestamp 0 is unknown. Inventory excludes carried material. "
         "Construction consumes 2 sticks and the cap only at completion. Offline settles once with "
         "verified UTC, max 12h.\"}";
    return o.str();
}
inline std::string AvailableItemsJson(const GameState& s) {
    std::ostringstream o;
    o << "{\"resonance\":" << s.resonance << ",\"items\":[";
    bool first = true;
    for (auto item : {Item::kBottleCap, Item::kCotton, Item::kPaperclip}) {
        if (!first)
            o << ',';
        first = false;
        const char* reason = DeliveryBlock(s, item, s.storage_available);
        o << "{\"id\":\"" << ItemName(item) << "\",\"cost\":" << ItemCost(item) << ",\"available\":"
          << (s.storage_available && ItemStage(s, item) == ConstructionState::kNotStarted &&
                      s.resonance >= ItemCost(item)
                  ? "true"
                  : "false")
          << ",\"reason\":\"" << reason << "\",\"stage\":\"" << ConstructionName(ItemStage(s, item))
          << "\",\"result\":\"" << ItemName(FurnitureFor(item)) << "\"}";
    }
    o << "],\"delivery_confirmation\":\"Player must confirm on the device. AI requests only open a "
         "preview; never deduct resonance.\"}";
    return o.str();
}
inline std::string RecentEventsJson(const GameState& s) {
    std::ostringstream o;
    o << "{\"revision\":" << s.revision
      << ",\"order\":\"oldest_first\",\"retained_limit\":20,\"older_events_dropped\":"
      << (s.event_count && s.events[0].id > 1 ? "true" : "false") << ",\"events\":[";
    for (uint32_t i = 0; i < s.event_count; ++i) {
        if (i)
            o << ',';
        o << EventJson(s.events[i]);
    }
    o << "],\"note\":\"Historical events, not necessarily just now. For current bottle location "
         "and construction query get_world_state. Timestamp 0 means unknown. No events means no "
         "evidence, do not invent.\"}";
    return o.str();
}
}  // namespace desk_gap
