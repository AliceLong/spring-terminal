#include "game/spring_mind_slot.h"
#include "game/spring_social.h"
#include <cassert>
#include <iostream>
using namespace spring;
int main() {
  Core c;
  c.EnableMemory(11, 22);
  const auto water = c.s.water, energy = c.s.energy, fiber = c.s.fiber,
             revision = c.s.story.revision;
  auto social = EmptySocial(c.Get());
  assert(!OfferSocial(c.Get(), 1, social));
  auto &life = c.s.life;
  life.count = 1;
  life.records[0] = {1,  1, 0, 0, 1, unsigned(LifeResult::WorkInterrupted),
                     42, 0, 0};
  assert(!OfferSocial(c.Get(), 1, social));
  life.records[0].result = 0;
  assert(!OfferSocial(c.Get(), 1, social));
  life.records[0].reached = 1;
  assert(OfferSocial(c.Get(), 1, social));
  assert(social.pending.id == 1 && !social.accepted.id);
  assert(!OfferSocial(c.Get(), 1, social));
  assert(!AcceptSocial(2, social));
  assert(AcceptSocial(1, social));
  assert(!social.pending.id && social.accepted.id == 1);
  life.records[0].id = 2;
  assert(!OfferSocial(c.Get(), 2, social)); // Same routine topic never spams.
  life.records[0].intent = 3;
  assert(OfferSocial(c.Get(), 2, social));
  assert(!AcceptSocial(1, social)); // Old taps cannot clear a new invitation.
  const auto json = SocialJson(c.Get(), social, true);
  assert(json.find("\"event_id\":1") != std::string::npos);
  assert(json.find("\"initiator\":\"robot\"") != std::string::npos);
  assert(json.find("You initiated contact") != std::string::npos);
  assert(json.find("\"player_participated\":false") != std::string::npos);
  assert(json.find("specific_scenery_observed\":false") != std::string::npos);
  Core other;
  other.EnableMemory(99, 22);
  assert(!SocialMatches(other.Get(), social));
  assert(!OfferSocial(other.Get(), 2, social));
  assert(SocialJson(other.Get(), social, true).find("event_id") ==
         std::string::npos);
  auto restored = social; // Sidecar is a self-contained factual record after
                          // history eviction.
  life.count = 0;
  assert(SocialJson(c.Get(), restored, true).find("\"event_id\":1") !=
         std::string::npos);
  MindPacket packet;
  assert(MindLine::Parse("MIND2 1 2 b 16 9 0 0 2", packet));
  assert(packet.social_record == 2 && packet.intent == 0);
  assert(!MindLine::Parse("MIND2 1 2 b 16 9 0 0 -1", packet));
  assert(!MindLine::Parse("MIND2 1 2 b 16 9 0 0 2 extra", packet));
  assert(MindLine::Parse("MIND1 1 2 b 16 9 1 0", packet));
  assert(packet.social_record == 0);
  Core busy;
  busy.EnableMemory(11, 22);
  busy.s.work_ms = 1000;
  busy.s.life.count = 1;
  busy.s.life.records[0] = {7,  3, 0, 0, 3, unsigned(LifeResult::Completed),
                            42, 0, 1};
  auto invitation = EmptySocial(busy.Get());
  MindPacket share{1, 2, 11, 22, 8, 3, 0, 7};
  MindReplyGuard reply{1, 2, 11, 22, 9, 20000};
  assert(!reply.BodyAvailable(share, 10000, 0));
  assert(reply.Fresh(share, 10000) &&
         OfferSocial(busy.Get(), share.social_record, invitation));
  assert(invitation.pending_source == 7 && invitation.pending.id == 1 &&
         busy.s.work_ms == 1000 && !busy.LifeActive());
  assert(c.s.water == water && c.s.energy == energy && c.s.fiber == fiber &&
         c.s.story.revision == revision);
  // Faster roadshow pacing changes opportunities, never facts or formal deduplication.
  static_assert(AttentionPacing(false).model_ms == 120000);
  static_assert(AttentionPacing(false).life_ms == 120000);
  static_assert(AttentionPacing(false).repeat_ms == 300000);
  static_assert(AttentionPacing(false).wish_ms == 600000);
  static_assert(AttentionPacing(true).model_ms == 30000);
  static_assert(AttentionPacing(true).life_ms == 45000);
  Core show;
  show.EnableMemory(77, 88);
  show.s.life.count = 1;
  auto& record = show.s.life.records[0];
  record = {10, 2, 0, 0, 2, unsigned(LifeResult::Completed), 80, 0, 1};
  auto repeat = EmptySocial(show.Get());
  assert(!OfferSocial(show.Get(), 10, repeat));  // Routine inspection stays silent in formal play.
  show.roadshow = true;
  assert(OfferSocial(show.Get(), 10, repeat));
  assert(std::string(SocialTopic(repeat.pending)) == "一次例行植物检查");
  assert(!OfferSocial(show.Get(), 10, repeat));  // One pending contact.
  assert(AcceptSocial(repeat.pending.id, repeat));
  auto replay = repeat;  // Persistent sidecar retains deduplication after reboot.
  assert(!OfferSocial(show.Get(), 10, replay));
  record.id = 11;
  assert(OfferSocial(show.Get(), 11, replay));  // Same topic, new verified completion.
  assert(AcceptSocial(replay.pending.id, replay));
  record.id = 12;
  record.reached = 0;
  assert(!OfferSocial(show.Get(), 12, replay));
  record.reached = 1;
  record.result = unsigned(LifeResult::WorkInterrupted);
  assert(!OfferSocial(show.Get(), 12, replay));
  record.result = unsigned(LifeResult::Completed);
  show.roadshow = false;
  assert(!OfferSocial(show.Get(), 12, replay));  // Normal speed demo is not a roadshow.
  for (bool roadshow_mode : {false, true}) {
    Core paced;
    paced.roadshow = roadshow_mode;
    paced.s.life.intent = 2;
    paced.s.life.sequence = 1;
    paced.s.life.reached = 1;
    paced.FinishLife(LifeResult::Completed);
    assert(paced.s.life.cooldown == (roadshow_mode ? 45000u : 120000u));
    assert(paced.s.life.repeat_cooldown[1] == (roadshow_mode ? 90000u : 300000u));
  }
  auto story = c.Get();
  story.story.enabled = true;
  story.story.available = 1u << unsigned(StoryNode::Drought);
  story.story.pending = unsigned(StoryNode::Drought);
  story.story.active = kNoStory;
  assert(DroughtCommunicationPending(story)); // Deterministic, no model needed.
  story.story.active = unsigned(StoryNode::Drought);
  assert(!DroughtCommunicationPending(
      story)); // No second invitation during discussion.
  story.story.active = kNoStory;
  story.story.completed = story.story.available;
  assert(!DroughtCommunicationPending(story));
  std::cout << "social: factual completion, deduplication, old taps, world "
               "isolation and bounded packets pass\n";
}
