#pragma once
#include "spring_character.h"
namespace spring {
// Route the invitation that is happening now before the historical unread inbox.
// Physical chapter actions and choices retain their existing dedicated entry points.
inline unsigned StoryCommunicationNode(const Snapshot& s) {
    if (!s.story.enabled || s.story.active != kNoStory)
        return kNoStory;
    const auto id = StoryCurrentNode(s.story);
    if (id >= kStoryCount || id == s.story.paused || !(s.story.available & (1u << id)) ||
        (s.story.completed & (1u << id)) || StoryContactHandled(s.story, id))
        return kNoStory;
    return StoryContactNode(id) ? id : kNoStory;
}
inline bool CommunicationBubbleIdle(const Snapshot& s) {
    return s.storage_ok && s.work == Work::Rest && !s.work_ms && !s.tree_work_ms &&
           s.economy.construction >= 8 && !s.economy.recycle_ms && !s.economy.journey_work &&
           !s.economy.emergency_work && !PlantActionActive(s.plant_action) && s.life.phase != 1 &&
           s.life.phase != 2;
}
// UI-only receipt: subtitles alone are not a played opening. Reset per session;
// the board supplies true only after speaking and physical playback have drained.
struct StoryPlaybackReceipt {
    unsigned playing = kNoStory, delivered = kNoStory;
    void Reset() { playing = delivered = kNoStory; }
    unsigned Poll(unsigned response, bool speaking, bool listening_and_drained) {
        if (response >= kStoryCount || response == delivered)
            return kNoStory;
        if (speaking)
            playing = response;
        if (playing == response && listening_and_drained) {
            delivered = response;
            return response;
        }
        return kNoStory;
    }
};
enum class CommunicationRouteKind { None, Story, Archive, Wish, Social };
struct CommunicationRoute {
    CommunicationRouteKind kind = CommunicationRouteKind::None;
    unsigned id = 0;
};
inline CommunicationRoute SelectCommunication(const Snapshot& s, unsigned wish, unsigned social,
                                              bool include_history = true) {
    const auto node = StoryCommunicationNode(s);
    if (node < kStoryCount) {
        const auto archive = kStorySpecs[node].archive;
        if (archive < 7 && (s.story.archives & (1u << archive)))
            return {CommunicationRouteKind::Archive, archive};
        return {CommunicationRouteKind::Story, node};
    }
    if (wish)
        return {CommunicationRouteKind::Wish, wish};
    if (social)
        return {CommunicationRouteKind::Social, social};
    if (include_history)
        for (unsigned archive = 0; archive < 7; ++archive)
            if ((s.story.archives & ~s.story.archives_read) & (1u << archive))
                return {CommunicationRouteKind::Archive, archive};
    return {};
}
inline bool DroughtCommunicationPending(const Snapshot& s) {
    const auto id = unsigned(StoryNode::Drought);
    return s.story.enabled && s.story.pending == id && s.story.active == kNoStory &&
           (s.story.available & (1u << id)) && !(s.story.completed & (1u << id));
}
// V1 remains readable; V2 adds typed evidence while retaining the original core save.
struct LegacySocialState {
    uint32_t version = 1, world_hi = 0, world_lo = 0, last_offered_id = 0, topic_mask = 0;
    LifeRecord pending{}, accepted{};
};
struct SocialState {
    uint32_t version = 2, world_hi = 0, world_lo = 0, last_offered_id = 0, topic_mask = 0;
    LifeRecord pending{}, accepted{};  // id is an invitation ticket, never an inferred life action.
    uint32_t pending_kind = 1, accepted_kind = 1, pending_source = 0, accepted_source = 0;
    uint32_t pending_chain = 0, accepted_chain = 0, last_life_source = 0;
};
inline bool SocialMatches(const Snapshot& s, const SocialState& c) {
    return s.memory && c.version == 2 && c.world_hi == s.memory->world_hi &&
           c.world_lo == s.memory->world_lo;
}
inline SocialState EmptySocial(const Snapshot& s) {
    SocialState c;
    if (s.memory) {
        c.world_hi = s.memory->world_hi;
        c.world_lo = s.memory->world_lo;
    }
    return c;
}
inline SocialState MigrateSocial(const Snapshot& s, const LegacySocialState& old) {
    auto c = EmptySocial(s);
    if (!s.memory || old.version != 1 || old.world_hi != s.memory->world_hi ||
        old.world_lo != s.memory->world_lo)
        return c;
    c.last_offered_id = c.last_life_source = old.last_offered_id;
    c.topic_mask = old.topic_mask;
    c.pending = old.pending;
    c.accepted = old.accepted;
    c.pending_source = old.pending.id;
    c.accepted_source = old.accepted.id;
    return c;
}
inline bool OfferSocialEvidence(const Snapshot& s, uint32_t kind, uint32_t source, SocialState& c,
                                const CharacterState* character = nullptr) {
    if (!SocialMatches(s, c) || c.pending.id || c.last_offered_id == UINT32_MAX)
        return false;
    LifeRecord r{};
    uint32_t chain = 0, bit = 0;
    if (kind == 1) {
        if (!source)
            return false;
        FindLifeEvidence(s, source, r, chain);
        if (character && CharacterMatches(s, *character) &&
            character->important_invitation.id == source) {
            r = character->important_invitation;
            chain = character->invitation_chain;
        }
        if (r.id != source || r.result != 0 || !r.reached ||
            (r.intent != 1 && r.intent != 3 && !CompletedInvitation(r) &&
             !(s.roadshow && r.intent == 2)))
            return false;
        bit = 1u << r.intent;
    } else if (kind == 2) {
        if (source >= 7 || !(s.story.archives & s.story.archives_read & (1u << source)))
            return false;
        bit = 1u << (8 + source);
    } else if (kind == 3) {
        const auto* f = FindMilestone(s, source);
        if (!f)
            return false;
        bit = 1u << (f->type == 3 ? 16 : f->type == 4 ? 17 : f->type);
        r.second = f->second;
        r.reason = f->type;
        r.intent = f->kind;
        r.target = f->target;
        r.plant = f->value;
        chain = f->chain;
    } else
        return false;
    // Roadshows may share a new completed activity of the same kind, never replay its source.
    if (s.roadshow && kind == 1) {
        if (source <= c.last_life_source)
            return false;
    } else if (c.topic_mask & bit)
        return false;
    r.id = ++c.last_offered_id;
    c.pending = r;
    c.pending_kind = kind;
    c.pending_source = source;
    c.pending_chain = chain;
    c.topic_mask |= bit;
    if (kind == 1)
        c.last_life_source = std::max(c.last_life_source, source);
    return true;
}
inline bool OfferSocial(const Snapshot& s, uint32_t id, SocialState& c) {
    return OfferSocialEvidence(s, 1, id, c);
}
inline bool AcceptSocial(uint32_t id, SocialState& c) {
    if (!id || c.pending.id != id)
        return false;
    c.accepted = c.pending;
    c.accepted_kind = c.pending_kind;
    c.accepted_source = c.pending_source;
    c.accepted_chain = c.pending_chain;
    c.pending = {};
    c.pending_source = c.pending_chain = 0;
    return true;
}
inline const char* SocialTopic(const LifeRecord& r) {
    return r.intent == 1   ? "一次窗边停留"
           : r.intent == 3 ? "一次休息"
           : r.intent == 2 ? (r.reason == 4 ? "你请我完成的检查" : "一次例行植物检查")
                           : "";
}
inline const char* SocialTopic(const SocialState& state, bool accepted) {
    const auto kind = accepted ? state.accepted_kind : state.pending_kind;
    const auto source = accepted ? state.accepted_source : state.pending_source;
    return kind == 2 && source < 7 ? kStoryArchiveTitles[source]
           : kind == 3             ? "已完成的建设"
                                   : SocialTopic(accepted ? state.accepted : state.pending);
}
inline std::string SocialJson(const Snapshot& s, const SocialState& c, bool active) {
    if (!SocialMatches(s, c))
        return "{\"pending\":false,\"active\":false}";
    const auto& r = active ? c.accepted : c.pending;
    const auto kind = active ? c.accepted_kind : c.pending_kind;
    const auto source = active ? c.accepted_source : c.pending_source;
    const auto chain = active ? c.accepted_chain : c.pending_chain;
    std::ostringstream o;
    o << "{\"pending\":" << (c.pending.id ? "true" : "false")
      << ",\"active\":" << (active && c.accepted.id ? "true" : "false")
      << ",\"last_offered_id\":" << c.last_offered_id
      << ",\"last_life_source\":" << c.last_life_source << ",\"topic_mask\":" << c.topic_mask;
    if (r.id) {
        o << ",\"initiator\":\"robot\",\"event_id\":" << r.id << ",\"source_kind\":" << kind
          << ",\"source_id\":" << source << ",\"chain\":" << chain << ",\"topic\":"
          << StoryQuote(kind == 1   ? SocialTopic(r)
                        : kind == 2 ? "读过的旧档案"
                                    : "已完成的建设")
          << ",\"result\":\"completed\",\"source_game_second\":" << r.second
          << ",\"source_time_known\":" << (kind == 2 ? "false" : "true");
        if (kind == 1) {
            o << ",\"intent\":" << StoryQuote(LifeIntentName(r.intent))
              << ",\"arrived\":true,\"player_requested\":" << (r.reason == 4 ? "true" : "false");
            if (r.intent == 2)
                o << InvitationTargetJson(r);
        }
        if (kind == 2)
            o << ",\"archive_id\":" << source << ",\"archive\":" << StoryArchiveJson(s, source);
        if (kind == 3)
            o << ",\"target\":" << r.target << ",\"value\":" << r.plant
              << ",\"milestone_type\":" << r.reason << ",\"kind\":" << r.intent;
        o << ",\"player_participated\":false,\"specific_scenery_observed\":false,\"interpretation_"
             "rule\":"
          << StoryQuote(
                 "You initiated contact. Share this event, not why player called. No invented "
                 "observations or shared viewing.");
    }
    o << "}";
    return o.str();
}
}  // namespace spring
