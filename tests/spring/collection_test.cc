#include "game/spring_collection.h"
#include <cassert>
#include <iostream>
using namespace spring_collection;
int main() {
  assert(SegmentHit(0, 0, 100, 0, 50, 28));
  assert(!SegmentHit(0, 0, 100, 0, 50, 29));
  assert(SegmentHit(100, 0, 0, 0, 50, 0));
  assert(SegmentHit(20, 0, 20, 100, 20, 60));
  assert(SegmentHit(0, 0, 0, 0, 0, 0));
  assert(!SegmentHit(0, 0, 0, 0, 40, 0));
  Session s;
  assert(s.Begin(1000, false, false));
  assert(!s.Begin(1001, true, false) && s.batches == 1 && !s.growth);
  s.Tick(1500);
  s.Down(90, 240);
  assert(s.Move(90, 240, 1500) == 0);  // No stationary-tap reward.
  assert(s.Move(390, 240, 1501) == 6); // Sparse sample spans every particle.
  assert(s.Move(90, 240, 1502) == 0 && s.hits == 6); // Cannot collect twice.
  s.Up();
  assert(!s.Move(390, 240, 1503));
  s.Tick(1900);
  assert(!s.active && s.misses == 0);
  assert(!s.Begin(2000, false, false));
  assert(s.Begin(8000, true, false) && s.count == 4);
  s.Tick(14000);
  assert(!s.active && s.misses == 4);
  Session late;
  assert(late.Begin(0, false, false));
  late.Down(90, 245);
  assert(!late.Move(390, 245, 6001) && late.hits == 0 && late.misses == 6);
  Session long_uptime;
  constexpr uint64_t start = uint64_t(UINT32_MAX) + 5000;
  assert(long_uptime.Begin(start, false, true));
  long_uptime.Tick(start + 500);
  for (const auto &p : long_uptime.particles)
    assert(p.y >= 296 && p.y <= 338);
  long_uptime.Down(90, 310);
  assert(long_uptime.Move(390, 310, start + 501) == 6);
  long_uptime.Tick(start + 502);
  assert(long_uptime.active); // No 32-bit timestamp truncation after 49 days.
  long_uptime.Tick(start + 900);
  assert(!long_uptime.active);
  for (unsigned round = 0; round < 10000; ++round) {
    Session pool;
    assert(pool.Begin(1000, round % 2, round % 3 == 0));
    for (unsigned t = 0; t < 6000; t += 73) {
      pool.Tick(1000 + t);
      for (unsigned i = 0; i < pool.count; ++i) {
        const auto &p = pool.particles[i];
        assert(p.x >= 106 && p.x <= 370 && p.y >= 228 && p.y <= 342);
      }
    }
    pool.Cancel();
    assert(!pool.active);
  }
  std::cout << "Collection: fixed pool " << sizeof(Session)
            << " bytes; sparse swipe, duplicate/cooldown, deadline, voice "
               "bounds, long uptime and 10000 cancellations pass\n";
}
