#include "game/spring_social.h"
#include "game/spring_story_json.h"
#include "spring_roadshow.h"
#include <cassert>
#include <iostream>

using namespace spring;
int main() {
  // Reproduce the live scene: old archives remain unread while a new leaf is
  // pending.
  Core game;
  assert(spring_show::Prepare(game, 6, 100, 6));
  const auto archives = game.s.story.archives;
  const auto read = game.s.story.archives_read;
  assert(archives & 1u);
  assert(!(read & 1u));
  for (bool history : {false, true}) { // Bubble and main communication entry.
    const auto route = SelectCommunication(game.Get(), 42, 43, history);
    assert(route.kind == CommunicationRouteKind::Story);
    assert(route.id == unsigned(StoryNode::NewLeaf));
  }
  // Routine work must not redirect a visible invitation to a historical
  // archive.
  game.work = Work::Water;
  game.s.story.pending = kNoStory;
  auto route = SelectCommunication(game.Get(), 0, 0);
  assert(route.kind == CommunicationRouteKind::Story);
  assert(route.id == unsigned(StoryNode::NewLeaf));
  assert(game.AcceptStory(route.id));
  assert(StoryJson(game.Get()).find("你看，我头顶的幼苗又长了一片叶子。") !=
         std::string::npos);
  assert(StoryCommunicationNode(game.Get()) ==
         kNoStory); // No duplicate invitation.
  assert(game.s.story.archives == archives &&
         game.s.story.archives_read == read);
  const auto leaf = unsigned(StoryNode::NewLeaf);
  assert(game.EndStoryCommunication(
      leaf)); // Connection failed before any played reply.
  assert(!StoryContactHandled(game.s.story, leaf));
  assert(SelectCommunication(game.Get(), 0, 0, false).id == leaf);
  assert(game.AcceptStory(leaf));
  StoryPlaybackReceipt receipt;
  assert(receipt.Poll(leaf, false, true) ==
         kNoStory); // Text alone is insufficient.
  assert(receipt.Poll(leaf, true, false) == kNoStory);
  assert(receipt.Poll(leaf, false, false) ==
         kNoStory); // PCM still buffered / interrupted.
  assert(receipt.Poll(leaf, false, true) == leaf);
  assert(receipt.Poll(leaf, false, true) == kNoStory); // Exactly one receipt.
  const auto water = game.s.water, energy = game.s.energy;
  assert(game.StoryPresented(leaf));
  assert(game.EndStoryCommunication(leaf));
  assert(StoryContactHandled(game.s.story, leaf));
  assert(!(game.s.story.completed &
           (1u << leaf))); // No invented narrative choice.
  assert(game.s.water == water && game.s.energy == energy);
  assert(!game.EndStoryCommunication(leaf));
  assert(SelectCommunication(game.Get(), 0, 0, false).kind ==
         CommunicationRouteKind::None);
  Core restored;
  assert(restored.RestoreStory(*game.PackStory()));
  assert(StoryContactHandled(restored.s.story,
                             leaf)); // Reboot cannot invite it again.
  receipt.Reset();
  assert(receipt.Poll(leaf, false, true) ==
         kNoStory); // New session cannot reuse old audio.
  const auto self = StoryJson(game.Get());
  assert(self.find("头顶透明种植舱") != std::string::npos);
  assert(self.find("\"origin\":\"unknown\"") != std::string::npos);
  game.s.story.archives_read |= 1u << 6;
  assert(StoryJson(game.Get()).find("\"origin\":\"G-17活枝\"") !=
         std::string::npos);

  // Invitation visibility requires a free body, not merely an idle audio
  // session.
  auto idle = game.Get();
  idle.work = Work::Rest;
  idle.work_ms = idle.tree_work_ms = idle.economy.journey_work = 0;
  idle.economy.construction = 255;
  idle.life.phase = 0;
  assert(CommunicationBubbleIdle(idle));
  idle.work = Work::Water;
  assert(!CommunicationBubbleIdle(idle));
  idle.work = Work::Rest;
  idle.work_ms = 100;
  assert(!CommunicationBubbleIdle(idle));
  idle.work_ms = 0;
  idle.economy.construction = 1;
  assert(!CommunicationBubbleIdle(idle));
  idle.economy.construction = 255;
  idle.life.phase = 2;
  assert(!CommunicationBubbleIdle(idle));
  idle.life.phase = 3;
  assert(CommunicationBubbleIdle(
      idle)); // Can share during a quiet living activity.

  // A current archive opens its own document, not the oldest unread document.
  for (const auto scene : {4u, 5u}) {
    assert(spring_show::Prepare(game, scene, 100, scene));
    route = SelectCommunication(game.Get(), 0, 0);
    assert(route.kind == CommunicationRouteKind::Archive);
    assert(route.id == (scene == 4 ? 1u : 6u));
    assert(game.ReadStoryArchive(route.id));
    assert(game.s.story.archive_selected == route.id);
    assert(!(game.s.story.archives_read & 1u)); // Old report is not auto-read.
    assert(game.StoryArchiveSeen(route.id));
    assert(SelectCommunication(game.Get(), 0, 0, false).kind ==
           CommunicationRouteKind::None);
  }

  // No current narrative: valid personal invitations precede the historical
  // inbox.
  game.s.story.pending = game.s.story.active = kNoStory;
  game.s.story.completed = game.s.story.available;
  route = SelectCommunication(game.Get(), 42, 43);
  assert(route.kind == CommunicationRouteKind::Wish && route.id == 42);
  route = SelectCommunication(game.Get(), 0, 43);
  assert(route.kind == CommunicationRouteKind::Social && route.id == 43);
  route = SelectCommunication(game.Get(), 0, 0);
  assert(route.kind == CommunicationRouteKind::Archive && route.id == 0);
  assert(SelectCommunication(game.Get(), 0, 0, false).kind ==
         CommunicationRouteKind::None);

  // Locked, paused and physical chapter actions cannot start a new call.
  game.s.story.pending = unsigned(StoryNode::NewLeaf);
  game.s.story.completed = 0;
  game.s.story.available = 0;
  assert(StoryCommunicationNode(game.Get()) == kNoStory);
  game.s.story.available = 1u << game.s.story.pending;
  game.s.story.paused = game.s.story.pending;
  assert(StoryCommunicationNode(game.Get()) == kNoStory);
  game.s.story.paused = kNoStory;
  game.s.story.pending = unsigned(StoryNode::HeadPlant);
  game.s.story.available = 1u << game.s.story.pending;
  assert(StoryCommunicationNode(game.Get()) == kNoStory);
  game.s.story.pending = unsigned(StoryNode::Drought);
  game.s.story.available = 1u << game.s.story.pending;
  assert(StoryCommunicationNode(game.Get()) == unsigned(StoryNode::Drought));
  game.s.story.enabled = false;
  assert(StoryCommunicationNode(game.Get()) == kNoStory);
  std::cout << "Communication routing: current leaf/archive, work concurrency, "
               "shared entries, "
               "history preservation and invitation guards passed.\n";
}
