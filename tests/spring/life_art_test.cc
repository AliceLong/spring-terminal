#include "spring_gardener.h"
#include <cassert>
#include <iostream>
using namespace spring_gardener;
spring::Snapshot Scene(){
 spring::Core c;c.storage_ok=true;c.EnableEconomy(true);c.Wake();
 auto s=c.Get();s.work=spring::Work::Rest;s.work_ms=0;
 for(auto& p:s.plots)p.ready=false;
 s.tree_stage=2;s.economy.built=255;s.life.phase=3;return s;
}
int main(){
 for(unsigned target=0;target<4;++target){
  auto s=Scene();s.life.intent=s.life.reason=2;s.life.target=target;
  Controller c;bool direction=false,care=false;
  for(uint64_t t=100;t<=45000;t+=100){
   auto p=c.Step(s,t,Voice::None);
   if(p.mode==Mode::InspectPlant){
    if(p.animation==(target==0?Animation::InspectLeft:Animation::InspectRight))direction=true;
    if(p.animation==Animation::Care)care=true;
   }
  }
  assert(direction);assert(care==(target!=0));
 }
 auto s=Scene();s.life.intent=s.life.reason=3;s.life.under_tree=1;
 Controller c;
 for(uint64_t t=100;t<60000;t+=100)c.Step(s,t,Voice::None);
 auto p=c.Get();assert(LifeArrived(s,p)&&p.animation==Animation::RestTree);
 const float floor=p.y;
 for(uint64_t t=60000;t<65000;t+=100){p=c.Step(s,t,Voice::None);assert(std::abs(p.y-floor)<.01);}
 // Leaving the lower floor lifts locally, then travels; no jump to old319 anchor.
 s.life={};
 float previous_y=c.Get().y;
 for(uint64_t t=65000;t<70000;t+=100){
  p=c.Step(s,t,Voice::None);
  assert(std::abs(p.y-previous_y)<3.5);
  previous_y=p.y;
 }
 s.work=spring::Work::Harvest;s.work_ms=2500;
 Controller harvest;
 for(uint64_t t=100;t<10000;t+=100){p=harvest.Step(s,t,Voice::None);}
 assert(p.animation==Animation::Harvest&&p.frame==3); // Never loops another pickup.
 std::cout<<"New art: both inspection directions, empty care on normal3 slots, fixed rest foot, actual arrival, harvest single playback passed\n";
}
