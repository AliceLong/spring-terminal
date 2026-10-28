#include "game/spring_core.h"
#include "game/spring_json.h"
#include "boards/waveshare/esp32-s3-touch-amoled-2.16/spring_tilt.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
using namespace spring;
Core Start() {
  Core c;
  c.storage_ok = true;
  assert(c.Wake());
  return c;
}
void Mature(Core &c, unsigned i, Plant p) {
  c.s.plots[i].plant = c.s.plots[i].next = p;
  c.s.plots[i].ready = true;
  c.s.plots[i].growth_ms = kGrowMs[static_cast<unsigned>(p)];
  c.s.plots[i].mature_ms = 0;
}
int main() {
  {
    using spring_ui::LateralTilt;
    assert(std::abs(LateralTilt(0, 0, 1)) < .01f);
    assert(std::abs(LateralTilt(-1, 0, 0)) < .01f);
    assert(std::abs(LateralTilt(-.7071f, 0, .7071f)) < .01f);
    assert(LateralTilt(-.94f, .342f, 0) > 19);
    assert(LateralTilt(-.94f, -.342f, 0) < -19);
    assert(LateralTilt(0, .342f, .94f) > 19);
  }

  {
    Core c;
    assert(!c.Wake());
    assert(!c.Select(0, Plant::Vine));
    assert(!c.BuildTree());
    c.Step(10000);
    assert(c.s.game_ms == 0);
  }
  {
    auto c = Start();
    assert(!c.Wake());
    assert(c.s.notices == 1);
    assert(c.Select(0, Plant::Vine));
    assert(c.s.plots[0].plant == Plant::Light);
    Mature(c, 0, Plant::Light);
    c.Select(0, Plant::Vine);
    const auto energy = c.s.energy;
    assert(c.Harvest(0));
    assert(c.s.energy == energy + 4);
    assert(c.s.plots[0].plant == Plant::Vine);
    assert(!c.Harvest(0));
    assert(!c.Select(3, Plant::Moss));
    assert(!c.Select(0, static_cast<Plant>(3)));
  }
  {
    auto c = Start();
    for (unsigned i = 0; i < 3; ++i)
      Mature(c, i, static_cast<Plant>(i));
    auto water = c.s.water;
    assert(c.Shake() == 2);
    assert(c.s.plots[1].ready);
    assert(c.s.fiber == 2);
    assert(c.s.water == water - 2);
    assert(!c.Shake());
  }
  {
    auto c = Start();
    Mature(c, 1, Plant::Moss);
    auto water = c.s.water;
    for (int i = 0; i < 30; ++i)
      assert(!c.Tilt(-20, false, 100));
    assert(c.s.water == water);
    c.Tilt(0, true, 100);
    for (int i = 0; i < 17; ++i)
      assert(!c.Tilt(-20, true, 100));
    assert(c.Tilt(-20, true, 100));
    assert(c.s.water == water + 4);
    assert(!c.Tilt(-20, true, 100));
    assert(!c.Tilt(std::numeric_limits<float>::quiet_NaN(), true, 100));
  }
  {
    auto c = Start();
    Mature(c, 1, Plant::Moss);
    auto water = c.s.water;
    c.Tilt(0, true, 100);
    for (int i = 0; i < 18; ++i)
      c.Tilt(20, true, 100);
    assert(c.s.water == water);
    assert(c.s.plots[0].growth_ms == 30000);
    assert(!c.s.plots[1].ready);
  }
  {
    auto c = Start();
    Mature(c, 1, Plant::Moss);
    c.Tilt(0, true, 100);
    for (int i = 0; i < 10; ++i)
      c.Tilt(20, true, 100);
    c.Tilt(20, false, 100);
    assert(c.dew_path == 0);
    assert(c.s.plots[1].ready);
  }
  {
    auto c = Start();
    Mature(c, 1, Plant::Moss);
    const auto water = c.s.water;
    c.Tilt(0, true, 100);
    c.Tilt(-15, false, 100); // transition from neutral must not disarm
    assert(c.tilt_armed && c.dew_path == 0);
    for (int i = 0; i < 18; ++i) c.Tilt(-20, true, 100);
    assert(c.s.water == water + 4);
    Mature(c, 1, Plant::Moss);
    for (int i = 0; i < 25; ++i) c.Tilt(-20, true, 100);
    assert(c.s.plots[1].ready); // next dew still requires its own neutral
    c.Tilt(0, true, 100);
    c.Tilt(60, true, 100);
    assert(!c.tilt_armed); // excessive rotation cancels arming
  }
  {
    auto c = Start();
    c.s.water = 0;
    c.s.energy = 0;
    c.Advance(90000);
    assert(c.s.water > 0 && c.s.energy > 0);
    assert(c.s.plots[0].growth_ms > 0);
  }
  {
    auto c = Start();
    assert(!c.BuildTree());
    c.s.fiber = 4;
    c.s.water = 8;
    assert(c.BuildTree());
    assert(c.s.water == 2 && c.s.fiber == 0);
    assert(!c.BuildTree());
    c.Advance(60000);
    assert(c.s.tree_stage == 1);
    assert(c.s.story_stage == 2);
    assert(c.s.notices & 4);
    assert(!c.BuildTree());
    assert(c.Accept(2));
    assert(!c.Accept(2));
    assert(!(c.s.notices & 4));
    assert(c.s.accepted & 4);
    // Existing stage-1 saves remain loadable; second cultivation is resumable.
    c.s.water = 20; c.s.fiber = 12;
    assert(c.BuildTree());
    assert(c.s.water == 14 && c.s.fiber == 8);
    assert(!c.BuildTree());
    c.Advance(30000);
    Core resumed; resumed.storage_ok = true;
    assert(resumed.Restore(c.Pack()));
    assert(resumed.s.tree_stage == 1 && resumed.s.tree_work_ms > 0);
    resumed.Advance(60000);
    assert(resumed.s.tree_stage == 2);
    assert(!resumed.BuildTree());
    Core final_save; final_save.storage_ok = true;
    assert(final_save.Restore(resumed.Pack()));
    assert(final_save.s.tree_stage == 2);

  }
  {
    auto c = Start();
    c.Advance(540000);
    assert(c.s.notices & 8);
    assert(c.Announce(3));
    assert(!c.Announce(3));
    assert(!c.Announce(4));
    assert(c.Accept(3));
    c.Advance(10000);
    assert(!(c.s.notices & 8));
  }
  {
    auto c = Start();
    c.Advance(43200000);
    const auto packed = c.Pack();
    Core restored;
    assert(restored.Restore(packed));
    assert(restored.Pack() == packed);
    auto bad = packed;
    bad[10] ^= 1;
    assert(!restored.Restore(bad));
    bad = packed;
    bad[2] = 100;
    bad.back() = Core::Checksum(bad);
    assert(!restored.Restore(bad));
  }
  {
    auto c = Start();
    c.Sync(1800000000);
    auto save = c.Pack();
    Core restored;
    restored.storage_ok = true;
    assert(restored.Restore(save));
    restored.Step(1000);
    restored.Sync(1800000011);
    assert(restored.offline_seconds == 10);
    save = restored.Pack();
    Core again;
    again.storage_ok = true;
    assert(again.Restore(save));
    again.Sync(1800000011);
    assert(!again.offline_seconds);
  }
  {
    auto c = Start();
    c.Sync(1800000000);
    auto save = c.Pack();
    Core r;
    r.storage_ok = true;
    assert(r.Restore(save));
    r.Sync(1799999990);
    assert(!r.offline_seconds);
  }
  {
    auto c = Start();
    c.Sync(1800000000);
    Core r;
    r.storage_ok = true;
    assert(r.Restore(c.Pack()));
    r.Sync(1800090000);
    assert(r.offline_seconds == 43200);
  }
  {
    auto a = Start(), b = Start();
    a.demo = true;
    a.Step(1000);
    b.Step(1000);
    assert(a.s.game_ms == b.s.game_ms * 10);
    auto normal = b.Pack();
    assert(normal != a.Pack());
  }
  {
    auto c = Start();
    c.s.game_ms = (uint64_t(1) << 32) + 999;
    auto saved = c.Pack();
    Core r;
    assert(r.Restore(saved));
    assert(r.s.game_ms == c.s.game_ms);
  }
  {
    auto c = Start();
    Mature(c, 0, Plant::Light);
    c.s.energy = 98;
    assert(c.Harvest(0));
    assert(c.s.energy == 99);
    assert(c.s.events[c.s.event_count - 1].amount == 1);
  }
  {
    auto c = Start();
    c.Advance(3600000);
    auto s = c.Get();
    assert(s.event_count <= 12);
    assert(s.next_event > 12);
    std::cout << WorldJson(s, 2) << '\n' << EventsJson(s) << '\n';
  }
  {
    auto c = Start();
    Mature(c, 1, Plant::Moss);
    const auto water = c.s.water;
    for (int i = 0; i < 100; ++i)
      c.Tilt(-20, true, 100);
    assert(c.s.water == water && c.s.plots[1].ready);
  }
  std::cerr << "Spring core: 19 scenario groups passed\n";
}
