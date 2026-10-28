#pragma once
#include <type_traits>
#include "spring_story_json.h"

namespace spring {
// Fixed numeric sidecar. Interest is not a newly discovered historical fact.
struct CharacterState {
    uint32_t version = 1, world_hi = 0, world_lo = 0, tracking_second = 0;
    uint32_t focus_kind = 0, focus_source = 0, focus_target = 0, focus_chain = 0;
    uint32_t focus_trigger_kind = 0, focus_trigger_source = 0, focus_since_second = 0;
    LifeRecord focus_life{}, important_invitation{};
    uint32_t invitation_chain = 0, handled_invitation = 0, handled_archives = 0,
             handled_milestones = 0;
};
static_assert(std::is_trivially_copyable<CharacterState>::value, "numeric character sidecar");
struct AttentionEvent {
    uint32_t kind = 0, source = 0, target = 0;
};  // 1 invitation, 2 read archive, 3 milestone
inline bool CharacterMatches(const Snapshot& s, const CharacterState& c) {
    return s.memory && c.version == 1 && c.world_hi == s.memory->world_hi &&
           c.world_lo == s.memory->world_lo;
}
inline bool CompletedInvitation(const LifeRecord& r) {
    return r.id && r.intent == 2 && r.reason == 4 && r.result == 0 && r.reached;
}
inline bool FindLifeEvidence(const Snapshot& s, uint32_t id, LifeRecord& out, uint32_t& chain) {
    chain = 0;
    bool found = false;
    for (unsigned i = 0; i < s.life.count; ++i)
        if (s.life.records[i].id == id && s.life.records[i].result == 0 &&
            s.life.records[i].reached) {
            out = s.life.records[i];
            found = true;
        }
    if (s.memory)
        for (unsigned i = 0; i < s.memory->count; ++i) {
            const auto& e = s.memory->records[i];
            if (e.category == 1 && e.action == unsigned(MemoryAction::Finished) &&
                e.source_event == id && e.result == 0 && (e.flags & 16)) {
                out = {id,
                       e.kind,
                       e.target,
                       e.value & 255,
                       (e.value >> 8) & 255,
                       0,
                       e.second,
                       (e.flags >> 5) & 1,
                       1};
                chain = e.chain;
                found = true;
            }
        }
    return found && out.id == id && out.result == 0 && out.reached;
}
inline bool RefreshCharacter(const Snapshot& s, CharacterState& c) {
    if (!CharacterMatches(s, c))
        return false;
    LifeRecord newest = c.important_invitation;
    uint32_t chain = c.invitation_chain;
    for (unsigned i = 0; i < s.life.count; ++i) {
        auto r = s.life.records[i];
        uint32_t found_chain = 0;
        if (CompletedInvitation(r) && r.id > newest.id) {
            FindLifeEvidence(s, r.id, r, found_chain);
            newest = r;
            chain = found_chain;
        }
    }
    if (s.memory)
        for (unsigned i = 0; i < s.memory->count; ++i) {
            const auto& e = s.memory->records[i];
            if (e.category != 1 || e.action != unsigned(MemoryAction::Finished) ||
                e.source_event <= newest.id)
                continue;
            LifeRecord r{};
            uint32_t found_chain = 0;
            if (FindLifeEvidence(s, e.source_event, r, found_chain) && CompletedInvitation(r)) {
                newest = r;
                chain = found_chain;
            }
        }
    if (newest.id == c.important_invitation.id)
        return false;
    c.important_invitation = newest;
    c.invitation_chain = chain;
    return true;
}
inline bool ImportantMilestone(const Snapshot& s, const MemoryFact& f) {
    if (!f.event || (f.flags & 10))
        return false;  // Imported/baseline facts are not a new discovery.
    if (f.type >= 20 && f.type < 28)
        return f.kind == 1 && (s.economy.built & (1u << (f.type - 20)));
    return (f.type == 3 || f.type == 4) && f.kind == 7 && f.value == f.type - 2 &&
           s.tree_stage >= f.value;
}
inline const MemoryFact* FindMilestone(const Snapshot& s, uint32_t id) {
    if (s.memory)
        for (unsigned i = 0; i < s.memory->fact_count; ++i)
            if (s.memory->facts[i].event == id && ImportantMilestone(s, s.memory->facts[i]))
                return &s.memory->facts[i];
    return nullptr;
}
inline CharacterState EmptyCharacter(const Snapshot& s) {
    CharacterState c;
    if (s.memory) {
        c.world_hi = s.memory->world_hi;
        c.world_lo = s.memory->world_lo;
    }
    c.tracking_second = s.game_ms / 1000;
    RefreshCharacter(s, c);
    // Baseline records remain readable, but upgrading does not claim they just happened.
    c.handled_invitation = c.important_invitation.id;
    c.handled_archives = s.story.archives & s.story.archives_read;
    if (s.memory)
        for (unsigned i = 0; i < s.memory->fact_count; ++i)
            if (ImportantMilestone(s, s.memory->facts[i]))
                c.handled_milestones |= 1u << s.memory->facts[i].type;
    return c;
}
inline AttentionEvent NextAttention(const Snapshot& s, const CharacterState& c) {
    if (!CharacterMatches(s, c))
        return {};
    if (c.important_invitation.id > c.handled_invitation)
        return {1, c.important_invitation.id, c.important_invitation.target};
    const auto read = s.story.archives & s.story.archives_read;
    for (unsigned i = 0; i < 7; ++i)
        if ((read & ~c.handled_archives) & (1u << i))
            return {2, i, 0};
    if (s.memory)
        for (unsigned i = 0; i < s.memory->fact_count; ++i) {
            const auto& f = s.memory->facts[i];
            if (ImportantMilestone(s, f) && !(c.handled_milestones & (1u << f.type)))
                return {3, f.event, f.type};
        }
    return {};
}
inline void MarkAttention(CharacterState& c, const AttentionEvent& e) {
    if (e.kind == 1)
        c.handled_invitation = std::max(c.handled_invitation, e.source);
    if (e.kind == 2 && e.source < 7)
        c.handled_archives |= 1u << e.source;
    if (e.kind == 3 && e.target < 32)
        c.handled_milestones |= 1u << e.target;
}
inline const char* AttentionReason(uint32_t kind) {
    return kind == 1   ? "PLAYER_REQUEST_COMPLETED"
           : kind == 2 ? "READ_ARCHIVE_CHANGED_KNOWLEDGE"
           : kind == 3 ? "VERIFIED_MILESTONE_COMPLETED"
                       : "IDLE_OPPORTUNITY";
}
inline bool SetCharacterFocus(const Snapshot& s, CharacterState& c, uint32_t mode, uint32_t kind,
                              uint32_t source, uint32_t target) {
    if (!CharacterMatches(s, c) || mode > 2)
        return false;
    if (mode == 0)
        return true;
    if (mode == 1) {
        c.focus_kind = c.focus_source = c.focus_target = c.focus_chain = 0;
        c.focus_trigger_kind = c.focus_trigger_source = c.focus_since_second = 0;
        c.focus_life = {};
        return true;
    }
    LifeRecord record{};
    uint32_t chain = 0;
    bool valid = false;
    if (kind == 1)
        valid = source == 0 && target < (3u + bool(s.economy.built & (1u << 4)));
    if (kind == 2) {
        if (c.important_invitation.id == source) {
            record = c.important_invitation;
            chain = c.invitation_chain;
        } else
            FindLifeEvidence(s, source, record, chain);
        valid = CompletedInvitation(record) && target == record.target;
    }
    if (kind == 3)
        valid = source < 7 && target == 0 &&
                (s.story.archives & s.story.archives_read & (1u << source));
    if (kind == 4) {
        const auto* fact = FindMilestone(s, source);
        valid = fact && fact->type == target;
        chain = fact ? fact->chain : 0;
    }
    if (!valid)
        return false;
    c.focus_kind = kind;
    c.focus_source = source;
    c.focus_target = target;
    c.focus_chain = chain;
    c.focus_life = record;
    return true;
}
inline std::string InvitationTargetJson(const LifeRecord& r) {
    const bool plot = r.target < 4;
    const std::string name = plot            ? "种植位" + std::to_string(r.target + 1)
                             : r.target == 4 ? "纪念花"
                                             : "动物小窝";
    std::ostringstream o;
    o << ",\"target_kind\":"
      << StoryQuote(plot            ? "plot"
                    : r.target == 4 ? "memorial_flower"
                                    : "animal_home")
      << ",\"target_name\":" << StoryQuote(name.c_str())
      << ",\"target_slot\":" << (plot ? r.target + 1 : 0) << ",\"plant_id\":";
    if (plot)
        o << r.plant;
    else
        o << "null";
    return o.str();
}
inline std::string CharacterJson(const Snapshot& s, const CharacterState& c) {
    if (!CharacterMatches(s, c))
        return "{\"available\":false,\"current_focus\":null}";
    std::ostringstream o;
    o << "{\"available\":true,\"authority\":\"device\",\"current_focus\":";
    if (!c.focus_kind)
        o << "null";
    else {
        const char* label = c.focus_kind == 1   ? "留意一株植物"
                            : c.focus_kind == 2 ? (c.focus_target == 5   ? "玩家请我查看的动物"
                                                   : c.focus_target == 4 ? "玩家请我查看的纪念花"
                                                                         : "玩家请我完成的检查")
                            : c.focus_kind == 3 ? "读过的旧档案"
                                                : "已完成的建设";
        o << "{\"kind\":" << c.focus_kind << ",\"source_id\":" << c.focus_source
          << ",\"target\":" << c.focus_target << ",\"chain\":" << c.focus_chain
          << ",\"label\":" << StoryQuote(label) << ",\"question\":"
          << StoryQuote(c.focus_kind == 3 ? "这份记录和现在的照料有什么不同？"
                        : c.focus_kind == 2
                            ? (StoryLanguage(s.story, s.story_stage >= 2 ? 1 : 0) < 2
                                   ? (c.focus_target == 5   ? "下次查看动物小窝。"
                                      : c.focus_target == 4 ? "下次查看纪念花。"
                                                            : "下次检查这株植物。")
                                   : "这次受托检查，值得继续留意吗？")
                        : c.focus_kind == 4 ? "这里多了一种可以做的事情。"
                                            : "这株植物值得继续留意吗？")
          << ",\"selected_after\":" << StoryQuote(AttentionReason(c.focus_trigger_kind))
          << ",\"trigger_source\":" << c.focus_trigger_source
          << ",\"since_game_second\":" << c.focus_since_second;
        if (c.focus_kind == 2 && c.focus_life.id)
            o << ",\"evidence\":{\"id\":" << c.focus_life.id
              << ",\"target\":" << c.focus_life.target << InvitationTargetJson(c.focus_life)
              << ",\"game_second\":" << c.focus_life.second
              << ",\"result\":\"completed\",\"arrived\":true,\"player_requested\":true}";
        o << ",\"certainty\":\"interest_not_fact\"}";
    }
    const auto& r = c.important_invitation;
    o << ",\"important_invitation\":";
    if (!r.id)
        o << "null";
    else
        o << "{\"id\":" << r.id << ",\"chain\":" << c.invitation_chain
          << ",\"intent\":\"inspect_plant\",\"target\":" << r.target << InvitationTargetJson(r)
          << ",\"game_second\":" << r.second
          << ",\"result\":\"completed\",\"arrived\":true,\"player_requested\":true,\"player_"
             "watched\":null}";
    o << ",\"rule\":\"Interest is not recovered memory or new evidence. Request proves an "
         "invitation, not shared viewing.\"}";
    return o.str();
}
inline std::string ObservationJson(const Snapshot& s, const CharacterState& c) {
    const auto e = NextAttention(s, c);
    std::ostringstream o;
    o << "{\"schema\":55,\"character\":" << CharacterJson(s, c)
      << ",\"trigger\":{\"kind\":" << e.kind << ",\"source_id\":" << e.source
      << ",\"target\":" << e.target << ",\"reason\":" << StoryQuote(AttentionReason(e.kind))
      << "},\"known_archives\":[";
    bool comma = false;
    for (unsigned i = 0; i < 7; ++i)
        if (s.story.archives & s.story.archives_read & (1u << i)) {
            if (comma)
                o << ',';
            comma = true;
            o << StoryArchiveJson(s, i);
        }
    o << "],\"chapter_context\":" << ChapterJson(s, true) << ",\"milestones\":[";
    comma = false;
    if (s.memory)
        for (unsigned i = 0; i < s.memory->fact_count; ++i) {
            const auto& f = s.memory->facts[i];
            if (!ImportantMilestone(s, f))
                continue;
            if (comma)
                o << ',';
            comma = true;
            o << "{\"id\":" << f.event << ",\"type\":" << f.type << ",\"kind\":" << f.kind
              << ",\"target\":" << f.target << ",\"value\":" << f.value << ",\"chain\":" << f.chain
              << ",\"game_second\":" << f.second << "}";
        }
    o << "]}";
    return o.str();
}
}  // namespace spring
