#include "game/spring_json.h"
#include "spring_gardener.h"
#include <cassert>
#include <iostream>
using namespace spring;
using namespace spring_gardener;

Core Fresh() {
  Core c;
  c.storage_ok = true;
  c.EnableEconomy(true);
  c.Wake();
  c.s.water = c.s.energy = 40;
  return c;
}
void Tick(Core &c, Controller &body, uint64_t now, Voice voice = Voice::None) {
  c.Step(100);
  c.StepLife(100, voice != Voice::None, LifeArrived(c.Get(), body.Get()));
  body.Step(c.Get(), now, voice);
}
int main() {
  // Use the real economy and pose feedback, not a synthetic forever-idle
  // snapshot.
  auto c = Fresh();
  Controller body;
  unsigned seen = 0;
  for (uint64_t t = 100; t <= 1800000; t += 100) {
    const auto previous = body.Get();
    Tick(c, body, t);
    assert(std::abs(body.Get().x - previous.x) <= 2.1f);
    if (c.s.life.phase == 3) {
      assert(c.s.life.reached);
      seen |= 1u << c.s.life.intent;
    }
  }
  assert(seen ==
         14); // Window, plant inspection and actual landed rest all happened.
  assert(c.s.life.count == 6);
  for (const auto &r : c.s.life.records)
    assert(r.id && (r.result || r.reached));
  assert(!c.s.life.under_tree); // No tree unlocked in this run.
  const auto packed = c.PackFull();
  Core roundtrip;
  roundtrip.storage_ok = true;
  assert(roundtrip.RestoreFull(packed));
  assert(roundtrip.PackFull() == packed);

  // A request and approach never turn into a visit without arrival feedback.
  auto approaching = Fresh();
  approaching.s.plots[1].ready = false;
  approaching.s.life.cooldown = 0;
  approaching.StepLife(100, false, false);
  assert(approaching.s.life.phase == 1);
  approaching.StepLife(100, false, false);
  assert(approaching.s.life.phase == 2 && !approaching.s.life.reached);
  auto before = approaching.PackFull();
  Core restored;
  restored.storage_ok = true;
  assert(restored.RestoreFull(before));
  restored.StepLife(100, false, true);
  assert(!restored.LifeActive());
  assert(restored.s.life.records[0].result == 2 &&
         !restored.s.life.records[0].reached);
  approaching.s.economy.construction = 2;
  approaching.s.economy.construction_ms = 1;
  approaching.StepLife(100, false, false);
  assert(!approaching.LifeActive());
  assert(approaching.s.life.records[0].result == 1 &&
         !approaching.s.life.records[0].reached);
  assert(std::string(RobotActivity(approaching.Get())) == "construct_facility");

  // Tree-story unlock changes the real rest destination, never before growth.
  auto tree = Fresh();
  tree.s.tree_stage = 1;
  tree.s.story_stage = 2;
  tree.s.plots[1].ready = false;
  tree.s.life.cooldown = 0;
  tree.s.life.repeat_cooldown = {300000, 300000, 0};
  Controller tree_body;
  for (uint64_t t = 100; t <= 90000 && !tree.s.life.count; t += 100)
    Tick(tree, tree_body, t);
  assert(tree.s.life.count == 1 && tree.s.life.records[0].intent == 3 &&
         tree.s.life.records[0].under_tree && tree.s.life.records[0].reached &&
         tree.s.life.records[0].result == 0);
  for (uint64_t t = 90100; t <= 100000; t += 100) {
    const auto previous = tree_body.Get();
    Tick(tree, tree_body, t);
    assert(std::abs(tree_body.Get().x - previous.x) <= 2.1f);
  }
  const auto query_before = tree.PackFull();
  assert(LifeJson(tree.Get()).find("under_tree") != std::string::npos);
  assert(tree.PackFull() == query_before); // Queries cannot create experiences.

  auto changed = Fresh();
  changed.s.life.intent = changed.s.life.reason = 2;
  changed.s.life.phase = 2;
  changed.s.life.sequence = 1;
  changed.s.life.plant = 0;
  changed.s.plots[0].plant = Plant::Vine;
  changed.StepLife(100, false, false);
  assert(changed.s.life.records[0].result == 4 &&
         !changed.s.life.records[0].reached);

  // Interaction pauses; resumption requires current pose confirmation again.
  auto paused = Fresh();
  paused.s.life.intent = paused.s.life.reason = 1;
  paused.s.life.phase = 3;
  paused.s.life.sequence = paused.s.life.reached = 1;
  paused.s.life.elapsed = 5000;
  paused.StepLife(100, true, false);
  assert(paused.s.life.phase == 4 && paused.s.life.elapsed == 5000);
  paused.StepLife(100, false, false);
  assert(paused.s.life.phase == 2 && paused.s.life.elapsed == 5000);
  paused.StepLife(100, false, true);
  assert(paused.s.life.phase == 3);
  for (int i = 0; i < 150; ++i)
    paused.StepLife(100, false, true);
  assert(!paused.LifeActive() && paused.s.life.records[0].result == 0);

  // An explicit voice invitation travels, arrives and completes while the
  // same voice conversation remains active; autonomous life still pauses above.
  auto invited = Fresh();
  invited.EnableMemory(71, 72);
  for (auto& plot : invited.s.plots) {
    plot.ready = false;
    plot.growth_ms = plot.mature_ms = 0;
  }
  invited.s.life.cooldown = 0;
  assert(invited.InviteLife(2, 0));
  Controller invited_body;
  bool arrived_on_call = false;
  for (uint64_t t = 100; t <= 60000 && invited.LifeActive(); t += 100) {
    Tick(invited, invited_body, t, Voice::Listen);
    arrived_on_call |= invited.s.life.phase == 3 && invited.s.life.reached;
    assert(invited.s.life.phase != 4);
  }
  assert(arrived_on_call && !invited.LifeActive());
  assert(invited.s.life.records[0].reached && invited.s.life.records[0].result == 0);

  // Same active real-time duration in demo mode; offline economic catch-up
  // invents no visits.
  auto normal = Fresh(), demo = Fresh();
  demo.demo = true;
  for (auto *p : {&normal, &demo}) {
    p->s.life.intent = p->s.life.reason = 1;
    p->s.life.phase = 3;
    p->s.life.sequence = p->s.life.reached = 1;
    for (int i = 0; i < 100; ++i)
      p->StepLife(100, false, true);
    assert(p->s.life.elapsed == 10000);
  }
  auto offline = Fresh();
  offline.Advance(43200000);
  assert(!offline.s.life.count && !offline.s.life.sequence);

  // No economic effects from life; paid production and outcomes remain
  // identical.
  auto reference = Fresh(), living = Fresh();
  Controller living_body;
  for (uint64_t t = 100; t <= 7200000; t += 100) {
    reference.Step(100);
    Tick(living, living_body, t);
  }
  assert(reference.s.water == living.s.water &&
         reference.s.energy == living.s.energy &&
         reference.s.fiber == living.s.fiber);
  for (unsigned i = 0; i < 3; ++i)
    assert(reference.PlotAt(i).cycle == living.PlotAt(i).cycle);

  // Schema3 loads without life; tree progress and stocks survive migration.
  auto legacy = reference.PackFull();
  legacy[129] = 3;
  std::fill(legacy.begin() + 252, legacy.end(), 0);
  legacy.back() = Core::FullChecksum(legacy);
  Core migrated;
  migrated.storage_ok = true;
  assert(migrated.RestoreFull(legacy));
  assert(migrated.s.water == reference.s.water && !migrated.s.life.sequence);
  assert(migrated.s.life.cooldown == 120000);
  auto broken = c.PackFull();
  broken[255] = 99;
  broken.back() = Core::FullChecksum(broken);
  assert(!migrated.RestoreFull(broken));
  std::cout << "Life: real core+pose all3, bounded history, no false arrival, "
               "interruption, "
               "voice resume, restart, real-time pacing, offline silence, "
               "economy equivalence, "
               "schema3 migration and corrupt-save rejection passed\n";
  std::cout << WorldJson(c.Get()) << '\n';
}
