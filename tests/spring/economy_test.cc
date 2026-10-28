#include "game/spring_core.h"
#include "game/spring_json.h"
#include <cassert>
#include <iostream>
using namespace spring;
Core StartEconomy() {
  Core c;
  c.storage_ok = true;
  c.EnableEconomy(true);
  assert(c.Wake());
  c.s.plots[1].ready = false;
  c.s.plots[1].growth_ms = 0;
  return c;
}
int main() {
  {
    auto c = StartEconomy();
    c.Advance(1000);
    assert(c.s.water == 12 && c.s.energy == 10);
    auto save = c.PackFull();
    Core copy;
    copy.storage_ok = true;
    assert(copy.RestoreFull(save));
    assert(copy.PackFull() == save);
    copy.Advance(1000);
    assert(copy.s.water == 12 && copy.s.energy == 10);
    auto bad = save;
    bad[134] = 999;
    bad.back() = Core::FullChecksum(bad);
    assert(!copy.RestoreFull(bad));
  }
  {
    auto c = StartEconomy();
    c.Advance(600000);
    assert(c.s.plots[0].ready);
    assert(c.s.energy == 9);
    assert(c.Harvest(0));
    assert(c.s.energy == 14);
    assert(!c.Harvest(0));
    assert(c.Harvest(1));
    assert(c.s.water == 18);
    assert(c.Harvest(2));
    assert(c.s.fiber == 3);
  }
  {
    auto c = StartEconomy();
    c.s.fiber = 40;
    c.s.economy.parts = 10;
    auto before = c.PackFull();
    assert(c.Propose(1, 0));
    assert(c.s.water == 16 && c.s.fiber == 40);
    auto token = c.s.economy.proposal.token;
    assert(c.ConfirmProposal(token));
    assert(c.s.water == 12 && c.s.fiber == 31 && c.s.economy.parts == 9);
    assert(!c.ConfirmProposal(token));
    auto save = c.PackFull();
    Core resumed;
    resumed.storage_ok = true;
    assert(resumed.RestoreFull(save));
    resumed.Advance(120000);
    assert(resumed.HasFacility(0));
    assert(resumed.WaterCapacity() == 100);
    assert(!resumed.Propose(1, 99));
    assert(resumed.Propose(1, 0));
    assert(!resumed.ConfirmProposal(resumed.s.economy.proposal.token));
    (void)before;
  }
  {
    auto c = StartEconomy();
    assert(c.Propose(3, 3));
    auto token = c.s.economy.proposal.token;
    assert(c.Propose(3, 1));
    assert(!c.ConfirmProposal(token));
    assert(c.ConfirmProposal(c.s.economy.proposal.token));
    assert(c.s.economy.priorities[0] == 1);
  }
  {
    auto c = StartEconomy();
    c.s.economy.journey_clock = 1799000;
    c.Advance(1000);
    assert(c.s.economy.journey == 1 && (c.s.notices & 16));
    auto id = c.s.economy.journey_id;
    assert(c.Announce(4));
    assert(!c.Announce(4));
    assert(c.Accept(4));
    assert(c.s.economy.journey_read == id);
    assert(c.Propose(5, id, 1));
    auto token = c.s.economy.proposal.token;
    assert(c.ConfirmProposal(token));
    assert(c.s.economy.scrap == 0);
    assert(!c.ConfirmProposal(token));
    assert(!c.Propose(5, id, 1));
    c.Advance(120000);
    assert(c.s.economy.scrap == 1 && c.s.economy.parts == 2);
    c.s.economy.built |= 1u << 5;
    assert(c.Propose(6, 0));
    assert(c.ConfirmProposal(c.s.economy.proposal.token));
    assert(!c.s.economy.scrap);
    c.Advance(60000);
    assert(c.s.economy.parts == 4);
  }
  {
    auto c = StartEconomy();
    c.s.economy.journey = 4;
    c.s.economy.journey_id = 1;
    c.s.economy.built |= 1u << 7;
    c.s.economy.food = 10;
    assert(c.Propose(5, 1, 1));
    assert(c.ConfirmProposal(c.s.economy.proposal.token));
    assert(c.s.economy.animals == 0 && c.s.economy.food == 8);
    c.Advance(60000);
    assert(c.s.economy.animals == 1);
    c.s.economy.food = 0;
    c.s.economy.animal_elapsed[0] = 1799000;
    c.Advance(1000);
    assert(c.s.economy.animals == 1 && !c.s.economy.animal_help);
  }
  {
    auto c = StartEconomy();
    c.s.economy.built |= 1u << 4;
    assert(c.PlotCount() == 4);
    assert(c.Select(3, Plant::Food));
    c.Advance(300000);
    auto save = c.PackFull();
    Core r;
    r.storage_ok = true;
    assert(r.RestoreFull(save));
    assert(r.PlotAt(3).growth_ms == 300000);
    r.Advance(300000);
    assert(r.PlotAt(3).ready);
    assert(r.Harvest(3));
    assert(r.s.economy.food == 2);
  }
  {
    auto c = StartEconomy();
    c.s.water = c.s.energy = 0;
    c.Advance(43200000);
    assert(c.s.economy.journey_id == 1);
    assert(c.s.water <= 60 && c.s.energy <= 60);
    assert(!c.s.economy.animals);
  }
  // Every facility: exact single payment, completion, durable restore and no
  // rebuilding.
  for (unsigned id = 0; id < 8; ++id) {
    auto c = StartEconomy();
    c.s.water = 60;
    c.s.energy = 60;
    c.s.fiber = 60;
    c.s.economy.parts = 20;
    if (id == 7) { c.s.tree_stage = 2; c.s.story_stage = 2; }
    const auto b = kFacilities[id];
    assert(c.Propose(1, id));
    auto token = c.s.economy.proposal.token;
    assert(c.ConfirmProposal(token));
    assert(c.s.water == 60 - b.water && c.s.energy == 60 - b.energy &&
           c.s.fiber == 60 - b.fiber && c.s.economy.parts == 20 - b.parts);
    c.Advance(b.ms);
    assert(c.HasFacility(id));
    Core r;
    r.storage_ok = true;
    assert(r.RestoreFull(c.PackFull()));
    assert(r.HasFacility(id));
    assert(c.Propose(1, id));
    assert(!c.ConfirmProposal(c.s.economy.proposal.token));
  }
  // Migration preserves resources, fractional progress and the old serialized
  // blob.
  {
    Core old;
    old.storage_ok = true;
    old.Wake();
    old.s.plots[0].growth_ms = 90000;
    const auto legacy = old.Pack();
    Core r;
    r.storage_ok = true;
    assert(r.Restore(legacy));
    r.EnableEconomy(false);
    assert(r.s.plots[0].growth_ms == 300000);
    assert(r.s.plots[1].ready && r.s.plots[1].growth_ms == 600000);
    assert(old.Pack() == legacy);
    Core again;
    again.storage_ok = true;
    assert(again.RestoreFull(r.PackFull()));
    assert(again.s.water == old.s.water);
  }
  // A priority creates a concrete, unspent proposal and one call; declining
  // doesn't nag.
  {
    auto c = StartEconomy();
    c.s.fiber = 10;
    c.Propose(3, 1);
    assert(c.ConfirmProposal(c.s.economy.proposal.token));
    c.Advance(1000);
    assert(c.s.economy.proposal.kind == 7 && (c.s.notices & 32));
    auto before = c.s.fiber;
    auto token = c.s.economy.proposal.token;
    assert(c.DismissProposal(token));
    c.Advance(1000);
    assert(!c.s.economy.proposal.kind && c.s.fiber == before);
  }
  // Paid care applies to the next crop cycle once, and a restarted device can't
  // re-charge.
  {
    auto c = StartEconomy();
    c.s.economy.built |= 1u << 6;
    assert(c.Propose(4, 2));
    assert(c.ConfirmProposal(c.s.economy.proposal.token));
    c.Advance(1000);
    assert(c.s.economy.boost_paid & 4);
    auto energy = c.s.energy;
    Core r;
    r.storage_ok = true;
    assert(r.RestoreFull(c.PackFull()));
    r.Advance(1000);
    assert(r.s.energy == energy);
    r.Advance(598000);
    assert(r.Harvest(2));
    assert(r.s.fiber == 4);
  }
  // A delayed journey task survives reboot; only completion gives
  // supplies/animals.
  {
    auto c = StartEconomy();
    c.s.economy.journey = 2;
    c.s.economy.journey_id = 7;
    c.Propose(5, 7, 1);
    assert(c.ConfirmProposal(c.s.economy.proposal.token));
    assert(!c.s.economy.food);
    c.Advance(30000);
    Core r;
    r.storage_ok = true;
    assert(r.RestoreFull(c.PackFull()));
    r.Advance(30000);
    assert(r.s.economy.food == 0 && !r.s.economy.journey_task);
    auto food = r.s.economy.food;
    r.Advance(1000);
    assert(r.s.economy.food == food);
  }
  // Product events cannot evict decision history.
  {
    auto c = StartEconomy();
    c.Propose(3, 2);
    c.ConfirmProposal(c.s.economy.proposal.token);
    auto history = c.s.economy.decision_count;
    for (unsigned i = 0; i < 40; ++i)
      c.Add(Event::Harvest, 0, 6);
    assert(c.s.economy.decision_count == history);
    Core r;
    r.storage_ok = true;
    assert(r.RestoreFull(c.PackFull()));
    assert(r.s.economy.decision_count == history);
  }
  // All-water-consuming layouts still recover enough water for a paid cycle.
  {
    auto c = StartEconomy();
    for (auto &p : c.s.plots) {
      p.plant = p.next = Plant::Vine;
      p.ready = false;
      p.growth_ms = 0;
    }
    c.s.water = c.s.energy = 0;
    c.s.economy.paid_mask = 0;
    c.Advance(43200000);
    assert(c.s.fiber == 0 && c.s.water == 2); // No unconditional free energy.
    assert(c.Propose(2,0,static_cast<unsigned>(Plant::Light)));
    assert(c.ConfirmProposal(c.s.economy.proposal.token));
    assert(c.s.plots[0].plant == Plant::Light);
    c.Advance(700000);
    assert(c.s.energy > 0);
  }
  // Upkeep starts a full30 minutes after rescue, not at an unrelated global
  // tick.
  {
    auto c = StartEconomy();
    c.s.economy.built |= 128;
    c.s.economy.journey = 4;
    c.s.economy.journey_id = 1;
    c.s.economy.food = 10;
    c.Propose(5, 1, 1);
    c.ConfirmProposal(c.s.economy.proposal.token);
    c.Advance(60000);
    auto food = c.s.economy.food;
    c.Advance(1799000);
    assert(c.s.economy.food == food);
    c.Advance(1000);
    assert(c.s.economy.food == food - 2);
  }
  std::cout << "Economy: costs, paid cycles, migration, token replay, "
               "facilities, journey call, rescue, starvation, fourth plot and "
               "offline bounds passed\n";
}
