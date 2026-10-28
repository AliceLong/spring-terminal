#include "conversation_handoff.h"
#include "game/spring_json.h"
#include "spring_gardener.h"
#include <cassert>
#include <iostream>
using namespace spring;
Core Fresh() {
  Core c;
  c.storage_ok = true;
  c.EnableEconomy(true);
  c.EnableMemory(71, 72);
  c.Wake();
  for (auto &p : c.s.plots) {
    p.ready = false;
    p.growth_ms = p.mature_ms = 0;
  }
  c.s.water = c.s.energy = 40;
  return c;
}
int main() {
  ConversationHandoff h;
  h.Arm(100);
  assert(!h.Ready(200, true)); // Quiet gap before reply must not disconnect.
  assert(h.TtsStopped(300, true));
  assert(!h.Ready(400, false)); // TTS stop but speaker still playing.
  assert(h.Ready(500, true));
  h.Cancel();
  assert(!h.Ready(600, true));
  h.Arm(100);
  assert(!h.TtsStopped(200, false));
  assert(!h.Ready(300, true));
  h.Arm(100);
  assert(!h.TtsStopped(30000101, true)); // Expired request, no later hangup.
  h.Arm(100);
  h.Cancel();
  assert(!h.TtsStopped(200, true)); // Barge-in/disconnect.
  for (unsigned intent = 1; intent <= 3; ++intent) {
    auto c = Fresh();
    const auto water = c.s.water, energy = c.s.energy;
    assert(c.InviteLife(intent, 1));
    assert(c.s.water == water && c.s.energy == energy);
    const auto chain = c.s.memory->life_chain;
    const auto &request = c.s.memory->records[c.s.memory->count - 1];
    assert(request.chain == chain && request.actor == 4 && (request.flags & 1));
    assert(c.s.life.reason == 4 && !c.s.life.reached);
    assert(!c.InviteLife(intent, 1)); // Duplicate cannot spawn a second action.
    c.StepLife(100, true, false);
    assert(c.s.life.phase == 2 &&
           !c.s.life.reached); // Explicit invitation starts during the call.
    Core reboot;
    reboot.storage_ok = true;
    assert(
        reboot.RestoreArchive(*c.PackArchive())); // New reason survives save.
    assert(reboot.RestoreCheckpoint(c.PackCheckpoint()));
    reboot.StepLife(100, false, false);
    assert(reboot.s.life.records[0].result ==
           2); // Never invent completion after restart.
    spring_gardener::Controller body;
    for (uint64_t t = 100; t < 60000 && !c.s.life.count; t += 100) {
      c.Step(100);
      c.StepLife(100, true, spring_gardener::LifeArrived(c.Get(), body.Get()));
      body.Step(c.Get(), t, spring_gardener::Voice::Listen);
    }
    assert(c.s.life.count && c.s.life.records[0].result == 0 &&
           c.s.life.records[0].reached);
    assert(c.s.life.records[0].reason == 4);
    bool actual_finish = false;
    for (unsigned i = 0; i < c.s.memory->count; ++i) {
      const auto &e = c.s.memory->records[i];
      if (e.chain == chain && e.action == 6) {
        actual_finish = true;
        assert(e.actor == 1 && !(e.flags & 1));
      }
    }
    assert(actual_finish);
    assert(LifeJson(c.Get()).find("player_invitation") != std::string::npos);
  }
  auto c = Fresh();
  const auto original = *c.PackArchive();
  assert(!c.InviteLife(0));
  assert(!c.InviteLife(4));
  assert(!c.InviteLife(2, 4));
  assert(*c.PackArchive() == original);
  c.s.economy.construction = 0;
  c.s.economy.construction_ms = 1;
  assert(!c.InviteLife(1));
  c = Fresh();
  c.s.energy = 0;
  assert(!c.InviteLife(1));
  c = Fresh();
  c.s.plots[0].ready = true;
  assert(!c.InviteLife(1));
  auto working = Fresh();
  assert(working.InviteLife(1));
  const auto old_sequence = working.s.life.sequence;
  const auto old_water = working.s.water, old_energy = working.s.energy;
  assert(working.ResumeWork());
  assert(!working.LifeActive());
  assert(working.s.life.records[0].result ==
         unsigned(LifeResult::TargetChanged));
  assert(working.s.life.sequence == old_sequence &&
         working.s.life.cooldown == 120000);
  assert(working.s.water == old_water && working.s.energy == old_energy);
  working.s.story.enabled = true;
  working.s.story.active = 0;
  assert(!working.ResumeWork()); // Never displace an active mandatory story.
  working = Fresh();
  working.storage_ok = false;
  assert(!working.ResumeWork());
  auto animal = Fresh();
  animal.EnableStory();
  animal.s.economy.animals = 1;
  assert(animal.InviteLife(
      2, 5)); // Existing occupied-animal action is not a new system.
  std::cout << "Invitation: actual body arrival for3 intents during voice, "
               "drain gate, expiry, interruption, no spending, rejection, "
               "idempotence and memory/save evidence passed\n";
}
