#pragma once
#include <sstream>
#include "spring_core.h"
#include "spring_memory_json.h"
#include "spring_schedule_json.h"
#include "spring_story_json.h"
namespace spring {
inline std::string PlantActionJson(const PlantAction& a, bool overview = false) {
    if (overview || a.status == PlantActionStatus::Unknown)
        return std::string("{\"request_id\":") + std::to_string(a.id) + ",\"status\":\"" +
               PlantActionStatusName(a.status) + "\"}";
    std::ostringstream o;
    o << "{\"request_id\":" << a.id << ",\"kind\":\""
      << (a.kind == 1   ? "CARE"
          : a.kind == 2 ? "HARVEST"
                        : "NONE")
      << "\",\"slot\":" << (a.id ? a.slot + 1 : 0) << ",\"cycle\":" << a.cycle << ",\"status\":\""
      << PlantActionStatusName(a.status) << "\",\"reason\":\"" << PlantActionReasonName(a.reason)
      << "\",\"arrived\":" << (a.arrived ? "true" : "false")
      << ",\"completed\":" << (a.status == PlantActionStatus::Completed ? "true" : "false")
      << ",\"wait_reason\":\"" << ScheduleBlockerName(a.wait) << "\""
      << ",\"credited_amount\":" << a.amount << ",\"extra_cost\":0,\"growth_bonus\":0}";
    return o.str();
}
inline std::string LifeJson(const Snapshot& s, bool include_history = true) {
    const auto& l = s.life;
    std::ostringstream o;
    o << "{\"intent\":\"" << LifeIntentName(l.intent) << "\",\"phase\":\""
      << (s.schedule.pause != ScheduleBlocker::None ? "paused_for_work"
          : s.schedule.waiting_short                ? "waiting_safe_point"
                                                    : LifePhaseName(l.phase))
      << "\",\"reason\":\"" << LifeReasonName(l.reason)
      << "\",\"target_slot\":" << (l.intent == 2 && l.target < 4 ? l.target + 1 : 0)
      << ",\"plant\":\""
      << (l.intent == 2 ? l.target == 4   ? "纪念花"
                          : l.target == 5 ? "动物小窝"
                          : l.target == 6 ? "头顶幼苗"
                                          : kPlantNames[l.plant]
                        : "")
      << "\",\"rest_location\":\""
      << (l.under_tree == 2   ? "passenger_seat"
          : l.under_tree == 1 ? "under_tree"
                              : "parking_spot")
      << "\",\"arrived\":" << (l.phase == 3 && l.reached ? "true" : "false")
      << ",\"has_reached_target\":" << (l.reached ? "true" : "false")
      << ",\"expression\":\"NONE\",\"request_contact\":false,\"player_participated\":false"
      << ",\"observed_specific_scenery\":false,\"language_stage\":"
      << StoryLanguage(s.story, s.story_stage >= 2 ? 1 : 0) << ",\"history\":[";
    for (unsigned i = 0; i < (include_history ? l.count : 0u); ++i) {
        if (i)
            o << ',';
        const auto& d = l.records[i];
        o << "{\"id\":" << d.id << ",\"intent\":\"" << LifeIntentName(d.intent)
          << "\",\"reason\":\"" << LifeReasonName(d.reason) << "\",\"result\":\""
          << LifeResultName(d.result) << "\",\"game_second\":" << d.second
          << ",\"target_slot\":" << (d.intent == 2 && d.target < 4 ? d.target + 1 : 0)
          << ",\"plant\":\""
          << (d.intent == 2 ? d.target == 4   ? "纪念花"
                              : d.target == 5 ? "动物小窝"
                              : d.target == 6 ? "头顶幼苗"
                                              : kPlantNames[d.plant]
                            : "")
          << "\",\"under_tree\":" << (d.under_tree == 1 ? "true" : "false")
          << ",\"rest_location\":\""
          << (d.under_tree == 2   ? "passenger_seat"
              : d.under_tree == 1 ? "under_tree"
                                  : "parking_spot")
          << "\""
          << ",\"arrived\":" << (d.reached ? "true" : "false") << ",\"player_participated\":false}";
    }
    o << "]";
    if (!include_history)
        o << ",\"history_included\":false,\"history_tool\":\"get_life_state\"";
    if (include_history)
        o << ",\"schedule\":" << ScheduleJson(s);
    o << "}";
    return o.str();
}
inline const char* RobotActivity(const Snapshot& s) {
    if (s.plant_action.status == PlantActionStatus::Traveling ||
        s.plant_action.status == PlantActionStatus::Running)
        return "requested_plant_action";
    if (s.economy.emergency_work)
        return "emergency_water_recovery";
    if (s.work == Work::Harvest)
        return "collect";
    if (s.economy.construction < 8)
        return "construct_facility";
    if (s.economy.recycle_ms)
        return "recycle";
    if (s.economy.journey_work) {
        static constexpr const char* tasks[] = {"journey_task",   "explore_discarded_device",
                                                "carry_supplies", "collect_rainwater",
                                                "help_bird",      "help_mouse",
                                                "collect_food"};
        const auto id = s.economy.journey_task;
        return id < 7 ? tasks[id] : tasks[0];
    }
    if (s.tree_work_ms)
        return "tend_tree";
    if (s.work == Work::Water)
        return "tend_plants";
    if (s.life.phase == 4)
        return s.schedule.pause != ScheduleBlocker::None ? "life_paused_for_work"
                                                         : "life_paused_for_voice";
    if (s.life.phase == 1 || s.life.phase == 2)
        return "approaching_life_target";
    if (s.life.phase == 3)
        return LifeIntentName(s.life.intent);
    return "idle";
}
inline std::string WorldJson(const Snapshot& s, unsigned active_call = 99, bool overview = false) {
    std::ostringstream o;
    o << "{\"game\":\"终点站是春天\",\"revision\":" << s.revision << ",\"mode\":\""
      << (s.roadshow ? "roadshow"
          : s.demo   ? "demo_10x"
                     : "normal")
      << "\",\"roadshow_scene\":" << s.roadshow_scene << ",\"roadshow_epoch\":" << s.roadshow_epoch
      << ",\"resources\":{\"water\":" << s.water << ",\"energy\":" << s.energy
      << ",\"fiber\":" << s.fiber << ",\"food\":" << s.economy.food
      << ",\"parts\":" << s.economy.parts << "},\"plants\":[";
    for (unsigned i = 0; i < (s.economy.enabled && (s.economy.built & 16) ? 4u : 3u); ++i) {
        const auto& p = i < 3 ? s.plots[i] : s.economy.fourth;
        if (i)
            o << ',';
        o << "{\"slot\":" << i + 1 << ",\"id\":\"" << kPlantIds[static_cast<unsigned>(p.plant)]
          << "\",\"name\":\"" << kPlantNames[static_cast<unsigned>(p.plant)] << "\",\"next\":\""
          << kPlantIds[static_cast<unsigned>(p.next)]
          << "\",\"ready\":" << (p.ready ? "true" : "false") << ",\"progress\":"
          << double(p.growth_ms) /
                 (s.economy.enabled ? 600000u : kGrowMs[static_cast<unsigned>(p.plant)])
          << ",\"water_cost\":" << (p.plant == Plant::Moss ? 0 : 2)
          << ",\"energy_cost\":" << (p.plant == Plant::Light ? 0 : 1)
          << ",\"region_yield\":" << kRegionYields[s.economy.region][static_cast<unsigned>(p.plant)]
          << ",\"locked_yield\":" << s.economy.mature_yield[i] << ",\"cycle\":" << p.cycle << '}';
    }
    o << "],\"plant_action\":" << PlantActionJson(s.plant_action, overview)
      << ",\"tree\":{\"stage\":" << s.tree_stage
      << ",\"building\":" << (s.tree_work_ms ? "true" : "false") << ",\"progress\":"
      << (s.tree_work_ms
              ? double(s.tree_work_ms) / (s.economy.enabled ? TreeDuration(s.tree_stage) : 60000u)
          : s.tree_stage ? 1.
                         : 0.)
      << ",\"cost_water\":" << TreeWater(s.tree_stage)
      << ",\"cost_fiber\":" << TreeFiber(s.tree_stage)
      << "},\"robot\":{\"language_stage\":" << StoryLanguage(s.story, s.story_stage >= 2 ? 1 : 0)
      << ",\"awake\":" << (s.story_stage ? "true" : "false") << ",\"work\":\"" << RobotActivity(s)
      << "\"},\"journey\":\""
      << (s.economy.enabled     ? kRegions[s.economy.region]
          : s.game_ms >= 900000 ? "past_drought"
          : s.game_ms >= 600000 ? "drought"
          : s.game_ms >= 540000 ? "approaching_drought"
                                : "wasteland")
      << "\",\"pending_communications\":" << s.notices
      << ",\"storage_available\":" << (s.storage_ok ? "true" : "false")
      << ",\"clock_synced\":" << (s.clock_ok ? "true" : "false")
      << ",\"offline_seconds\":" << s.offline_seconds
      << ",\"motion_available\":" << (s.motion_ok ? "true" : "false");
    o << ",\"game_seconds\":" << s.game_ms / 1000;
    const auto& e = s.economy;
    Core live;
    static_cast<State&>(live.s) = s;
    live.storage_ok = s.storage_ok;
    o << ",\"economy\":{\"version\":3,\"enabled\":" << (e.enabled ? "true" : "false")
      << ",\"region\":\"" << kRegions[e.region]
      << "\",\"region_remaining_seconds\":" << (1800000 - e.region_ms) / 1000
      << ",\"bird_visited\":" << (e.bird_visit ? "true" : "false")
      << ",\"emergency_seconds\":" << e.emergency_work / 1000
      << ",\"paused_no_energy\":" << (!s.energy ? "true" : "false")
      << ",\"warning_reason\":" << e.warning_reason << ",\"parts_gap\":" << live.MissingParts()
      << ",\"manual_recycle_available\":" << (e.scrap && s.energy ? "true" : "false")
      << ",\"round_seconds\":600,\"food\":" << e.food << ",\"parts\":" << e.parts
      << ",\"scrap\":" << e.scrap << ",\"water_capacity\":" << live.WaterCapacity()
      << ",\"energy_capacity\":" << live.EnergyCapacity() << ",\"facilities\":[";
    for (unsigned i = 0; i < 8; ++i) {
        if (i)
            o << ',';
        const auto& f = kFacilities[i];
        o << "{\"id\":" << i << ",\"name\":\"" << f.name
          << "\",\"built\":" << (live.HasFacility(i) ? "true" : "false")
          << ",\"cost\":{\"water\":" << f.water << ",\"energy\":" << f.energy
          << ",\"fiber\":" << f.fiber << ",\"parts\":" << f.parts << "},\"seconds\":" << f.ms / 1000
          << ",\"benefit\":\"" << f.benefit << "\",\"upkeep\":\"" << f.upkeep << "\"}";
    }
    o << "],\"construction\":" << e.construction << ",\"construction_ms\":" << e.construction_ms
      << ",\"animals\":" << e.animals << ",\"animal_help\":" << e.animal_help
      << ",\"recycle_seconds\":" << e.recycle_ms / 1000
      << ",\"journey_work_seconds\":" << e.journey_work / 1000 << ",\"boost_slot\":" << e.boost
      << ",\"priorities\":[";
    for (unsigned i = 0; i < 4; ++i) {
        if (i)
            o << ',';
        o << e.priorities[i];
    }
    o << "],\"proposal\":{\"kind\":" << e.proposal.kind << ",\"target\":" << e.proposal.target
      << ",\"value\":" << e.proposal.value << ",\"token\":" << e.proposal.token
      << ",\"can_confirm\":" << (live.CanConfirm(e.proposal.token) ? "true" : "false")
      << "},\"journey_call\":{\"id\":" << e.journey_id << ",\"kind\":" << e.journey
      << ",\"unread\":" << (e.journey && e.journey_read != e.journey_id ? "true" : "false")
      << ",\"opening\":\"" << kJourneyOpenings[e.journey] << "\"}}";
    if (e.journey_work && e.journey_task > 0 && e.journey_task < kJourneyTaskNames.size())
        o << ",\"active_journey\":{\"name\":\"" << kJourneyTaskNames[e.journey_task]
          << "\",\"expected_result\":\"" << kJourneyTaskResults[e.journey_task]
          << "\",\"remaining_seconds\":" << (e.journey_work + 999) / 1000 << ",\"paused\":"
          << ((!s.energy || e.emergency_work || s.work == Work::Harvest) ? "true" : "false") << '}';
    if (!overview) {
        o << ",\"plan_history\":[";
        for (unsigned i = 0; i < e.decision_count; ++i) {
            if (i)
                o << ',';
            const auto& d = e.decisions[i];
            o << "{\"id\":" << d.id << ",\"second\":" << d.second << ",\"type\":\""
              << EventName(d.type) << "\",\"target\":" << d.target << ",\"amount\":" << d.amount
              << '}';
        }
        o << ']';
    }
    if (active_call < 7)
        o << ",\"accepted_communication\":{\"id\":" << active_call << ",\"opening\":\""
          << Opening(active_call, s) << "\"}";
    o << ",\"furniture\":{\"passenger_seat\":{\"present\":true,\"robot_use_unlocked\":"
      << (ChapterBench(s.chapters) ? "true" : "false")
      << ",\"name\":\"旧乘客观光座椅\",\"buildable\":false,\"unlock_source\":\""
      << (ChapterBench(s.chapters) ? "explicit_player_invitation" : "not_yet_invited")
      << "\",\"first_invitation_ready\":"
      << ((s.chapters.version == 1 && ChapterHas(s.chapters, BeyondWorkObserved) &&
           ((s.story.archives_read & 2u) ||
            (s.story.completed & (1u << unsigned(StoryNode::Performance)))))
              ? "true"
              : "false")
      << "}}";
    o << ",\"life\":" << LifeJson(s, !overview)
      << ",\"memory\":{\"enabled\":" << (s.memory ? "true" : "false");
    if (s.memory)
        o << ",\"retained\":" << s.memory->count << ",\"long_term_count\":" << s.memory->fact_count
          << ",\"overwritten\":" << s.memory->dropped;
    o << "}}";
    // Insert versioned narrative facts into the world JSON object.
    auto json = o.str();
    if (s.story.enabled && !json.empty() && json.back() == '}')
        json.insert(json.size() - 1,
                    ",\"story\":" + (overview ? StoryOverviewJson(s) : StoryJson(s)));
    return json;
}
inline std::string EventsJson(const Snapshot& s) {
    std::ostringstream o;
    o << "{\"bounded_history\":12,\"events\":[";
    for (unsigned i = 0; i < s.event_count; ++i) {
        if (i)
            o << ',';
        const auto& e = s.events[i];
        o << "{\"id\":" << e.id << ",\"game_second\":" << e.second << ",\"type\":\""
          << EventName(e.type) << "\",\"target\":" << e.target << ",\"amount\":" << e.amount << '}';
    }
    o << "],\"life\":" << LifeJson(s) << '}';
    return o.str();
}
}  // namespace spring
