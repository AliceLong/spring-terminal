#pragma once
#include <sstream>
#include "spring_core.h"
namespace spring {
inline bool SnapshotApprovedLife(const Snapshot& s) {
    return s.life.intent && s.schedule.approved_token &&
           s.schedule.approved_sequence == s.life.sequence && AgencyLevel(s.story) == 1;
}
inline ScheduleBlocker SnapshotBodyBlocker(const Snapshot& s) {
    if (s.economy.emergency_work)
        return ScheduleBlocker::Emergency;
    if (s.story.enabled && s.story.active != kNoStory &&
        !(s.life.intent == 3 && s.life.under_tree == 2 &&
          s.story.active == unsigned(StoryNode::Rest)))
        return ScheduleBlocker::Story;
    if (s.economy.construction != 255)
        return ScheduleBlocker::Construction;
    if (s.economy.journey_work)
        return ScheduleBlocker::Exploration;
    if (s.economy.recycle_ms)
        return ScheduleBlocker::Recycling;
    if (s.tree_work_ms)
        return ScheduleBlocker::TreeWork;
    if (s.plant_action.status == PlantActionStatus::Traveling ||
        s.plant_action.status == PlantActionStatus::Running)
        return ScheduleBlocker::PlayerTask;
    if (s.work == Work::Water && s.work_ms)
        return ScheduleBlocker::WaterFinishing;
    if (s.work == Work::Harvest && s.work_ms)
        return ScheduleBlocker::HarvestFinishing;
    return ScheduleBlocker::None;
}
inline std::string ScheduleJson(const Snapshot& s) {
    const auto& l = s.life;
    const auto& state = s.schedule;
    const auto body = SnapshotBodyBlocker(s);
    const bool approved = SnapshotApprovedLife(s),
               paused = l.intent && state.pause != ScheduleBlocker::None;
    auto wait = body;
    if (paused && wait == ScheduleBlocker::None) {
        if (s.water < 6 || s.energy < 4)
            wait = ScheduleBlocker::Resources;
        else
            for (unsigned i = 0; i < (s.economy.built & 16 ? 4u : 3u); ++i)
                if ((i < 3 ? s.plots[i] : s.economy.fourth).ready)
                    wait = ScheduleBlocker::MatureCrop;
    }
    const unsigned stage = StoryLanguage(s.story, 0);
    std::ostringstream o;
    o << "{\"authority\":\"device\",\"next_action\":\""
      << (s.plant_action.status == PlantActionStatus::Queued ? "player_plant_task"
          : state.waiting_short && l.intent ? (approved ? "approved_wish" : "explicit_player_life")
          : approved && paused              ? "revalidate_approved_wish"
                                            : "none")
      << "\",\"agency_level\":" << AgencyLevel(s.story) << ",\"body_owner\":\""
      << (body != ScheduleBlocker::None ? ScheduleBlockerName(body)
          : l.intent && !paused         ? approved        ? "approved_wish"
                                          : l.reason == 4 ? "player_life_request"
                                          : l.reason == 5 ? "story_observation"
                                                          : "routine_life"
                                        : "idle")
      << "\",\"blocker\":\"" << ScheduleBlockerName(wait) << "\",\"wait_text\":\""
      << ScheduleWaitText(wait, stage) << "\",\"plant_request\":{\"id\":" << s.plant_action.id
      << ",\"status\":\"" << PlantActionStatusName(s.plant_action.status)
      << "\",\"result_reason\":\"" << PlantActionReasonName(s.plant_action.reason)
      << "\",\"wait_reason\":\"" << ScheduleBlockerName(s.plant_action.wait)
      << "\"},\"life\":{\"sequence\":" << l.sequence << ",\"intent\":\"" << LifeIntentName(l.intent)
      << "\",\"source\":\""
      << (!l.intent       ? "none"
          : approved      ? "approved_wish"
          : l.reason == 4 ? "explicit_player"
          : l.reason == 5 ? "story_observation"
                          : "routine")
      << "\",\"phase\":\""
      << (paused                ? "paused_for_work"
          : state.waiting_short ? "waiting_safe_point"
                                : LifePhaseName(l.phase))
      << "\",\"approved_token\":" << (approved ? state.approved_token : 0)
      << ",\"resume_after_task\":" << (approved && paused ? "true" : "false")
      << ",\"observed_ms\":" << l.elapsed << ",\"remaining_observation_ms\":"
      << (l.intent ? 20000 - std::min<uint32_t>(20000, l.elapsed) : 0)
      << ",\"arrived_now\":" << (l.phase == 3 && l.reached && !paused ? "true" : "false")
      << ",\"has_reached_before\":" << (l.reached ? "true" : "false")
      << "},\"last_life_result\":{\"scope\":\"current_boot\",\"sequence\":" << state.last_sequence
      << ",\"intent\":\"" << LifeIntentName(state.last_intent) << "\",\"result\":\""
      << (state.last_sequence ? LifeResultName(state.last_result) : "none") << "\",\"cause\":\""
      << ScheduleBlockerName(state.last_cause)
      << "\"},\"policy\":\"Emergency and active story first; explicit player task before routine "
         "life. Finish short tool actions safely. Only an already-approved stage1 wish resumes "
         "after temporary work. A new life arrangement or return-to-work cancels the old life "
         "plan. No offline replay.\"}";
    return o.str();
}
}  // namespace spring
