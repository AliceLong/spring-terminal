#include "spring_gardener.h"
#include <cassert>
#include <cmath>
#include <iostream>
using namespace spring_gardener;
int main() {
  // Both world versions retain a real watering task long enough to arrive and
  // pour. Mid-task saves round-trip, and the put-away pose occurs only at end.
  for (bool economy : {false, true}) {
    spring::Core water;
    water.storage_ok = true;
    if (economy)
      water.EnableEconomy(true);
    water.Wake();
    for (auto &plot : water.s.plots) {
      plot.ready = false;
      plot.growth_ms = 0;
    }
    water.Advance(20000);
    assert(water.work == spring::Work::Water);
    assert(water.s.work_ms == spring::kWaterWorkMs);
    spring::Core copy;
    copy.storage_ok = true;
    assert(economy ? copy.RestoreFull(water.PackFull())
                   : copy.Restore(water.Pack()));
    assert(copy.s.work_ms == spring::kWaterWorkMs);
    Controller pouring;
    unsigned visible_ms = 0;
    for (uint64_t t = 0; t < spring::kWaterWorkMs; t += 100) {
      auto pose = pouring.Step(water.Get(), t + 1000, Voice::None);
      if (pose.mode == Mode::Water && pose.frame == 2)
        visible_ms += 100;
      if (pose.mode == Mode::Water && water.s.work_ms > 1000)
        assert(pose.frame != 3);
      water.Advance(100);
    }
    assert(visible_ms >= 2500);
    assert(water.work == spring::Work::Rest && water.s.work_ms == 0);
  }
  assert(WaterFrame(0, 10000) == 0);
  assert(WaterFrame(800, 9200) == 1);
  assert(WaterFrame(1500, 8500) == 2);
  assert(WaterFrame(8000, 2000) == 2);
  assert(WaterFrame(9000, 1000) == 3);
  spring::Core core;
  core.storage_ok = true;
  core.EnableEconomy(true);
  core.Wake();
  auto s = core.Get();
  // Legacy presentation-only auto-rest. Economy life is tested with real Core
  // feedback in life_test.
  s.economy.enabled = false;
  s.work = spring::Work::Rest;
  s.notices = 0;
  const auto save = core.PackFull();
  Controller c;
  auto p = c.Step(s, 1000, Voice::None);
  float last = p.x;
  float lowest = p.y, highest = p.y, previous_y = p.y;
  for (uint64_t t = 1100; t < 60000; t += 100) {
    p = c.Step(s, t, Voice::None);
    assert(p.x >= 190 && p.x <= 310 && p.y > 238 && p.y < 325);
    assert(std::abs(p.x - last) <= 2.1);
    if (t < 20000) {
      lowest = std::min(lowest, p.y);
      highest = std::max(highest, p.y);
      assert(std::abs(p.y - previous_y) <= 2.2);
    }
    previous_y = p.y;
    last = p.x;
  }
  assert(highest - lowest >
         30); // Actual flight height, beyond the2px idle bob.
  assert(p.mode == Mode::Rest);
  // A high flyer stops in place for voice rather than sinking to the old lane.
  Controller flyer;
  for (uint64_t t = 1000; t < 11000; t += 100)
    flyer.Step(s, t, Voice::None);
  const float flight_y = flyer.Get().y;
  for (uint64_t t = 11000; t < 14000; t += 100) {
    auto frozen = flyer.Step(s, t, Voice::Listen);
    assert(std::abs(frozen.y - flight_y) < 4.1);
  }
  assert(flyer.Wave(14000));
  bool saw_wave_end = false, saw_last_frame = false;
  for (uint64_t t = 14000; t <= 15300; t += 100) {
    auto waving = flyer.Step(s, t, Voice::Listen);
    assert(waving.voice == Voice::Listen);
    saw_last_frame |= waving.mode == Mode::Wave && waving.frame == 3;
    saw_wave_end |= saw_last_frame && waving.mode != Mode::Wave;
  }
  assert(saw_last_frame && saw_wave_end);
  auto high_task = s;
  high_task.work = spring::Work::Water;
  high_task.target = 0;
  high_task.work_ms = spring::kWaterWorkMs;
  unsigned poured = 0;
  for (uint64_t t = 14000; t < 24000; t += 100) {
    auto approach = flyer.Step(high_task, t, Voice::None);
    if (approach.mode == Mode::Water && approach.frame == 2)
      poured += 100;
    high_task.work_ms -= 100;
  }
  assert(poured >= 2500); // Travel must still leave time for the10-second care.
  c.Activity(60000);
  p = c.Step(s, 60000, Voice::None);
  assert(p.mode == Mode::Takeoff);
  for (uint64_t t = 60100; t < 61500; t += 100)
    p = c.Step(s, t, Voice::None);
  assert(p.mode == Mode::Move || p.mode == Mode::Hover);
  // Real work continues during voice; presentation never mutates the world.
  s.work = spring::Work::Water;
  s.target = 2;
  for (uint64_t t = 61500; t < 72000; t += 100)
    p = c.Step(s, t, Voice::None);
  assert(p.mode == Mode::Water);
  float x = p.x;
  p = c.Step(s, 72100, Voice::Listen);
  assert(p.mode == Mode::Water && p.x == x && p.voice == Voice::Listen);
  p = c.Step(s, 72200, Voice::Wait);
  assert(p.mode == Mode::Water);
  p = c.Step(s, 72300, Voice::Speak);
  assert(p.mode == Mode::Water && p.voice == Voice::Speak);
  p = c.Step(s, 72400, Voice::None);
  assert(p.mode == Mode::Water);
  assert(c.Shake(72500, 0, Voice::None));
  p = c.Step(s, 72600, Voice::None);
  assert(p.mode == Mode::Water && p.animation == Animation::Water);
  assert(!c.Shake(73000, 1, Voice::None));
  p = c.Step(s, 75200, Voice::None);
  assert(p.mode == Mode::Water);
  assert(c.Shake(80500, 1, Voice::None));
  p = c.Step(s, 80600, Voice::None);
  assert(p.mode == Mode::Water && p.animation == Animation::Water);
  assert(c.Shake(88500, 2, Voice::None));
  p = c.Step(s, 88600, Voice::None);
  assert(p.mode == Mode::Water && p.animation == Animation::Water);
  // A shake during a working conversation preserves the real task and voice
  // channel.
  assert(c.Shake(96500, 0, Voice::Speak));
  p = c.Step(s, 96600, Voice::Speak);
  assert(p.mode == Mode::Water && p.voice == Voice::Speak);
  s.work = spring::Work::Build;
  s.economy.construction = 0;
  for (uint64_t t = 96700; t < 110000; t += 100)
    p = c.Step(s, t, Voice::None);
  assert(p.mode == Mode::Build);
  s.economy.construction = 255;
  s.economy.built = 1;
  s.work = spring::Work::Rest;
  p = c.Step(s, 110000, Voice::None);
  assert(p.mode == Mode::Celebrate);
  p = c.Step(s, 110100, Voice::Listen);
  assert(p.mode == Mode::Listen);
  p = c.Step(s, 110200, Voice::None);
  assert(p.mode != Mode::Celebrate);
  // Persistent idle network wait eventually releases the character.
  p = c.Step(s, 111000, Voice::Wait);
  assert(p.mode == Mode::Listen);
  p = c.Step(s, 141100, Voice::Wait);
  assert(p.mode != Mode::Listen);
  // Only idle gestures may change harvest/work; busy gestures are cosmetic.
  auto input = s;
  input.work = spring::Work::Water;
  input.economy.construction = 255;
  assert(!CanHarvestOnShake(input, Voice::None));
  input.work = spring::Work::Build;
  assert(!CanHarvestOnShake(input, Voice::None));
  input.work = spring::Work::Rest;
  assert(CanHarvestOnShake(input, Voice::None));
  assert(!CanHarvestOnShake(input, Voice::Speak));
  // The discarded-device journey lasts two minutes but must read as
  // exploration, while genuine facility construction retains the construction
  // animation.
  Controller exploring;
  auto trip = input;
  trip.work = spring::Work::Build;
  trip.economy.journey_task = 1;
  trip.economy.journey_work = 120000;
  Pose journey_pose;
  for (uint64_t t = 0; t < 16000; t += 100)
    journey_pose = exploring.Step(trip, t + 1000, Voice::None);
  assert(journey_pose.mode == Mode::Explore &&
         journey_pose.animation == Animation::InspectLeft);
  trip.economy.journey_task = trip.economy.journey_work = 0;
  trip.work = spring::Work::Rest;
  assert(exploring.Step(trip, 17000, Voice::None).mode != Mode::Explore);
  Controller idle;
  idle.Step(input, 1000, Voice::None);
  assert(idle.Shake(1100, 0, Voice::None));
  assert(idle.Step(input, 1200, Voice::None).mode == Mode::Recover);
  assert(!idle.Shake(1600, 1, Voice::None));
  assert(idle.Step(input, 2300, Voice::None).mode != Mode::Recover);
  assert(idle.Shake(
      2600, 1, Voice::None)); // Acknowledges during6.5s big-action cooldown.
  auto small = idle.Step(input, 2700, Voice::None);
  assert(small.mode != Mode::Squint && small.mode != Mode::Wave);
  assert(idle.Shake(8000, 1, Voice::None));
  assert(idle.Step(input, 8100, Voice::None).mode == Mode::Squint);
  idle.Step(input, 9300, Voice::None);
  assert(idle.Shake(14500, 2, Voice::None));
  assert(idle.Step(input, 14600, Voice::None).mode == Mode::Wave);
  for (uint64_t t = 14700; t < 80000; t += 100)
    idle.Step(input, t, Voice::None);
  assert(idle.Get().mode == Mode::Rest);
  assert(idle.Shake(80100, 2, Voice::None));
  auto wake = idle.Step(input, 80200, Voice::None);
  assert(wake.mode == Mode::Rest && wake.animation == Animation::Takeoff &&
         wake.frame == 0);
  assert(idle.Step(input, 80500, Voice::None).mode == Mode::Takeoff);
  assert(core.PackFull() == save);
  std::cout << "Gardener: bounded eased flight, rest/takeoff, task resume, "
               "voice priority/timeout, tool continuity,1s "
               "acknowledgement/6.5s big cooldown, "
               "celebration; no world writes.\n";
}
