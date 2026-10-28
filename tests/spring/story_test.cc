#include "game/spring_core.h"
#include <cassert>
#include <iostream>
using namespace spring;
static void finish(Core &c, StoryNode id, unsigned choice = 0) {
  c.TickStory(0);
  unsigned i = static_cast<unsigned>(id);
  assert(c.s.story.available & (1u << i));
  assert(c.AcceptStory(i));
  assert(c.CompleteStory(i, choice));
}
int main() {
  Core c;
  c.storage_ok = true;
  c.EnableEconomy(true);
  c.EnableMemory(1, 42);
  c.EnableStory();
  c.s.chapters.version = 0; // Legacy story regression; new chapter flow has a dedicated test.
  // Reusing the render buffer must overwrite stale base and runtime state.
  Snapshot reused;
  reused.motion_ok = true;
  reused.water = 999;
  reused.demo = true;
  reused.story.enabled = false;
  c.Get(reused);
  assert(reused.water == c.s.water && reused.story.enabled);
  assert(reused.demo == c.demo && !reused.motion_ok);
  assert(reused.memory == c.s.memory);
  assert(c.Wake());
  c.TickStory(100);
  assert(c.AcceptStory(0));
  assert(!(c.s.story.presented & 1));
  assert(c.StoryPresented(0));
  assert(c.s.story.presented & 1);
  finish(c, StoryNode::Wake);
  assert(c.Harvest(0, false, MemoryActor::Player));
  finish(c, StoryNode::Cooperation);
  c.s.economy.region = 1;
  c.TickStory(100);
  finish(c, StoryNode::Drought);
  finish(c, StoryNode::HeadPlant);
  finish(c, StoryNode::Performance);
  c.s.economy.region = 2;
  c.TickStory(100);
  finish(c, StoryNode::Rain);
  assert(c.s.story.language == 1);
  finish(c, StoryNode::Return);
  finish(c, StoryNode::Identity);
  finish(c, StoryNode::Seed, 2);
  finish(c, StoryNode::G17);
  c.s.economy.journey = 4;
  c.TickStory(0);
  finish(c, StoryNode::Animal, 2);
  finish(c, StoryNode::Clearing);
  finish(c, StoryNode::LastReport);
  finish(c, StoryNode::ReturnProof);
  finish(c, StoryNode::Responsibility);
  assert(c.s.story.language == 3);
  finish(c, StoryNode::Destination);
  c.s.story.care_after_truth = 1;
  for (int i = 0; i < 60; ++i)
    c.TickStory(1000);
  finish(c, StoryNode::NewLeaf);
  finish(c, StoryNode::Share);
  assert(!c.s.story.flower && !c.s.economy.animals && !c.s.story.robot_named);
  auto save = c.PackStory();
  Core restored;
  restored.storage_ok = true;
  assert(restored.RestoreStory(*save));
  assert(restored.s.story.completed == c.s.story.completed);
  assert(restored.PackStory()->checksum == save->checksum);
  assert(c.OfferStoryFlower());
  auto token = c.s.story.offer_token;
  auto water = c.s.water;
  assert(c.ApproveStoryFlower(token));
  assert(c.s.water == water - 2);
  assert(!c.ApproveStoryFlower(token));
  for (int i = 0; i < 360; ++i)
    c.TickStory(1000);
  assert(c.s.story.flower == 4);
  assert(c.ProposeStoryName(2, "小芽"));
  auto t = c.s.story.candidate_token;
  assert(!c.s.story.robot_named);
  assert(c.ConfirmStoryName(t));
  assert(!c.ConfirmStoryName(t));
  assert(std::string(c.s.story.robot_name.data()) == "小芽");
  assert(!c.ProposeStoryName(2, "\xed\xa0\x80"));
  assert(!c.ReadStoryArchive(7));
  auto broken = *c.PackStory();
  broken.story.language = 9;
  broken.checksum = Core::StoryChecksum(broken);
  assert(!restored.RestoreStory(broken));
  // Optional flower responses must not cancel an unrelated active mainline
  // discussion.
  auto branch = c;
  branch.s.story.flower = 0;
  branch.s.story.completed &= ~(1u << 11);
  branch.s.story.completed &= ~(1u << 19);
  branch.s.story.active = 19;
  assert(branch.OfferStoryFlower());
  assert(branch.CompleteStory(11, 2));
  assert(branch.s.story.active == 19 && !branch.s.story.offered);
  assert(branch.OfferStoryFlower());
  assert(branch.CompleteStory(11, 3));
  assert(branch.s.story.active == 19 && branch.s.story.seed_choice == 3);
  assert(!branch.CompleteStory(11, 1));
  // Routine life invitations must never hide a ready important discovery.
  auto incoming = c;
  incoming.s.story.active = incoming.s.story.paused = kNoStory;
  incoming.s.story.available |= (1u << 6) | (1u << 8);
  incoming.s.story.completed &= ~((1u << 6) | (1u << 8));
  incoming.s.story.discussed &= ~(1u << 8);
  incoming.s.story.presented &= ~(1u << 8);
  incoming.s.story.archives_read &= ~(1u << 2); // A fresh, unread discovery.
  incoming.s.story.elapsed = 900000;
  incoming.s.story.last_contact_at = 0;
  incoming.s.story.life_shown = 1;
  incoming.work = Work::Rest;
  incoming.s.work_ms = 0;
  incoming.TickStory(0);
  assert(incoming.s.story.pending == 8);
  incoming.work = Work::Water;
  incoming.TickStory(0);
  assert(incoming.s.story.pending == kNoStory);
  std::cout
      << "Story: full no-name/no-flower/no-animal route; costs idempotent; "
         "growth; naming confirmation; save and malformed state passed\n";
}
