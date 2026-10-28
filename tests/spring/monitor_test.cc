#include "game/spring_monitor_json.h"
#include <cassert>
#include <iostream>
int main() {
  spring::Core c;
  c.storage_ok = true;
  c.EnableEconomy(true);
  c.Wake();
  const auto before = c.PackFull();
  spring::MonitorRuntime r;
  r.uptime_ms = 9000;
  r.fsm = "move";
  r.voice = "idle";
  r.power = "展示";
  r.x = 242.5f;
  r.y = 290;
  r.queries = {2, 1, 0, 0};
  std::cout << spring::MonitorJson(c.Get(), r, 99) << '\n';
  assert(c.PackFull() == before);
}
