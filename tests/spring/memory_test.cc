#include "game/spring_memory_json.h"
#include <cassert>
#include <iostream>
using namespace spring;
Core Fresh() {
  Core c;
  c.storage_ok = true;
  c.EnableEconomy(true);
  c.EnableMemory(11, 22);
  c.Wake();
  c.s.water = 50;
  c.s.energy = 50;
  c.s.fiber = 50;
  return c;
}
void Approve(Core &c, unsigned kind, unsigned target = 0, unsigned value = 0) {
  assert(c.Propose(kind, target, value, MemoryActor::VoiceRequest));
  assert(c.ConfirmProposal(c.s.economy.proposal.token));
}
unsigned Count(const Core &c, MemoryAction a, uint32_t chain = 0) {
  unsigned n = 0;
  for (unsigned i = 0; i < c.s.memory->count; ++i) {
    const auto &e = c.s.memory->records[i];
    if (e.action == static_cast<unsigned>(a) && (!chain || e.chain == chain))
      ++n;
  }
  return n;
}
int main() {
  auto c = Fresh();
  const auto untouched = c.Get(); // Must remain immutable after later writes.
  const auto original = untouched.memory->last_id;
  Approve(c, 1, 0);
  const auto chain = c.s.memory->jobs[0].chain;
  assert(Count(c, MemoryAction::Proposed, chain) == 1 &&
         Count(c, MemoryAction::Approved, chain) == 1);
  assert(untouched.memory->last_id == original);
  const auto charged = c.s.water;
  assert(!c.ConfirmProposal(c.s.economy.proposal_seq));
  assert(c.s.water == charged);
  c.s.energy = 0;
  c.Advance(1000);
  assert(Count(c, MemoryAction::Paused, chain) == 1);
  c.s.energy = 50;
  c.Advance(1000);
  assert(Count(c, MemoryAction::Resumed, chain) == 1);
  auto checkpoint = c.PackCheckpoint();
  auto archive = c.PackArchive();
  Core restored;
  restored.storage_ok = true;
  assert(restored.RestoreArchive(*archive));
  assert(restored.RestoreCheckpoint(checkpoint));
  assert(*restored.PackArchive() == *archive);
  restored.Advance(180000);
  assert(restored.HasFacility(0));
  assert(Count(restored, MemoryAction::Completed, chain) == 1);
  bool pinned = false;
  for (unsigned i = 0; i < restored.s.memory->fact_count; ++i) {
    const auto &f = restored.s.memory->facts[i];
    if (f.type == 20) {
      pinned = true;
      assert(f.flags & 1);
      assert(f.chain == chain);
      assert((f.cost & 255) == 4);
      assert(((f.cost >> 16) & 255) == 9);
    }
  }
  assert(pinned);
  // An old checkpoint may not overwrite newly committed memory or a different
  // world.
  assert(!restored.RestoreCheckpoint(checkpoint));
  auto other = Fresh();
  other.s.memory->world_lo = 99;
  assert(!other.RestoreCheckpoint(checkpoint));
  auto damaged = *archive;
  damaged.back() ^= 1;
  assert(!other.RestoreArchive(damaged));
  // Rejection and replacement never become completed work.
  auto decisions = Fresh();
  assert(decisions.Propose(1, 1));
  auto rejected = decisions.s.memory->pending.chain;
  assert(decisions.DismissProposal(decisions.s.economy.proposal.token));
  assert(Count(decisions, MemoryAction::Rejected, rejected) == 1);
  assert(!Count(decisions, MemoryAction::Completed, rejected));
  assert(decisions.Propose(1, 1));
  auto replaced = decisions.s.memory->pending.chain;
  assert(decisions.Propose(1, 2));
  assert(Count(decisions, MemoryAction::Superseded, replaced) == 1);
  // Next-cycle plant is not applied until actual cycle transition.
  auto plants = Fresh();
  Approve(plants, 2, 0, 2);
  const auto plant_chain = plants.s.memory->jobs[4].chain;
  assert(!Count(plants, MemoryAction::Completed, plant_chain));
  plants.s.plots[0].ready = true;
  plants.s.economy.mature_yield[0] = 5;
  plants.Harvest(0, false, MemoryActor::Player);
  assert(Count(plants, MemoryAction::Completed, plant_chain) == 1);
  bool cooperative = false;
  for (unsigned i = 0; i < plants.s.memory->fact_count; ++i)
    if (plants.s.memory->facts[i].type == 2)
      cooperative = true;
  assert(cooperative);
  // Automatic harvest does not count as player cooperation.
  auto autonomous = Fresh();
  autonomous.s.plots[0].ready = true;
  autonomous.s.economy.mature_yield[0] = 5;
  autonomous.Harvest(0);
  for (unsigned i = 0; i < autonomous.s.memory->fact_count; ++i)
    assert(autonomous.s.memory->facts[i].type != 2);
  // Tree direct approval retains its own chain and exact actual completion.
  auto tree = Fresh();
  assert(tree.BuildTree());
  auto tree_chain = tree.s.memory->jobs[1].chain;
  assert(tree_chain);
  tree.Advance(70000);
  assert(Count(tree, MemoryAction::Completed, tree_chain) == 1);
  // Imported baselines cannot become fabricated first-time cooperation.
  Core old;
  old.storage_ok = true;
  old.EnableEconomy(true);
  old.Wake();
  old.s.tree_stage = 2;
  old.s.story_stage = 2;
  old.s.economy.animals = 1;
  old.EnableMemory(33, 44);
  bool baseline = false;
  for (unsigned i = 0; i < old.s.memory->fact_count; ++i) {
    auto f = old.s.memory->facts[i];
    assert(!(f.flags & 1));
    if (f.type == 40) {
      baseline = true;
      assert(f.flags & 8);
      assert(f.actor == 0);
    }
  }
  assert(baseline);
  // Detailed ring rolls, permanent evidence and totals survive reboot.
  auto pinned_count = restored.s.memory->fact_count;
  for (unsigned i = 0; i < 300; ++i)
    restored.Add(Event::AnimalFed, 0);
  assert(restored.s.memory->count == 128 && restored.s.memory->dropped > 0);
  assert(restored.s.memory->fact_count == pinned_count);
  auto saved = restored.PackArchive();
  Core reboot;
  reboot.storage_ok = true;
  assert(reboot.RestoreArchive(*saved));
  assert(*reboot.PackArchive() == *saved);
  assert(Count(reboot, MemoryAction::Completed, chain) ==
         0); // Raw basis rolled; pinned fact still holds it.
  // Active life chains survive restore and end as restart interruption.
  auto life = Fresh();
  life.s.plots[1].ready = false;
  life.s.life.cooldown = 0;
  life.StepLife(100, false, false);
  life.StepLife(100, false, false);
  assert(life.s.life.phase == 2);
  const auto life_chain = life.s.memory->life_chain;
  life.StepLife(100, false, true);
  assert(Count(life, MemoryAction::Arrived, life_chain) == 1);
  life.StepLife(100, true, false);
  assert(Count(life, MemoryAction::Paused, life_chain) == 1);
  Core life_reboot;
  life_reboot.storage_ok = true;
  assert(life_reboot.RestoreArchive(*life.PackArchive()));
  assert(life_reboot.RestoreCheckpoint(life.PackCheckpoint()));
  life_reboot.StepLife(100, false, false);
  assert(Count(life_reboot, MemoryAction::Finished, life_chain) == 1);
  assert(life_reboot.s.life.records[0].result == 2);
  // One actual offline day retains memory, does not change economic outcomes.
  auto day = Fresh();
  auto control = day;
  control.s.memory.reset();
  for (unsigned minute = 0; minute < 1440; ++minute) {
    day.Advance(60000);
    control.Advance(60000);
    control.s.revision = day.s.revision; // Evidence adds revision, not economic changes.
    assert(day.PackFull() == control.PackFull());
    if (minute % 60 == 0) {
      Core next;
      next.storage_ok = true;
      assert(next.RestoreArchive(*day.PackArchive()));
      assert(next.RestoreCheckpoint(day.PackCheckpoint()));
      day = std::move(next);
    }
  }
  // Read-only queries cannot mutate memory or state.
  auto before = reboot.PackArchive();
  auto json = MemoryJson(reboot.Get(), 0, 0, 0, 128, true);
  assert(*reboot.PackArchive() == *before);
  std::cout << "Memory: linked decisions, real costs, COW snapshots, "
               "idempotence, pause/resume, crash checkpoint guards, actual "
               "crop application, actors, migration baselines, pinned "
               "retention and read-only queries passed\n";
  std::cout << json << '\n';
}
