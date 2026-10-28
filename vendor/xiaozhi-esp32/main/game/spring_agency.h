#pragma once
#include "spring_agency_types.h"
#include "spring_character.h"
#include "spring_story_authority.h"

namespace spring {
enum class WishStatus : uint32_t {
    None,
    Proposed,
    Presented,
    Executing,
    Completed,
    Interrupted,
    Deferred,
    Declined,
    Expired,
    RestartCancelled,
    Superseded
};
struct WishRecord {
    uint32_t token = 0, intent = 0, target = 0, created_second = 0, approved_second = 0;
    uint32_t sequence = 0, chain = 0, result = 0, reached = 0, status = 0;
};
struct AgencyState {
    uint32_t version = 1, world_hi = 0, world_lo = 0, next_token = 0;
    WishRecord current{};
    uint32_t count = 0;
    std::array<WishRecord, 6> history{};
};
static_assert(std::is_trivially_copyable<AgencyState>::value, "numeric agency sidecar");
inline AgencyState EmptyAgency(const Snapshot& s) {
    AgencyState a;
    if (s.memory) {
        a.world_hi = s.memory->world_hi;
        a.world_lo = s.memory->world_lo;
    }
    return a;
}
inline bool AgencyMatches(const Snapshot& s, const AgencyState& a) {
    return s.memory && a.version == 1 && a.world_hi == s.memory->world_hi &&
           a.world_lo == s.memory->world_lo && a.count <= 6 && a.next_token <= INT32_MAX &&
           a.current.token <= a.next_token &&
           a.current.status <= unsigned(WishStatus::Superseded) &&
           (!a.current.token ||
            (a.current.intent >= 1 && a.current.intent <= 3 && a.current.target <= 5));
}
inline bool WishPending(const AgencyState& a) {
    return a.current.token && (a.current.status == 1 || a.current.status == 2);
}
inline const char* WishStatusName(unsigned n) {
    static const char* names[] = {"none",
                                  "proposed",
                                  "presented",
                                  "executing",
                                  "completed",
                                  "interrupted",
                                  "deferred",
                                  "declined",
                                  "expired",
                                  "cancelled_by_restart",
                                  "superseded_by_player"};
    return names[n < 11 ? n : 0];
}
inline const char* WishStatusText(unsigned n) {
    static const char* texts[] = {"没有新的打算",
                                  "还没行动，想听听你的意见。",
                                  "尚未行动，等你同意。",
                                  "已同意，查看实际执行进度。",
                                  "已经完成了。",
                                  "刚才被工作打断了。",
                                  "这次先放一放。",
                                  "这次不去。",
                                  "这个想法先放下了。",
                                  "重新启动后已取消。",
                                  "按你的新安排，原来的先不继续。"};
    return texts[n < 11 ? n : 0];
}
inline const char* WishName(unsigned intent, unsigned target) {
    return intent == 1   ? "去窗边看看"
           : intent == 3 ? "休息一会儿"
           : target == 5 ? "看看动物"
           : target == 4 ? "看看纪念花"
                         : "检查植物";
}
inline void FinishWish(AgencyState& a, WishStatus status, unsigned result = 0) {
    auto r = a.current;
    r.status = unsigned(status);
    r.result = result;
    if (a.count == a.history.size()) {
        std::move(a.history.begin() + 1, a.history.end(), a.history.begin());
        --a.count;
    }
    a.history[a.count++] = r;
    a.current = r;
}
inline bool OfferWish(const Snapshot& s, AgencyState& a, unsigned intent, unsigned target) {
    if (!AgencyMatches(s, a) || !s.storage_ok || WishPending(a) || a.current.status == 3 ||
        a.next_token >= INT32_MAX || AgencyMode(s.story, intent, target) != 1 ||
        !PersonalLife(intent, target) || target > 5 || (intent != 2 && target != 0))
        return false;
    a.current = {++a.next_token,
                 intent,
                 target,
                 uint32_t(s.game_ms / 1000),
                 0,
                 0,
                 0,
                 0,
                 0,
                 unsigned(WishStatus::Proposed)};
    return true;
}
inline bool PresentWish(AgencyState& a, unsigned token) {
    if (!WishPending(a) || token != a.current.token)
        return false;
    a.current.status = unsigned(WishStatus::Presented);
    return true;
}
inline bool CanApproveWish(const Snapshot& s, const AgencyState& a, unsigned token) {
    return s.storage_ok && (!s.story.enabled || s.story.active == kNoStory) &&
           AgencyMatches(s, a) && a.current.token == token &&
           a.current.status == unsigned(WishStatus::Presented) &&
           AgencyMode(s.story, a.current.intent, a.current.target) == 1;
}
enum class WishReplyDecision { Rejected, Approve, Defer, Decline, AlreadyExecuting };
struct WishReplyContext {
    bool from_voice = false, in_wish_session = false;
    uint32_t current_turn = 0, presented_turn = UINT32_MAX;
};
// The board and isolated integration runner share this authorization decision.
// A transport retry may acknowledge an existing action, but must still belong
// to the same world and a valid, later player turn when it comes from voice.
inline WishReplyDecision DecideWishReply(const Snapshot& s, const AgencyState& a, unsigned token,
                                         unsigned choice, const WishReplyContext& reply) {
    if (!s.storage_ok || !AgencyMatches(s, a) || !token || token != a.current.token || choice < 1 ||
        choice > 3 ||
        (reply.from_voice && (!reply.in_wish_session ||
                              !FreshVoiceConfirmation(reply.current_turn, reply.presented_turn))))
        return WishReplyDecision::Rejected;
    if (a.current.status == unsigned(WishStatus::Executing) && choice == 1)
        return WishReplyDecision::AlreadyExecuting;
    if (!CanApproveWish(s, a, token))
        return WishReplyDecision::Rejected;
    return choice == 1   ? WishReplyDecision::Approve
           : choice == 2 ? WishReplyDecision::Defer
                         : WishReplyDecision::Decline;
}
inline bool StageWishApproval(Core& candidate, AgencyState& next, unsigned token,
                              MemoryActor actor = MemoryActor::VoiceRequest) {
    if (!candidate.storage_ok || !CanApproveWish(candidate.Get(), next, token) ||
        !candidate.StartLifeRequest(next.current.intent, next.current.target,
                                    unsigned(LifeReason::PlayerInvitation), actor))
        return false;
    candidate.MarkApprovedLife(token);
    next.current.status = unsigned(WishStatus::Executing);
    next.current.approved_second = candidate.s.game_ms / 1000;
    next.current.sequence = candidate.s.life.sequence;
    next.current.chain = candidate.s.memory ? candidate.s.memory->life_chain : 0;
    return true;
}
inline bool RecoverPendingWish(AgencyState& a) {
    if (!WishPending(a))
        return false;
    FinishWish(a, WishStatus::RestartCancelled);
    return true;
}
inline bool RefreshAgency(const Snapshot& s, AgencyState& a) {
    if (!AgencyMatches(s, a) || a.current.status != 3)
        return false;
    LifeRecord r{};
    uint32_t chain = 0;
    if (FindLifeEvidence(s, a.current.sequence, r, chain)) {
        a.current.reached = r.reached;
        FinishWish(a, r.result == 0 ? WishStatus::Completed : WishStatus::Interrupted, r.result);
        return true;
    }
    // Interrupted records also have evidence; they must not be mistaken for completion.
    for (unsigned i = 0; i < s.life.count; ++i)
        if (s.life.records[i].id == a.current.sequence) {
            const auto& item = s.life.records[i];
            a.current.reached = item.reached;
            FinishWish(a,
                       item.result == 0 && item.reached ? WishStatus::Completed
                       : item.result == unsigned(LifeResult::TargetChanged) &&
                               s.schedule.last_sequence == item.id &&
                               s.schedule.last_cause == ScheduleBlocker::PlayerActivity
                           ? WishStatus::Superseded
                           : WishStatus::Interrupted,
                       item.result);
            return true;
        }
    if (s.life.sequence == a.current.sequence && s.life.intent && s.life.reached &&
        !a.current.reached) {
        a.current.reached = 1;
        return true;
    }
    if (!s.life.intent || s.life.sequence != a.current.sequence) {
        FinishWish(a, WishStatus::Interrupted, unsigned(LifeResult::Rebooted));
        return true;  // Missing evidence is never promoted to completion.
    }
    return false;
}
inline void WriteWish(std::ostringstream& o, const WishRecord& r) {
    o << "{\"token\":" << r.token << ",\"intent\":" << StoryQuote(LifeIntentName(r.intent))
      << ",\"intent_id\":" << r.intent << ",\"target\":" << r.target
      << ",\"topic\":" << StoryQuote(WishName(r.intent, r.target))
      << ",\"status\":" << StoryQuote(WishStatusName(r.status))
      << ",\"created_game_second\":" << r.created_second
      << ",\"approved_game_second\":" << r.approved_second << ",\"action_sequence\":" << r.sequence
      << ",\"chain\":" << r.chain << ",\"arrived\":" << (r.reached ? "true" : "false")
      << ",\"life_result\":" << r.result << "}";
}
inline std::string AgencyJson(const Snapshot& s, const AgencyState& a, bool active = false,
                              bool include_history = true) {
    std::ostringstream o;
    o << "{\"authority\":\"device\",\"level\":" << AgencyLevel(s.story)
      << ",\"story_language_stage\":" << StoryLanguage(s.story, 0)
      << ",\"implemented_max_level\":1,\"personal_action_mode\":"
      << StoryQuote(AgencyLevel(s.story) ? "ASK_PERMISSION" : "WAIT_INSTRUCTION")
      << ",\"routine_maintenance_automatic\":true,\"experience_reports_allowed\":true,"
         "\"available\":"
      << (AgencyMatches(s, a) ? "true" : "false")
      << ",\"pending\":" << (WishPending(a) ? "true" : "false")
      << ",\"active\":" << (active && WishPending(a) ? "true" : "false") << ",\"wish\":";
    if (AgencyMatches(s, a) && a.current.token)
        WriteWish(o, a.current);
    else
        o << "null";
    const bool same = AgencyMatches(s, a) && s.life.intent && s.life.sequence == a.current.sequence;
    const char* phase = !same                                       ? "no_active_body"
                        : s.schedule.pause != ScheduleBlocker::None ? "paused_for_work"
                        : s.schedule.waiting_short                  ? "waiting_safe_point"
                                                                    : LifePhaseName(s.life.phase);
    o << ",\"execution\":{\"phase\":" << StoryQuote(phase) << ",\"resume_after_player_task\":"
      << (same && AgencyLevel(s.story) == 1 && s.schedule.approved_token == a.current.token &&
                  s.schedule.pause != ScheduleBlocker::None
              ? "true"
              : "false")
      << "}";
    o << ",\"history\":[";
    if (include_history && AgencyMatches(s, a))
        for (unsigned i = 0; i < a.count; ++i) {
            if (i)
                o << ',';
            WriteWish(o, a.history[i]);
        }
    o << "],\"rule\":\"Opening a wish is not approval. Explicit later consent is required. "
         "Completed and interrupted are distinct. No resource spending.\"}";
    return o.str();
}
inline std::string CharacterSummaryJson(const Snapshot& s, const CharacterState& c) {
    std::ostringstream o;
    o << "{\"available\":" << (CharacterMatches(s, c) ? "true" : "false") << ",\"current_focus\":";
    if (!CharacterMatches(s, c) || !c.focus_kind)
        o << "null";
    else
        o << "{\"kind\":" << c.focus_kind << ",\"source_id\":" << c.focus_source
          << ",\"target\":" << c.focus_target << ",\"label\":"
          << StoryQuote(c.focus_kind == 1   ? "留意一株植物"
                        : c.focus_kind == 2 ? "上次受托查看的经历"
                        : c.focus_kind == 3 ? "读过的旧档案"
                                            : "已完成的建设")
          << "}";
    o << ",\"details_tool\":\"get_character_state\"}";
    return o.str();
}
inline std::string AgencySummaryJson(const Snapshot& s, const AgencyState& a, bool active) {
    std::ostringstream o;
    o << "{\"level\":" << AgencyLevel(s.story)
      << ",\"implemented_max_level\":1,\"active\":" << (active && WishPending(a) ? "true" : "false")
      << ",\"pending\":" << (WishPending(a) ? "true" : "false")
      << ",\"wish_token\":" << a.current.token << ",\"details_tool\":\"get_character_state\"}";
    return o.str();
}
// Voice uses a bounded observation. Archive bodies are fetched from the existing
// archive tool; the PC observation still receives the full read-only excerpts.
inline std::string AgencyObservationJson(const Snapshot& s, const CharacterState& c,
                                         const AgencyState& a, bool active = false) {
    std::ostringstream o;
    const auto trigger = NextAttention(s, c);
    o << "{\"schema\":56,\"character\":" << CharacterJson(s, c)
      << ",\"agency\":" << AgencyJson(s, a, active) << ",\"trigger\":{\"kind\":" << trigger.kind
      << ",\"source_id\":" << trigger.source << "},\"known_archive_ids\":[";
    bool comma = false;
    for (unsigned i = 0; i < 7; ++i)
        if (s.story.archives & s.story.archives_read & (1u << i)) {
            if (comma)
                o << ',';
            o << i;
            comma = true;
        }
    o << "],\"archive_details_tool\":\"get_story_archive\",\"milestone_details_tool\":\"get_"
         "memories\"}";
    return o.str();
}
}  // namespace spring
