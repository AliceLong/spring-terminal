// Candidate balance and bonus arithmetic only. Never linked into firmware.
#include "game/spring_json.h"
#include <array>
#include <cassert>
#include <cstdlib>
#include <iostream>
using namespace spring;
struct Pending {
  unsigned at = 0, kind = 0, base = 0;
};
void Sample(const Core &c, unsigned sec) {
  std::cout << "{\"second\":" << sec << ",\"water\":" << c.s.water
            << ",\"energy\":" << c.s.energy << ",\"fiber\":" << c.s.fiber
            << ",\"food\":" << c.s.economy.food
            << ",\"parts\":" << c.s.economy.parts << '}';
}
int main(int argc, char **argv) {
  assert(argc == 13);
  const unsigned normal = std::atoi(argv[1]), dry = std::atoi(argv[2]),
                 percent = std::atoi(argv[3]), hits = std::atoi(argv[4]),
                 particles = std::atoi(argv[5]);
  const int region = std::atoi(argv[6]);
  const unsigned seconds = std::atoi(argv[7]), water = std::atoi(argv[8]),
                 energy = std::atoi(argv[9]), scale = std::atoi(argv[10]),
                 special = std::atoi(argv[11]), capture = std::atoi(argv[12]);
  assert(normal && dry && percent <= 25 && hits <= particles &&
         particles <= 6 && particles && 60 % particles == 0 &&
         seconds <= 86400 && region >= -1 && region <= 4);
  Core c;
  c.storage_ok = true;
  c.EnableEconomy(true);
  c.Wake();
  c.s.water = water;
  c.s.energy = energy;
  for (auto &p : c.s.plots) {
    p.ready = false;
    p.growth_ms = p.mature_ms = 0;
  }
  c.s.economy.paid_mask = 0;
  c.s.economy.mature_yield.fill(0);
  if (special == 1) {
    c.s.fiber = 60;
    c.s.economy.food = 30;
  }
  if (special == 3) {
    c.s.wall_epoch = 1800000000;
    c.Sync(1800000000 + seconds);
    assert(c.offline_seconds == 43200);
    std::cout << "{\"offline_simulated_seconds\":" << c.offline_seconds
              << ",\"candidate_override_applied\":false,\"final\":";
    Sample(c, c.offline_seconds);
    std::cout << ",\"scope\":\"unchanged actual Core offline path; no manual "
                 "bonus\"}\n";
    return 0;
  }
  std::array<unsigned, 4> carry{}, base_gain{}, bonus_gain{}, overflow{},
      bonus_waste{};
  std::array<unsigned, 4> last_cycle;
  last_cycle.fill(UINT32_MAX);
  std::array<Pending, 4> pending{};
  uint32_t last_event = c.s.events[c.s.event_count - 1].id;
  unsigned blocked = 0, harvests = 0, first_harvest = 0;
  std::cout << "{\"samples\":[";
  Sample(c, 0);
  for (unsigned sec = 1; sec <= seconds; ++sec) {
    if (region >= 0) {
      c.s.economy.region = region;
      c.s.economy.region_ms = 0;
    }
    std::array<unsigned, 4> kinds{}, expected{};
    std::array<bool, 4> was_ready{};
    for (unsigned i = 0; i < c.PlotCount(); ++i) {
      was_ready[i] = c.PlotAt(i).ready;
      kinds[i] = unsigned(c.PlotAt(i).plant);
      expected[i] =
          c.s.economy.mature_yield[i] + !!(c.s.economy.boost_paid & (1u << i));
    }
    c.Step(1000);
    // A newly matured crop has not been auto-harvested yet (45s grace).
    for (unsigned i = 0; i < c.PlotCount(); ++i) {
      if (!was_ready[i] && c.PlotAt(i).ready &&
          c.PlotAt(i).plant == Plant::Moss) {
        if (c.s.economy.region == 0)
          c.s.economy.mature_yield[i] = normal;
        if (c.s.economy.region == 1)
          c.s.economy.mature_yield[i] = dry;
      }
    }
    if (hits && c.work == Work::Rest && !c.LifeWorkPending()) {
      for (unsigned i = 0; i < c.PlotCount(); ++i) {
        if (!c.PlotAt(i).ready)
          continue;
        const unsigned cycle = c.PlotAt(i).cycle;
        const auto kind = unsigned(c.PlotAt(i).plant);
        const unsigned amount = c.s.economy.mature_yield[i] +
                                !!(c.s.economy.boost_paid & (1u << i));
        if (!c.Harvest(i, false, MemoryActor::Player))
          continue;
        assert(last_cycle[i] != cycle && !pending[i].at);
        last_cycle[i] = cycle;
        kinds[i] = kind;
        expected[i] = amount;
        // Base is committed once by existing Core. Fine bonus arrives later.
        pending[i] = {sec + capture * scale, kind, amount};
      }
    }
    for (auto &p : pending) {
      if (!p.at || p.at > sec)
        continue;
      auto &pool = p.kind == 0   ? c.s.energy
                   : p.kind == 1 ? c.s.water
                   : p.kind == 2 ? c.s.fiber
                                 : c.s.economy.food;
      const unsigned cap = p.kind == 0   ? c.EnergyCapacity()
                           : p.kind == 1 ? c.WaterCapacity()
                           : p.kind == 2 ? 60
                                         : 30;
      const unsigned numerator =
          carry[p.kind] + p.base * percent * hits * (60 / particles);
      const unsigned gain = numerator / 6000;
      // No virtual bonus is banked while full, including fractional carry.
      const bool full = pool >= cap;
      const unsigned before = pool;
      c.Gain(pool, gain, cap);
      bonus_gain[p.kind] += pool - before;
      bonus_waste[p.kind] += gain - (pool - before);
      carry[p.kind] = full || pool - before < gain ? 0 : numerator % 6000;
      p = {};
    }
    for (unsigned i = 0; i < c.s.event_count; ++i) {
      const auto &e = c.s.events[i];
      if (e.id <= last_event || e.type != Event::Harvest ||
          e.target >= c.PlotCount())
        continue;
      const auto kind = kinds[e.target];
      base_gain[kind] += e.amount;
      overflow[kind] +=
          expected[e.target] > e.amount ? expected[e.target] - e.amount : 0;
      ++harvests;
      if (!first_harvest)
        first_harvest = sec;
    }
    last_event = c.s.events[c.s.event_count - 1].id;
    bool shortage = false;
    for (unsigned i = 0; i < c.PlotCount(); ++i) {
      const auto &p = c.PlotAt(i);
      if (!p.ready && !(c.s.economy.paid_mask & (1u << i)))
        shortage |= c.s.water < (p.plant == Plant::Moss ? 0u : 2u) ||
                    c.s.energy < (p.plant == Plant::Light ? 0u : 1u);
    }
    blocked += shortage;
    assert(c.s.water <= c.WaterCapacity() && c.s.energy <= c.EnergyCapacity() &&
           c.s.fiber <= 60 && c.s.economy.food <= 30);
    if (sec % 60 == 0) {
      std::cout << ',';
      Sample(c, sec);
    }
  }
  std::cout << "],\"harvests\":" << harvests
            << ",\"first_harvest_second\":" << first_harvest
            << ",\"shortage_seconds\":" << blocked;
  for (const auto &pair : {std::make_pair("base_gain", base_gain),
                           std::make_pair("manual_bonus_gain", bonus_gain),
                           std::make_pair("base_overflow", overflow),
                           std::make_pair("bonus_overflow", bonus_waste)}) {
    std::cout << ",\"" << pair.first << "\":[";
    for (unsigned i = 0; i < 4; ++i) {
      if (i)
        std::cout << ',';
      std::cout << pair.second[i];
    }
    std::cout << ']';
  }
  std::cout << ",\"final\":";
  Sample(c, seconds);
  std::cout << ",\"invariants_pass\":true}\n";
}
