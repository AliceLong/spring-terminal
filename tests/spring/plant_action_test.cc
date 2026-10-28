#include "game/spring_tool_json.h"
#include "spring_gardener.h"
#include <cassert>
#include <iostream>
using namespace spring;
Core Fresh() {
  Core c;
  c.storage_ok = true;
  c.EnableEconomy(true);
  c.EnableMemory(101, 102);
  c.Wake();
  for (auto &p : c.s.plots) {
    p.ready = false;
    p.growth_ms = p.mature_ms = 0;
  }
  c.s.economy.paid_mask = 7;
  c.s.energy = c.s.water = 40;
  return c;
}
void Mature(Core &c, unsigned slot) {
  c.PlotAt(slot).ready = true;
  c.PlotAt(slot).growth_ms = 600000;
  c.s.economy.mature_yield[slot] = 6;
}
bool Run(Core &c, spring_gardener::Controller &body, bool voice = true) {
  bool traveling = false, arrived = false;
  for (uint64_t t = 100; t <= 60000 && c.PlantWorkActive(); t += 100) {
    c.Step(100);
    body.Step(c.Get(), t,
              voice ? spring_gardener::Voice::Listen
                    : spring_gardener::Voice::None);
    c.StepPlantWork(100,
                    spring_gardener::PlantWorkArrived(c.Get(), body.Get()));
    traveling |= c.s.plant_action.status == PlantActionStatus::Traveling;
    arrived |= c.s.plant_action.arrived;
  }
  assert(traveling && arrived);
  return c.s.plant_action.status == PlantActionStatus::Completed;
}
int main() {
  for (unsigned slot = 0; slot < 4; ++slot) {
    auto c = Fresh();
    c.s.economy.built |= 16;
    c.s.economy.paid_mask = 15;
    auto &p = c.PlotAt(slot);
    p.plant = p.next = static_cast<Plant>(slot);
    const auto water = c.s.water, energy = c.s.energy;
    assert(c.RequestPlantWork(10, 1, slot, p.cycle));
    assert(!c.RequestPlantWork(11, 1, slot, p.cycle));
    assert(!c.CanInviteLife(1));
    spring_gardener::Controller body;
    assert(Run(c, body));
    assert(c.s.water == water && c.s.energy == energy);
    assert(c.PlotAt(slot).growth_ms ==
           c.s.game_ms); // Normal growth only, no task bonus.
    assert(c.s.plant_action.arrived && c.s.plant_action.amount == 0);
  }
  for (unsigned slot = 0; slot < 4; ++slot) {
    auto c = Fresh();
    c.s.economy.built |= 16;
    c.s.economy.paid_mask = 15;
    auto &p = c.PlotAt(slot);
    p.plant = p.next = static_cast<Plant>(slot);
    Mature(c, slot);
    const auto cycle = p.cycle;
    const auto water = c.s.water, energy = c.s.energy, fiber = c.s.fiber,
               food = c.s.economy.food;
    assert(c.RequestPlantWork(20, 2, slot, cycle));
    assert(c.s.water == water && c.s.energy == energy &&
           p.ready); // No reward at request.
    spring_gardener::Controller body;
    assert(Run(c, body));
    assert(c.s.plant_action.amount == 6 && p.cycle == cycle + 1 && !p.ready);
    assert(c.s.energy == energy + (slot == 0 ? 6 : 0));
    assert(c.s.water == water + (slot == 1 ? 6 : 0));
    assert(c.s.fiber == fiber + (slot == 2 ? 6 : 0));
    assert(c.s.economy.food == food + (slot == 3 ? 6 : 0));
    assert(!c.RequestPlantWork(21, 2, slot, cycle) && !c.Harvest(slot));
    const auto events = c.s.event_count;
    c.StepPlantWork(1000, true);
    assert(c.s.event_count == events);
    assert(GameToolReplyBytes(ToolWorldJson(c.Get())) <= kGameToolReplyBudget);
  }
  {
    auto c = Fresh();
    assert(c.CheckPlantWork(2, 0, 1) == PlantActionReason::NotMature);
    assert(c.CheckPlantWork(1, 4, 1) == PlantActionReason::InvalidTarget);
    assert(c.CheckPlantWork(1, 0, 2) == PlantActionReason::StaleCycle);
    c.s.energy = 0;
    assert(c.CheckPlantWork(1, 0, 1) == PlantActionReason::NoEnergy);
    Mature(c, 0);
    assert(c.RequestPlantWork(1, 2, 0, 1));
    spring_gardener::Controller body;
    assert(Run(c, body)); // Low-power harvest recovers energy.
  }
  {
    auto c = Fresh();
    c.s.economy.paid_mask = 0;
    assert(c.CheckPlantWork(1, 0, 1) == PlantActionReason::UnpaidCycle);
    c.s.economy.construction = 0;
    assert(c.CheckPlantWork(2, 0, 1) == PlantActionReason::Busy);
    c.storage_ok = false;
    assert(c.CheckPlantWork(1, 0, 1) == PlantActionReason::Storage);
  }
  {
    auto c = Fresh();
    Mature(c, 2);
    assert(c.RequestPlantWork(1, 2, 2, 1));
    c.StepPlantWork(100, false);
    assert(c.Harvest(
        2, false,
        MemoryActor::Player)); // A competing physical claim wins once.
    const auto fiber = c.s.fiber;
    c.StepPlantWork(100, true);
    assert(c.s.plant_action.status == PlantActionStatus::Failed &&
           c.s.fiber == fiber);
    assert(c.s.plant_action.reason == PlantActionReason::StaleCycle);
  }
  {
    auto c = Fresh();
    Mature(c, 0);
    assert(c.RequestPlantWork(1, 2, 0, 1));
    c.Advance(50000);
    assert(c.PlotAt(0).ready); // Auto harvest cannot steal the body/crop.
    for (unsigned i = 0; i < 61; ++i)
      c.StepPlantWork(1000, false);
    assert(c.s.plant_action.reason == PlantActionReason::Timeout &&
           c.PlotAt(0).ready);
    c.Advance(10000);
    assert(!c.PlotAt(0).ready); // Automatic fallback resumes.
  }
  {
    auto c = Fresh();
    c.work = Work::Water;
    c.s.work_ms = 9000;
    Mature(c, 0);
    assert(c.RequestPlantWork(1, 2, 0, 1));
    c.StepPlantWork(100, false);
    assert(c.s.plant_action.status == PlantActionStatus::Queued);
    spring_gardener::Controller body;
    assert(Run(c, body)); // Finishes safe short work first.
  }
  {
    auto c = Fresh();
    c.demo = true;
    Mature(c, 1);
    assert(c.RequestPlantWork(1, 2, 1, 1));
    c.StepPlantWork(100, false);
    c.Advance(20000); // Game time/offline does NOT finish the real-body task.
    assert(c.s.plant_action.status == PlantActionStatus::Traveling &&
           c.PlotAt(1).ready);
    Core reboot;
    reboot.storage_ok = true;
    assert(reboot.RestoreArchive(*c.PackArchive()));
    assert(!reboot.PlantWorkActive() && reboot.work == Work::Rest);
    reboot.RecoverPlantWork();
    const auto &e = reboot.s.memory->records[reboot.s.memory->count - 1];
    assert(e.action == unsigned(MemoryAction::Finished) &&
           e.result == unsigned(PlantActionReason::Restarted));
    const auto count = reboot.s.memory->count;
    reboot.RecoverPlantWork();
    assert(reboot.s.memory->count == count);
    assert(reboot.PlotAt(1).ready && reboot.PlotAt(1).cycle == 1);
  }
  {
    auto c = Fresh();
    Mature(c, 0);
    c.s.energy = 59;
    assert(c.RequestPlantWork(1, 2, 0, 1));
    spring_gardener::Controller body;
    assert(Run(c, body));
    assert(c.s.energy == 60 && c.s.plant_action.amount == 1);
    Core reboot;
    reboot.storage_ok = true;
    assert(reboot.RestoreArchive(*c.PackArchive()));
    reboot.RecoverPlantWork();
    assert(reboot.s.energy == 60 && !reboot.PlotAt(0).ready);
  }
  {
    auto c = Fresh();
    assert(c.RequestPlantWork(31, 1, 0, 1));
    c.StepPlantWork(100, false);
    const auto growth = c.PlotAt(0).growth_ms;
    c.s.economy.construction = 0;
    c.StepPlantWork(100, true);
    assert(c.s.plant_action.status == PlantActionStatus::Failed &&
           c.s.plant_action.reason == PlantActionReason::Busy);
    assert(c.PlotAt(0).growth_ms == growth);
  }
  {
    auto c = Fresh();
    assert(c.RequestPlantWork(32, 1, 0, 1));
    c.StepPlantWork(100, false);
    Mature(c, 0);
    c.StepPlantWork(100, true);
    assert(c.s.plant_action.reason == PlantActionReason::AlreadyMature);
  }
  {
    auto c = Fresh();
    for (unsigned i = 0; i < 128; ++i)
      c.Remember(3, MemoryAction::WorldEvent, MemoryActor::System);
    assert(c.RequestPlantWork(33, 1, 0, 1));
    Core reboot;
    reboot.storage_ok = true;
    assert(reboot.RestoreArchive(*c.PackArchive()));
    reboot.RecoverPlantWork();
    assert(reboot.s.memory->count == 128);
    const auto last = reboot.s.memory->last_id;
    reboot.RecoverPlantWork();
    assert(reboot.s.memory->last_id == last);
  }
  {
    auto c = Fresh();
    c.s.story.enabled = 1;
    c.s.story.completed = 1u << unsigned(StoryNode::Destination);
    assert(c.RequestPlantWork(34, 1, 0, 1));
    c.StepPlantWork(100, false);
    c.TickStory(0);
    c.FinishPlantWork(PlantActionStatus::Cancelled,
                      PlantActionReason::Interrupted);
    c.TickStory(0);
    assert(!c.s.story.care_after_truth);
    assert(c.RequestPlantWork(35, 1, 0, 1));
    spring_gardener::Controller body;
    assert(Run(c, body));
    assert(c.s.story.care_after_truth);
  }
  std::cout << "Plant actions: real-body care/harvest, four slots, "
               "conversation, no premature reward, bounds, safe queue, "
               "duplicate/stale, low energy, timeout, restart and cap passed\n";
}
