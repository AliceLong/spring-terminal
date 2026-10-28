#include "game/spring_core.h"
#include "game/spring_mind_slot.h"
#include <cassert>
#include <iostream>
#include <string>
using namespace spring;
Core FreshMind() {
  Core c;
  c.storage_ok = true;
  c.EnableEconomy(true);
  c.EnableMemory(11, 22);
  c.Wake();
  c.s.water = c.s.energy = 40;
  for (auto &p : c.s.plots)
    p.ready = false;
  c.s.life.cooldown = 0;
  return c;
}
int main() {
  MindPacket p;
  assert(MindLine::Parse("MIND1 1 2 b4200200 887481cd 9 2 1", p));
  assert(p.intent == 2 && p.target == 1 && p.world_hi == 0xb4200200);
  for (const char *bad :
       {"MIND1 1 2 3 4 5 4 0", "MIND1 1 2 3 4 5 1 1", "MIND1 1 2 3 4 5 2 4",
        "MIND1 100000000 2 3 4 5 1 0", "MIND1 1 2 3 4 5 1 0 extra",
        "BUILD 1 2 3 4 5 1 0", "MIND1 1 2 3 4 5 1 -1"})
    assert(!MindLine::Parse(bad, p));
  MindLine parser;
  for (int i = 0; i < 10000; ++i)
    assert(!parser.Feed('x', p));
  assert(!parser.Feed('\n', p));
  const std::string line = "MIND1 1 2 3 4 5 3 0\n";
  bool good = false;
  for (char c : line)
    good |= parser.Feed(c, p);
  assert(good && p.intent == 3);
  MindReplyGuard reply{1, 2, 11, 22, 9, 20000};
  MindPacket share{1, 2, 11, 22, 9, 3, 0, 7};
  assert(reply.BodyAvailable(share, 10000, 0x100));
  assert(reply.Fresh(share, 10000));
  assert(!reply.BodyAvailable(
      share, 10000, 0)); // Work took the body, not the historical fact.
  reply.sequence = 10;
  assert(reply.Fresh(share, 10000));
  assert(!reply.BodyAvailable(share, 10000,
                              0x100)); // A newer action cannot be overwritten.
  assert(!reply.Fresh(share, 20000));
  for (auto changed : {MindPacket{2, 2, 11, 22, 9, 3, 0, 7},
                       MindPacket{1, 3, 11, 22, 9, 3, 0, 7},
                       MindPacket{1, 2, 99, 22, 9, 3, 0, 7}})
    assert(!reply.Fresh(changed, 10000));
  auto c = FreshMind();
  const auto water = c.s.water, energy = c.s.energy, fiber = c.s.fiber;
  assert(c.RequestLocalMindLife(1));
  assert(c.s.life.phase == 1 && !c.s.life.reached);
  assert(c.s.memory->records[c.s.memory->count - 1].actor ==
         static_cast<unsigned>(MemoryActor::Robot));
  assert(!c.RequestLocalMindLife(3));
  c.StepLife(100, false, false, false);
  assert(c.s.life.phase == 2 && !c.s.life.reached);
  c.StepLife(100, false, true, false);
  assert(c.s.life.phase == 3 && c.s.life.reached);
  c.StepLife(100, true, false, false);
  assert(c.s.life.phase == 4);
  assert(c.s.water == water && c.s.energy == energy && c.s.fiber == fiber);
  Core restored;
  restored.storage_ok = true;
  assert(restored.RestoreFull(c.PackFull()));
  c.FinishLife(LifeResult::WorkInterrupted);
  c.s.life.cooldown = 0;
  assert(
      c.CanRequestLocalMindLife(1)); // Resume the genuine interrupted activity.
  c.s.life.records[c.s.life.count - 1].result = 0;
  assert(!c.CanRequestLocalMindLife(
      1)); // Completed activities retain repeat cooldown.
  auto edge = FreshMind();
  edge.s.life.cooldown = 100;
  assert(edge.CanRequestLocalMindLife(1, 0, 1000));
  assert(!edge.RequestLocalMindLife(
      1)); // Reservation never starts before cooldown ends.
  edge.StepLife(100, false, false, false);
  assert(!edge.LifeActive() && edge.s.life.cooldown == 0);
  assert(edge.RequestLocalMindLife(1));
  auto idle = FreshMind();
  idle.StepLife(100, false, false, false);
  assert(!idle.LifeActive());
  idle.StepLife(100, false, false, true);
  assert(idle.LifeActive()); // Original fallback is still available.
  std::cout << "USB parser, intent gates, real lifecycle, robot actor and save "
               "compatibility pass\n";
}
