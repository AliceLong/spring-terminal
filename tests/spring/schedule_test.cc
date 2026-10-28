#include "game/spring_agency.h"
#include "game/spring_tool_json.h"
#include "spring_gardener.h"
#include <cassert>
#include <iostream>
using namespace spring;
Core Fresh(unsigned stage) {
  Core c;
  c.storage_ok = true;
  c.EnableEconomy(true);
  c.EnableMemory(101, 102);
  c.Wake();
  c.s.story.enabled = 1;
  c.s.story.active = kNoStory;
  c.s.story.language = stage;
  for (auto &p : c.s.plots) {
    p.ready = false;
    p.growth_ms = p.mature_ms = 0;
  }
  c.s.economy.paid_mask = 7;
  c.s.water = c.s.energy = 40;
  c.s.life.cooldown = 0;
  return c;
}
unsigned Count(const Core &c, MemoryAction action, uint32_t sequence) {
  unsigned n = 0;
  for (unsigned i = 0; i < c.s.memory->count; ++i) {
    const auto &e = c.s.memory->records[i];
    n += e.category == 1 && e.action == unsigned(action) &&
         e.source_event == sequence;
  }
  return n;
}
void Tick(Core &c, spring_gardener::Controller &body, uint64_t t,
          bool voice = true) {
  c.Step(100);
  c.StepLife(100, voice, spring_gardener::LifeArrived(c.Get(), body.Get()),
             false);
  body.Step(c.Get(), t,
            voice ? spring_gardener::Voice::Listen
                  : spring_gardener::Voice::None);
  if (c.PlantWorkActive())
    c.StepPlantWork(100,
                    spring_gardener::PlantWorkArrived(c.Get(), body.Get()));
}
AgencyState ApprovedRest(Core &c) {
  auto a = EmptyAgency(c.Get());
  assert(OfferWish(c.Get(), a, 3, 0));
  assert(PresentWish(a, a.current.token));
  assert(StageWishApproval(c, a, a.current.token));
  assert(c.ApprovedLife());
  return a;
}
int main() {
  // Stage0 cannot propose a private wish or resume one without an actual
  // permission.
  {
    auto c = Fresh(0);
    auto a = EmptyAgency(c.Get());
    assert(!OfferWish(c.Get(), a, 3, 0));
    assert(!c.RequestLocalMindLife(3));
    assert(!c.StartLifeRequest(3, 0, unsigned(LifeReason::RestAfterWork),
                               MemoryActor::Robot));
    c.StepLife(100, false, false, true);
    assert(!PersonalLife(c.s.life.intent, c.s.life.target));
  }
  // Both stages: a real explicit invitation waits for the existing short tool
  // safely.
  for (unsigned stage : {0u, 1u}) {
    auto c = Fresh(stage);
    c.work = Work::Water;
    c.s.work_ms = 3500;
    assert(c.InviteLife(1));
    assert(c.s.schedule.waiting_short);
    auto text = ScheduleJson(c.Get());
    assert(text.find("finishing_watering") != std::string::npos);
    assert(text.find("waiting_safe_point") != std::string::npos);
    assert(!c.ApprovedLife());
    c.StepLife(100, true, false, false);
    assert(c.s.life.intent && c.s.life.phase == 1 && !c.s.life.count);
    spring_gardener::Controller body;
    for (uint64_t t = 100; t <= 55000 && c.LifeActive(); t += 100)
      Tick(c, body, t);
    assert(!c.LifeActive() && c.s.life.records[0].result == 0 &&
           c.s.life.records[0].reached);
  }
  // Actual plant task waits, gives a reason, and cannot be superseded by
  // another task.
  {
    auto c = Fresh(0);
    c.work = Work::Harvest;
    c.s.work_ms = 1500;
    assert(c.RequestPlantWork(1, 1, 0, 1));
    assert(c.s.plant_action.wait == ScheduleBlocker::HarvestFinishing);
    assert(!c.InviteLife(1));
    assert(!c.RequestPlantWork(2, 1, 1, 1));
    assert(!c.CanResumeWork());
    c.StepPlantWork(100, false);
    assert(c.s.plant_action.status == PlantActionStatus::Queued);
  }
  // Stage1 permission survives a TEMPORARY inserted plant job; actual same life
  // resumes.
  {
    auto c = Fresh(1);
    auto a = ApprovedRest(c);
    auto seq = c.s.life.sequence, chain = a.current.chain;
    c.StepLife(100, true, false, false);
    c.StepLife(100, true, true, false);
    c.StepLife(3000, true, true,
               false); // Clamped to a real online interval, not catch-up.
    const auto elapsed = c.s.life.elapsed;
    assert(elapsed > 0);
    assert(c.RequestPlantWork(20, 1, 0, 1));
    assert(c.s.life.phase == 4 &&
           c.s.schedule.pause == ScheduleBlocker::PlayerTask);
    assert(c.s.life.sequence == seq && c.s.life.elapsed == elapsed);
    assert(Count(c, MemoryAction::Paused, seq) == 1);
    RefreshAgency(c.Get(), a);
    assert(a.current.status == unsigned(WishStatus::Executing));
    assert(AgencyJson(c.Get(), a).find("paused_for_work") != std::string::npos);
    spring_gardener::Controller body;
    bool resumed = false, reapproached = false;
    for (uint64_t t = 100; t <= 60000 && c.LifeActive(); t += 100) {
      Tick(c, body, t);
      resumed |= Count(c, MemoryAction::Resumed, seq) == 1;
      reapproached |= resumed && c.s.life.phase == 2;
    }
    assert(c.s.plant_action.status == PlantActionStatus::Completed);
    assert(resumed && reapproached && !c.LifeActive());
    assert(Count(c, MemoryAction::Finished, seq) == 1 &&
           c.s.life.records[0].result == 0);
    assert(RefreshAgency(c.Get(), a) &&
           a.current.status == unsigned(WishStatus::Completed));
    assert(a.current.sequence == seq && a.current.chain == chain);
  }
  // Stage1 approval may queue behind short work; opening/clicking is still no
  // permission.
  {
    auto c = Fresh(1);
    auto a = EmptyAgency(c.Get());
    assert(OfferWish(c.Get(), a, 3, 0));
    assert(!StageWishApproval(c, a, a.current.token));
    assert(PresentWish(a, a.current.token));
    c.work = Work::Water;
    c.s.work_ms = 3000;
    assert(StageWishApproval(c, a, a.current.token));
    assert(c.ApprovedLife() && c.s.schedule.waiting_short);
    assert(AgencyJson(c.Get(), a).find("waiting_safe_point") !=
           std::string::npos);
  }
  // An explicit NEW destination cancels the old wish; never resume the obsolete
  // plan.
  {
    auto c = Fresh(1);
    auto a = ApprovedRest(c);
    auto old = c.s.life.sequence;
    assert(c.InviteLife(1));
    assert(c.s.life.sequence == old + 1 && !c.ApprovedLife());
    assert(RefreshAgency(c.Get(), a) &&
           a.current.status == unsigned(WishStatus::Superseded));
    assert(c.s.life.intent == 1 && c.s.schedule.pause == ScheduleBlocker::None);
  }
  // Return-to-work cancels a life plan, not a committed economic operation.
  {
    auto c = Fresh(1);
    auto a = ApprovedRest(c);
    assert(c.ResumeWork());
    assert(!c.LifeActive() && !c.s.schedule.approved_token);
    assert(RefreshAgency(c.Get(), a));
    assert(a.current.status == unsigned(WishStatus::Superseded));
  }
  // Critical work cancels rather than indefinitely holding an approved wish.
  for (unsigned kind = 0; kind < 3; ++kind) {
    auto c = Fresh(1);
    auto a = ApprovedRest(c);
    if (kind == 0)
      c.s.economy.construction = 0;
    else if (kind == 1)
      c.s.economy.emergency_work = 1000;
    else
      c.s.story.active = 0;
    c.StepLife(100, true, false, false);
    assert(!c.LifeActive());
    assert(RefreshAgency(c.Get(), a) &&
           a.current.status == unsigned(WishStatus::Interrupted));
  }
  // Routine low-priority life is interrupted, not upgraded to an approved
  // private wish.
  {
    auto c = Fresh(0);
    assert(c.StartLifeRequest(2, 0, unsigned(LifeReason::CheckPlant),
                              MemoryActor::Robot));
    auto seq = c.s.life.sequence;
    assert(c.RequestPlantWork(1, 1, 0, 1));
    assert(!c.LifeActive());
    assert(c.s.schedule.last_sequence == seq &&
           c.s.schedule.last_result == unsigned(LifeResult::WorkInterrupted));
  }
  // Explicit life already reserved cannot be silently replaced by a plant job.
  {
    auto c = Fresh(0);
    assert(c.InviteLife(1));
    assert(c.CheckPlantWork(1, 0, 1) == PlantActionReason::PlayerActivity);
    assert(!c.RequestPlantWork(1, 1, 0, 1));
  }
  // Cancel while paused prevents automatic resumption, and stale IDs do not
  // cancel newer work.
  {
    auto c = Fresh(1);
    auto a = ApprovedRest(c);
    auto seq = c.s.life.sequence;
    assert(c.RequestPlantWork(2, 1, 0, 1));
    assert(!c.CancelGardenerActivity(0, seq + 1));
    assert(c.CancelGardenerActivity(0, seq));
    assert(!c.LifeActive());
    assert(RefreshAgency(c.Get(), a) &&
           a.current.status == unsigned(WishStatus::Superseded));
    assert(c.PlantWorkActive() && !c.CancelGardenerActivity(3, 0));
    assert(c.CancelGardenerActivity(2, 0));
    assert(c.s.plant_action.status == PlantActionStatus::Cancelled &&
           !c.CancelGardenerActivity(2, 0));
  }
  // Pause expiry, changed target and phase downgrade never fabricate
  // completion.
  {
    auto c = Fresh(1);
    auto a = ApprovedRest(c);
    assert(c.RequestPlantWork(2, 1, 0, 1));
    c.FinishPlantWork(PlantActionStatus::Cancelled,
                      PlantActionReason::Interrupted);
    c.s.water = 0;
    for (unsigned i = 0; i < 61; ++i)
      c.StepLife(1000, true, false, false);
    assert(!c.LifeActive() && RefreshAgency(c.Get(), a) &&
           a.current.status == unsigned(WishStatus::Interrupted));
  }
  {
    auto c = Fresh(1);
    auto a = ApprovedRest(c);
    assert(c.RequestPlantWork(2, 1, 0, 1));
    c.FinishPlantWork(PlantActionStatus::Cancelled,
                      PlantActionReason::Interrupted);
    c.s.story.language = 0;
    c.StepLife(100, true, false, false);
    assert(!c.LifeActive() && RefreshAgency(c.Get(), a));
  }
  // Restart retains interruption evidence but never replays transient
  // permission or a reward.
  {
    auto c = Fresh(1);
    auto a = ApprovedRest(c);
    assert(c.RequestPlantWork(3, 1, 0, 1));
    auto save = *c.PackArchive();
    Core reboot;
    reboot.storage_ok = true;
    assert(reboot.RestoreArchive(save));
    assert(!reboot.s.schedule.approved_token && !reboot.PlantWorkActive());
    reboot.RecoverPlantWork();
    reboot.StepLife(100, false, false, false);
    assert(!reboot.LifeActive() && RefreshAgency(reboot.Get(), a) &&
           a.current.status == unsigned(WishStatus::Interrupted));
  }
  {
    auto c = Fresh(1);
    auto a = ApprovedRest(c);
    auto seq = c.s.life.sequence;
    c.s.plots[0].ready = true;
    c.s.plots[0].growth_ms = 600000;
    c.s.economy.mature_yield[0] = 6;
    assert(c.RequestPlantWork(50, 2, 0, 1));
    assert(LifeJson(c.Get()).find("paused_for_work") != std::string::npos);
    assert(ScheduleJson(c.Get()).find("revalidate_approved_wish") !=
               std::string::npos ||
           ScheduleJson(c.Get()).find("player_plant_task") !=
               std::string::npos);
    spring_gardener::Controller body;
    for (uint64_t t = 100; t < 60000 && c.LifeActive(); t += 100)
      Tick(c, body, t);
    assert(c.s.plant_action.status == PlantActionStatus::Completed &&
           c.s.plant_action.amount == 6);
    assert(c.s.plots[0].cycle == 2 &&
           Count(c, MemoryAction::Paused, seq) == 1 &&
           Count(c, MemoryAction::Resumed, seq) == 1);
    assert(RefreshAgency(c.Get(), a) &&
           a.current.status == unsigned(WishStatus::Completed));
  }
  {
    auto c = Fresh(1);
    auto a = ApprovedRest(c);
    assert(c.RequestPlantWork(51, 1, 0, 1));
    assert(c.CancelGardenerActivity(51, 0));
    c.StepLife(100, true, false, false);
    assert(c.LifeActive() && c.ApprovedLife() && c.s.life.phase == 2);
    assert(c.s.schedule.pause == ScheduleBlocker::None);
    assert(a.current.status == unsigned(WishStatus::Executing));
  }
  {
    auto c = Fresh(1);
    auto a = EmptyAgency(c.Get());
    assert(OfferWish(c.Get(), a, 3, 0));
    assert(PresentWish(a, a.current.token));
    c.s.economy.construction = 0;
    auto copy = a;
    assert(!StageWishApproval(c, copy, a.current.token));
    assert(copy.current.status == unsigned(WishStatus::Presented));
    assert(!c.LifeActive() && !c.s.schedule.approved_token);
  }
  {
    auto c = Fresh(0);
    assert(c.InviteLife(1));
    assert(c.ResumeWork());
    auto json = MemoryJson(c.Get());
    assert(json.find("player_invitation") != std::string::npos);
    assert(json.find("another_player_activity") != std::string::npos);
    Core reboot;
    reboot.storage_ok = true;
    assert(reboot.RestoreArchive(*c.PackArchive()));
    assert(MemoryJson(reboot.Get()).find("another_player_activity") !=
           std::string::npos);
    assert(MemoryJson(reboot.Get()).find("player_invitation") !=
           std::string::npos);
  }
  {
    auto c = Fresh(0);
    assert(c.InviteLife(1));
    c.s.plots[0].ready = true;
    c.s.plots[0].growth_ms = 600000;
    c.s.plots[0].mature_ms = 45000;
    c.s.economy.mature_yield[0] = 6;
    c.Advance(1000);
    assert(c.s.plots[0]
               .ready); // Ordinary harvest cannot steal an explicit life turn.
    c.s.energy = 0;
    c.Advance(1000);
    assert(!c.s.plots[0].ready); // Actual energy recovery is urgent.
    c.StepLife(100, false, false, false);
    assert(!c.LifeActive());
  }
  {
    auto c = Fresh(1);
    auto a = ApprovedRest(c);
    c.s.plots[0].ready = true;
    c.s.plots[0].growth_ms = 600000;
    c.s.plots[0].mature_ms = 45000;
    c.s.economy.mature_yield[0] = 6;
    c.Advance(1000);
    assert(
        c.s.plots[0].ready); // Already-approved wishes also reserve their turn.
    c.s.energy = 0;
    c.Advance(1000);
    assert(!c.s.plots[0].ready);
    c.StepLife(100, false, false, false);
    assert(c.LifeActive() &&
           c.s.schedule.pause == ScheduleBlocker::HarvestFinishing);
    assert(a.current.status == unsigned(WishStatus::Executing));
  }
  std::cout << "Stage01 scheduling: safe waits, body priority, permission, "
               "pause/resume, replacement/cancel, critical work, timeout and "
               "restart passed\n";
}
