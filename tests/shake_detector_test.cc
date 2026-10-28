#include <cassert>
#include <cmath>
#include <iostream>
#include "game/shake_detector.h"
using namespace desk_gap;
int main() {
    ShakeDetector still;
    for(int t=0;t<20000;t+=20) assert(!still.Sample(t,0.02f*std::sin(t),0,1));
    ShakeDetector lift;
    for(int t=0;t<2000;t+=20) assert(!lift.Sample(t,0,0,1));
    assert(!lift.Sample(2000,1.5f,0,1));
    for(int t=2020;t<4000;t+=20) assert(!lift.Sample(t,0,0,1));
    ShakeDetector alternating;
    for(int t=0;t<2000;t+=20) assert(!alternating.Sample(t,0,0,1));
    unsigned count=0;
    for(int t=2000;t<4000;t+=20) count+=alternating.Sample(t,1.5f*std::sin((t-2000)*3.1415926f/160),0,1);
    assert(count>=1 && count<=2);
    ShakeDetector gap;
    assert(!gap.Sample(0,0,0,1)); assert(!gap.Sample(5000,2,0,1)); assert(!gap.Sample(5020,-2,0,1));
    std::cout<<"Stationary, single lift, alternating shake, sample-gap reset passed\n";
}
