#include "game/spring_json.h"
#include "game/spring_story_authority.h"
#include "spring_gardener.h"
#include <cassert>
#include <fstream>
#include <iostream>
using namespace spring;
Core Fresh(unsigned world) {
  Core c;
  c.storage_ok = true;
  c.EnableEconomy(true);
  c.EnableMemory(46, world);
  c.EnableStory();
  c.s.chapters.version = 0; // Legacy story regression; new chapter flow has a dedicated test.
  assert(c.Wake());
  return c;
}
void RoundTrip(Core &c) {
  auto save = c.PackStory();
  Core restored;
  restored.storage_ok = true;
  assert(restored.RestoreStory(*save));
  assert(restored.PackStory()->checksum == save->checksum);
  auto checkpoint = c.PackStoryCheckpoint();
  assert(restored.RestoreStoryCheckpoint(*checkpoint));
  auto bad = *checkpoint;
  ++bad.world_lo;
  bad.checksum = Core::StoryCheckpointChecksum(bad);
  assert(!restored.RestoreStoryCheckpoint(bad));
}
int main(int argc, char **argv) {
  assert(argc == 2);
  std::ofstream replay(argv[1]);
  replay << '[';
  bool comma = false;
  unsigned routes = 0, saves = 0;
  for (unsigned seed : {1u, 2u, 3u}) {
    auto c = Fresh(seed);
    spring_gardener::Controller body;
    assert(!c.AcceptStory(unsigned(StoryNode::ReturnProof)));
    assert(StoryArchiveJson(c.Get(), 6).find("false") != std::string::npos);
    assert(c.Harvest(0, false, MemoryActor::Player));
    if (seed == 1) {
      assert(c.ProposeStoryName(2, "小芽"));
      assert(!FreshVoiceConfirmation(1, 1));
      assert(FreshVoiceConfirmation(2, 1));
      assert(c.ConfirmStoryName(c.s.story.candidate_token));
    }
    bool paused = false, flower = false, rest = false, special_save = false;
    for (uint64_t now = 100; now <= 5400000; now += 100) {
      c.Step(100);
      // This replay supplies explicit player permission; unapproved personal
      // wishes no longer silently execute at agency levels 0/1.
      if ((!rest || (flower && !special_save)) && !c.LifeActive() && c.s.story.active == kNoStory) {
        const unsigned intent = !rest ? 3 : 2, target = !rest ? 0 : 4;
        if (c.CanInviteLife(intent,target)) c.InviteLife(intent,target);
      }
      c.StepLife(100, false, spring_gardener::LifeArrived(c.Get(), body.Get()));
      body.Step(c.Get(), now, spring_gardener::Voice::None);
      if (c.s.life.phase == 3 && c.s.life.reached) {
        assert(c.ShowStoryLife(c.s.life.sequence));
        if (c.s.life.intent == 3) {
          rest = true;
          // Merely rendering cannot be recorded as player attention.
          if (!c.StoryDone(StoryNode::Rest)) {
            assert(!(c.s.story.life_attention & 1));
            assert(c.NoticeStoryLife());
            assert(c.CompleteStory(unsigned(StoryNode::Rest)));
          }
        }
        if (c.s.life.target == 4 || c.s.life.target == 5) {
          RoundTrip(c);
          ++saves;
          special_save = true;
        }
      }
      c.TickStory(0);
      if (c.s.economy.journey && c.s.economy.journey != 4 &&
          c.s.economy.journey != 5) {
        assert(c.Propose(5, c.s.economy.journey_id, 0));
        assert(c.ConfirmProposal(c.s.economy.proposal.token));
      }
      const unsigned id = StoryCurrentNode(c.s.story);
      if (id < kStoryCount) {
        if (id == unsigned(StoryNode::PlantWish)) {
          assert(c.OfferStoryFlower());
          const auto water = c.s.water;
          if (water >= 2) {
            assert(c.ApproveStoryFlower(c.s.story.offer_token));
            assert(c.s.water == water - 2);
            flower = true;
          }
        } else {
          assert(c.AcceptStory(id));
          if (id == unsigned(StoryNode::Return) && !paused) {
            assert(c.PauseStory(id));
            auto saved = c.PackStory();
            Core reboot;
            reboot.storage_ok = true;
            assert(reboot.RestoreStory(*saved));
            assert(reboot.s.story.paused == id &&
                   reboot.s.story.active == kNoStory);
            assert(reboot.AcceptStory(id));
            c = std::move(reboot);
            paused = true;
          }
          const unsigned choice = id == unsigned(StoryNode::Seed) ? seed : 0;
          assert(c.CompleteStory(id, choice));
          if (comma)
            replay << ',';
          comma = true;
          replay << "{\"route\":" << seed << ",\"node\":" << id
                 << ",\"state\":" << WorldJson(c.Get()) << '}';
          RoundTrip(c);
          ++saves;
          // A newly unlocked subject must not cause an immediate new
          // invitation.
          assert(c.s.story.pending == kNoStory);
        }
      }
      if (now % 60000 == 0) {
        RoundTrip(c);
        ++saves;
      }
      assert(c.s.water <= 100 && c.s.energy <= 100 && c.s.fiber <= 100);
      if (c.StoryDone(StoryNode::Share) &&
          (seed != 1 || (c.s.story.flower == 4 && special_save)))
        break;
    }
    assert(c.StoryDone(StoryNode::Share));
    assert(c.s.story.language == 3 && c.s.story.new_leaf &&
           c.s.story.care_after_truth);
    assert(rest && paused);
    assert(!c.s.economy.animals); // No rescue is needed to complete any route.
    assert(flower == (seed == 1));
    assert(c.s.story.flower == (seed == 1 ? 4u : 0u));
    assert(c.s.story.choices[unsigned(StoryNode::Seed)] == seed);
    ++routes;
  }
  replay << ']';
  auto c = Fresh(10);
  c.AcceptStory(0);
  c.CompleteStory(0);
  c.Step(100);
  const auto base = c.PackStory();
  Core reboot;
  reboot.storage_ok = true;
  assert(reboot.RestoreStory(*base));
  c.Step(100);
  auto newer = c.PackStoryCheckpoint();
  assert(reboot.RestoreStoryCheckpoint(*newer));
  const auto older = *base;
  auto old_ck = Core{};
  old_ck.storage_ok = true;
  assert(old_ck.RestoreStory(older));
  assert(!reboot.RestoreStoryCheckpoint(*old_ck.PackStoryCheckpoint()));
  c.s.story.elapsed = 86400000;
  c.TickStory(1000);
  assert(c.s.story.elapsed == 86401000);
  c.s.story.elapsed = UINT32_MAX - 99;
  c.TickStory(100);
  assert(c.s.story.elapsed == 0);
  std::cout << "Story natural flow: " << routes << " seed routes, " << saves
            << " save/checkpoint round-trips; actual five-region economy, body "
               "arrival, rest noticing, flower growth/inspection, "
               "pause/restart and no-purchase finale passed\n";
}
