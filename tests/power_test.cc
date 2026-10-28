#include <cassert>
#include <iostream>
#include "spring_power_state.h"
using spring_power::Mode;
int main() {
    spring_power::State s;
    s.Activity(1000);
    s.Tick(30999, false); assert(s.mode == Mode::Play);
    s.Tick(31000, false); assert(s.mode == Mode::Quiet);
    s.Tick(120999, false); assert(s.mode == Mode::Quiet);
    s.Tick(121000, false); assert(s.mode == Mode::Sleep);
    assert(s.Touch(121010, true)); assert(s.mode == Mode::Play);
    assert(s.Touch(121100, true)); assert(s.Touch(121200, false));
    assert(!s.Touch(121300, true)); assert(!s.Touch(121400, false));
    // Voice/boot blockers reset the timer at completion, even across deadlines.
    s.Tick(300000, true); assert(s.mode == Mode::Play);
    s.Tick(329999, false); assert(s.mode == Mode::Play);
    s.Tick(330000, false); assert(s.mode == Mode::Quiet);
    // A held finger keeps the device awake, but missing frames cannot pin it.
    for (unsigned t = 330100; t < 500000; t += 100) {
        s.Touch(t, true); s.Tick(t, false); assert(s.mode == Mode::Play);
    }
    s.Touch(500000, false); s.Tick(530000, false); assert(s.mode == Mode::Quiet);
    s.Touch(540000, true); s.Tick(541600, false);
    s.Tick(570000, false); assert(s.mode == Mode::Quiet);
    s.Tick(660000, false); assert(s.mode == Mode::Sleep);
    s.Activity(660010); assert(s.mode == Mode::Play);
    assert(s.motion_block_until == 661510);
    std::cout << "Power transitions:30s/120s; wake-only full gesture; held/stale touch; voice blockers; pickup guard passed\n";
}
